@echo off
echo ========================================
echo MOBA MMO - Test Environment
echo ========================================
echo.

echo [1/3] Database baslatiliyor...
start "MOBA MMO - Database" cmd /k "cd /d %~dp0server && npm run db:start"

timeout /t 5 /nobreak > nul

echo [2/3] Backend Server baslatiliyor...
start "MOBA MMO - Backend" cmd /k "cd /d %~dp0server && npm run dev"

timeout /t 3 /nobreak > nul

echo [3/3] Game Server baslatiliyor...
start "MOBA MMO - Game Server" cmd /k "cd /d %~dp0 && powershell -ExecutionPolicy Bypass -File .\scripts\run-staged-server.ps1"

echo.
echo ========================================
echo Tum terminaller acildi!
echo ========================================
echo.
echo Siradaki adimlar:
echo 1. Unreal Editor'da Play dugmesine basin (Standalone or New Editor Window)
echo 2. Login ekraninda Mock Login yapin
echo 3. Karakter secimi yapin
echo 4. Server'a baglanin
echo ========================================
pause
