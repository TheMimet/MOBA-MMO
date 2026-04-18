@echo off
setlocal
title MOBA MMO - Canonical Launcher

set "PROJECT_ROOT=%~dp0"
set "CANONICAL_LAUNCHER=%PROJECT_ROOT%TamBaslatma.bat"

if /I "%~1"=="/?" goto :help
if /I "%~1"=="-h" goto :help
if /I "%~1"=="--help" goto :help

if not exist "%CANONICAL_LAUNCHER%" (
  echo HATA: Kanonik baslatma dosyasi bulunamadi:
  echo %CANONICAL_LAUNCHER%
  pause
  exit /b 1
)

echo Bu proje artik tek kanonik akistan yonetiliyor:
echo %CANONICAL_LAUNCHER%
echo.
call "%CANONICAL_LAUNCHER%"
endlocal
exit /b %ERRORLEVEL%

:help
echo MOBA MMO kanonik launcher.
echo.
echo Normal kullanim:
echo   baslat.bat
echo.
echo Bu komut TamBaslatma.bat dosyasina yonlenir ve lokal DB/backend, dedicated server ve staged client akisini baslatir.
endlocal
exit /b 0
