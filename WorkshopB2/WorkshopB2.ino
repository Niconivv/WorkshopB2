/*
  Détecteur d'objet suspect

  Principe :
  Un capteur ultrason surveille
  Dès qu'un objet est détecté à proximité (distance < seuil),
    on attend 2 secondes (le temps que la main qui a posé l'objet s'écarte) avant de lancer la mesure.
  On lit alors :
   le poids via le capteur de charge 5kg + HX711
   - la couleur via le module TCS3200 / GY-31
  On decide si l'objet est "suspect" ou "normal", et on affiche le résultat :
    LED verte allumée + silence = objet normal
    LED rouge allumée + buzzer = objet sus
*/

#include <HX711.h>

// conf des broches

// capteur de poid
#define HX711_DT_PIN   0
#define HX711_SCK_PIN  0

// cap ulrason
#define TRIG_PIN       0
#define ECHO_PIN       0

// capteur couleur
#define TCS_S0_PIN     0
#define TCS_S1_PIN     0
#define TCS_S2_PIN     0
#define TCS_S3_PIN     0
#define TCS_OUT_PIN    0

// LED
#define LED_OK_PIN     -1   // verte
#define LED_ALERT_PIN  -1   // rouge

// Buzzer
#define BUZZER_PIN     -1

// parametre detection

// Distance (cm) ou on considère qu'un objet est poser
const float DISTANCE_DETECTION_CM = 10.0;

// Délai (ms) a attendre après détection avant de lancer la mesure
const unsigned long DELAI_STABILISATION_MS = 2000;

// Scan tt les 4 sec pour eviter de re-scanner en boucle
const unsigned long DELAI_ANTI_REBOND_MS = 4000;

// Calibration du capteur de poids : a ajuster avec un poids étalon connu
// (voir méthode tt en bas)
float FACTEUR_CALIBRATION = -7050.0;

// objet

HX711 balance;

enum EtatCouleur { ROUGE, VERT, BLEU, JAUNE, ORANGE, VIOLET, BLANC, NOIR, COULEUR_INCONNUE };

// setup

void setup() {
  Serial.begin(115200);
  delay(200);

  // ultrason
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // couleur
  pinMode(TCS_S0_PIN, OUTPUT);
  pinMode(TCS_S1_PIN, OUTPUT);
  pinMode(TCS_S2_PIN, OUTPUT);
  pinMode(TCS_S3_PIN, OUTPUT);
  pinMode(TCS_OUT_PIN, INPUT);

  // netteté vs vitesse, + bas = + nette actuellement 20% (20% recommandé)
  digitalWrite(TCS_S0_PIN, HIGH);
  digitalWrite(TCS_S1_PIN, LOW);

  // LEDs
  if (LED_OK_PIN >= 0)    pinMode(LED_OK_PIN, OUTPUT);
  if (LED_ALERT_PIN >= 0) pinMode(LED_ALERT_PIN, OUTPUT);

  // Buzzer
  if (BUZZER_PIN >= 0) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
  }
  
  //john seed
  randomSeed(esp_random());

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
    delay(DELAI_STABILISATION_MS); // le delai

    // On revérifie que l'objet est toujours là après le délai
    float distanceApres = mesurerDistanceCm();
    if (distanceApres > 0 && distanceApres < DISTANCE_DETECTION_CM) {
      analyserObjet();
      dernierScanMs = millis();
    } else {
      Serial.println("Rien détecté après le délai (fausse alerte).");
    }
  }

  delay(150); // petite pause pour ne pas spammer le capteur ultrason
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

  Serial.print("Couleur detectee : ");
  Serial.println(couleurVersTexte(couleur));

  Serial.print("Verdict : ");
  Serial.println(suspect ? "OBJET SUSPECT !" : "Objet normal");
  Serial.println("---------------------------------");

  // LED verte allumée si normal, rouge allumée si suspect
  if (LED_OK_PIN >= 0)    digitalWrite(LED_OK_PIN, suspect ? LOW : HIGH);
  if (LED_ALERT_PIN >= 0) digitalWrite(LED_ALERT_PIN, suspect ? HIGH : LOW);

  // Buzzer : sonne uniquement si l'objet est sussy
  if (suspect) {
    sonnerAlerte();
  }
}

// fait sonner le buzzer par bips courts (3 fois) si objet suspect
void sonnerAlerte() {
  if (BUZZER_PIN < 0) return;

  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

bool estSuspect(float poidsG, EtatCouleur couleur) {
  int resultat = random(0, 2); // 0 ou 1
  return (resultat == 1);
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

// mesure couleur

EtatCouleur mesurerCouleur() {
  int rouge = lireFrequenceCouleur(LOW, LOW);   // filtre rouge
  int vert  = lireFrequenceCouleur(HIGH, HIGH); // filtre vert
  int bleu  = lireFrequenceCouleur(LOW, HIGH);  // filtre bleu

  Serial.print("RGB brut -> R:");
  Serial.print(rouge);
  Serial.print(" V:");
  Serial.print(vert);
  Serial.print(" B:");
  Serial.println(bleu);

  int total = rouge + vert + bleu;

  // tres peu de lumiere sur les 3 canaux -> noir
  if (total < 30) {
    return NOIR;
  }

  // on met chaque canal en % du total pour ignorer la luminosite ambiante
  float rPct = (float)rouge / total;
  float vPct = (float)vert  / total;
  float bPct = (float)bleu  / total;

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

// Lit la fréquence de sortie du TCS3200 pour un filtre donné (R, V ou B)
// Note : avec le TCS3200, plus la fréquence est BASSE, plus la couleur
// mesurée est INTENSE (ce qui correspond ici, après inversion, à des
// valeurs plus GRANDES = plus de lumière/couleur détectée).
int lireFrequenceCouleur(int s2, int s3) {
  digitalWrite(TCS_S2_PIN, s2);
  digitalWrite(TCS_S3_PIN, s3);
  delay(50); // laisse le temps au capteur de se stabiliser

  unsigned long duree = pulseIn(TCS_OUT_PIN, LOW, 50000);
  if (duree == 0) return 0;

  // On inverse pour que "plus grand = plus de couleur/lumière"
  return (int)(100000UL / duree);
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