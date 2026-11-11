#include "connection.h"
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QStringList>
#include <QCoreApplication>
#include <QSettings>

Connection::Connection() : isOracle(false)
{
}

Connection::~Connection()
{
    closeConnection();
}

QString Connection::cheminFichierConfig()
{
    const QString env = qEnvironmentVariable("HACKFLOW_CONFIG");
    if (!env.isEmpty() && QFile::exists(env))
        return env;

    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidat = dir.filePath(QStringLiteral("config.local.ini"));
        if (QFile::exists(candidat))
            return candidat;
        if (!dir.cdUp())
            break;
    }

    const QString courant = QDir::currentPath() + QStringLiteral("/config.local.ini");
    if (QFile::exists(courant))
        return courant;

    return QString();
}

bool Connection::initialiserSqlite()
{
    if (QSqlDatabase::contains(QStringLiteral("qt_sql_default_connection"))) {
        db = QSqlDatabase::database(QStringLiteral("qt_sql_default_connection"));
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
        db.setDatabaseName(QStringLiteral("smarthackathon.db"));
    }
    return true;
}

bool Connection::configurerOracle()
{
    const QString chemin = cheminFichierConfig();
    if (chemin.isEmpty()) {
        qWarning() << "Aucun fichier config.local.ini trouve.";
        return false;
    }

    QSettings cfg(chemin, QSettings::IniFormat);
    const QString host = cfg.value(QStringLiteral("oracle/host")).toString().trimmed();
    const QString port = cfg.value(QStringLiteral("oracle/port")).toString().trimmed();
    const QString service = cfg.value(QStringLiteral("oracle/service")).toString().trimmed();
    const QString user = cfg.value(QStringLiteral("oracle/user")).toString().trimmed();
    const QString password = cfg.value(QStringLiteral("oracle/password")).toString();
    QString odbcDriver = cfg.value(QStringLiteral("oracle/odbcDriver")).toString().trimmed();

    if (host.isEmpty() || port.isEmpty() || service.isEmpty()
            || user.isEmpty() || password.isEmpty()) {
        qWarning() << "Configuration Oracle incomplete dans" << chemin << ": repli sur SQLite.";
        return false;
    }

    if (odbcDriver.isEmpty())
        odbcDriver = QStringLiteral("Oracle in instantclient_19_8");

    if (QSqlDatabase::isDriverAvailable(QStringLiteral("QODBC"))) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"));
        // La chaine DSN ne contient pas les identifiants : ils sont passes separement.
        db.setDatabaseName(QStringLiteral("DRIVER={%1};Dbq=%2:%3/%4;").arg(odbcDriver, host, port, service));
        db.setUserName(user);
        db.setPassword(password);
    } else if (QSqlDatabase::isDriverAvailable(QStringLiteral("QOCI"))) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QOCI"));
        db.setHostName(host);
        db.setDatabaseName(service);
        db.setUserName(user);
        db.setPassword(password);
        db.setPort(port.toInt());
    } else {
        qWarning() << "Pilote Oracle indisponible, repli automatique sur SQLite.";
        return false;
    }
    return true;
}

bool Connection::createConnection(bool useOracle)
{
    isOracle = useOracle;

    if (isOracle) {
        // Configuration Oracle lue depuis config.local.ini (fichier local ignore par Git).
        // Si le fichier est absent ou incomplet, repli automatique sur SQLite.
        if (!configurerOracle()) {
            qWarning() << "Configuration Oracle absente ou incomplete : repli automatique sur SQLite.";
            isOracle = false;
            initialiserSqlite();
        }
    } else {
        // Mode SQLite : 100% autonome, fonctionne instantanÃ©ment sur n'importe quel PC
        if (QSqlDatabase::contains("qt_sql_default_connection")) {
            db = QSqlDatabase::database("qt_sql_default_connection");
        } else {
            db = QSqlDatabase::addDatabase("QSQLITE");
            QString dbPath = "smarthackathon.db";
            db.setDatabaseName(dbPath);
        }
    }

    if (!db.open()) {
        qCritical() << "Erreur de connexion Ã  la base de donnÃ©es:" << db.lastError().text();
        return false;
    }

    qDebug() << "Connexion Ã©tablie avec succÃ¨s avec le pilote:" << db.driverName();

    // Ancienne base SQLite (sans les colonnes tÃ©lÃ©phone / rÃ´le / Ã©quipe) : on la sauvegarde puis on repart d'une base neuve
    if (db.driverName() == "QSQLITE") {
        bool ancienSchema = false;
        {
            QSqlQuery pragma("PRAGMA table_info(JURY)");
            QStringList colonnes;
            while (pragma.next()) colonnes << pragma.value(1).toString();
            ancienSchema = !colonnes.isEmpty() && !colonnes.contains("role");
        }
        if (ancienSchema) {
            const QString chemin = db.databaseName();
            db.close();
            QFile::remove(chemin + ".ancien.bak");
            QFile::rename(chemin, chemin + ".ancien.bak");
            qWarning() << "Ancien schÃ©ma dÃ©tectÃ© : base sauvegardÃ©e dans" << (chemin + ".ancien.bak");
            if (!db.open()) {
                qCritical() << "Impossible de recrÃ©er la base:" << db.lastError().text();
                return false;
            }
        }
    }
    return initializeDatabase();
}

void Connection::closeConnection()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool Connection::initializeDatabase()
{
    QSqlQuery query;

    // Table JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
    QString createJuryTable =
        "CREATE TABLE IF NOT EXISTS JURY ("
        "id_jury INTEGER PRIMARY KEY AUTOINCREMENT, "
        "nom TEXT NOT NULL, "
        "prenom TEXT NOT NULL, "
        "email TEXT NOT NULL UNIQUE, "
        "telephone TEXT NOT NULL DEFAULT '', "
        "role TEXT NOT NULL, "
        "equipe TEXT NOT NULL, "
        "disponibilite TEXT NOT NULL DEFAULT 'Disponible'"
        ");";

    if (!query.exec(createJuryTable)) {
        qWarning() << "Erreur crÃ©ation table JURY:" << query.lastError().text();
    }

    // Table PROJET (id_projet, titre, nom_equipe, statut)
    QString createProjectTable =
        "CREATE TABLE IF NOT EXISTS PROJET ("
        "id_projet INTEGER PRIMARY KEY, "
        "titre TEXT NOT NULL, "
        "nom_equipe TEXT NOT NULL, "
        "statut TEXT NOT NULL"
        ");";

    if (!query.exec(createProjectTable)) {
        qWarning() << "Erreur crÃ©ation table PROJET:" << query.lastError().text();
    }

    // Table EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
    QString createEvalTable =
        "CREATE TABLE IF NOT EXISTS EVALUATION ("
        "id_evaluation INTEGER PRIMARY KEY AUTOINCREMENT, "
        "id_jury INTEGER NOT NULL, "
        "id_projet INTEGER NOT NULL, "
        "note REAL NOT NULL, "
        "critere_innovation REAL DEFAULT 0, "
        "critere_technique REAL DEFAULT 0, "
        "critere_presentation REAL DEFAULT 0, "
        "critere_pitch REAL DEFAULT 0, "
        "commentaire TEXT, "
        "date_evaluation TEXT NOT NULL, "
        "FOREIGN KEY(id_jury) REFERENCES JURY(id_jury) ON DELETE CASCADE, "
        "FOREIGN KEY(id_projet) REFERENCES PROJET(id_projet) ON DELETE CASCADE"
        ");";

    if (!query.exec(createEvalTable)) {
        qWarning() << "Erreur crÃ©ation table EVALUATION:" << query.lastError().text();
    }

    // Colonnes supplÃ©mentaires de EVALUATION pour la grille Ã  7 critÃ¨res (page Â« Saisie et Grille d'Ã‰valuation Â») :
    // ajoutÃ©es sans perdre les donnÃ©es existantes. Correspondance avec les colonnes historiques :
    // Innovation = critere_innovation, FaisabilitÃ© = critere_technique, PrÃ©sentation = critere_presentation.
    {
        QStringList colonnes;
        QSqlQuery pragma("PRAGMA table_info(EVALUATION)");
        while (pragma.next()) colonnes << pragma.value(1).toString();
        for (const QString &c : {QString("crit_qualite"), QString("crit_impact"), QString("crit_business"), QString("crit_equipe")}) {
            if (!colonnes.contains(c) && !query.exec(QString("ALTER TABLE EVALUATION ADD COLUMN %1 REAL").arg(c))) {
                qWarning() << "Erreur ajout colonne EVALUATION." << c << ":" << query.lastError().text();
            }
        }
    }

    // Table PARAMETRES_JURY (cle, valeur) : rÃ©glages de la page Â« ParamÃ¨tres du jury Â»
    if (!query.exec("CREATE TABLE IF NOT EXISTS PARAMETRES_JURY ("
                    "cle TEXT PRIMARY KEY, "
                    "valeur TEXT NOT NULL"
                    ");")) {
        qWarning() << "Erreur crÃ©ation table PARAMETRES_JURY:" << query.lastError().text();
    }

    // Colonne Â« description Â» de PROJET (sous-titre affichÃ© dans le planning) : ajoutÃ©e sans perdre les donnÃ©es existantes
    {
        bool aDescription = false;
        QSqlQuery pragma("PRAGMA table_info(PROJET)");
        while (pragma.next()) {
            if (pragma.value(1).toString() == "description") aDescription = true;
        }
        if (!aDescription && !query.exec("ALTER TABLE PROJET ADD COLUMN description TEXT DEFAULT ''")) {
            qWarning() << "Erreur ajout colonne PROJET.description:" << query.lastError().text();
        }
    }

    // Table SESSION_EVALUATION (planning des Ã©valuations : un jurÃ© Ã©value un projet Ã  une date / heure / salle)
    if (!query.exec("CREATE TABLE IF NOT EXISTS SESSION_EVALUATION ("
                    "id_session INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "date_session TEXT NOT NULL, "
                    "heure TEXT NOT NULL, "
                    "id_projet INTEGER NOT NULL, "
                    "id_jury INTEGER NOT NULL, "
                    "salle TEXT NOT NULL, "
                    "statut TEXT NOT NULL DEFAULT 'PlanifiÃ©e', "
                    "FOREIGN KEY(id_jury) REFERENCES JURY(id_jury) ON DELETE CASCADE, "
                    "FOREIGN KEY(id_projet) REFERENCES PROJET(id_projet) ON DELETE CASCADE"
                    ");")) {
        qWarning() << "Erreur crÃ©ation table SESSION_EVALUATION:" << query.lastError().text();
    }

    // Remplissage avec des donnÃ©es de dÃ©monstration si la table est vide
    query.exec("SELECT COUNT(*) FROM JURY");
    if (query.next() && query.value(0).toInt() == 0) {
        seedSampleData();
    }

    // Descriptions courtes des projets de dÃ©monstration (uniquement si elles sont vides)
    const QList<QPair<QString, QString>> descriptions = {
        {"EcoTrack", "Solution de suivi Ã©nergÃ©tique"},
        {"HealthCare AI", "IA pour la santÃ©"},
        {"SmartCity Mobility", "MobilitÃ© intelligente"},
        {"Clean Energy Grid", "RÃ©seau d'Ã©nergie propre"},
        {"UrbanAI Surveillance", "Surveillance intelligente"}};
    for (const auto &d : descriptions) {
        QSqlQuery u;
        u.prepare("UPDATE PROJET SET description = ? WHERE titre = ? AND (description IS NULL OR description = '')");
        u.addBindValue(d.second);
        u.addBindValue(d.first);
        u.exec();
    }

    // Sessions de dÃ©monstration (12), datÃ©es Ã  partir de la veille du premier lancement
    query.exec("SELECT COUNT(*) FROM SESSION_EVALUATION");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("SELECT COUNT(*) FROM JURY");
        const bool aJures = query.next() && query.value(0).toInt() > 0;
        query.exec("SELECT COUNT(*) FROM PROJET");
        const bool aProjets = query.next() && query.value(0).toInt() > 0;
        if (aJures && aProjets) seedSessions();
    }

    return true;
}

void Connection::seedSampleData()
{
    QSqlQuery q;

    // Insertion des projets
    q.exec("INSERT OR IGNORE INTO PROJET (id_projet, titre, nom_equipe, statut) VALUES "
           "(1, 'EcoTrack', 'GreenTeam', 'En cours'), "
           "(2, 'HealthCare AI', 'MedTech', 'ValidÃ©'), "
           "(3, 'SmartCity Mobility', 'TechVision', 'En attente'), "
           "(4, 'Clean Energy Grid', 'EcoSolutions', 'En attente'), "
           "(5, 'UrbanAI Surveillance', 'SmartCity', 'En cours');");

    // Insertion des 10 membres du jury (identiques Ã  la maquette HackFlow)
    q.exec("INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES "
           "('Ben Ali', 'Ahmed', 'ahmed.benali@example.com', '+216 98 123 456', 'Expert Tech', 'Technologie', 'Disponible'), "
           "('Trabelsi', 'Siwa', 'sara.trabelsi@example.com', '+216 92 234 567', 'Investisseur', 'Finance', 'Disponible'), "
           "('Khaled', 'Omar', 'omar.khaled@example.com', '+216 93 345 678', 'Business', 'StratÃ©gie', 'En ligne'), "
           "('Ayari', 'Meriem', 'meriem.ayari@example.com', '+216 95 567 890', 'Designer', 'UX/UI', 'Disponible'), "
           "('Jabri', 'Yassine', 'yassine.jabri@example.com', '+216 97 123 456', 'DÃ©veloppeur', 'WebFactory', 'En attente'), "
           "('Zitibi', 'Ims', 'imes.zitibi@example.com', '+216 91 234 567', 'Expert MÃ©tier', 'Innovation', 'Disponible'), "
           "('Gharbi', 'Amine', 'amine.gharbi@example.com', '+216 93 456 789', 'Architecte', 'SystÃ¨me', 'Disponible'), "
           "('Mansouri', 'Leila', 'leila.mansouri@example.com', '+216 94 567 890', 'Consultant', 'Marketing', 'En ligne'), "
           "('Slimani', 'Rami', 'rami.slimani@example.com', '+216 95 678 901', 'Investisseur', 'Finances', 'En attente'), "
           "('Boukhris', 'Karim', 'karim.boukhris@example.com', '+216 96 789 012', 'Expert Tech', 'Technologie', 'Disponible');");

    // Insertion des Ã©valuations avec critÃ¨res dÃ©taillÃ©s
    // Insertion des évaluations avec critères détaillés
    // Projet 1: EcoTrack (bon consensus : 17.5 et 16.5)
    q.exec("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation) VALUES "
           "(1, 1, 17.5, 18.0, 17.0, 17.5, 17.5, 'Excellente approche écologique avec intégration de capteurs intelligents.', '2026-03-15 10:30'), "
           "(2, 1, 16.5, 16.0, 17.0, 16.5, 16.5, 'Architecture cloud solide, dashboard intuitif et réactif.', '2026-03-15 11:15');");

    // Projet 2: HealthCare AI (notes très élevées : 19.0 et 18.5)
    q.exec("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation) VALUES "
           "(4, 2, 19.0, 19.5, 19.0, 18.5, 19.0, 'Modèle d analyse prédictive impressionnant pour le milieu hospitalier.', '2026-03-15 14:00'), "
           "(7, 2, 18.5, 18.0, 19.0, 18.5, 18.5, 'Design UX médical parfait, respect de la confidentialité des données.', '2026-03-15 14:45');");

    // Projet 3: SmartCity Mobility (notes modérées : 14.0 et 14.5)
    q.exec("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation) VALUES "
           "(5, 3, 14.5, 14.0, 15.0, 14.5, 14.5, 'Bonne idée IoT urbaine mais manque de tests sous forte charge.', '2026-03-15 15:30'), "
           "(6, 3, 14.0, 14.0, 14.0, 14.0, 14.0, 'Application mobile fluide, API bien documentée.', '2026-03-15 16:10');");

    // Projet 5: UrbanAI Surveillance (FORTE DIVERGENCE : Juré 1 a mis 18.5, Juré 3 a mis 11.0 -> Écart de 7.5 pts pour démontrer le métier innovant d'alerte!)
    q.exec("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation) VALUES "
           "(1, 5, 18.5, 19.0, 18.5, 18.0, 18.5, 'Algorithmes de détection vidéo de pointe avec haute précision.', '2026-03-15 16:45'), "
           "(3, 5, 11.0, 12.0, 11.0, 10.0, 11.0, 'Risques majeurs de sécurité et failles RGPD non traitées.', '2026-03-15 17:15');");

    // Projet 4: Clean Energy Grid (évaluation par Juré 8)
    q.exec("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation) VALUES "
           "(8, 4, 16.0, 16.5, 15.5, 16.0, 16.0, 'Modèle économique très viable pour le secteur de l énergie verte.', '2026-03-15 17:40');");

    qDebug() << "Données de test initialisées avec succès.";
}

void Connection::seedSessions()
{
    struct S { int jour; const char *heure; int projet; int jury; const char *salle; const char *statut; };
    static const S sessions[] = {
        {0, "09:00", 1, 1, "Salle 1", "Planifiée"},
        {0, "10:30", 3, 3, "Salle 2", "Planifiée"},
        {0, "14:00", 2, 4, "Salle 3", "Planifiée"},
        {1, "09:30", 5, 2, "Salle 1", "En cours"},
        {1, "11:00", 1, 6, "Salle 2", "Planifiée"},
        {2, "14:30", 4, 5, "Salle 3", "Planifiée"},
        {3, "09:00", 3, 7, "Salle 2", "Planifiée"},
        {4, "10:30", 4, 8, "Salle 2", "Planifiée"},
        {5, "14:00", 2, 3, "Salle 3", "Planifiée"},
        {6, "11:00", 5, 4, "Salle 1", "Planifiée"},
        {6, "14:00", 1, 9, "Salle 2", "Planifiée"},
        {6, "15:30", 3, 10, "Salle 3", "Planifiée"}};

    const QDate base = QDate::currentDate().addDays(-1);
    QSqlQuery q;
    for (const S &s : sessions) {
        q.prepare("INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
        q.addBindValue(base.addDays(s.jour).toString("yyyy-MM-dd"));
        q.addBindValue(QString::fromUtf8(s.heure));
        q.addBindValue(s.projet);
        q.addBindValue(s.jury);
        q.addBindValue(QString::fromUtf8(s.salle));
        q.addBindValue(QString::fromUtf8(s.statut));
        q.exec();
    }
}