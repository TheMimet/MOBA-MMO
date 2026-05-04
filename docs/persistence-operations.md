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

Son persistence olaylarini incelemek icin:

```powershell
Invoke-RestMethod "http://127.0.0.1:3000/ops/persistence/events?limit=50"
```

Event history `PersistenceEvent` tablosunda tutulur. Her olay event tipi, karakter id, session id, status code, hata kodu, kisa mesaj ve opsiyonel metadata ile saklanir. `/ops/persistence` cevabi ayrica son olaylardan kucuk bir pencere dondurur.

Tarayicidan hizli operasyon panelini acmak icin:

```text
http://127.0.0.1:3000/ops/persistence/dashboard
```

Bu panel persistence sayaclarini, session timeout/reaper ayarlarini ve son persistence event'lerini 10 saniyede bir yenilenen basit bir dashboard olarak gosterir.

WebUI icindeki admin/ops ekrani:

```text
http://127.0.0.1:3000/ops/persistence
```

Bu ekran backend `GET /ops/persistence` endpoint'ini okuyarak save, heartbeat, rejected, timeout ve recent event verilerini WebUI icinde gosterir. Reset ve export aksiyonlari icin session server secret kullanir.

Production ops korumasi:

- `OPS_REQUIRE_ADMIN_SECRET=true` ise `GET /ops/persistence`, `/ops/persistence/dashboard`, `/ops/persistence/events` ve `/ops/persistence/metrics` endpoint'leri `X-Ops-Admin-Secret: <OPS_ADMIN_SECRET>` header'i ister.
- Production-like ortamlarda `OPS_ADMIN_SECRET` zorunludur ve `OPS_REQUIRE_ADMIN_SECRET=false` ile backend baslamaz.
- Ops endpoint'leri `Authorization: Bearer <admin-access-token>` ile de acilabilir. Admin yetkisi `Account.role = "admin"` alanindan gelir.
- `AUTH_ADMIN_USERNAMES` comma-separated listesi, ilgili username'leri login/register sirasinda otomatik admin role'e yukseltebilir.
- Admin recovery secret ile manuel rol atamak icin `POST /auth/admin/accounts/role` kullanilir.
- Reset/export aksiyonlari `X-Ops-Admin-Secret`, admin bearer token veya `X-Session-Server-Secret` kabul eder.
- Local/dev ortamda `OPS_REQUIRE_ADMIN_SECRET=false` ile okuma endpoint'leri acik kalabilir; WebUI ops ekrani yine secret girildiginde header gonderir.
- WebUI `/ops/persistence` ekrani admin username/password ile `/auth/login` cagirabilir; account role `admin` ise access token sessionStorage'da tutulur ve ops isteklerine Bearer token olarak eklenir.
- Backend CORS preflight cevaplari `Authorization`, `X-Ops-Admin-Secret`, `X-Session-Server-Secret` ve admin recovery header'larini destekler; bu sayede WebUI farkli local porttan ops API'ye erisebilir.
- Unreal WebUI bridge login/character/main-menu state'ine `accountRole` ve admin ise `opsAccessToken` alanlarini push eder. `/ops/persistence` sayfasi UE state icinden gelen admin token'i otomatik kullanabilir.
- Ops mutasyonlari `OpsAuditEvent` tablosuna yazilir. Export, reset, admin password reset issue ve admin role update aksiyonlari actor tipi, account, hedef, status ve zaman bilgisiyle izlenir.
- WebUI ops ekraninda audit kayitlari `Audit Trail` panelinde server-side search, status filtresi, prev/next sayfalama ve metadata detay gorunumuyle incelenebilir.
- `/ops/persistence` cevabi secret degerlerini aciga cikarmadan `opsSecurity` snapshot'i dondurur. WebUI `Security Readiness` panelinde ops read/mutation protection, password reset provider, audit cleanup, admin seed ve session secret rotation durumunu gosterir.
- `GET /ops/security/preflight` ayni security snapshot'ini readiness score, checklist, aciklama ve remediation notlariyla JSON olarak indirilebilir hale getirir.
- WebUI `Security Readiness` paneli insan okunur `Deploy Runbook` listesi gosterir; failed maddelerde hangi environment/config degerinin duzeltilmesi gerektigini yazar.
- Preflight export islemleri `ops_security_preflight` audit event'i olarak kaydedilir. `/ops/persistence` son accepted preflight zamanini, skorunu ve failed check sayisini WebUI'ye gonderir.
- `GET /ops/password-reset/provider` password reset delivery provider durumunu secret aciga cikarmadan dondurur.
- `POST /ops/password-reset/provider/test` varsayilan dry-run modunda provider config'ini test eder ve `password_reset_provider_test` audit event'i yazar. `dryRun=false` sadece webhook provider ve URL hazirsa test payload'i yollar.
- Password reset request'leri `AuthPasswordResetDelivery` outbox kaydi olusturur. Reset token DB'ye duz metin yazilmaz; retry icin gereken token payload'i `AUTH_TOKEN_SECRET` tabanli AES-GCM ile sifrelenir.
- `GET /ops/password-reset/deliveries` son delivery outbox kayitlarini token aciga cikarmadan listeler.
- `POST /ops/password-reset/deliveries/:id/retry` failed/pending delivery icin provider'a tekrar gonderim dener ve sonucu `password_reset_delivery_retry` audit event'i olarak yazar.
- `AUTH_PASSWORD_RESET_DELIVERY_RETRY_INTERVAL_SECONDS` failed/pending outbox kayitlari icin otomatik retry worker araligini belirler; `0` verilirse worker kapanir.
- `/ops/persistence` password reset delivery status sayimlarini ve retry worker snapshot'ini dondurur. WebUI provider panelinde delivered/failed/pending/skipped sayilari, worker run sayisi ve son worker sonucu gorunur.
- `/ops/persistence/metrics` `mobammo_password_reset_delivery_status_total{status="..."}` gauge satirlariyla outbox durumlarini Prometheus formatinda yayinlar.
- Retry worker en az bir due kayit islediginde `password_reset_delivery_worker` audit event'i yazar ve actor tipi `system-worker` olur.
- `AUTH_PASSWORD_RESET_DELIVERY_RETENTION_DAYS` delivered/skipped/expired outbox kayitlarinin retention suresini belirler. Worker bu kayitlari otomatik temizler.
- `POST /ops/password-reset/deliveries/prune` ayni retention kuralini manuel calistirir ve `password_reset_delivery_prune` audit event'i yazar.
- `GET /ops/audit` action, actor, status, target, search, limit ve offset query alanlariyla audit kayitlarini filtreler/sayfalar.
- `GET /ops/audit/export` filtrelenmis audit kayitlarini JSON olarak indirir.
- `POST /ops/audit/prune` `OPS_AUDIT_RETENTION_DAYS` degerinden eski audit kayitlarini temizler.
- Server acilisinda ve `OPS_AUDIT_PRUNE_INTERVAL_SECONDS` araliginda background cleanup calisir. Varsayilan 6 saattir; `0` verilirse otomatik temizlik kapanir.

Admin role verme ornegi:

```powershell
$headers = @{ "X-Admin-Recovery-Secret" = $env:AUTH_ADMIN_RECOVERY_SECRET }
Invoke-RestMethod -Method POST http://127.0.0.1:3000/auth/admin/accounts/role -Headers $headers -ContentType "application/json" -Body '{"username":"Mimet","role":"admin"}'
```

Prometheus-style metrik cikisi almak icin:

```powershell
Invoke-RestMethod http://127.0.0.1:3000/ops/persistence/metrics
```

Bu endpoint `mobammo_persistence_*_total`, `mobammo_session_timeout_seconds`, `mobammo_session_reaper_interval_seconds` ve son olay timestamp metrigini dondurur. Production monitoring tarafinda scrape edilecek en basit kaynak burasidir.

Persistence route'lari artik `/auth/login` tarafindan verilen imzali bearer token'lari zorunlu tutar:

```http
Authorization: Bearer <signed-auth-token>
```

Auth lifecycle artik su endpoint'lerden olusur:

- `POST /auth/register`: username + password ile account olusturur veya local legacy account'a password baglar.
- `POST /auth/login`: password ile login olur; local/dev ortamda mevcut UE bridge icin username-only legacy fallback devam eder.
- `POST /auth/refresh`: refresh token'i rotate eder ve yeni access token dondurur.
- `POST /auth/logout`: refresh token'i revoke eder.
- `POST /auth/password-reset/request`: username icin password reset token'i uretir.
- `POST /auth/password-reset/confirm`: reset token + yeni password ile sifreyi degistirir.
- `GET /auth/me`: bearer token ile aktif account bilgisini dondurur.

Account password'leri scrypt hash olarak tutulur. Refresh token'lar ham haliyle DB'ye yazilmaz; sadece SHA-256 hash'i, expiry ve revoke bilgisi saklanir. Production-like ortamlarda `AUTH_ALLOW_LEGACY_USERNAME_LOGIN=true` ile backend baslatilmaz.

Login guvenlik katmani:

- IP + username kombinasyonu icin memory rate-limit uygulanir.
- Varsayilan pencere `AUTH_LOGIN_RATE_LIMIT_WINDOW_SECONDS=300`, limit `AUTH_LOGIN_RATE_LIMIT_MAX_ATTEMPTS=20`.
- Password'lu login'de account bazli basarisiz deneme sayaci tutulur.
- Varsayilan olarak `AUTH_LOGIN_LOCKOUT_THRESHOLD=5` hatali denemeden sonra account `AUTH_LOGIN_LOCKOUT_SECONDS=900` saniye kilitlenir.
- Basarili login/register/refresh session uretimi account lockout sayaclarini sifirlar.

Password reset katmani:

- Reset token'lar ham haliyle DB'ye yazilmaz; `AuthPasswordResetToken.tokenHash` alaninda hash olarak tutulur.
- Reset confirm basarili olunca password degisir, account lockout sayaclari temizlenir ve mevcut refresh token'lar revoke edilir.
- Local/dev ortamda email provider olmadigi icin `AUTH_PASSWORD_RESET_EXPOSE_TOKEN=true` reset token'i API cevabinda dondurebilir.
- Production-like ortamlarda `AUTH_PASSWORD_RESET_EXPOSE_TOKEN=true` ile backend baslamaz; bu adimda token'in email/provider uzerinden gonderilmesi gerekir.
- `AUTH_PASSWORD_RESET_DELIVERY_PROVIDER=response|log|webhook` delivery davranisini belirler.
- Production-like ortamlarda provider `webhook` olmak zorundadir ve `AUTH_PASSWORD_RESET_WEBHOOK_URL` zorunludur.
- Webhook provider `POST AUTH_PASSWORD_RESET_WEBHOOK_URL` istegiyle `event`, `accountId`, `username`, `resetToken`, `expiresAt` ve `expiresIn` alanlarini gonderir.
- `AUTH_PASSWORD_RESET_WEBHOOK_SECRET` verilirse webhook istegine `X-Password-Reset-Webhook-Secret` header'i eklenir.
- Email provider baglanana kadar admin recovery kanali vardir: `POST /auth/admin/password-reset/issue-token`.
- Admin recovery endpoint'i `X-Admin-Recovery-Secret: <AUTH_ADMIN_RECOVERY_SECRET>` header'i ister, account icin yeni tek kullanimlik reset token uretir ve aktif eski reset token'larini tuketir.
- Production-like ortamlarda `AUTH_ADMIN_RECOVERY_SECRET` zorunludur ve dev/default secret ile backend baslamaz.

Admin recovery ile sifre sifirlama ornegi:

```powershell
$headers = @{ "X-Admin-Recovery-Secret" = $env:AUTH_ADMIN_RECOVERY_SECRET }
$issued = Invoke-RestMethod -Method POST http://127.0.0.1:3000/auth/admin/password-reset/issue-token -Headers $headers -ContentType "application/json" -Body '{"username":"Mimet"}'
Invoke-RestMethod -Method POST http://127.0.0.1:3000/auth/password-reset/confirm -ContentType "application/json" -Body (@{ resetToken = $issued.resetToken; password = "new-password" } | ConvertTo-Json)
```

Karakter listeleme/olusturma/yukleme/kaydetme ve session start/end/heartbeat istekleri, oyuncu tarafindan yapiliyorsa eksik, gecersiz, suresi dolmus veya karakter sahibi olmayan access token'lari reddeder.

Unreal WebUI bridge tarafinda login ekrani su `UE_ACTION` tiplerini destekler:

- `mockLogin`: eski username-only local/dev login akisi; password alani gelirse `/auth/login` istegine eklenir.
- `login` veya `authLogin`: username + password ile `/auth/login` cagrisi yapar.
- `register` veya `createAccount`: username + password ile `/auth/register` cagrisi yapar.
- `refreshAuth`: saklanan refresh token ile `/auth/refresh` cagrisi yapar ve token'i rotate eder.
- `logout`: saklanan refresh token'i `/auth/logout` ile revoke eder ve local auth/session state'ini temizler.

Bu bridge gecis katmani sayesinde mevcut WebUI username-only login'i local ortamda bozulmadan calisir; password/register destekli UI guncellemesi yapildiginda ayni widget backend'in production lifecycle'ini kullanmaya baslar.

Aktif WebUI kaynak projesi:

```text
C:\Users\PC\OneDrive\Desktop\MOBA MMO\LoginAndCharacterFlowDesign
```

`/login` sayfasi artik su davranisi uygular:

- Password bos ve login modundaysa local/dev uyumlulugu icin `mockLogin` gonderir.
- Password dolu ve login modundaysa `login` action'i gonderir.
- `Create Account` modundaysa en az 6 karakter sifre ister ve `register` action'i gonderir.
- `Remember me` aktifse WebUI sadece son kullanici adini ve tercihi localStorage'da tutar; refresh token WebUI tarafina yazilmaz.

Refresh token saklama politikasi:

- `rememberMe=true` login/register basarisinda refresh token Unreal `Saved/Auth/session.json` dosyasina yazilir.
- Login ekrani acildiginda kayitli refresh token varsa Unreal `/auth/refresh` ile session'i otomatik yeniler.
- Refresh token rotate edildikce ayni dosya yeni token ile guncellenir.
- Logout veya `rememberMe=false` login/register denemesi local dosyayi temizler.
- WebUI localStorage icinde access token veya refresh token tutulmaz.

Dedicated server persistence yazimlari, oyuncunun bearer token'i yerine ayri bir servis header'i kullanir:

```http
X-Session-Server-Secret: <session-server-secret>
```

Unreal client artik imzali kullanici token'ini travel URL'lerine koymaz. Dedicated server autosave, heartbeat ve logout final-save islemleri session server olarak authenticate olur. Buna ragmen hangi session'in karakter durumunu yazabilecegini hala aktif `characterId` + `sessionId` ikilisi belirler.

`SESSION_SERVER_SECRET` artik checked-in Unreal config icinde tutulmaz. Yerel calismada backend `server/.env` dosyasindan okur; staged dedicated server ise ayni degeri environment uzerinden `MOBAMMO_SESSION_SERVER_SECRET` olarak alir. `APP_ENV` veya `NODE_ENV` development disi bir degere ayarlandiginda backend, dev/default secret degerleriyle baslamayi reddeder.

## Dedicated Server Secret Rotasyonu

Backend primary secret olarak `SESSION_SERVER_SECRET` degerini kullanir. Gecis penceresinde eski dedicated server instance'larinin kisa sure daha save/heartbeat yazabilmesi icin `SESSION_SERVER_SECRET_PREVIOUS` comma-separated liste olarak verilebilir:

```env
SESSION_SERVER_SECRET=new-secret
SESSION_SERVER_SECRET_PREVIOUS=old-secret,older-secret
```

Dedicated server process'i `MOBAMMO_SESSION_SERVER_SECRET` ortam degiskeninden okur. Local script'ler bu degeri once environment'tan, yoksa `server/.env` icindeki `SESSION_SERVER_SECRET` alanindan alir.

Rotasyon sirasi:

1. Backend deploy ortaminda `SESSION_SERVER_SECRET_PREVIOUS` icine mevcut secret'i koy.
2. `SESSION_SERVER_SECRET` alanini yeni secret ile guncelle ve backend'i restart et.
3. Dedicated server instance'larini yeni `MOBAMMO_SESSION_SERVER_SECRET` ile restart et.
4. Eski instance kalmadigini dogrula.
5. `SESSION_SERVER_SECRET_PREVIOUS` alanini temizle ve backend'i tekrar restart et.

Test sunlari dogrular:

- Bitmis session'lar save yazamaz.
- Aktif session'lar heartbeat gonderebilir.
- Timeout olmus session'lar heartbeat veya save gonderemez.
- Yeni bir session, son kabul edilen pozisyondan devam eder.
- Yeni session tekrar save yazabilir.

## Siradaki Roadmap

1. Password reset token delivery icin gercek email/provider entegrasyonu eklemek.
2. Password reset webhook receiver/email template provider secimi ve canli delivery entegrasyonu.
