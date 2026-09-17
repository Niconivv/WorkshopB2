/* 
  Détecteur d'objet suspect (décision par bibliothèque couleur/poids) 
 
  Principe : 
  Un capteur ultrason surveille 
  Dès qu'un objet est détecté à proximité (distance < seuil), 
    on attend 5 sec (le temps que la main qui a posé l'objet s'écarte) avant de lancer la mesure. 
  On lit alors : 
   le poids via le capteur de charge 5kg  et la couleur via le capteur couleur 
  On consulte une bibliothèque (table couleur x tranche de poids) pour 
  décider si l'objet est "suspect" ou "normal", et on affiche le résultat : 
    LED verte allumée + silence = objet normal 
    LED rouge allumée + buzzer = objet sus 
*/ 
 
#include <HX711.h> 
#include <Wire.h> 
#include <Adafruit_TCS34725.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi
const char* ssid = "Bonjour";
const char* password = "aurevoir";

// Raspberry Pi
const char* serverUrl = "http://172.20.10.7:5000/scan";
 
// conf des broches 
 
// capteur de poid 
#define HX711_DT_PIN   4 
#define HX711_SCK_PIN 18 
 
// cap ulrason 
#define TRIG_PIN       25 
#define ECHO_PIN       26 
 
// capteur couleur 
#define TCS_SDA_PIN    22 
#define TCS_SCL_PIN    23 
 
// LED 
#define LED_OK_PIN     13 
#define LED_ALERT_PIN  14 
 
// Buzzer 
#define BUZZER_PIN     27 
 
// parametre detection 
 
// Distance (cm) ou on considère qu'un objet est poser 
const float DISTANCE_DETECTION_CM = 10.0; 
 
// Délai (ms) a attendre après détection avant de lancer la mesure 
const unsigned long DELAI_STABILISATION_MS = 4000; 
 
// Scan tt les 4 sec pour eviter de re-scanner en boucle 
const unsigned long DELAI_ANTI_REBOND_MS = 4000; 
 
// Calibration du capteur de poids : a ajuster avec un poids étalon connu 
// (voir méthode tt en bas) 
float FACTEUR_CALIBRATION = -7050.0; 
 
// objet globaux 
 
HX711 balance; 
 
// Capteur de couleur : 
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X); 
 
enum EtatCouleur { ROUGE, VERT, BLEU, JAUNE, ORANGE, VIOLET, BLANC, NOIR, COULEUR_INCONNUE }; 
 
// se qui définit sus et norm 
#include "bibliotheque_suspect.h" 
 
// connexion WiFi

void connexionWiFi() {
  Serial.print("Connexion au WiFi ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);

  int tentatives = 0;

  while (WiFi.status() != WL_CONNECTED && tentatives < 30) {
    delay(500);
    Serial.print(".");
    tentatives++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connecte ! IP ESP32 : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Echec de connexion WiFi !");
  }
}

// envoi vers la Raspberry

void envoyerScan(float poids, String couleur, String verdict) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecte, envoi annule.");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<384> doc;
  doc["poids"] = poids;
  doc["couleur"] = couleur;
  doc["verdict"] = verdict;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("Envoi reussi, code HTTP : ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Erreur envoi, code : ");
    Serial.println(httpResponseCode);
  }

  http.end();
}
 
// setup 
 
void setup() { 
  Serial.begin(115200); 
  delay(200); 

  connexionWiFi();
 
  // ultrason 
  pinMode(TRIG_PIN, OUTPUT); 
  pinMode(ECHO_PIN, INPUT); 
 
  // couleur 
  Wire.begin(TCS_SDA_PIN, TCS_SCL_PIN); 
  if (!tcs.begin()) { 
    Serial.println("Capteur de couleur TCS34725 non detecte."); 
  } 
 
  // LEDs 
  if (LED_OK_PIN >= 0)    pinMode(LED_OK_PIN, OUTPUT); 
  if (LED_ALERT_PIN >= 0) pinMode(LED_ALERT_PIN, OUTPUT); 
 
  // Buzzer 
  if (BUZZER_PIN >= 0) { 
    pinMode(BUZZER_PIN, OUTPUT); 
    digitalWrite(BUZZER_PIN, LOW); 
  } 
 
  // poids 
  balance.begin(HX711_DT_PIN, HX711_SCK_PIN); 
  balance.set_scale(FACTEUR_CALIBRATION); 
  balance.tare(); // remet le poids à zéro 
 
  Serial.println("pret. en attente d'un objet"); 
} 
 
// boucle 
 
unsigned long dernierScanMs = 0; 
 
void loop() { 
  float distance = mesurerDistanceCm(); 
 
  bool objetDetecte = (distance > 0 && distance < DISTANCE_DETECTION_CM); 
  bool delaiEcoule = (millis() - dernierScanMs) > DELAI_ANTI_REBOND_MS; 
 
  if (objetDetecte && delaiEcoule) { 
    Serial.println("Objet detecte, attente de stabilisation..."); 
    delay(DELAI_STABILISATION_MS); 
 
    // On check que l'objet est toujours la apres le délai 
    float distanceApres = mesurerDistanceCm(); 
    if (distanceApres > 0 && distanceApres < DISTANCE_DETECTION_CM) { 
      analyserObjet(); 
      dernierScanMs = millis(); 
    } else { 
      Serial.println("Rien détecté après le délai (fausse alerte)."); 
    } 
  } 
 
  delay(150); 
} 
 
// analyse 
 
void analyserObjet() { 
  float poids = mesurerPoidsG(); 
  EtatCouleur couleur = mesurerCouleur(); 
 
  bool suspect = estSuspect(poids, couleur); 
 
  Serial.println("---------------------------------"); 
  Serial.print("Poids mesure : "); 
  Serial.print(poids); 
  Serial.println(" g"); 
 
  Serial.print("Tranche de poids : "); 
  Serial.println(trouverTrancheIndex(poids)); 
 
  Serial.print("Verdict : "); 
  Serial.println(suspect ? "OBJET SUSPECT !" : "Objet normal"); 
  Serial.println("---------------------------------"); 
 
  if (LED_OK_PIN >= 0)    digitalWrite(LED_OK_PIN, suspect ? LOW : HIGH); 
  if (LED_ALERT_PIN >= 0) digitalWrite(LED_ALERT_PIN, suspect ? HIGH : LOW); 
 
  if (suspect) { 
    sonnerAlerte(); 
  }

  String verdict = suspect ? "OBJET SUSPECT !" : "Objet normal";
  envoyerScan(poids, String(couleurVersTexte(couleur)), verdict);
} 
 
void sonnerAlerte() { 
  if (BUZZER_PIN < 0) return; 
 
  for (int i = 0; i < 3; i++) { 
    digitalWrite(BUZZER_PIN, HIGH); 
    delay(200); 
    digitalWrite(BUZZER_PIN, LOW); 
    delay(150); 
  } 
} 
 
// decision par bibliotheque (table couleur x tranche de poids) 
bool estSuspect(float poidsG, EtatCouleur couleur) { 
  return consulterBibliotheque(couleur, poidsG); 
} 
 
const char* couleurVersTexte(EtatCouleur c) { 
  switch (c) { 
    case ROUGE:  return "Rouge"; 
    case VERT:   return "Vert"; 
    case BLEU:   return "Bleu"; 
    case JAUNE:  return "Jaune"; 
    case ORANGE: return "Orange"; 
    case VIOLET: return "Violet"; 
    case BLANC:  return "Blanc"; 
    case NOIR:   return "Noir"; 
    default:     return "Inconnue"; 
  } 
} 
 
// ultrason 
 
float mesurerDistanceCm() { 
  digitalWrite(TRIG_PIN, LOW); 
  delayMicroseconds(2); 
  digitalWrite(TRIG_PIN, HIGH); 
  delayMicroseconds(10); 
  digitalWrite(TRIG_PIN, LOW); 
 
  long duree = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms (~5m max) 
  if (duree == 0) return -1; // rien détecté 
 
  float distanceCm = duree * 0.0343 / 2.0; 
  return distanceCm; 
} 
 
// mesure poid 
 
float mesurerPoidsG() { 
  if (!balance.is_ready()) { 
    Serial.println("capteur de poids non pret !"); 
    return 0; 
  } 
  // moyenne de 5 lectures pour stabiliser la mesure 
  float grammes = balance.get_units(5); 
  if (grammes < 0) grammes = 0; 
  return grammes; 
} 
 
// mesure couleur (TCS34725 via I2C) 
 
EtatCouleur mesurerCouleur() { 
  uint16_t r, g, b, c; 
  tcs.getRawData(&r, &g, &b, &c); 
 
  Serial.print("RGB brut -> R:"); 
  Serial.print(r); 
  Serial.print(" V:"); 
  Serial.print(g); 
  Serial.print(" B:"); 
  Serial.print(b); 
  Serial.print(" Clair:"); 
  Serial.println(c); 
 
  // canal "clair" (lumiere totale) tres faible -> rien ou objet noir 
  if (c < 50) { 
    return NOIR; 
  } 
 
  int total = r + g + b; 
  if (total == 0) { 
    return NOIR; 
  } 
 
  // on met chaque canal en % du total pour ignorer la luminosite ambiante 
  float rPct = (float)r / total; 
  float vPct = (float)g / total; 
  float bPct = (float)b / total; 
 
  float maxPct = max(rPct, max(vPct, bPct)); 
  float minPct = min(rPct, min(vPct, bPct)); 
 
  // les 3 canaux sont proches -> blanc 
  if ((maxPct - minPct) < 0.08) { 
    return BLANC; 
  } 
 
  // rouge + vert forts, bleu faible -> jaune 
  if (rPct > 0.30 && vPct > 0.30 && bPct < 0.25) { 
    return JAUNE; 
  } 
 
  // rouge dominant, un peu de vert, bleu faible -> orange 
  if (rPct > bPct && vPct > bPct && (rPct - vPct) > 0.10 && (rPct - bPct) > 0.15) { 
    return ORANGE; 
  } 
 
  // rouge + bleu forts, vert nettement plus faible -> violet 
  if (rPct > vPct && bPct > vPct && (rPct - vPct) > 0.10 && (bPct - vPct) > 0.10) { 
    return VIOLET; 
  } 
 
  // sinon on regarde juste quel canal domine 
  if (rPct >= vPct && rPct >= bPct) return ROUGE; 
  if (vPct >= rPct && vPct >= bPct) return VERT; 
  if (bPct >= rPct && bPct >= vPct) return BLEU; 
 
  return COULEUR_INCONNUE; 
} 
 
/* 
  a faire pour le capteur de poids d'apres tuto 
   telecharger un code qui affiche balance.get_units(10) en boucle, avec FACTEUR_CALIBRATION = 1.0 
     et le plateau VIDE (tare() appelé au démarrage). 
    Posez un poids connu (ex: 100g) sur le plateau, notez la valeur brute affichée (ex: -70500). 
    Calculez : FACTEUR_CALIBRATION = valeur_brute / poids_connu_en_g 
     (ex: -70500 / 100 = -705.0) 
    Mettre cette valeur dans FACTEUR_CALIBRATION ci-dessus. 
   
*/