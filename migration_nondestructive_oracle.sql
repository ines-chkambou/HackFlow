-- ============================================================
-- migration_nondestructive_oracle.sql
-- Projet : SmartHackathon / HackFlow Jury
-- Source : SQLite 3.40.1
-- Fichier source : C:\Users\DELL\Downloads\HackFlow_Jury\SmartHackathon\build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\debug\smarthackathon.db
-- SHA-256 source : 3DAD9E6600318C4536AF82DB934695801AC4D37552EAAFDA359A894854FAA8E5
-- Cible  : Oracle XE 11g, schema INES
-- Genere le : 2026-11-12 01:56:00
--
-- GARANTIES :
--   * Aucun DROP, TRUNCATE ou DELETE dans ce script.
--   * Aucune donnee de demonstration.
--   * Aucun seed init_oracle.sql.
--   * Aucun GRANT, aucun ALTER USER.
--   * Aucune modification de SQLite.
--   * Les identifiants, relations, contraintes et NULL sont preserves.
--
-- ATTENTION :
--   * Ne pas executer sans autorisation explicite.
--   * Executer avec NLS_LANG=AMERICAN_AMERICA.AL32UTF8.
--   * Le DDL Oracle est auto-commit. En cas d'echec apres creation,
--     utiliser rollback_migration_oracle.sql uniquement avec autorisation.
--   * Les tables/index sont crees explicitement dans le tablespace USERS.
-- ============================================================

SET DEFINE OFF
SET SERVEROUTPUT ON SIZE 1000000
SET ECHO OFF
SET FEEDBACK ON
SET VERIFY OFF
SET AUTOCOMMIT OFF
WHENEVER SQLERROR EXIT ROLLBACK
WHENEVER OSERROR EXIT FAILURE

ALTER SESSION SET NLS_NUMERIC_CHARACTERS = '.,';

PROMPT ============================================================
PROMPT Verification de l'utilisateur, des privileges et des objets
PROMPT ============================================================

DECLARE
  v_user VARCHAR2(30);
  v_count NUMBER;
BEGIN
  SELECT SYS_CONTEXT('USERENV','CURRENT_USER') INTO v_user FROM DUAL;
  IF v_user <> 'INES' THEN
    RAISE_APPLICATION_ERROR(-20000,
      'Ce script doit etre execute dans le schema INES. Utilisateur courant : ' || v_user);
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM SESSION_PRIVS
   WHERE PRIVILEGE IN ('CREATE TABLE','CREATE SEQUENCE','CREATE TRIGGER','CREATE INDEX');
  IF v_count < 4 THEN
    RAISE_APPLICATION_ERROR(-20001,
      'Privileges insuffisants : CREATE TABLE, CREATE SEQUENCE, CREATE TRIGGER et CREATE INDEX sont requis.');
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM USER_TS_QUOTAS
   WHERE TABLESPACE_NAME = 'USERS'
     AND (MAX_BYTES > 0 OR MAX_BYTES = -1);
  IF v_count = 0 THEN
    SELECT COUNT(*) INTO v_count
      FROM SESSION_PRIVS
     WHERE PRIVILEGE = 'UNLIMITED TABLESPACE';
    IF v_count = 0 THEN
      RAISE_APPLICATION_ERROR(-20002,
        'Aucun quota utilisable sur le tablespace USERS et pas de privilege UNLIMITED TABLESPACE. Aucun objet ne sera cree dans SYSTEM.');
    END IF;
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM USER_TABLES
   WHERE TABLE_NAME IN ('JURY','PROJET','EVALUATION','PARAMETRES_JURY','SESSION_EVALUATION');
  IF v_count > 0 THEN
    RAISE_APPLICATION_ERROR(-20003,
      'Migration annulee : au moins une table cible existe deja. Aucun objet n''a ete modifie.');
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM USER_SEQUENCES
   WHERE SEQUENCE_NAME IN ('JURY_SEQ','EVAL_SEQ','SESSION_SEQ');
  IF v_count > 0 THEN
    RAISE_APPLICATION_ERROR(-20004,
      'Migration annulee : au moins une sequence cible existe deja.');
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM USER_TRIGGERS
   WHERE TRIGGER_NAME IN ('JURY_BIR','EVAL_BIR','SESSION_BIR');
  IF v_count > 0 THEN
    RAISE_APPLICATION_ERROR(-20005,
      'Migration annulee : au moins un trigger cible existe deja.');
  END IF;

  SELECT COUNT(*) INTO v_count
    FROM USER_INDEXES
   WHERE INDEX_NAME IN ('IDX_EVAL_JURY','IDX_EVAL_PROJET','IDX_SESS_JURY','IDX_SESS_PROJET','IDX_SESS_DATE');
  IF v_count > 0 THEN
    RAISE_APPLICATION_ERROR(-20006,
      'Migration annulee : au moins un index cible existe deja.');
  END IF;
END;
/

PROMPT ============================================================
PROMPT Creation des tables
PROMPT ============================================================

CREATE TABLE JURY (
  id_jury NUMBER(10) NOT NULL,
  nom VARCHAR2(100 CHAR) NOT NULL,
  prenom VARCHAR2(100 CHAR) NOT NULL,
  email VARCHAR2(254 CHAR) NOT NULL,
  telephone VARCHAR2(50 CHAR) NOT NULL,
  role VARCHAR2(100 CHAR) NOT NULL,
  equipe VARCHAR2(100 CHAR) NOT NULL,
  disponibilite VARCHAR2(50 CHAR) DEFAULT 'Disponible' NOT NULL,
  CONSTRAINT PK_JURY PRIMARY KEY (id_jury) USING INDEX TABLESPACE USERS,
  CONSTRAINT UQ_JURY_EMAIL UNIQUE (email) USING INDEX TABLESPACE USERS
) TABLESPACE USERS;

-- SQLite : telephone TEXT NOT NULL DEFAULT ''
-- Oracle : DEFAULT '' equivaut a NULL et est interdit avec NOT NULL.
-- Aucune valeur par defaut Oracle ; l'application fournit toujours un telephone non vide.

CREATE TABLE PROJET (
  id_projet NUMBER(10) NOT NULL,
  titre VARCHAR2(200 CHAR) NOT NULL,
  nom_equipe VARCHAR2(150 CHAR) NOT NULL,
  statut VARCHAR2(50 CHAR) NOT NULL,
  description VARCHAR2(500 CHAR),
  CONSTRAINT PK_PROJET PRIMARY KEY (id_projet) USING INDEX TABLESPACE USERS
) TABLESPACE USERS;

-- SQLite : description TEXT DEFAULT ''
-- Oracle : pas de DEFAULT '' (equivaut a NULL) ; la colonne reste nullable.

CREATE TABLE EVALUATION (
  id_evaluation NUMBER(10) NOT NULL,
  id_jury NUMBER(10) NOT NULL,
  id_projet NUMBER(10) NOT NULL,
  note NUMBER(5,2) NOT NULL,
  critere_innovation NUMBER(5,2) DEFAULT 0,
  critere_technique NUMBER(5,2) DEFAULT 0,
  critere_presentation NUMBER(5,2) DEFAULT 0,
  critere_pitch NUMBER(5,2) DEFAULT 0,
  commentaire VARCHAR2(1000 CHAR),
  date_evaluation VARCHAR2(16 CHAR) NOT NULL,
  crit_qualite NUMBER(5,2),
  crit_impact NUMBER(5,2),
  crit_business NUMBER(5,2),
  crit_equipe NUMBER(5,2),
  CONSTRAINT PK_EVALUATION PRIMARY KEY (id_evaluation) USING INDEX TABLESPACE USERS,
  CONSTRAINT FK_EVAL_JURY FOREIGN KEY (id_jury)
    REFERENCES JURY(id_jury) ON DELETE CASCADE,
  CONSTRAINT FK_EVAL_PROJET FOREIGN KEY (id_projet)
    REFERENCES PROJET(id_projet) ON DELETE CASCADE
) TABLESPACE USERS;

-- Les quatre colonnes crit_* restent nullable sans DEFAULT : les 9 valeurs NULL sont preservees.
-- Les quatre colonnes critere_* gardent DEFAULT 0 comme dans SQLite.

CREATE TABLE PARAMETRES_JURY (
  cle VARCHAR2(100 CHAR) NOT NULL,
  valeur VARCHAR2(500 CHAR) NOT NULL,
  CONSTRAINT PK_PARAMETRES_JURY PRIMARY KEY (cle) USING INDEX TABLESPACE USERS
) TABLESPACE USERS;

CREATE TABLE SESSION_EVALUATION (
  id_session NUMBER(10) NOT NULL,
  date_session VARCHAR2(10 CHAR) NOT NULL,
  heure VARCHAR2(5 CHAR) NOT NULL,
  id_projet NUMBER(10) NOT NULL,
  id_jury NUMBER(10) NOT NULL,
  salle VARCHAR2(100 CHAR) NOT NULL,
  statut VARCHAR2(50 CHAR) DEFAULT 'Planifiée' NOT NULL,
  CONSTRAINT PK_SESSION_EVALUATION PRIMARY KEY (id_session) USING INDEX TABLESPACE USERS,
  CONSTRAINT FK_SESS_JURY FOREIGN KEY (id_jury)
    REFERENCES JURY(id_jury) ON DELETE CASCADE,
  CONSTRAINT FK_SESS_PROJET FOREIGN KEY (id_projet)
    REFERENCES PROJET(id_projet) ON DELETE CASCADE
) TABLESPACE USERS;

-- Le DEFAULT de statut reproduit exactement la valeur SQLite 'Planifiée'.
-- Le fichier est en UTF-8 et doit etre execute avec NLS_LANG=AL32UTF8.

PROMPT ============================================================
PROMPT Creation des sequences
PROMPT ============================================================

CREATE SEQUENCE JURY_SEQ START WITH 12 INCREMENT BY 1 NOCACHE;
CREATE SEQUENCE EVAL_SEQ START WITH 10 INCREMENT BY 1 NOCACHE;
CREATE SEQUENCE SESSION_SEQ START WITH 13 INCREMENT BY 1 NOCACHE;

PROMPT ============================================================
PROMPT Creation des triggers
PROMPT ============================================================

CREATE TRIGGER JURY_BIR
BEFORE INSERT ON JURY
FOR EACH ROW
WHEN (NEW.id_jury IS NULL)
BEGIN
  SELECT JURY_SEQ.NEXTVAL INTO :NEW.id_jury FROM DUAL;
END;
/

CREATE TRIGGER EVAL_BIR
BEFORE INSERT ON EVALUATION
FOR EACH ROW
WHEN (NEW.id_evaluation IS NULL)
BEGIN
  SELECT EVAL_SEQ.NEXTVAL INTO :NEW.id_evaluation FROM DUAL;
END;
/

CREATE TRIGGER SESSION_BIR
BEFORE INSERT ON SESSION_EVALUATION
FOR EACH ROW
WHEN (NEW.id_session IS NULL)
BEGIN
  SELECT SESSION_SEQ.NEXTVAL INTO :NEW.id_session FROM DUAL;
END;
/

PROMPT ============================================================
PROMPT Creation des index
PROMPT ============================================================

CREATE INDEX IDX_EVAL_JURY ON EVALUATION(id_jury) TABLESPACE USERS;
CREATE INDEX IDX_EVAL_PROJET ON EVALUATION(id_projet) TABLESPACE USERS;
CREATE INDEX IDX_SESS_JURY ON SESSION_EVALUATION(id_jury) TABLESPACE USERS;
CREATE INDEX IDX_SESS_PROJET ON SESSION_EVALUATION(id_projet) TABLESPACE USERS;
CREATE INDEX IDX_SESS_DATE ON SESSION_EVALUATION(date_session, heure) TABLESPACE USERS;

PROMPT ============================================================
PROMPT Chargement des donnees reelles SQLite
PROMPT Aucune donnee de demonstration
PROMPT ============================================================

PROMPT JURY : 11 lignes

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (1, 'Ben Ali', 'Ahmed', 'ahmed.benali@example.com', '+216 98 123 456', 'Expert Tech', 'Technologie', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (2, 'Trabelsi', 'Siwa', 'sara.trabelsi@example.com', '+216 92 234 567', 'Investisseur', 'Finance', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (3, 'Khaled', 'Omar', 'omar.khaled@example.com', '+216 93 345 678', 'Business', 'Stratégie', 'En ligne');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (4, 'Ayari', 'Meriem', 'meriem.ayari@example.com', '+216 95 567 890', 'Designer', 'UX/UI', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (5, 'Jabri', 'Yassine', 'yassine.jabri@example.com', '+216 97 123 456', 'Développeur', 'WebFactory', 'En attente');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (6, 'Zitibi', 'Ims', 'imes.zitibi@example.com', '+216 91 234 567', 'Expert Métier', 'Innovation', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (7, 'Gharbi', 'Amine', 'amine.gharbi@example.com', '+216 93 456 789', 'Architecte', 'Système', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (8, 'Mansouri', 'Leila', 'leila.mansouri@example.com', '+216 94 567 890', 'Consultant', 'Marketing', 'En ligne');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (9, 'Slimani', 'Rami', 'rami.slimani@example.com', '+216 95 678 901', 'Investisseur', 'Finances', 'En attente');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (10, 'Boukhris', 'Karim', 'karim.boukhris@example.com', '+216 96 789 012', 'Expert Tech', 'Technologie', 'Disponible');

INSERT INTO JURY (id_jury, nom, prenom, email, telephone, role, equipe, disponibilite)
VALUES (11, 'machraoui', 'farah', 'farah.machraoui@email.com', '+21629401536', 'Architecte', 'Technologie', 'En ligne');


PROMPT PROJET : 5 lignes

INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description)
VALUES (1, 'EcoTrack', 'GreenTeam', 'En cours', 'Solution de suivi énergétique');

INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description)
VALUES (2, 'HealthCare AI', 'MedTech', 'Validé', 'IA pour la santé');

INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description)
VALUES (3, 'SmartCity Mobility', 'TechVision', 'En attente', 'Mobilité intelligente');

INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description)
VALUES (4, 'Clean Energy Grid', 'EcoSolutions', 'En attente', 'Réseau d''énergie propre');

INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description)
VALUES (5, 'UrbanAI Surveillance', 'SmartCity', 'En cours', 'Surveillance intelligente');


PROMPT EVALUATION : 9 lignes

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (1, 1, 1, 17.5, 18.0, 17.0, 17.5, 17.5, 'Excellente approche écologique avec intégration de capteurs intelligents.', '2026-03-15 10:30', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (2, 2, 1, 16.5, 16.0, 17.0, 16.5, 16.5, 'Architecture cloud solide, dashboard intuitif et réactif.', '2026-03-15 11:15', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (3, 4, 2, 19.0, 19.5, 19.0, 18.5, 19.0, 'Modèle d analyse prédictive impressionnant pour le milieu hospitalier.', '2026-03-15 14:00', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (4, 7, 2, 18.5, 18.0, 19.0, 18.5, 18.5, 'Design UX médical parfait, respect de la confidentialité des données.', '2026-03-15 14:45', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (5, 5, 3, 14.5, 14.0, 15.0, 14.5, 14.5, 'Bonne idée IoT urbaine mais manque de tests sous forte charge.', '2026-03-15 15:30', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (6, 6, 3, 14.0, 14.0, 14.0, 14.0, 14.0, 'Application mobile fluide, API bien documentée.', '2026-03-15 16:10', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (7, 1, 5, 18.5, 19.0, 18.5, 18.0, 18.5, 'Algorithmes de détection vidéo de pointe avec haute précision.', '2026-03-15 16:45', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (8, 3, 5, 11.0, 12.0, 11.0, 10.0, 11.0, 'Risques majeurs de sécurité et failles RGPD non traitées.', '2026-03-15 17:15', NULL, NULL, NULL, NULL);

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe)
VALUES (9, 8, 4, 16.0, 16.5, 15.5, 16.0, 16.0, 'Modèle économique très viable pour le secteur de l énergie verte.', '2026-03-15 17:40', NULL, NULL, NULL, NULL);


PROMPT SESSION_EVALUATION : 12 lignes

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (1, '2026-10-22', '09:00', 1, 1, 'Salle 1', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (2, '2026-10-22', '10:30', 3, 3, 'Salle 2', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (3, '2026-10-22', '14:00', 2, 4, 'Salle 3', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (4, '2026-10-23', '09:30', 5, 2, 'Salle 1', 'En cours');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (5, '2026-10-23', '11:00', 1, 6, 'Salle 2', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (6, '2026-10-24', '14:30', 4, 5, 'Salle 3', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (7, '2026-10-25', '09:00', 3, 7, 'Salle 2', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (8, '2026-10-26', '10:30', 4, 8, 'Salle 2', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (9, '2026-10-27', '14:00', 2, 3, 'Salle 3', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (10, '2026-10-28', '11:00', 5, 4, 'Salle 1', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (11, '2026-10-28', '14:00', 1, 9, 'Salle 2', 'Planifiée');

INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut)
VALUES (12, '2026-10-28', '15:30', 3, 10, 'Salle 3', 'Planifiée');


PROMPT PARAMETRES_JURY : 0 ligne, aucun INSERT

-- PARAMETRES_JURY: 0 ligne dans SQLite, aucun INSERT genere.


PROMPT ============================================================
PROMPT Validation des donnees avant COMMIT
PROMPT ============================================================

DECLARE
  v_count NUMBER;
  PROCEDURE verifier(p_ok BOOLEAN, p_message VARCHAR2) IS
  BEGIN
    IF NOT p_ok THEN
      RAISE_APPLICATION_ERROR(-20010, p_message);
    END IF;
  END;
BEGIN
  SELECT COUNT(*) INTO v_count FROM JURY;
  verifier(v_count = 11, 'JURY : attendu 11, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM PROJET;
  verifier(v_count = 5, 'PROJET : attendu 5, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION;
  verifier(v_count = 9, 'EVALUATION : attendu 9, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM SESSION_EVALUATION;
  verifier(v_count = 12, 'SESSION_EVALUATION : attendu 12, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM PARAMETRES_JURY;
  verifier(v_count = 0, 'PARAMETRES_JURY : attendu 0, trouve ' || v_count);

  SELECT MAX(id_jury) INTO v_count FROM JURY;
  verifier(v_count = 11, 'JURY.id_jury max attendu 11, trouve ' || v_count);
  SELECT MAX(id_projet) INTO v_count FROM PROJET;
  verifier(v_count = 5, 'PROJET.id_projet max attendu 5, trouve ' || v_count);
  SELECT MAX(id_evaluation) INTO v_count FROM EVALUATION;
  verifier(v_count = 9, 'EVALUATION.id_evaluation max attendu 9, trouve ' || v_count);
  SELECT MAX(id_session) INTO v_count FROM SESSION_EVALUATION;
  verifier(v_count = 12, 'SESSION_EVALUATION.id_session max attendu 12, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE crit_qualite IS NULL;
  verifier(v_count = 9, 'crit_qualite : attendu 9 NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE crit_impact IS NULL;
  verifier(v_count = 9, 'crit_impact : attendu 9 NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE crit_business IS NULL;
  verifier(v_count = 9, 'crit_business : attendu 9 NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE crit_equipe IS NULL;
  verifier(v_count = 9, 'crit_equipe : attendu 9 NULL, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE critere_pitch IS NOT NULL;
  verifier(v_count = 9, 'critere_pitch : attendu 9 non NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE critere_innovation IS NOT NULL;
  verifier(v_count = 9, 'critere_innovation : attendu 9 non NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE critere_technique IS NOT NULL;
  verifier(v_count = 9, 'critere_technique : attendu 9 non NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE critere_presentation IS NOT NULL;
  verifier(v_count = 9, 'critere_presentation : attendu 9 non NULL, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM EVALUATION e
   LEFT JOIN JURY j ON j.id_jury = e.id_jury
   WHERE j.id_jury IS NULL;
  verifier(v_count = 0, 'FK EVALUATION.id_jury : ' || v_count || ' orphelin(s)');
  SELECT COUNT(*) INTO v_count FROM EVALUATION e
   LEFT JOIN PROJET p ON p.id_projet = e.id_projet
   WHERE p.id_projet IS NULL;
  verifier(v_count = 0, 'FK EVALUATION.id_projet : ' || v_count || ' orphelin(s)');
  SELECT COUNT(*) INTO v_count FROM SESSION_EVALUATION s
   LEFT JOIN JURY j ON j.id_jury = s.id_jury
   WHERE j.id_jury IS NULL;
  verifier(v_count = 0, 'FK SESSION_EVALUATION.id_jury : ' || v_count || ' orphelin(s)');
  SELECT COUNT(*) INTO v_count FROM SESSION_EVALUATION s
   LEFT JOIN PROJET p ON p.id_projet = s.id_projet
   WHERE p.id_projet IS NULL;
  verifier(v_count = 0, 'FK SESSION_EVALUATION.id_projet : ' || v_count || ' orphelin(s)');

  SELECT COUNT(*) INTO v_count FROM (
    SELECT id_jury, id_projet
    FROM EVALUATION
    GROUP BY id_jury, id_projet
    HAVING COUNT(*) > 1
  ) d;
  verifier(v_count = 0, 'Doublons logiques EVALUATION : ' || v_count);

  SELECT COUNT(*) INTO v_count FROM (
    SELECT id_jury, date_session, heure
    FROM SESSION_EVALUATION
    GROUP BY id_jury, date_session, heure
    HAVING COUNT(*) > 1
  ) d;
  verifier(v_count = 0, 'Doublons logiques SESSION par jury/date/heure : ' || v_count);

  SELECT COUNT(*) INTO v_count FROM (
    SELECT salle, date_session, heure
    FROM SESSION_EVALUATION
    GROUP BY salle, date_session, heure
    HAVING COUNT(*) > 1
  ) d;
  verifier(v_count = 0, 'Doublons logiques SESSION par salle/date/heure : ' || v_count);

  SELECT COUNT(DISTINCT LOWER(email)) INTO v_count FROM JURY;
  verifier(v_count = 11, 'JURY emails distincts : attendu 11, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM JURY WHERE LOWER(email) LIKE '%@example.com';
  verifier(v_count = 10, 'JURY emails @example.com : attendu 10, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM JURY WHERE LOWER(email) NOT LIKE '%@example.com';
  verifier(v_count = 1, 'JURY email non-demo : attendu 1, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE LENGTH(date_evaluation) <> 16;
  verifier(v_count = 0, 'date_evaluation : longueur differente de 16 : ' || v_count);
  SELECT COUNT(*) INTO v_count FROM SESSION_EVALUATION
   WHERE LENGTH(date_session) <> 10 OR LENGTH(heure) <> 5;
  verifier(v_count = 0, 'date_session/heure : longueur incorrecte : ' || v_count);
  SELECT COUNT(*) INTO v_count FROM SESSION_EVALUATION
   WHERE date_session < '2026-10-22' OR date_session > '2026-10-28';
  verifier(v_count = 0, 'date_session hors plage 2026-10-22..2026-10-28 : ' || v_count);

  SELECT COUNT(*) INTO v_count FROM EVALUATION WHERE commentaire IS NOT NULL;
  verifier(v_count = 9, 'commentaire : attendu 9 non NULL, trouve ' || v_count);
  SELECT COUNT(*) INTO v_count FROM PROJET WHERE description IS NOT NULL;
  verifier(v_count = 5, 'description PROJET : attendu 5 non NULL, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_TABLES
   WHERE TABLE_NAME IN ('JURY','PROJET','EVALUATION','PARAMETRES_JURY','SESSION_EVALUATION')
     AND TABLESPACE_NAME = 'USERS';
  verifier(v_count = 5, 'Tables dans USERS : attendu 5, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_INDEXES
   WHERE TABLE_NAME IN ('JURY','PROJET','EVALUATION','PARAMETRES_JURY','SESSION_EVALUATION')
     AND UNIQUENESS = 'UNIQUE'
     AND TABLESPACE_NAME = 'USERS';
  verifier(v_count = 6, 'Index uniques (PK/UNIQUE) dans USERS : attendu 6, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_INDEXES
   WHERE INDEX_NAME IN ('IDX_EVAL_JURY','IDX_EVAL_PROJET','IDX_SESS_JURY','IDX_SESS_PROJET','IDX_SESS_DATE')
     AND TABLESPACE_NAME = 'USERS';
  verifier(v_count = 5, 'Index explicites dans USERS : attendu 5, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_CONSTRAINTS
   WHERE CONSTRAINT_NAME IN (
     'PK_JURY','UQ_JURY_EMAIL','PK_PROJET','PK_EVALUATION',
     'FK_EVAL_JURY','FK_EVAL_PROJET','PK_PARAMETRES_JURY',
     'PK_SESSION_EVALUATION','FK_SESS_JURY','FK_SESS_PROJET'
   )
     AND STATUS = 'ENABLED';
  verifier(v_count = 10, 'Contraintes actives : attendu 10, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_TRIGGERS
   WHERE TRIGGER_NAME IN ('JURY_BIR','EVAL_BIR','SESSION_BIR')
     AND STATUS = 'ENABLED';
  verifier(v_count = 3, 'Triggers actifs : attendu 3, trouve ' || v_count);

  SELECT COUNT(*) INTO v_count FROM USER_SEQUENCES
   WHERE SEQUENCE_NAME = 'JURY_SEQ' AND LAST_NUMBER >= 12;
  verifier(v_count = 1, 'JURY_SEQ : LAST_NUMBER < 12');
  SELECT COUNT(*) INTO v_count FROM USER_SEQUENCES
   WHERE SEQUENCE_NAME = 'EVAL_SEQ' AND LAST_NUMBER >= 10;
  verifier(v_count = 1, 'EVAL_SEQ : LAST_NUMBER < 10');
  SELECT COUNT(*) INTO v_count FROM USER_SEQUENCES
   WHERE SEQUENCE_NAME = 'SESSION_SEQ' AND LAST_NUMBER >= 13;
  verifier(v_count = 1, 'SESSION_SEQ : LAST_NUMBER < 13');

  DBMS_OUTPUT.PUT_LINE('Validation des donnees : OK');
END;
/

COMMIT;

PROMPT ============================================================
PROMPT Migration terminee avec succes.
PROMPT Controle recommande : counts, IDs, NULL, FK, sequences, checksum.
PROMPT ============================================================
