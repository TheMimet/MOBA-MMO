# Inventory Sistemi — Entegrasyon Kılavuzu

Bu klasördeki dosyalar MOBA-MMO projesine doğrudan kopyalanabilir.

---

## 1. Backend Entegrasyonu

### 1.1 Dosyaları kopyala
```
server/prisma/schema.prisma              ← mevcut schema'yı bu ile değiştir
server/src/modules/inventory/
  ├── itemDefinitions.ts                 ← item kataloğu
  ├── service.ts                         ← business logic
  └── routes.ts                          ← HTTP endpoint'ler
server/prisma/migrations/add_inventory/
  └── migration.sql                      ← referans (prisma migrate dev kullan)
```

### 1.2 Migration çalıştır
```bash
cd server
npx prisma migrate dev --name add_inventory
npx prisma generate
```

### 1.3 Ana router'a kaydet
`server/src/index.ts` veya `server/src/app.ts`'e ekle:

```typescript
import { inventoryRoutes } from './modules/inventory/routes';

// Mevcut character route kayıt satırının yanına:
app.use('/characters', inventoryRoutes);
```

### 1.4 accountId header'ı (mock auth, Phase 2'de JWT'ye geçilecek)
Tüm inventory istekleri `x-account-id` header'ı bekler.
BackendSubsystem zaten accountId'yi biliyor, her isteğe ekle.

---

## 2. Unreal C++ Entegrasyonu

### 2.1 Dosyaları kopyala
```
Source/MOBAMMO/
  ├── MOBAMMOInventoryComponent.h
  ├── MOBAMMOInventoryComponent.cpp
  ├── MOBAMMOPlayerState_InventoryAdditions.h   ← içindeki yorum bloklarını uygula
  └── MOBAMMOBackendSubsystem_InventoryAdditions.cpp ← ilgili metodları BackendSubsystem'a taşı
```

### 2.2 Build.cs'e Json modülleri ekle
`Source/MOBAMMO/MOBAMMO.Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    // ... mevcut ...
    "Json",
    "JsonUtilities",
});
```

### 2.3 MOBAMMOCharacter'a component ekle
`MOBAMMOCharacter.h`:
```cpp
#include "MOBAMMOInventoryComponent.h"

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
UMOBAMMOInventoryComponent* InventoryComponent;
```

`MOBAMMOCharacter.cpp` constructor:
```cpp
InventoryComponent = CreateDefaultSubobject<UMOBAMMOInventoryComponent>(
    TEXT("InventoryComponent"));
```

### 2.4 MOBAMMOPlayerState'e bonus alanları ekle
`MOBAMMOPlayerState_InventoryAdditions.h` içindeki yorum bloklarını uygula:
- `SetEquipmentBonuses()` metodu
- `EquipMaxHpBonus`, `EquipMaxManaBonus` replicated alanlar
- `GetEffectiveMaxHp()`, `GetEffectiveMaxMana()` hesaplamaları

### 2.5 Character join'de inventory yükle
`MOBAMMOCharacter.cpp` → `PossessedBy()` override:
```cpp
void AMOBAMMOCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    UMOBAMMOBackendSubsystem* Backend =
        GetWorld()->GetSubsystem<UMOBAMMOBackendSubsystem>();
    if (!Backend || !InventoryComponent) return;

    FString CharId = /* PS->GetCharacterId() */;
    Backend->LoadInventory(CharId, [this](const FString& Json)
    {
        if (Json.IsEmpty()) return;
        InventoryComponent->LoadFromBackendJson(Json);
    });
}
```

### 2.6 Minion ölünce loot ver
`MOBAMMOGameMode.cpp`'deki minion death callback'ine:
```cpp
// Minion öldüğünde:
GrantMinionLootToPlayer(KillerController);
```
`GrantMinionLootToPlayer` örneği: `MOBAMMOBackendSubsystem_InventoryAdditions.cpp` içinde.

---

## 3. Endpoint Referansı

| Method | URL | Açıklama |
|--------|-----|----------|
| GET | /characters/:id/inventory | Tüm envanter |
| GET | /characters/:id/inventory/catalog | Item kataloğu |
| POST | /characters/:id/inventory/add | Item ekle |
| POST | /characters/:id/inventory/:itemId/equip | Equip et |
| POST | /characters/:id/inventory/:itemId/unequip | Unequip et |
| POST | /characters/:id/inventory/:itemId/use | Consumable kullan |
| DELETE | /characters/:id/inventory/:itemId | Item sil |

---

## 4. Test Akışı

```
1. TamBaslatma.bat ile sistemi başlat
2. Login ol, karakter seç
3. Dedicated server'a gir
4. Debug: POST /characters/:id/inventory/add { "itemDefinitionId": "sparring_token", "quantity": 3 }
5. GET /characters/:id/inventory → item listesini gör
6. Minion'u öldür → sparring_token loot düşmeli
7. POST /characters/:id/inventory/add { "itemDefinitionId": "health_potion_small", "quantity": 1 }
8. POST /characters/:id/inventory/:itemId/use → HP arttı mı?
9. Logout → tekrar login → inventory korunuyor mu?
```

---

## 5. Sonraki Adımlar

- HUD'a inventory panel ekle (Blueprint / native widget)
- Equip edilince character mesh'ine weapon attach et
- Loot tablosunu GameMode'dan ayrı bir `LootTableComponent`'a taşı
- Phase 2: JWT token ile `x-account-id` header'ını kaldır
