"""
Serveur Flask pour le Diagnostiqueur d'Objets Suspects.

Reçoit les données de l'ESP32 (poids, couleur, hauteur, verdict) et les
stocke dans la base SQLite. Sert aussi l'interface tactile (résultat en
direct + historique) affichée sur l'écran du Raspberry Pi.

Lancement :
    python3 app.py

Le serveur écoute sur le port 5000, accessible depuis le réseau local à
l'adresse http://IP_DU_RASPBERRY:5000
"""

import sqlite3
from datetime import datetime
from flask import Flask, request, jsonify, render_template

app = Flask(__name__)
DB_NAME = "diagnostiqueur.db"


def get_connection():
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row  # permet d'accéder aux colonnes par nom
    return conn


# --------------------------------------------------------------------------
# Route appelée par l'ESP32 pour envoyer un nouveau scan
# --------------------------------------------------------------------------
@app.route("/scan", methods=["POST"])
def recevoir_scan():
    data = request.get_json(force=True, silent=True)

    if not data:
        return jsonify({"erreur": "Aucune donnée JSON reçue"}), 400

    poids = data.get("poids")
    couleur = data.get("couleur")
    hauteur = data.get("hauteur")
    verdict = data.get("verdict")
    date_heure = datetime.now().strftime("%d/%m/%Y %H:%M:%S")

    conn = get_connection()
    conn.execute(
        """
        INSERT INTO scans (date_heure, poids, couleur, hauteur, verdict)
        VALUES (?, ?, ?, ?, ?)
        """,
        (date_heure, poids, couleur, hauteur, verdict),
    )
    conn.commit()
    conn.close()

    print(f"[SCAN REÇU] {date_heure} | {poids}g | {couleur} | {hauteur}cm | {verdict}")

    return jsonify({"statut": "ok"}), 200


# --------------------------------------------------------------------------
# API interne utilisée par la page d'accueil pour se rafraîchir sans reload
# --------------------------------------------------------------------------
@app.route("/api/dernier")
def api_dernier():
    conn = get_connection()
    row = conn.execute(
        "SELECT * FROM scans ORDER BY id DESC LIMIT 1"
    ).fetchone()
    conn.close()

    if row is None:
        return jsonify(None)

    return jsonify(dict(row))


# --------------------------------------------------------------------------
# Page d'accueil : affiche le dernier résultat
# --------------------------------------------------------------------------
@app.route("/")
def accueil():
    return render_template("index.html")


# --------------------------------------------------------------------------
# Page historique : liste tous les scans passés
# --------------------------------------------------------------------------
@app.route("/historique")
def historique():
    conn = get_connection()
    rows = conn.execute(
        "SELECT * FROM scans ORDER BY id DESC"
    ).fetchall()
    conn.close()

    return render_template("historique.html", scans=rows)


if __name__ == "__main__":
    # host="0.0.0.0" est indispensable pour que l'ESP32 (sur le réseau
    # local) puisse joindre ce serveur, pas seulement le Raspberry Pi lui-même
    app.run(host="0.0.0.0", port=5000, debug=True)
