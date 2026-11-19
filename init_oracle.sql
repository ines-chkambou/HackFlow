-- ========================================================
-- ATTENTION : SCRIPT DESTRUCTIF — NE PAS EXÉCUTER SANS SAUVEGARDE
--
-- Ce script DROP les tables et séquences existantes, puis recrée un schéma
-- qui peut diverger de la base réelle. La référence de schéma à utiliser est
-- migration_nondestructive_oracle.sql.
--
-- Aucune exécution de ce fichier ne doit être faite sur une base contenant
-- des données à conserver.
-- ========================================================
-- Script d'initialisation Oracle Database 11g Express Edition
-- Projet : HackFlow / SmartHackathon (Module 6 : Jury & Évaluations)
-- Utilisateur : INES (ou SYSTEM)
-- ========================================================

-- 1. Nettoyage préalable (si réinitialisation)
BEGIN
  EXECUTE IMMEDIATE 'DROP TABLE SESSION_EVALUATION CASCADE CONSTRAINTS';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP TABLE EVALUATION CASCADE CONSTRAINTS';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP TABLE PROJET CASCADE CONSTRAINTS';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP TABLE JURY CASCADE CONSTRAINTS';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP TABLE PARAMETRES_JURY CASCADE CONSTRAINTS';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP SEQUENCE JURY_SEQ';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP SEQUENCE EVAL_SEQ';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/
BEGIN
  EXECUTE IMMEDIATE 'DROP SEQUENCE SESSION_SEQ';
EXCEPTION WHEN OTHERS THEN NULL;
END;
/

-- 2. Création des tables

CREATE TABLE JURY (
    id_jury NUMBER PRIMARY KEY,
    nom VARCHAR2(100) NOT NULL,
    prenom VARCHAR2(100) NOT NULL,
    email VARCHAR2(150) NOT NULL UNIQUE,
    telephone VARCHAR2(50) DEFAULT '',
    role VARCHAR2(100) NOT NULL,
    equipe VARCHAR2(100) NOT NULL,
    disponibilite VARCHAR2(50) DEFAULT 'Disponible'
);

CREATE SEQUENCE JURY_SEQ START WITH 1 INCREMENT BY 1 NOCACHE;

CREATE OR REPLACE TRIGGER JURY_BIR
BEFORE INSERT ON JURY
FOR EACH ROW
BEGIN
  IF :NEW.id_jury IS NULL THEN
    SELECT JURY_SEQ.NEXTVAL INTO :NEW.id_jury FROM DUAL;
  END IF;
END;
/

CREATE TABLE PROJET (
    id_projet NUMBER PRIMARY KEY,
    titre VARCHAR2(150) NOT NULL,
    nom_equipe VARCHAR2(100) NOT NULL,
    statut VARCHAR2(50) NOT NULL,
    description VARCHAR2(255) DEFAULT ''
);

CREATE TABLE EVALUATION (
    id_evaluation NUMBER PRIMARY KEY,
    id_jury NUMBER NOT NULL,
    id_projet NUMBER NOT NULL,
    note NUMBER(5,2) NOT NULL,
    critere_innovation NUMBER(5,2) DEFAULT 0,
    critere_technique NUMBER(5,2) DEFAULT 0,
    critere_presentation NUMBER(5,2) DEFAULT 0,
    critere_pitch NUMBER(5,2) DEFAULT 0,
    commentaire VARCHAR2(500),
    date_evaluation VARCHAR2(50) NOT NULL,
    crit_qualite NUMBER(5,2) DEFAULT 0,
    crit_impact NUMBER(5,2) DEFAULT 0,
    crit_business NUMBER(5,2) DEFAULT 0,
    crit_equipe NUMBER(5,2) DEFAULT 0,
    CONSTRAINT FK_EVAL_JURY FOREIGN KEY (id_jury) REFERENCES JURY(id_jury) ON DELETE CASCADE,
    CONSTRAINT FK_EVAL_PROJET FOREIGN KEY (id_projet) REFERENCES PROJET(id_projet) ON DELETE CASCADE
);

CREATE SEQUENCE EVAL_SEQ START WITH 1 INCREMENT BY 1 NOCACHE;

CREATE OR REPLACE TRIGGER EVAL_BIR
BEFORE INSERT ON EVALUATION
FOR EACH ROW
BEGIN
  IF :NEW.id_evaluation IS NULL THEN
    SELECT EVAL_SEQ.NEXTVAL INTO :NEW.id_evaluation FROM DUAL;
  END IF;
END;
/

CREATE TABLE PARAMETRES_JURY (
    cle VARCHAR2(100) PRIMARY KEY,
    valeur VARCHAR2(255) NOT NULL
);

CREATE TABLE SESSION_EVALUATION (
    id_session NUMBER PRIMARY KEY,
    date_session VARCHAR2(50) NOT NULL,
    heure VARCHAR2(20) NOT NULL,
    id_projet NUMBER NOT NULL,
    id_jury NUMBER NOT NULL,
    salle VARCHAR2(50) NOT NULL,
    statut VARCHAR2(50) DEFAULT 'Planifiée',
    CONSTRAINT FK_SESS_JURY FOREIGN KEY (id_jury) REFERENCES JURY(id_jury) ON DELETE CASCADE,
    CONSTRAINT FK_SESS_PROJET FOREIGN KEY (id_projet) REFERENCES PROJET(id_projet) ON DELETE CASCADE
);

CREATE SEQUENCE SESSION_SEQ START WITH 1 INCREMENT BY 1 NOCACHE;

CREATE OR REPLACE TRIGGER SESSION_BIR
BEFORE INSERT ON SESSION_EVALUATION
FOR EACH ROW
BEGIN
  IF :NEW.id_session IS NULL THEN
    SELECT SESSION_SEQ.NEXTVAL INTO :NEW.id_session FROM DUAL;
  END IF;
END;
/

-- 3. Données de démonstration

-- Projets
INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description) VALUES (1, 'EcoTrack', 'GreenTeam', 'En cours', 'Solution de suivi énergétique');
INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description) VALUES (2, 'HealthCare AI', 'MedTech', 'Validé', 'IA pour la santé');
INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description) VALUES (3, 'SmartCity Mobility', 'TechVision', 'En attente', 'Mobilité intelligente');
INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description) VALUES (4, 'Clean Energy Grid', 'EcoSolutions', 'En attente', 'Réseau d''énergie propre');
INSERT INTO PROJET (id_projet, titre, nom_equipe, statut, description) VALUES (5, 'UrbanAI Surveillance', 'SmartCity', 'En cours', 'Surveillance intelligente');

-- Jurys (10 membres identiques à la maquette HackFlow)
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Ben Ali', 'Ahmed', 'ahmed.benali@example.com', '+216 98 123 456', 'Expert Tech', 'Technologie', 'Disponible');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Trabelsi', 'Siwa', 'sara.trabelsi@example.com', '+216 92 234 567', 'Investisseur', 'Finance', 'Disponible');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Khaled', 'Omar', 'omar.khaled@example.com', '+216 93 345 678', 'Business', 'Stratégie', 'En ligne');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Ayari', 'Meriem', 'meriem.ayari@example.com', '+216 95 567 890', 'Designer', 'UX/UI', 'Disponible');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Jabri', 'Yassine', 'yassine.jabri@example.com', '+216 97 123 456', 'Développeur', 'WebFactory', 'En attente');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Zitibi', 'Ims', 'imes.zitibi@example.com', '+216 91 234 567', 'Expert Métier', 'Innovation', 'Disponible');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Gharbi', 'Amine', 'amine.gharbi@example.com', '+216 93 456 789', 'Architecte', 'Système', 'Disponible');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Mansouri', 'Leila', 'leila.mansouri@example.com', '+216 94 567 890', 'Consultant', 'Marketing', 'En ligne');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Slimani', 'Rami', 'rami.slimani@example.com', '+216 95 678 901', 'Investisseur', 'Finances', 'En attente');
INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) VALUES ('Boukhris', 'Karim', 'karim.boukhris@example.com', '+216 96 789 012', 'Expert Tech', 'Technologie', 'Disponible');

-- Évaluations (20 évaluations couvrant les 10 jurés avec leurs équipes)
-- Juré 1 : Ben Ali Ahmed -> GreenTeam (1) et SmartCity (5)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (1, 1, 1, 17.5, 18.0, 17.0, 17.5, 17.5, 'Excellente approche écologique avec intégration de capteurs intelligents.', '2026-03-15 10:30');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (2, 1, 5, 18.5, 19.0, 18.5, 18.0, 18.5, 'Algorithmes de détection vidéo de pointe avec haute précision.', '2026-03-15 16:45');

-- Juré 2 : Trabelsi Siwa -> GreenTeam (1) et EcoSolutions (4)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (3, 2, 1, 16.5, 16.0, 17.0, 16.5, 16.5, 'Architecture cloud solide, dashboard intuitif et réactif.', '2026-03-15 11:15');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (4, 2, 4, 15.0, 15.5, 15.0, 14.5, 15.0, 'Modèle d investissement prometteur pour la transition écologique.', '2026-03-15 12:00');

-- Juré 3 : Khaled Omar -> SmartCity (5) et MedTech (2)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (5, 3, 5, 11.0, 12.0, 11.0, 10.0, 11.0, 'Risques majeurs de sécurité et failles RGPD non traitées.', '2026-03-15 17:15');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (6, 3, 2, 17.0, 17.5, 17.0, 16.5, 17.0, 'Stratégie de déploiement clinique bien pensée et claire.', '2026-03-15 13:30');

-- Juré 4 : Ayari Meriem -> MedTech (2) et TechVision (3)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (7, 4, 2, 19.0, 19.5, 19.0, 18.5, 19.0, 'Modèle d analyse prédictive impressionnant pour le milieu hospitalier.', '2026-03-15 14:00');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (8, 4, 3, 15.5, 16.0, 15.0, 16.0, 15.0, 'Interface utilisateur soignée, ergonomie très adaptée aux usagers.', '2026-03-15 14:40');

-- Juré 5 : Jabri Yassine -> TechVision (3) et EcoSolutions (4)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (9, 5, 3, 14.5, 14.0, 15.0, 14.5, 14.5, 'Bonne idée IoT urbaine mais manque de tests sous forte charge.', '2026-03-15 15:30');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (10, 5, 4, 15.0, 15.5, 15.0, 14.5, 15.0, 'Stack technique propre et intégration CI/CD réussie.', '2026-03-15 16:00');

-- Juré 6 : Zitibi Ims -> TechVision (3) et GreenTeam (1)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (11, 6, 3, 14.0, 14.0, 14.0, 14.0, 14.0, 'Application mobile fluide, API bien documentée.', '2026-03-15 16:10');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (12, 6, 1, 16.0, 16.5, 16.0, 15.5, 16.0, 'Potentiel d innovation élevé pour la transition écologique.', '2026-03-15 16:40');

-- Juré 7 : Gharbi Amine -> MedTech (2) et TechVision (3)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (13, 7, 2, 18.5, 18.0, 19.0, 18.5, 18.5, 'Design UX médical parfait, respect de la confidentialité des données.', '2026-03-15 14:45');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (14, 7, 3, 14.0, 14.0, 14.5, 13.5, 14.0, 'Architecture microservices robuste mais scalabilité à prouver.', '2026-03-15 17:00');

-- Juré 8 : Mansouri Leila -> EcoSolutions (4) et SmartCity (5)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (15, 8, 4, 16.0, 16.5, 15.5, 16.0, 16.0, 'Modèle économique très viable pour le secteur de l énergie verte.', '2026-03-15 17:40');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (16, 8, 5, 16.5, 17.0, 16.0, 16.5, 16.5, 'Plan marketing percutant, cible B2B bien identifiée.', '2026-03-15 18:00');

-- Juré 9 : Slimani Rami -> GreenTeam (1) et EcoSolutions (4)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (17, 9, 1, 15.5, 15.0, 16.0, 15.5, 15.5, 'Bon retour sur investissement prévu à moyen terme.', '2026-03-15 18:20');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (18, 9, 4, 16.5, 17.0, 16.0, 16.5, 16.5, 'Structure de coûts maîtrisée et business plan réaliste.', '2026-03-15 18:50');

-- Juré 10 : Boukhris Karim -> TechVision (3) et MedTech (2)
INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (19, 10, 3, 15.0, 15.5, 15.0, 14.5, 15.0, 'Technologies web modernes et respect des standards.', '2026-03-15 19:10');

INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, critere_innovation, critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation)
VALUES (20, 10, 2, 18.0, 18.5, 18.0, 17.5, 18.0, 'Excellente intégration d algorithmes d IA en temps réel.', '2026-03-15 19:30');

-- Sessions d'évaluation
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-14', '09:00', 1, 1, 'Salle 1', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-14', '10:30', 3, 3, 'Salle 2', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-14', '14:00', 2, 4, 'Salle 3', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-15', '09:30', 5, 2, 'Salle 1', 'En cours');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-15', '11:00', 1, 6, 'Salle 2', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-16', '14:30', 4, 5, 'Salle 3', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-17', '09:00', 3, 7, 'Salle 2', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-18', '10:30', 4, 8, 'Salle 2', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-19', '14:00', 2, 3, 'Salle 3', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-20', '11:00', 5, 4, 'Salle 1', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-20', '14:00', 1, 9, 'Salle 2', 'Planifiée');
INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) VALUES ('2026-03-20', '15:30', 3, 10, 'Salle 3', 'Planifiée');

COMMIT;
