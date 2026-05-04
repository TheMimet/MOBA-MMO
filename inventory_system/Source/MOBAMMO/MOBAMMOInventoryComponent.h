// Source/MOBAMMO/MOBAMMOInventoryComponent.h
// Replicated inventory component.
// Character'a eklenir, server authority ile çalışır.
// Backend sync: join sırasında load, logout/periyodik save.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOBAMMOInventoryComponent.generated.h"

// ─────────────────────────────────────────────────────────
// ITEM STRUCT
// ─────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EMOBAMMOItemRarity : uint8
{
    Common    UMETA(DisplayName = "Common"),
    Uncommon  UMETA(DisplayName = "Uncommon"),
    Rare      UMETA(DisplayName = "Rare"),
    Epic      UMETA(DisplayName = "Epic"),
};

UENUM(BlueprintType)
enum class EMOBAMMOEquipSlot : uint8
{
    None      UMETA(DisplayName = "None / Bag"),
    Weapon    UMETA(DisplayName = "Weapon"),
    Offhand   UMETA(DisplayName = "Offhand"),
    Head      UMETA(DisplayName = "Head"),
    Body      UMETA(DisplayName = "Body"),
    Boots     UMETA(DisplayName = "Boots"),
    Accessory UMETA(DisplayName = "Accessory"),
};

USTRUCT(BlueprintType)
struct FMOBAMMOInventoryItem
{
    GENERATED_BODY()

    // Backend'den gelen UUID
    UPROPERTY(BlueprintReadOnly)
    FString ItemId;

    // Item kataloğundaki tanım ID'si
    UPROPERTY(BlueprintReadOnly)
    FString ItemDefinitionId;

    // Görünen ad (backend'den / lokal katalogdan)
    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 1;

    UPROPERTY(BlueprintReadOnly)
    EMOBAMMOEquipSlot EquipSlot = EMOBAMMOEquipSlot::None;

    UPROPERTY(BlueprintReadOnly)
    EMOBAMMOItemRarity Rarity = EMOBAMMOItemRarity::Common;

    // Stat bonusları (equip edilince PlayerState'e eklenir)
    UPROPERTY(BlueprintReadOnly)
    int32 MaxHpBonus = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxManaBonus = 0;

    // Consumable ise: kullanınca HP/Mana restore
    UPROPERTY(BlueprintReadOnly)
    int32 OnUseHpRestore = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OnUseManaRestore = 0;

    bool IsValid() const { return !ItemId.IsEmpty() && !ItemDefinitionId.IsEmpty(); }
    bool IsEquipped() const { return EquipSlot != EMOBAMMOEquipSlot::None; }
    bool IsConsumable() const { return OnUseHpRestore > 0 || OnUseManaRestore > 0; }
};

// ─────────────────────────────────────────────────────────
// COMPONENT
// ─────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUsed, const FMOBAMMOInventoryItem&, Item);

UCLASS(ClassGroup=(MOBAMMO), meta=(BlueprintSpawnableComponent))
class MOBAMMO_API UMOBAMMOInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMOBAMMOInventoryComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Okuma (client + server) ──────────────────────────

    UFUNCTION(BlueprintCallable, Category="Inventory")
    const TArray<FMOBAMMOInventoryItem>& GetAllItems() const { return Items; }

    UFUNCTION(BlueprintCallable, Category="Inventory")
    TArray<FMOBAMMOInventoryItem> GetEquippedItems() const;

    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool FindItemById(const FString& ItemId, FMOBAMMOInventoryItem& OutItem) const;

    // Equip edilmiş tüm itemlardan toplam stat bonus
    UFUNCTION(BlueprintCallable, Category="Inventory")
    void GetEquippedStatBonuses(int32& OutMaxHpBonus, int32& OutMaxManaBonus) const;

    // ── Server-only (GameMode tarafından çağrılır) ───────

    // Backend'den yüklenen JSON item listesini parse et ve Items'a yaz
    void LoadFromBackendJson(const FString& JsonString);

    // Item ekle (loot drop, debug)
    UFUNCTION(BlueprintCallable, Category="Inventory", meta=(AuthorityOnly))
    bool Server_AddItem(const FMOBAMMOInventoryItem& NewItem);

    // Consumable kullan
    UFUNCTION(BlueprintCallable, Category="Inventory", meta=(AuthorityOnly))
    bool Server_UseItem(const FString& ItemId);

    // Equip/Unequip
    UFUNCTION(BlueprintCallable, Category="Inventory", meta=(AuthorityOnly))
    bool Server_EquipItem(const FString& ItemId, EMOBAMMOEquipSlot Slot);

    UFUNCTION(BlueprintCallable, Category="Inventory", meta=(AuthorityOnly))
    bool Server_UnequipItem(const FString& ItemId);

    // Item sil
    UFUNCTION(BlueprintCallable, Category="Inventory", meta=(AuthorityOnly))
    bool Server_RemoveItem(const FString& ItemId, int32 Quantity = 1);

    // Backend'e sync edilecek JSON payload oluştur
    FString BuildSavePayload() const;

    // ── Delegateler ──────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category="Inventory")
    FOnInventoryChanged OnInventoryChanged;

    UPROPERTY(BlueprintAssignable, Category="Inventory")
    FOnItemUsed OnItemUsed;

protected:
    virtual void BeginPlay() override;

private:
    // Replicated item listesi (server authority)
    UPROPERTY(ReplicatedUsing=OnRep_Items)
    TArray<FMOBAMMOInventoryItem> Items;

    UFUNCTION()
    void OnRep_Items();

    // Equip edilmiş itemların stat bonuslarını PlayerState'e uygula
    void ApplyEquippedBonusesToPlayerState();

    // Yardımcı: tek item'ı JSON objesinden parse et
    static bool ParseItemFromJson(const TSharedPtr<class FJsonObject>& Json,
                                  FMOBAMMOInventoryItem& OutItem);
};
