# Persistence Operasyonlari

Bu proje artik karakter kayit sistemini bir MMO servisi gibi ele aliyor:

- Kayitlar otomatiktir; manuel save butonu yoktur.
- Her kayit, o anda aktif olan `sessionId` ile iliskilidir.
- Eski, bitmis, zaman asimina ugramis veya yerine yenisi acilmis session'lar karakter durumunu yazamaz.
- Backend gecersiz pozisyonlari ve supheli hareket mesafelerini reddeder.
- Client HUD uzerinde `SAVE OK`, `SAVING`, `SAVE ISSUE` veya `RECONNECT [R]` durumu gosterilir.
- Reconnect gerekiyorsa `R` tusuna basmak yeni bir session baslatir ve oyuncuyu aktif oyun server'ina geri tasir.

## Backend Akisi

1. `POST /session/start` yeni bir aktif session olusturur ve karakterin en guncel snapshot'ini dondurur.
2. `POST /characters/:id/save` aktif session icin pozisyonu, statlari, combat sayaclarini ve save sequence degerini yazar.
3. `POST /session/heartbeat` tam karakter durumu yazmadan sadece `CharacterSession.updatedAt` alanini yeniler.
4. `POST /session/end` son kaydi alir ve session'i `ended` olarak isaretler.
5. Bir session `SESSION_TIMEOUT_SECONDS` suresini asarsa heartbeat/save istegi onu `timed_out` yapar ve `409` dondurur.
6. Arka plandaki session reaper, her `SESSION_REAPER_INTERVAL_SECONDS` saniyede bir eski `active` session'lari tarar ve onlari `timed_out` olarak isaretler.

## Runtime Akisi

1. Client login olur, karakter secer ve bir session baslatir.
2. Travel option'lari account, karakter, session, pozisyon, gorunum ve stat bilgilerini dedicated server'a tasir.
3. Server, pawn spawn olduktan sonra oyuncuyu kayitli pozisyona geri yerlestirir.
4. Autosave server timer'i ile calisir; heartbeat ise daha hafif bir canlilik sinyali olarak ayri calisir.
5. Logout olunca `/session/end` cagrilir; ani kopmalar timeout mekanizmasi ile ele alinir.

## Smoke Testler

Backend persistence smoke testini calistirmak icin:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-session-resume.ps1
```

Staged runtime reconnect smoke testini calistirmak icin:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-runtime-reconnect.ps1
```

Save, respawn veya session pozisyon mantigi degistiginde arena bounds runtime smoke testini calistirmak icin:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-arena-bounds.ps1
```

Bu test `BPHero` karakterini bilerek arenanin altinda guvensiz bir kayitli pozisyona yerlestirir, staged runtime akisni baslatir ve aktif session'in ayarlanmis arena sinirlari icine geri zorlandigini dogrular.

Kalici persistence telemetry durumunu incelemek icin:

```powershell
Invoke-RestMethod http://127.0.0.1:3000/ops/persistence
```

Telemetry export almak icin session server secret ile yetkili istek gonderilir:

```powershell
Invoke-RestMethod http://127.0.0.1:3000/ops/persistence/export -Headers @{ "X-Session-Server-Secret" = "<session-server-secret>" }
```

Telemetry sayaclarini sifirlamak icin:

```powershell
Invoke-RestMethod -Method POST http://127.0.0.1:3000/ops/persistence/reset -Headers @{ "X-Session-Server-Secret" = "<session-server-secret>" } -ContentType "application/json" -Body "{}"
```

Takip edilen sayaclar sunlari kapsar: kabul edilen save'ler, yok sayilan eski save'ler, reddedilen persistence istekleri, timeout olan session'lar, reconnect denemeleri, kabul edilen heartbeat'ler, baslatilan session'lar ve bitirilen session'lar. Bu sayaclar `PersistenceTelemetry` tablosunda kalici olarak tutulur ve backend yeniden basladiginda tekrar yuklenir. Ops cevabi ayrica aktif session timeout suresini ve reaper interval degerini de gosterir.

Persistence route'lari artik `/auth/login` tarafindan verilen imzali bearer token'lari zorunlu tutar:

```http
Authorization: Bearer <signed-auth-token>
```

Karakter listeleme/olusturma/yukleme/kaydetme ve session start/end/heartbeat istekleri, oyuncu tarafindan yapiliyorsa eksik, gecersiz, suresi dolmus veya karakter sahibi olmayan token'lari reddeder.

Dedicated server persistence yazimlari, oyuncunun bearer token'i yerine ayri bir servis header'i kullanir:

```http
X-Session-Server-Secret: <session-server-secret>
```

Unreal client artik imzali kullanici token'ini travel URL'lerine koymaz. Dedicated server autosave, heartbeat ve logout final-save islemleri session server olarak authenticate olur. Buna ragmen hangi session'in karakter durumunu yazabilecegini hala aktif `characterId` + `sessionId` ikilisi belirler.

`SESSION_SERVER_SECRET` artik checked-in Unreal config icinde tutulmaz. Yerel calismada backend `server/.env` dosyasindan okur; staged dedicated server ise ayni degeri environment uzerinden `MOBAMMO_SESSION_SERVER_SECRET` olarak alir. `APP_ENV` veya `NODE_ENV` development disi bir degere ayarlandiginda backend, dev/default secret degerleriyle baslamayi reddeder.

Test sunlari dogrular:

- Bitmis session'lar save yazamaz.
- Aktif session'lar heartbeat gonderebilir.
- Timeout olmus session'lar heartbeat veya save gonderemez.
- Yeni bir session, son kabul edilen pozisyondan devam eder.
- Yeni session tekrar save yazabilir.

## Siradaki Roadmap

1. Dedicated server secret rotasyonu icin deploy ortamina ozel secret kaynagini netlestirmek.
2. Persistence telemetry icin daha detayli olay gecmisi veya metrik backend entegrasyonu eklemek.
