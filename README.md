# HackFlow — Smart Hackathon Management
## Module 6 : Gestion du Jury et des Évaluations
**Équipe ChronoHack / NovaCrew • Année Universitaire : 2025 - 2026**  
*Membres de l'équipe : Ines Chkambou, Hanin Dhaouafy, Nayrouz Slimi, Tasnim Younsi*

---

## 📌 Présentation du Projet
Ce projet constitue le **Module 6 (Gestion du Jury et des Évaluations)** de l'application de bureau **HackFlow (Smart Hackathon Management)** développé en **C++ avec le framework Qt (Qt Creator / qmake)**.

L'interface a été conçue pour reproduire fidèlement l'esthétique graphique de la maquette HackFlow (palette de couleurs sombre #101B31, accents néon cyan #0BF8F6, violet #4B26A0 et bleu-violet #2F67E0).

---

## 🎯 Conformité aux "Bonnes Pratiques" Universitaires
Le projet respecte à 100% les consignes méthodologiques et techniques énoncées dans le guide de l'enseignant :

1. **Nom du projet thématique** : `SmartHackathon` / `HackFlow`.
2. **Système de build qmake** : Utilisation exclusive du fichier `SmartHackathon.pro` (plus simple et direct sous Qt Creator que CMake).
3. **Nommage de la classe principale** : La classe principale est nommée `GJury` (préfixe **"G"** pour Gestion, suivi du sujet).
4. **Fichier .UI unique** : Strictement **un seul fichier d'interface** `gjury.ui`.
5. **Gestion multi-vues par conteneur** : Toutes les sous-interfaces sont organisées dans un **`QStackedWidget`** :
   - **Page 0** : *Gestion des Jurys* (Vue exacte de la maquette avec formulaire CRUD gauche, tableau droit, et graphiques Donut / Bâtons en bas).
   - **Page 1** : *Gestion des Évaluations & Grille de Notation* (Innovation, Technique, UX, Pitch, note globale pondérée en direct).
   - **Page 2** : *Métiers Innovants* (Classement automatique avec départage des ex æquo et détecteur d'écarts de notation).
   - **Page 3** : *Tableau de Bord Global* (Indicateurs clés 28 participants, 7 équipes, 12 projets, 8 jurés).
   - **Page 4** : *Authentification & Privilèges* (Connexion sécurisée avec masquage de mot de passe et gestion des rôles).
   - **Page 7** : *Saisie et Grille d'Évaluation des Projets* (raccourci « Voir les évaluations » de la page Jury : grille à 7 critères pondérés avec note finale automatique, enregistrement dans la table EVALUATION, liste avec recherche / filtres / tri / pagination, consultation, modification, suppression ; **Retour** ramène à la page Jury).
   - **Page 6** : *Planning des évaluations* (bouton « Planning des évaluations » de la page Jury : filtres, mini-calendrier, recherche, vues jour/semaine/mois, ajout / consultation / modification / suppression de sessions, export PDF/CSV, pagination ; **Retour** ramène à la page Jury).
   - **Page 5** : *Paramètres du jury* (bouton « Paramètres du jury » en bas à droite de la page Jury : réglages enregistrés en base + liste des jurys avec recherche, filtre, tri, pagination ; le bouton **Retour** ramène à la page Jury).
6. **Gestion des accès et privilèges** : Rôles *Administrateur / Organisateur*, *Membre du Jury* et *Mentor* limitant dynamiquement les actions autorisées.

---

## ⚙️ Fonctionnalités Implémentées (Tableau 7 du Cahier des Spécifications)

### 1. CRUD Complet
- **Ajouter** un juré avec validation de saisie (nom, prénom, email unique avec regex, entreprise, spécialité, disponibilité).
- **Consulter** la fiche complète d'un expert avec l'historique détaillé de tous les projets qu'il a évalués et sa moyenne de notation.
- **Modifier** les informations d'un juré existant (chargement automatique dans le formulaire).
- **Supprimer** un juré avec boîte de dialogue de confirmation et gestion en cascade.

### 2. Les 4 Métiers de Base
1. **Recherche instantanée** : Filtre en temps réel sur la saisie textuelle (nom, prénom, entreprise, email, spécialité) combinable avec les listes déroulantes de spécialités et statuts.
2. **Tri multi-critères** : Tri croissant et décroissant (Nom A-Z, Nom Z-A, Entreprise, Spécialité, ID, Note moyenne).
3. **Exports professionnels** :
   - **Export PDF Officiel** : Génération d'un rapport PDF soigné aux couleurs de HackFlow avec en-tête, statistiques récapitulatives, tableau complet et zone de signature.
   - **Export CSV / Excel** : Fichier tableur avec encodage UTF-8 BOM pour une ouverture parfaite sous Microsoft Excel.
4. **Statistiques visuelles intégrées** :
   - **Donut Chart** : Camembert évidé personnalisé affichant la répartition par domaine d'expertise, avec centre dynamique et légende en pourcentages.
   - **Bar Chart** : Histogramme à barres arrondies et dégradés néon illustrant la charge d'évaluation de chaque juré.

### 3. Les 2 Métiers Innovants
1. **Classement automatique avec départage des ex æquo** :
   - Calcul automatique de la moyenne pondérée de chaque projet.
   - Algorithme de résolution d'égalité : en cas de score identique, départage par la note d'*Innovation*, puis par la note *Technique*, puis par la *Présentation UX*.
   - Affichage du podium (🥇 1er, 🥈 2ème, 🥉 3ème) et export PDF du palmarès officiel.
2. **Détection d'écart entre jurés (Alerte d'incohérence / Divergence)** :
   - Détection statistique en temps réel des projets évalués par plusieurs jurés avec un écart $\ge 4.0$ points ou un écart-type $\sigma \ge 2.0$.
   - Signalement par badges "CRITIQUE" ou "MODÉRÉE", affichage des deux jurés aux avis opposés et proposition d'une réunion d'harmonisation.

---

## 🚀 Comment Ouvrir et Exécuter le Projet dans Qt Creator

1. Lancez **Qt Creator**.
2. Cliquez sur **Fichier (File)** > **Ouvrir un fichier ou projet (Open File or Project)**.
3. Naviguez vers le dossier décompressé et sélectionnez le fichier **`SmartHackathon.pro`**.
4. Dans la fenêtre de configuration du kit (Kits Selection), choisissez votre kit standard (Desktop Qt 5 ou Qt 6 avec MinGW ou MSVC) et cliquez sur **Configure Project**.
5. Cliquez sur l'icône de **marteau gris** en bas à gauche (ou `Ctrl + B`) pour compiler.
6. Cliquez sur l'icône de **flèche verte** (ou `Ctrl + R`) pour exécuter l'application !

> [!NOTE]
> L'application utilise par défaut une base SQLite locale (`smarthackathon.db`) qui est créée et pré-remplie automatiquement dès le premier lancement. **Aucune installation de serveur externe n'est requise** pour tester immédiatement l'intégralité du projet.

### Pour basculer sur la base Oracle (si requis pour la soutenance) :

1. Copiez le modèle `config.example.ini` en `config.local.ini` (à la racine du projet ou à côté de l'exécutable).
2. Renseignez vos identifiants Oracle dans la section `[oracle]` de `config.local.ini`.
3. Dans `main.cpp`, passez l'argument à `true` :

```cpp
conn.createConnection(true);  // Mode Oracle (QODBC / QOCI)
```

> [!IMPORTANT]
> `config.local.ini` contient des identifiants : il est **ignoré par Git** et ne doit jamais être commité.
> Si le fichier est absent ou incomplet, l'application bascule automatiquement en SQLite (mode par défaut).
> Le chemin du fichier peut aussi être fourni par la variable d'environnement `HACKFLOW_CONFIG`.

---

## 📂 Arborescence des Fichiers
```
SmartHackathon/
├── SmartHackathon.pro       # Configuration qmake Qt (C++17, core, gui, widgets, sql, printsupport)
├── main.cpp                 # Point d'entrée de l'application
├── gjury.h                  # Déclaration de la classe principale GJury
├── gjury.cpp                # Logique métier, slots, affichage et navigation
├── gjury.ui                 # Formulaire unique Qt Designer (avec QStackedWidget)
├── connection.h             # Gestionnaire de base de données (SQLite & Oracle)
├── connection.cpp           # Schéma SQL, triggers et données de démonstration
├── jury.h                   # Modèle Jury (CRUD, Recherche, Tri, Exports, Stats)
├── jury.cpp                 # Implémentation du modèle Jury
├── evaluation.h             # Modèle Évaluation et Métiers Innovants (Classement, Divergences)
├── evaluation.cpp           # Algorithmes de départage et calculs statistiques
├── chartwidgets.h           # Widgets graphiques personnalisés (Donut et Barres)
├── chartwidgets.cpp         # Rendu QPainter haute définition
├── parametresjury.h/.cpp    # Page « Paramètres du jury » (page 5 du QStackedWidget)
├── planningevaluations.h/.cpp # Page « Planning des évaluations » (page 6)
├── evaluationprojets.h/.cpp # Page « Saisie et Grille d'Évaluation des Projets » (page 7)
├── uihelpers.h              # Éléments graphiques partagés par les pages 5 et 6
├── resources.qrc            # Fichier de ressources Qt
├── creer_zip.bat            # Script batch pour générer l'archive ZIP en 1 clic
└── README.md                # Documentation complète du projet
```
