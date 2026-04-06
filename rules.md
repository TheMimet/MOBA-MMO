# MOBA-MMO Server Rules

Bu dosya, MOBA-MMO projesinde server tarafını geliştirirken takip edeceğimiz temel kuralları ve aşamaları içerir.
Amaç, scope'u kontrol altında tutmak ve server mimarisini küçük, güvenli adımlarla kurmaktır.

## 1. Temel İlke

Server tarafında iki katmanı birbirine karıştırmayacağız:

- `Game Server`: Unreal Dedicated Server
- `Backend`: hesap, karakter, envanter, progression ve kayıt işlemleri

Canlı oyun durumu Unreal server'da tutulur.
Kalıcı veri backend ve veritabanında tutulur.

## 2. Sorumluluk Ayrımı

### Unreal Dedicated Server
- Oyuncu bağlantısı
- Spawn / respawn
- Hareket doğrulama
- Combat authority
- Skill kullanımı
- Enemy AI
- Loot drop
- Zone veya match state

### Backend API
- Login veya mock login
- Karakter listesi
- Karakter oluşturma
- Karakter yükleme
- Karakter kaydetme
- Inventory persistence
- Quest/progression persistence

### Database
- Account
- Character
- CharacterStats
- InventoryItem
- QuestState

## 3. İlk Sürümde Yapılmayacaklar

İlk aşamada aşağıdakiler yapılmayacak:

- Mikroservis mimarisi
- Kubernetes veya ileri deployment yapıları
- Çoklu shard koordinasyonu
- Gerçek zone transfer altyapısı
- Guild
- Marketplace
- Chat sistemi
- Karmaşık sosyal sistemler

Önce tek çalışan akış kurulacak.

## 4. Mimari Kuralı

Şu ayrım kesin korunacak:

- `Canlı gameplay state` -> Unreal Dedicated Server
- `Kalıcı gameplay state` -> Backend + Database

Örnek:

- Oyuncu hasar alır -> Unreal server
- Oyuncu item kazanır -> Unreal state değişir, uygun anda backend'e save edilir
- Oyuncu logout olur -> backend'e persist edilir
- Oyuncu login olur -> backend'den yüklenir

Combat mantığı backend API'ye taşınmayacak.

## 5. İlk Teknik Hedef

İlk çalışan yapı şu olacak:

- `1 Unreal Dedicated Server`
- `1 Backend API`
- `1 Database`

Önerilen başlangıç stack'i:

- Unreal Dedicated Server
- Backend: Node.js + Fastify veya NestJS
- Database: PostgreSQL

Bu yapı ilk geliştirme evresi için yeterlidir.

## 6. İlk Gerekli Endpointler

İlk backend sözleşmesi şu endpointleri içermelidir:

- `POST /auth/login`
- `GET /characters`
- `POST /characters`
- `POST /session/start`
- `GET /characters/{id}/load`
- `POST /characters/{id}/save`

İlk etapta mock auth kabul edilebilir.

## 7. İlk Milestone

İlk milestone şu akışı eksiksiz çalıştırmalıdır:

1. Oyuncu login olur
2. Karakter seçer veya oluşturur
3. Dedicated server'a bağlanır
4. Spawn olur
5. Hareket eder
6. Combat'a girebilir
7. Logout olur
8. Pozisyon, level ve inventory kaydedilir
9. Tekrar girişte veri geri yüklenir

Bu milestone tamamlanmadan ileri seviye MMO özelliklerine geçilmeyecek.

## 8. Çalışma Sırası

Server geliştirmesi şu sırayla ilerleyecek:

1. Server kapsamını kilitle
2. En basit mimariyi kur
3. Veri modellerini ve endpoint sözleşmesini yaz
4. Unreal ile backend arasındaki sınırı netleştir
5. İlk çalışan login -> join -> save akışını kur
6. Sonrasında güvenlik, loglama ve ölçekleme ele alınır

## 9. Bu Kuralları Kullanma Şekli

Her yeni server görevi başlamadan önce şu kontrol yapılacak:

- Bu iş Unreal server işi mi, backend işi mi?
- Bu veri canlı state mi, kalıcı state mi?
- Bu özellik ilk milestone için gerçekten gerekli mi?

Bu üç sorudan biri net cevaplanamıyorsa geliştirme başlamadan önce mimari not güncellenecek.

