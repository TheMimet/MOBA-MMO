@echo off
setlocal
title MOBA MMO - Tek Tik Baslat
color 0A

set "PROJECT_ROOT=%~dp0"
set "CLIENT_SCRIPT=%PROJECT_ROOT%scripts\run-staged-client.ps1"

echo ========================================================
echo              MOBA MMO - TEK TIK BASLATMA
echo ========================================================
echo.
echo Bu pencere sirasiyla sunlari hazirlar:
echo   1. Lokal database ve backend API
echo   2. Dedicated server
echo   3. Staged game client
echo.

if not exist "%CLIENT_SCRIPT%" (
  echo HATA: Baslatma scripti bulunamadi:
  echo %CLIENT_SCRIPT%
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%CLIENT_SCRIPT%"

if errorlevel 1 (
  echo.
  echo Baslatma sirasinda hata olustu. Ustteki log satirlarini kontrol edelim.
  pause
  exit /b 1
)

echo.
echo Oyun baslatma komutu tamamlandi. Acilan backend/server pencerelerini test boyunca kapatma.
timeout /t 5 /nobreak > nul
endlocal
