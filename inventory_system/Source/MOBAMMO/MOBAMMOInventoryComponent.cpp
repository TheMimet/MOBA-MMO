// Source/MOBAMMO/MOBAMMOInventoryComponent.cpp

#include "MOBAMMOInventoryComponent.h"
#include "MOBAMMOPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

// EquipSlot numaraları backend ile eşleşmeli:
// 0=Weapon 1=Offhand 2=Head 3=Body 4=Boots 5=Accessory
static EMOBAMMOEquipSlot SlotIndexToEnum(int32 Index)
{
    switch (Index)
    {
    case 0: return EMOBAMMOEquipSlot::Weapon;
    case 1: return EMOBAMMOEquipSlot::Offhand;
    case 2: return EMOBAMMOEquipSlot::Head;
    case 3: return EMOBAMMOEquipSlot::Body;
    case 4: return EMOBAMMOEquipSlot::Boots;
    case 5: return EMOBAMMOEquipSlot::Accessory;
    default: return EMOBAMMOEquipSlot::None;
    }
}

static int32 SlotEnumToIndex(EMOBAMMOEquipSlot Slot)
{
    switch (Slot)
    {
    case EMOBAMMOEquipSlot::Weapon:    return 0;
    case EMOBAMMOEquipSlot::Offhand:   return 1;
    case EMOBAMMOEquipSlot::Head:      return 2;
    case EMOBAMMOEquipSlot::Body:      return 3;
    case EMOBAMMOEquipSlot::Boots:     return 4;
    case EMOBAMMOEquipSlot::Accessory: return 5;
    default:                           return -1;
    }
}

// ─────────────────────────────────────────────────────────

UMOBAMMOInventoryComponent::UMOBAMMOInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    bReplicateUsingRegisteredSubObjectList = true;
}

void UMOBAMMOInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UMOBAMMOInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMOBAMMOInventoryComponent, Items);
}

// ─────────────────────────────────────────────────────────
// OnRep
// ─────────────────────────────────────────────────────────

void UMOBAMMOInventoryComponent::OnRep_Items()
{
    OnInventoryChanged.Broadcast();
    // Client tarafında equip bonuslarını göstermek için:
    // HUD refresh burada tetiklenebilir
}

// ─────────────────────────────────────────────────────────
// OKUMA
// ─────────────────────────────────────────────────────────

TArray<FMOBAMMOInventoryItem> UMOBAMMOInventoryComponent::GetEquippedItems() const
{
    return Items.FilterByPredicate([](const FMOBAMMOInventoryItem& I) {
        return I.IsEquipped();
    });
}

bool UMOBAMMOInventoryComponent::FindItemById(const FString& ItemId,
                                               FMOBAMMOInventoryItem& OutItem) const
{
    const FMOBAMMOInventoryItem* Found = Items.FindByPredicate(
        [&ItemId](const FMOBAMMOInventoryItem& I) { return I.ItemId == ItemId; });
    if (Found)
    {
        OutItem = *Found;
        return true;
    }
    return false;
}

void UMOBAMMOInventoryComponent::GetEquippedStatBonuses(
    int32& OutMaxHpBonus, int32& OutMaxManaBonus) const
{
    OutMaxHpBonus = OutMaxManaBonus = 0;
    for (const FMOBAMMOInventoryItem& Item : Items)
    {
        if (Item.IsEquipped())
        {
            OutMaxHpBonus   += Item.MaxHpBonus;
            OutMaxManaBonus += Item.MaxManaBonus;
        }
    }
}

// ─────────────────────────────────────────────────────────
// BACKEND JSON LOAD
// ─────────────────────────────────────────────────────────

bool UMOBAMMOInventoryComponent::ParseItemFromJson(
    const TSharedPtr<FJsonObject>& Json, FMOBAMMOInventoryItem& OutItem)
{
    if (!Json.IsValid()) return false;

    OutItem.ItemId           = Json->GetStringField(TEXT("id"));
    OutItem.ItemDefinitionId = Json->GetStringField(TEXT("itemDefinitionId"));
    OutItem.Quantity         = Json->GetIntegerField(TEXT("quantity"));

    // equippedSlot: JSON'da int veya null
    int32 SlotIdx = -1;
    if (Json->TryGetNumberField(TEXT("equippedSlot"), SlotIdx))
        OutItem.EquipSlot = SlotIndexToEnum(SlotIdx);
    else
        OutItem.EquipSlot = EMOBAMMOEquipSlot::None;

    // definition alt objesinden stat ve display bilgileri
    const TSharedPtr<FJsonObject>* DefObj = nullptr;
    if (Json->TryGetObjectField(TEXT("definition"), DefObj) && DefObj && (*DefObj).IsValid())
    {
        OutItem.DisplayName = (*DefObj)->GetStringField(TEXT("name"));

        const TSharedPtr<FJsonObject>* BonusObj = nullptr;
        if ((*DefObj)->TryGetObjectField(TEXT("statBonus"), BonusObj) && BonusObj)
        {
            (*BonusObj)->TryGetNumberField(TEXT("maxHp"),   OutItem.MaxHpBonus);
            (*BonusObj)->TryGetNumberField(TEXT("maxMana"), OutItem.MaxManaBonus);
        }

        const TSharedPtr<FJsonObject>* OnUseObj = nullptr;
        if ((*DefObj)->TryGetObjectField(TEXT("onUse"), OnUseObj) && OnUseObj)
        {
            (*OnUseObj)->TryGetNumberField(TEXT("hp"),   OutItem.OnUseHpRestore);
            (*OnUseObj)->TryGetNumberField(TEXT("mana"), OutItem.OnUseManaRestore);
        }

        FString RarityStr = (*DefObj)->GetStringField(TEXT("rarity"));
        if      (RarityStr == TEXT("uncommon")) OutItem.Rarity = EMOBAMMOItemRarity::Uncommon;
        else if (RarityStr == TEXT("rare"))     OutItem.Rarity = EMOBAMMOItemRarity::Rare;
        else if (RarityStr == TEXT("epic"))     OutItem.Rarity = EMOBAMMOItemRarity::Epic;
        else                                    OutItem.Rarity = EMOBAMMOItemRarity::Common;
    }

    return OutItem.IsValid();
}

void UMOBAMMOInventoryComponent::LoadFromBackendJson(const FString& JsonString)
{
    check(GetOwner() && GetOwner()->HasAuthority());

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* InventoryArr = nullptr;
    if (!Root->TryGetArrayField(TEXT("inventory"), InventoryArr)) return;

    Items.Empty();
    for (const TSharedPtr<FJsonValue>& Val : *InventoryArr)
    {
        const TSharedPtr<FJsonObject>* ItemObj = nullptr;
        if (!Val->TryGetObject(ItemObj)) continue;

        FMOBAMMOInventoryItem Parsed;
        if (ParseItemFromJson(*ItemObj, Parsed))
            Items.Add(Parsed);
    }

    // Equip bonuslarını PlayerState'e uygula
    ApplyEquippedBonusesToPlayerState();

    UE_LOG(LogTemp, Log, TEXT("[Inventory] Loaded %d items from backend."), Items.Num());
    GetOwner()->ForceNetUpdate();
}

// ─────────────────────────────────────────────────────────
// SERVER OPERATIONS
// ─────────────────────────────────────────────────────────

bool UMOBAMMOInventoryComponent::Server_AddItem(const FMOBAMMOInventoryItem& NewItem)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!NewItem.IsValid()) return false;

    // Stackable kontrol: aynı definition varsa quantity artır
    if (NewItem.Quantity > 0)
    {
        FMOBAMMOInventoryItem* Existing = Items.FindByPredicate(
            [&NewItem](const FMOBAMMOInventoryItem& I) {
                return I.ItemDefinitionId == NewItem.ItemDefinitionId
                    && I.EquipSlot == EMOBAMMOEquipSlot::None;
            });
        if (Existing)
        {
            Existing->Quantity += NewItem.Quantity;
            GetOwner()->ForceNetUpdate();
            OnInventoryChanged.Broadcast();
            return true;
        }
    }

    Items.Add(NewItem);
    GetOwner()->ForceNetUpdate();
    OnInventoryChanged.Broadcast();
    return true;
}

bool UMOBAMMOInventoryComponent::Server_UseItem(const FString& ItemId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    int32 Idx = Items.IndexOfByPredicate(
        [&ItemId](const FMOBAMMOInventoryItem& I) { return I.ItemId == ItemId; });
    if (Idx == INDEX_NONE) return false;

    FMOBAMMOInventoryItem& Item = Items[Idx];
    if (!Item.IsConsumable()) return false;

    // Stat'ları PlayerState üzerinden uygula
    AMOBAMMOPlayerState* PS = GetOwner()->GetPlayerState<AMOBAMMOPlayerState>();
    if (PS)
    {
        if (Item.OnUseHpRestore > 0)   PS->ApplyHeal(Item.OnUseHpRestore);
        if (Item.OnUseManaRestore > 0) PS->RestoreMana(Item.OnUseManaRestore);
    }

    OnItemUsed.Broadcast(Item);

    // Quantity azalt / sil
    Item.Quantity--;
    if (Item.Quantity <= 0)
        Items.RemoveAt(Idx);

    GetOwner()->ForceNetUpdate();
    OnInventoryChanged.Broadcast();
    return true;
}

bool UMOBAMMOInventoryComponent::Server_EquipItem(const FString& ItemId,
                                                    EMOBAMMOEquipSlot Slot)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    // Aynı slotta olanı önce çıkar
    for (FMOBAMMOInventoryItem& Existing : Items)
    {
        if (Existing.EquipSlot == Slot && Existing.ItemId != ItemId)
            Existing.EquipSlot = EMOBAMMOEquipSlot::None;
    }

    FMOBAMMOInventoryItem* Target = Items.FindByPredicate(
        [&ItemId](const FMOBAMMOInventoryItem& I) { return I.ItemId == ItemId; });
    if (!Target) return false;

    Target->EquipSlot = Slot;
    ApplyEquippedBonusesToPlayerState();
    GetOwner()->ForceNetUpdate();
    OnInventoryChanged.Broadcast();
    return true;
}

bool UMOBAMMOInventoryComponent::Server_UnequipItem(const FString& ItemId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    FMOBAMMOInventoryItem* Target = Items.FindByPredicate(
        [&ItemId](const FMOBAMMOInventoryItem& I) { return I.ItemId == ItemId; });
    if (!Target) return false;

    Target->EquipSlot = EMOBAMMOEquipSlot::None;
    ApplyEquippedBonusesToPlayerState();
    GetOwner()->ForceNetUpdate();
    OnInventoryChanged.Broadcast();
    return true;
}

bool UMOBAMMOInventoryComponent::Server_RemoveItem(const FString& ItemId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    int32 Idx = Items.IndexOfByPredicate(
        [&ItemId](const FMOBAMMOInventoryItem& I) { return I.ItemId == ItemId; });
    if (Idx == INDEX_NONE) return false;

    if (Quantity >= Items[Idx].Quantity)
        Items.RemoveAt(Idx);
    else
        Items[Idx].Quantity -= Quantity;

    GetOwner()->ForceNetUpdate();
    OnInventoryChanged.Broadcast();
    return true;
}

// ─────────────────────────────────────────────────────────
// SAVE PAYLOAD
// ─────────────────────────────────────────────────────────

FString UMOBAMMOInventoryComponent::BuildSavePayload() const
{
    // Şu an sadece item ID'lerini ve quantity'leri göndermek yeterli.
    // Backend zaten kendi DB'sindeki tam kaydı güncelliyor.
    TArray<TSharedPtr<FJsonValue>> ItemArray;

    for (const FMOBAMMOInventoryItem& Item : Items)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("itemId"),      Item.ItemId);
        Obj->SetNumberField(TEXT("quantity"),     Item.Quantity);
        Obj->SetNumberField(TEXT("equippedSlot"), SlotEnumToIndex(Item.EquipSlot));
        ItemArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("inventory"), ItemArray);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Output;
}

// ─────────────────────────────────────────────────────────
// PRIVATE
// ─────────────────────────────────────────────────────────

void UMOBAMMOInventoryComponent::ApplyEquippedBonusesToPlayerState()
{
    AMOBAMMOPlayerState* PS = GetOwner()->GetPlayerState<AMOBAMMOPlayerState>();
    if (!PS) return;

    int32 MaxHpBonus = 0, MaxManaBonus = 0;
    GetEquippedStatBonuses(MaxHpBonus, MaxManaBonus);

    // PlayerState'te SetEquipmentBonuses() olduğunu varsayıyoruz.
    // Bu fonksiyonu MOBAMMOPlayerState'e eklememiz gerekiyor.
    PS->SetEquipmentBonuses(MaxHpBonus, MaxManaBonus);
}
