# Debug UI Plan

## Amaç
İlk görünür arayüz olarak backend ve server akışını ekranda gösterecek basit bir debug widget kurmak.

## İlk Widget
Ad:
- `WBP_DebugLogin`

İçerik:
- `Backend URL`
- `Login Status`
- `Character Status`
- `Session Status`
- `Last Error`
- `Last Username`
- `Account Id`
- `Character Id`
- `Connect String`

Butonlar:
- `Mock Login`
- `Create Character`
- `Start Session`
- `Join Server`

## Kullanılacak Subsystem Getter'ları
- `GetBackendBaseUrl`
- `GetLoginStatus`
- `GetCharacterStatus`
- `GetSessionStatus`
- `GetLastErrorMessage`
- `GetLastUsername`
- `GetLastAccountId`
- `GetLastCharacterId`
- `GetLastSessionConnectString`

## İlk Görsel Düzen
Yerleşim:
- Sol üstte küçük bir panel
- Siyah yarı saydam arka plan
- Beyaz metin
- Yeşil başarılı durum
- Sarı bekleyen durum
- Kırmızı hata durumu

## Sonraki Adım
Bu widget çalıştıktan sonra:
1. gerçek join doğrulaması
2. loading ekranı
3. karakter seçimi
4. oyun içi HUD
