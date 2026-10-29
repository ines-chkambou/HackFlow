@echo off
echo ========================================================
echo Creation de l'archive ZIP du projet SmartHackathon
echo ========================================================

cd /d "%~dp0"
if exist "..\SmartHackathon_Jury.zip" del /f /q "..\SmartHackathon_Jury.zip"

powershell -NoProfile -Command "Compress-Archive -Path '*' -DestinationPath '..\SmartHackathon_Jury.zip' -Force"

if exist "..\SmartHackathon_Jury.zip" (
    echo [SUCCES] L'archive ZIP a ete creee avec succes : ..\SmartHackathon_Jury.zip
) else (
    tar -a -cf "..\SmartHackathon_Jury.zip" *
    echo [INFO] Tentative avec tar effectuee.
)

pause
