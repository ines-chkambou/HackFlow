HackFlow - Interface Jury (Qt Widgets, qmake)

Ouvrir : Qt Creator > Fichier > Ouvrir un fichier ou un projet... > HackFlow_Jury.pro
Puis "Configure Project" et lancer avec le bouton vert (Ctrl+R).

Fichiers :
  HackFlow_Jury.pro   projet qmake
  main.cpp            point d'entree
  mainwindow.h/.cpp   logique (tableau, recherche, pagination, tri, export, fiche d'evaluation)
  scrollhelpers.h     defilement fluide de la molette (header seulement)
  mainwindow.ui       UNIQUE fichier .ui (barre laterale + QStackedWidget + page Jury defilante)
  resources.qrc       ressources Qt
  images/logo.jpg     logo HackFlow (fichier fourni, non modifie)
  images/icons/       icones de navigation et d'actions (PNG)
