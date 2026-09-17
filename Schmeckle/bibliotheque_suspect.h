#ifndef BIBLIOTHEQUE_SUSPECT_H
#define BIBLIOTHEQUE_SUSPECT_H

// Nombre de tranches
#define NB_TRANCHES 4

//   Tranche 0  poids <= 50g
//   Tranche 1  poids <= 150g
//   Tranche 2  poids <= 300g
//   Tranche 3  poids >  300g (derniere borne = "illimite")
const float BORNES_POIDS_G[NB_TRANCHES] = { 50.0, 150.0, 300.0, 999999.0 };

// Table de correspondance couleur, statut par tranche de poids.
// L'ordre des lignes correspond a l'enum EtatCouleur.
// true = suspect, false = normal
//
//                          Tranche0  Tranche1  Tranche2  Tranche3
const bool TABLE_SUSPECT[COULEUR_INCONNUE + 1][NB_TRANCHES] = {
  /* ROUGE            */  { false,    false,    true,     true  },
  /* VERT             */  { false,    false,    false,    true  },
  /* BLEU             */  { false,    false,    false,    true  },
  /* JAUNE            */  { false,    false,    true,     true  },
  /* ORANGE           */  { false,    true,     true,     true  },
  /* VIOLET           */  { false,    false,    true,     true  },
  /* BLANC            */  { true,     true,     true,     true  },
  /* NOIR             */  { false,    true,     true,     true  },
  /* COULEUR_INCONNUE */  { true,     true,     true,     true  },
};

// Retourne l'index de tranche (0 a NB_TRANCHES-1) correspondant a un poids
int trouverTrancheIndex(float poidsG) {
  for (int i = 0; i < NB_TRANCHES; i++) {
    if (poidsG <= BORNES_POIDS_G[i]) {
      return i;
    }
  }
  return NB_TRANCHES - 1; // securite, ne devrait pas arriver
}

// Fonction principale de decision : consulte la bibliotheque
bool consulterBibliotheque(EtatCouleur couleur, float poidsG) {
  int tranche = trouverTrancheIndex(poidsG);
  return TABLE_SUSPECT[couleur][tranche];
}

#endif
