# Blueprint Backend Flow

## Amaç
Bu akış, Unreal Blueprint tarafında backend login -> karakter oluşturma -> session start -> dedicated server join zincirini kurmak için hazırlanmıştır.

## Hazır C++ Parça
Projede artık `UMOBAMMOBackendSubsystem` vardır.

Blueprint içinden erişim:
- `Get Game Instance Subsystem`
- Class olarak `MOBAMMOBackendSubsystem`

Kullanılabilir fonksiyonlar:
- `MockLogin`
- `CreateCharacter`
- `StartSession`
- `TravelToSession`

Kullanılabilir event'ler:
- `OnLoginSucceeded`
- `OnLoginFailed`
- `OnCharacterCreated`
- `OnCharacterCreateFailed`
- `OnSessionStarted`
- `OnSessionStartFailed`

## Önerilen Blueprint Yeri
İlk kurulum için en kolay yer:
- `BP_ThirdPersonPlayerController`
veya
- geçici bir `WBP_LoginTest` widget'ı

Benim önerim:
- test aşamasında `BP_ThirdPersonPlayerController`

## Basit Test Akışı
1. BeginPlay içinde `Get Game Instance Subsystem(MOBAMMOBackendSubsystem)` al
2. Referansı bir değişkene kaydet: `BackendSubsystem`
3. Event binding yap:
   - `Bind Event to OnLoginSucceeded`
   - `Bind Event to OnCharacterCreated`
   - `Bind Event to OnSessionStarted`
   - hata event'lerini de `Print String` ile bağla

## Event Zinciri

### 1. Login başlat
Örnek:
- `MockLogin("mimet_bp_test")`

### 2. Login başarılı olunca
`OnLoginSucceeded(Result)` içinde:
- `Result.AccountId` al
- `CreateCharacter(Result.AccountId, "BPHero", "fighter")` çağır

Not:
- şimdilik hızlı test için her girişte yeni karakter oluşturabilirsiniz
- sonra `GET /characters` ile seçim ekranı eklenir

### 3. Karakter oluşturulunca
`OnCharacterCreated(Result)` içinde:
- `StartSession(Result.CharacterId)` çağır

### 4. Session başlayınca
`OnSessionStarted(Result)` içinde:
- `Get Player Controller`
- `TravelToSession(PlayerController, Result.ConnectString)` çağır

Bu sizi `127.0.0.1:7777` üstündeki dedicated server'a gönderir.

## Backend Ayarı
Backend URL config dosyasında tutulur:
- `Config/DefaultGame.ini`

Alan:
- `BackendBaseUrl=http://127.0.0.1:3000`

Backend farklı makinede çalışırsa bunu güncelleyin.

## Local Test Sırası
1. Backend API'yi başlat
2. Dedicated server'ı başlat
3. Unreal client'ı aç
4. Blueprint zinciri login -> create -> session -> travel çalıştırır

## Local Komutlar

### Backend
```powershell
cd "C:\Users\PC\OneDrive\Desktop\MOBA MMO\MOBAMMO\server"
npm run db:start
npm run dev
```

### Dedicated Server
```powershell
cd "C:\Users\PC\OneDrive\Desktop\MOBA MMO\MOBAMMO"
powershell -ExecutionPolicy Bypass -File .\scripts\run-staged-server.ps1
```

### Kapatma
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\stop-server.ps1
```

## Sonraki Geliştirme
Bu ilk sürümden sonra şu iyileştirmeler yapılmalı:
- `GET /characters` ile karakter listeleme
- widget tabanlı login ekranı
- karakter zaten varsa yeniden oluşturmak yerine seçmek
- token/account state'ini GameInstance içinde saklamak
- logout ve save akışı
