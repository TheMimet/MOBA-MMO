@echo off
echo MOBA MMO Projesi Baslatiliyor...

echo Veritabani kontrol ediliyor ve baslatiliyor...
cd server
call npm run db:start

echo Backend sunucusu yeni pencerede baslatiliyor...
start "MOBA MMO Backend" cmd /k "npm run dev"

cd ..

echo Unreal Engine Editor baslatiliyor...
start MOBAMMO.uproject

echo Baslatma islemi tamamlandi.
