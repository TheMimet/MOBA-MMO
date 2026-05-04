#include "MOBAMMOPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "MOBAMMOQuestCatalog.h"

namespace
{
    int32 GetInventoryMaxStackForItem(const FString& ItemId)
    {
        if (ItemId == TEXT("health_potion_small") || ItemId == TEXT("mana_potion_small"))
        {
            return 20;
        }
        if (ItemId == TEXT("sparring_token"))
        {
            return 99;
        }
        if (ItemId == TEXT("iron_sword")
            || ItemId == TEXT("crystal_staff")
            || ItemId == TEXT("leather_helm")
            || ItemId == TEXT("chain_body")
            || ItemId == TEXT("swift_boots")
            || ItemId == TEXT("mana_ring"))
        {
            return 1;
        }

        return 0;
    }

    bool IsKnownEquipmentSlotForItem(const FString& ItemId, int32 SlotIndex)
    {
        if (ItemId == TEXT("iron_sword") || ItemId == TEXT("crystal_staff"))
        {
            return SlotIndex == 0;
        }
        if (ItemId == TEXT("leather_helm"))
        {
            return SlotIndex == 2;
        }
        if (ItemId == TEXT("chain_body"))
        {
            return SlotIndex == 3;
        }
        if (ItemId == TEXT("swift_boots"))
        {
            return SlotIndex == 4;
        }
        if (ItemId == TEXT("mana_ring"))
        {
            return SlotIndex == 5;
        }

        return false;
    }

    void GetEquipmentBonusesForItem(const FString& ItemId, float& OutMaxHealthBonus, float& OutMaxManaBonus)
    {
        OutMaxHealthBonus = 0.0f;
        OutMaxManaBonus = 0.0f;

        if (ItemId == TEXT("iron_sword"))
        {
            OutMaxHealthBonus = 10.0f;
        }
        else if (ItemId == TEXT("crystal_staff"))
        {
            OutMaxManaBonus = 20.0f;
        }
        else if (ItemId == TEXT("leather_helm"))
        {
            OutMaxHealthBonus = 15.0f;
        }
        else if (ItemId == TEXT("chain_body"))
        {
            OutMaxHealthBonus = 30.0f;
        }
        else if (ItemId == TEXT("swift_boots"))
        {
            OutMaxManaBonus = 10.0f;
        }
        else if (ItemId == TEXT("mana_ring"))
        {
            OutMaxHealthBonus = 10.0f;
            OutMaxManaBonus = 25.0f;
        }
    }

    void NormalizeRuntimeInventoryItems(const TArray<FMOBAMMOInventoryItem>& InItems, TArray<FMOBAMMOInventoryItem>& OutItems)
    {
        OutItems.Reset();
        TSet<int32> OccupiedEquipSlots;

        auto AddNormalizedStack = [&OutItems](const FString& ItemId, int32 Quantity, int32 SlotIndex)
        {
            OutItems.Add(FMOBAMMOInventoryItem(ItemId, Quantity, SlotIndex));
        };

        for (const FMOBAMMOInventoryItem& Item : InItems)
        {
            const int32 MaxStack = GetInventoryMaxStackForItem(Item.ItemId);
            if (MaxStack <= 0 || Item.Quantity <= 0)
            {
                continue;
            }

            int32 SlotIndex = -1;
            if (IsKnownEquipmentSlotForItem(Item.ItemId, Item.SlotIndex) && !OccupiedEquipSlots.Contains(Item.SlotIndex))
            {
                SlotIndex = Item.SlotIndex;
                OccupiedEquipSlots.Add(SlotIndex);
            }

            int32 RemainingQuantity = Item.Quantity;
            if (MaxStack <= 1)
            {
                while (RemainingQuantity > 0)
                {
                    AddNormalizedStack(Item.ItemId, 1, SlotIndex);
                    SlotIndex = -1;
                    --RemainingQuantity;
                }
                continue;
            }

            while (RemainingQuantity > 0)
            {
                const int32 StackQuantity = FMath::Min(MaxStack, RemainingQuantity);
                AddNormalizedStack(Item.ItemId, StackQuantity, -1);
                RemainingQuantity -= StackQuantity;
            }
        }
    }
}

AMOBAMMOPlayerState::AMOBAMMOPlayerState()
{
    bReplicates = true;

    // Replicate at 10 Hz — PlayerState holds persistent session data that
    // doesn't need frame-rate-level updates (default UE is 100 Hz).
    SetNetUpdateFrequency(10.0f);

    // All 4 abilities start at rank 1.
    AbilityRanks.Init(1, 4);
}

void AMOBAMMOPlayerState::ApplySessionIdentity(const FString& InAccountId, const FString& InCharacterId, const FString& InSessionId, const FString& InCharacterName, const FString& InClassId, int32 InLevel)
{
    if (!HasAuthority())
    {
        return;
    }

    AccountId = InAccountId;
    CharacterId = InCharacterId;
    SessionId = InSessionId;
    CharacterName = InCharacterName;
    ClassId = InClassId;
    CharacterLevel = FMath::Max(1, InLevel);
    PersistenceStatus = TEXT("Ready");
    PersistenceErrorMessage.Reset();
    SetPlayerName(CharacterName.IsEmpty() ? TEXT("Player") : CharacterName);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetPersistenceStatus(const FString& InStatus, const FString& InErrorMessage)
{
    if (!HasAuthority())
    {
        return;
    }

    PersistenceStatus = InStatus.IsEmpty() ? TEXT("Ready") : InStatus;
    PersistenceErrorMessage = InErrorMessage;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ApplyPersistentCharacterState(int32 InExperience, const FVector& InSavedWorldPosition, float InCurrentHealth, float InMaxHealth, float InCurrentMana, float InMaxMana, int32 InKillCount, int32 InDeathCount, int32 InGold)
{
    if (!HasAuthority())
    {
        return;
    }

    CharacterExperience = FMath::Max(0, InExperience);
    SavedWorldPosition = InSavedWorldPosition;
    MaxHealth = FMath::Max(1.0f, InMaxHealth);
    EquipmentMaxHealthBonus = 0.0f;
    EquipmentMaxManaBonus = 0.0f;
    CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, GetMaxHealth());
    MaxMana = FMath::Max(0.0f, InMaxMana);
    CurrentMana = FMath::Clamp(InCurrentMana, 0.0f, GetMaxMana());
    KillCount = FMath::Max(0, InKillCount);
    DeathCount = FMath::Max(0, InDeathCount);
    Gold = FMath::Max(0, InGold);
    bHasPersistentCharacterSnapshot = true;
    bPersistentSpawnLocationConsumed = false;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ApplyPersistentInventory(const TArray<FMOBAMMOInventoryItem>& InInventoryItems)
{
    if (!HasAuthority())
    {
        return;
    }

    NormalizeRuntimeInventoryItems(InInventoryItems, InventoryArray.Items);
    InventoryArray.MarkArrayDirty();
    RecalculateEquipmentBonusesFromInventory();
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ApplyAppearanceSelection(int32 InPresetId, int32 InColorIndex, int32 InShade, int32 InTransparent, int32 InTextureDetail)
{
    if (!HasAuthority())
    {
        return;
    }

    PresetId = InPresetId;
    ColorIndex = InColorIndex;
    Shade = InShade;
    Transparent = InTransparent;
    TextureDetail = InTextureDetail;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::InitializeAttributes(float InMaxHealth, float InMaxMana)
{
    if (!HasAuthority())
    {
        return;
    }

    MaxHealth = FMath::Max(1.0f, InMaxHealth);
    EquipmentMaxHealthBonus = 0.0f;
    EquipmentMaxManaBonus = 0.0f;
    CurrentHealth = MaxHealth;
    MaxMana = FMath::Max(0.0f, InMaxMana);
    CurrentMana = MaxMana;
    DamageCooldownEndServerTime = 0.0f;
    HealCooldownEndServerTime = 0.0f;
    DrainCooldownEndServerTime = 0.0f;
    ManaSurgeCooldownEndServerTime = 0.0f;
    ArcChargeEndServerTime = 0.0f;
    RespawnAvailableServerTime = 0.0f;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetEquipmentBonuses(float MaxHealthBonus, float MaxManaBonus)
{
    if (!HasAuthority())
    {
        return;
    }

    const float PreviousMaxHealth = GetMaxHealth();
    const float PreviousMaxMana = GetMaxMana();

    EquipmentMaxHealthBonus = FMath::Max(0.0f, MaxHealthBonus);
    EquipmentMaxManaBonus = FMath::Max(0.0f, MaxManaBonus);
    CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, GetMaxHealth());
    CurrentMana = FMath::Clamp(CurrentMana, 0.0f, GetMaxMana());

    if (!FMath::IsNearlyEqual(PreviousMaxHealth, GetMaxHealth()) || !FMath::IsNearlyEqual(PreviousMaxMana, GetMaxMana()))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }
}

void AMOBAMMOPlayerState::SetSelectedTargetIdentity(const FString& InCharacterId, const FString& InCharacterName)
{
    if (!HasAuthority())
    {
        return;
    }

    SelectedTargetCharacterId = InCharacterId;
    SelectedTargetName = InCharacterName;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ClearSelectedTarget()
{
    if (!HasAuthority())
    {
        return;
    }

    SelectedTargetCharacterId.Reset();
    SelectedTargetName.Reset();
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetCurrentHealth(float NewHealth)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentHealth = FMath::Clamp(NewHealth, 0.0f, GetMaxHealth());
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetCurrentMana(float NewMana)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentMana = FMath::Clamp(NewMana, 0.0f, GetMaxMana());
    ForceNetUpdate();
    BroadcastStateUpdated();
}

float AMOBAMMOPlayerState::ApplyDamage(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.0f, GetMaxHealth());

    if (!FMath::IsNearlyEqual(CurrentHealth, PreviousHealth))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return PreviousHealth - CurrentHealth;
}

float AMOBAMMOPlayerState::ApplyHealing(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, GetMaxHealth());

    if (!FMath::IsNearlyEqual(CurrentHealth, PreviousHealth))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return CurrentHealth - PreviousHealth;
}

bool AMOBAMMOPlayerState::ConsumeMana(float Amount)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (Amount <= 0.0f)
    {
        return true;
    }

    if (CurrentMana < Amount)
    {
        return false;
    }

    CurrentMana = FMath::Clamp(CurrentMana - Amount, 0.0f, GetMaxMana());
    ForceNetUpdate();
    BroadcastStateUpdated();
    return true;
}

float AMOBAMMOPlayerState::RestoreMana(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousMana = CurrentMana;
    CurrentMana = FMath::Clamp(CurrentMana + Amount, 0.0f, GetMaxMana());

    if (!FMath::IsNearlyEqual(CurrentMana, PreviousMana))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return CurrentMana - PreviousMana;
}

void AMOBAMMOPlayerState::RecordKill()
{
    if (!HasAuthority())
    {
        return;
    }

    ++KillCount;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::RecordDeath()
{
    if (!HasAuthority())
    {
        return;
    }

    ++DeathCount;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartRespawnCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    RespawnAvailableServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartDamageCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    DamageCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartHealCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    HealCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartDrainCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    DrainCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartManaSurgeCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    ManaSurgeCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartArcCharge(float DurationSeconds, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    ArcChargeEndServerTime = DurationSeconds > 0.0f
        ? FMath::Max(0.0f, ServerWorldTimeSeconds + DurationSeconds)
        : 0.0f;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

float AMOBAMMOPlayerState::GetDamageCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, DamageCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetHealCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, HealCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetDrainCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, DrainCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetManaSurgeCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, ManaSurgeCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetArcChargeRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, ArcChargeEndServerTime - ServerWorldTimeSeconds);
}

bool AMOBAMMOPlayerState::IsDamageSpellReady(float ServerWorldTimeSeconds) const
{
    return GetDamageCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsHealSpellReady(float ServerWorldTimeSeconds) const
{
    return GetHealCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsDrainSpellReady(float ServerWorldTimeSeconds) const
{
    return GetDrainCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsManaSurgeReady(float ServerWorldTimeSeconds) const
{
    return GetManaSurgeCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::HasArcCharge(float ServerWorldTimeSeconds) const
{
    return GetArcChargeRemaining(ServerWorldTimeSeconds) > KINDA_SMALL_NUMBER;
}

float AMOBAMMOPlayerState::GetRespawnCooldownRemaining(float ServerWorldTimeSeconds) const
{
    if (IsAlive())
    {
        return 0.0f;
    }

    return FMath::Max(0.0f, RespawnAvailableServerTime - ServerWorldTimeSeconds);
}

bool AMOBAMMOPlayerState::CanRespawn(float ServerWorldTimeSeconds) const
{
    return !IsAlive() && GetRespawnCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

void AMOBAMMOPlayerState::PushIncomingCombatFeedback(const FString& InFeedbackText, float DurationSeconds, bool bIsHealingFeedback)
{
    if (!HasAuthority())
    {
        return;
    }

    IncomingCombatFeedbackText = InFeedbackText;
    bIncomingCombatFeedbackHealing = bIsHealingFeedback;
    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    IncomingCombatFeedbackEndServerTime = ServerWorldTimeSeconds + FMath::Max(0.1f, DurationSeconds);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

const TArray<FMOBAMMOInventoryItem>& AMOBAMMOPlayerState::GetInventoryItems() const
{
    return InventoryArray.Items;
}

void AMOBAMMOPlayerState::GrantItem(const FString& ItemId, int32 Quantity, int32 SlotIndex)
{
    if (!HasAuthority() || Quantity <= 0 || ItemId.IsEmpty())
    {
        return;
    }

    const int32 MaxStack = GetInventoryMaxStackForItem(ItemId);
    if (MaxStack <= 0)
    {
        return;
    }

    int32 RemainingQuantity = Quantity;
    bool bChangedInventory = false;

    if (MaxStack > 1)
    {
        for (FMOBAMMOInventoryItem& ExistingItem : InventoryArray.Items)
        {
            if (RemainingQuantity <= 0)
            {
                break;
            }

            if (ExistingItem.ItemId != ItemId || ExistingItem.Quantity >= MaxStack || ExistingItem.SlotIndex >= 0)
            {
                continue;
            }

            const int32 AddQuantity = FMath::Min(MaxStack - ExistingItem.Quantity, RemainingQuantity);
            ExistingItem.Quantity += AddQuantity;
            RemainingQuantity -= AddQuantity;
            InventoryArray.MarkItemDirty(ExistingItem);
            bChangedInventory = true;
        }

        while (RemainingQuantity > 0)
        {
            const int32 StackQuantity = FMath::Min(MaxStack, RemainingQuantity);
            InventoryArray.Items.Add(FMOBAMMOInventoryItem(ItemId, StackQuantity, -1));
            InventoryArray.MarkItemDirty(InventoryArray.Items.Last());
            RemainingQuantity -= StackQuantity;
            bChangedInventory = true;
        }
    }
    else
    {
        for (int32 i = 0; i < Quantity; ++i)
        {
            const int32 NewSlotIndex = (i == 0 && IsKnownEquipmentSlotForItem(ItemId, SlotIndex)) ? SlotIndex : -1;
            InventoryArray.Items.Add(FMOBAMMOInventoryItem(ItemId, 1, NewSlotIndex));
            InventoryArray.MarkItemDirty(InventoryArray.Items.Last());
            bChangedInventory = true;
        }
    }

    if (!bChangedInventory)
    {
        return;
    }

    RecalculateEquipmentBonusesFromInventory();
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::RemoveItem(const FString& ItemId, int32 Quantity)
{
    if (!HasAuthority() || Quantity <= 0 || ItemId.IsEmpty())
    {
        return;
    }

    for (int32 i = 0; i < InventoryArray.Items.Num(); ++i)
    {
        if (InventoryArray.Items[i].ItemId == ItemId)
        {
            InventoryArray.Items[i].Quantity -= Quantity;
            if (InventoryArray.Items[i].Quantity <= 0)
            {
                InventoryArray.Items.RemoveAt(i);
                InventoryArray.MarkArrayDirty();
            }
            else
            {
                InventoryArray.MarkItemDirty(InventoryArray.Items[i]);
            }
            RecalculateEquipmentBonusesFromInventory();
            ForceNetUpdate();
            BroadcastStateUpdated();
            return;
        }
    }
}

bool AMOBAMMOPlayerState::ConsumeInventoryItem(const FString& ItemId, int32 Quantity)
{
    if (!HasAuthority() || Quantity <= 0 || ItemId.IsEmpty())
    {
        return false;
    }

    for (int32 i = 0; i < InventoryArray.Items.Num(); ++i)
    {
        FMOBAMMOInventoryItem& Item = InventoryArray.Items[i];
        if (Item.ItemId != ItemId || Item.Quantity < Quantity)
        {
            continue;
        }

        Item.Quantity -= Quantity;
        if (Item.Quantity <= 0)
        {
            InventoryArray.Items.RemoveAt(i);
            InventoryArray.MarkArrayDirty();
        }
        else
        {
            InventoryArray.MarkItemDirty(Item);
        }

        RecalculateEquipmentBonusesFromInventory();
        ForceNetUpdate();
        BroadcastStateUpdated();
        return true;
    }

    return false;
}

bool AMOBAMMOPlayerState::EquipInventoryItem(const FString& ItemId, int32 EquipSlot)
{
    if (!HasAuthority() || ItemId.IsEmpty() || !IsKnownEquipmentSlotForItem(ItemId, EquipSlot))
    {
        return false;
    }

    FMOBAMMOInventoryItem* ItemToEquip = nullptr;
    for (FMOBAMMOInventoryItem& Item : InventoryArray.Items)
    {
        if (Item.ItemId == ItemId)
        {
            ItemToEquip = &Item;
        }
        else if (Item.SlotIndex == EquipSlot)
        {
            Item.SlotIndex = -1;
            InventoryArray.MarkItemDirty(Item);
        }
    }

    if (!ItemToEquip)
    {
        return false;
    }

    ItemToEquip->SlotIndex = EquipSlot;
    InventoryArray.MarkItemDirty(*ItemToEquip);
    RecalculateEquipmentBonusesFromInventory();
    ForceNetUpdate();
    BroadcastStateUpdated();
    return true;
}

bool AMOBAMMOPlayerState::UnequipInventoryItem(const FString& ItemId)
{
    if (!HasAuthority() || ItemId.IsEmpty())
    {
        return false;
    }

    for (FMOBAMMOInventoryItem& Item : InventoryArray.Items)
    {
        if (Item.ItemId == ItemId && Item.SlotIndex >= 0)
        {
            Item.SlotIndex = -1;
            InventoryArray.MarkItemDirty(Item);
            RecalculateEquipmentBonusesFromInventory();
            ForceNetUpdate();
            BroadcastStateUpdated();
            return true;
        }
    }

    return false;
}

void AMOBAMMOPlayerState::RecalculateEquipmentBonusesFromInventory()
{
    float TotalMaxHealthBonus = 0.0f;
    float TotalMaxManaBonus = 0.0f;

    for (const FMOBAMMOInventoryItem& Item : InventoryArray.Items)
    {
        if (!IsKnownEquipmentSlotForItem(Item.ItemId, Item.SlotIndex))
        {
            continue;
        }

        float ItemMaxHealthBonus = 0.0f;
        float ItemMaxManaBonus = 0.0f;
        GetEquipmentBonusesForItem(Item.ItemId, ItemMaxHealthBonus, ItemMaxManaBonus);
        TotalMaxHealthBonus += ItemMaxHealthBonus;
        TotalMaxManaBonus += ItemMaxManaBonus;
    }

    EquipmentMaxHealthBonus = TotalMaxHealthBonus;
    EquipmentMaxManaBonus = TotalMaxManaBonus;
    CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, GetMaxHealth());
    CurrentMana = FMath::Clamp(CurrentMana, 0.0f, GetMaxMana());
}

// ─────────────────────────────────────────────────────────────
// Progression / Level-Up
// ─────────────────────────────────────────────────────────────
int32 AMOBAMMOPlayerState::GetXPRequiredForLevel(int32 Level) const
{
    // Linear curve: XP needed to advance from Level to Level+1 == 100 * Level.
    // Level 1 → 100, Level 5 → 500, Level 19 → 1900.  Total XP at L20 = 19 000.
    return FMath::Max(1, Level) * 100;
}

int32 AMOBAMMOPlayerState::GetTotalXPForLevel(int32 Level) const
{
    // Cumulative sum 100*1 + 100*2 + ... + 100*(Level-1) = 50 * Level * (Level-1).
    const int32 ClampedLevel = FMath::Clamp(Level, 1, MaxCharacterLevel);
    return 50 * ClampedLevel * (ClampedLevel - 1);
}

int32 AMOBAMMOPlayerState::GetExperienceToNextLevel() const
{
    if (CharacterLevel >= MaxCharacterLevel) return 0;
    return FMath::Max(0, GetTotalXPForLevel(CharacterLevel + 1) - CharacterExperience);
}

float AMOBAMMOPlayerState::GetExperienceProgressFraction() const
{
    if (CharacterLevel >= MaxCharacterLevel) return 1.0f;
    const int32 LevelStartXP = GetTotalXPForLevel(CharacterLevel);
    const int32 LevelEndXP   = GetTotalXPForLevel(CharacterLevel + 1);
    const int32 Range        = LevelEndXP - LevelStartXP;
    if (Range <= 0) return 1.0f;
    return FMath::Clamp(float(CharacterExperience - LevelStartXP) / float(Range), 0.0f, 1.0f);
}

int32 AMOBAMMOPlayerState::GrantExperience(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return 0;
    }

    if (CharacterLevel >= MaxCharacterLevel)
    {
        // Already capped — clamp XP to the total at max level so the bar stays full.
        CharacterExperience = GetTotalXPForLevel(MaxCharacterLevel);
        ForceNetUpdate();
        BroadcastStateUpdated();
        return 0;
    }

    CharacterExperience += Amount;

    int32 LevelsGained = 0;
    while (CharacterLevel < MaxCharacterLevel
        && CharacterExperience >= GetTotalXPForLevel(CharacterLevel + 1))
    {
        CharacterLevel++;
        LevelsGained++;

        // Scale base attributes per level: +15 HP, +8 MP each level after L1.
        const float NewBaseMaxHealth = 100.0f + (CharacterLevel - 1) * 15.0f;
        const float NewBaseMaxMana   =  50.0f + (CharacterLevel - 1) *  8.0f;
        const float HealthGained = FMath::Max(0.0f, NewBaseMaxHealth - MaxHealth);
        const float ManaGained   = FMath::Max(0.0f, NewBaseMaxMana   - MaxMana);

        MaxHealth = NewBaseMaxHealth;
        MaxMana   = NewBaseMaxMana;

        // Reward the level-up: grant the bonus HP/MP gained from the increase
        // (so a player at low HP doesn't have a half-empty bar after leveling).
        CurrentHealth = FMath::Min(CurrentHealth + HealthGained, GetMaxHealth());
        CurrentMana   = FMath::Min(CurrentMana   + ManaGained,   GetMaxMana());

        // Grant 1 skill point per level-up.
        SkillPoints++;
    }

    if (CharacterLevel >= MaxCharacterLevel)
    {
        // Cap XP so the bar reads "MAX" instead of overshooting.
        CharacterExperience = GetTotalXPForLevel(MaxCharacterLevel);
    }

    ForceNetUpdate();
    BroadcastStateUpdated();
    return LevelsGained;
}

// ─────────────────────────────────────────────────────────────
// Currency / Gold
// ─────────────────────────────────────────────────────────────
int32 AMOBAMMOPlayerState::GrantGold(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return 0;
    }

    // Clamp to a sane upper bound to avoid 32-bit overflow griefing.
    constexpr int32 GoldHardCap = 1'000'000'000;
    const int32 NewGold = FMath::Min(GoldHardCap, Gold + Amount);
    const int32 Granted = NewGold - Gold;
    Gold = NewGold;

    ForceNetUpdate();
    BroadcastStateUpdated();
    return Granted;
}

bool AMOBAMMOPlayerState::SpendGold(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0 || Gold < Amount)
    {
        return false;
    }

    Gold -= Amount;
    ForceNetUpdate();
    BroadcastStateUpdated();
    return true;
}

void AMOBAMMOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // --- Visible to ALL clients (nameplate, health bar, scoreboard, appearance) ---
    DOREPLIFETIME(AMOBAMMOPlayerState, Faction);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterName);
    DOREPLIFETIME(AMOBAMMOPlayerState, ClassId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterLevel);
    DOREPLIFETIME(AMOBAMMOPlayerState, PresetId);
    DOREPLIFETIME(AMOBAMMOPlayerState, ColorIndex);
    DOREPLIFETIME(AMOBAMMOPlayerState, Shade);
    DOREPLIFETIME(AMOBAMMOPlayerState, Transparent);
    DOREPLIFETIME(AMOBAMMOPlayerState, TextureDetail);
    DOREPLIFETIME(AMOBAMMOPlayerState, CurrentHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, MaxHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, KillCount);
    DOREPLIFETIME(AMOBAMMOPlayerState, DeathCount);

    // --- Owner-only: private session/persistence data ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, AccountId,                       COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, SessionId,                       COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, PersistenceStatus,               COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, PersistenceErrorMessage,         COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, SavedWorldPosition,              COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, bHasPersistentCharacterSnapshot, COND_OwnerOnly);

    // --- Owner-only: stats only the local player needs ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, CharacterExperience,             COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, CurrentMana,                     COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, MaxMana,                         COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, EquipmentMaxHealthBonus,         COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, EquipmentMaxManaBonus,           COND_OwnerOnly);

    // --- Owner-only: ability cooldown timers (only owner's HUD displays these) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, DamageCooldownEndServerTime,     COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, HealCooldownEndServerTime,       COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, DrainCooldownEndServerTime,      COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, ManaSurgeCooldownEndServerTime,  COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, ArcChargeEndServerTime,          COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, RespawnAvailableServerTime,      COND_OwnerOnly);

    // --- Owner-only: targeting & combat feedback (only owner's HUD uses these) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, SelectedTargetCharacterId,           COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, SelectedTargetName,                  COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, IncomingCombatFeedbackText,          COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, IncomingCombatFeedbackEndServerTime, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, bIncomingCombatFeedbackHealing,      COND_OwnerOnly);

    // --- Owner-only: inventory (never send other players' bags to everyone) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, InventoryArray,                  COND_OwnerOnly);

    // --- Owner-only: gold (private currency, never replicated to others) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, Gold,                            COND_OwnerOnly);

    // --- Owner-only: quest progress (only the owning player tracks their quests) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, QuestProgress,                   COND_OwnerOnly);

    // --- Owner-only: skill points and ability ranks ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, SkillPoints,                     COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, AbilityRanks,                    COND_OwnerOnly);

    // --- Owner-only: active status effects (buff/debuff display) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, ActiveStatusEffects,             COND_OwnerOnly);

    // --- Owner-only: current zone (HUD zone label) ---
    DOREPLIFETIME_CONDITION(AMOBAMMOPlayerState, CurrentZoneId,                   COND_OwnerOnly);
}

// ─────────────────────────────────────────────────────────────
// Zone
// ─────────────────────────────────────────────────────────────

void AMOBAMMOPlayerState::SetCurrentZoneId(FName NewZoneId)
{
    if (CurrentZoneId == NewZoneId)
    {
        return;
    }
    CurrentZoneId = NewZoneId;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_ZoneId()
{
    BroadcastStateUpdated();
}

// ─────────────────────────────────────────────────────────────
// Faction
// ─────────────────────────────────────────────────────────────

void AMOBAMMOPlayerState::SetFaction(EMOBAMMOFaction NewFaction)
{
    if (Faction == NewFaction)
    {
        return;
    }
    Faction = NewFaction;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_Faction()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_PlayerIdentity()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_PersistenceStatus()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_Attributes()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_TargetSelection()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_CombatFeedback()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_InventoryArray()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_QuestProgress()
{
    BroadcastStateUpdated();
}

// ─────────────────────────────────────────────────────────────
// Quest / Objective System
// ─────────────────────────────────────────────────────────────

void AMOBAMMOPlayerState::AssignStartingQuests()
{
    if (!HasAuthority())
    {
        return;
    }

    QuestProgress.Reset();
    for (const FString& QuestId : UMOBAMMOQuestCatalog::GetDefaultSessionQuestIds())
    {
        FMOBAMMOQuestProgress Entry;
        Entry.QuestId      = QuestId;
        Entry.CurrentCount = 0;
        Entry.bCompleted   = false;
        QuestProgress.Add(MoveTemp(Entry));
    }

    ForceNetUpdate();
    BroadcastStateUpdated();
}

TArray<FString> AMOBAMMOPlayerState::AdvanceQuestProgress(EMOBAMMOQuestType Type, int32 Amount)
{
    TArray<FString> NewlyCompleted;
    if (!HasAuthority() || Amount <= 0)
    {
        return NewlyCompleted;
    }

    bool bAnyChanged = false;

    for (FMOBAMMOQuestProgress& Entry : QuestProgress)
    {
        if (Entry.bCompleted)
        {
            continue;
        }

        // Look up the definition to check type and target.
        FMOBAMMOQuestDefinition Def;
        if (!UMOBAMMOQuestCatalog::FindQuest(Entry.QuestId, Def))
        {
            continue;
        }

        if (Def.Type != Type)
        {
            continue;
        }

        Entry.CurrentCount = FMath::Min(Entry.CurrentCount + Amount, Def.TargetCount);
        bAnyChanged = true;

        if (Entry.CurrentCount >= Def.TargetCount)
        {
            Entry.bCompleted = true;
            NewlyCompleted.Add(Entry.QuestId);
        }
    }

    if (bAnyChanged)
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return NewlyCompleted;
}

void AMOBAMMOPlayerState::OnRep_SkillProgress()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_StatusEffects()
{
    BroadcastStateUpdated();
}

// ─────────────────────────────────────────────────────────────
// Status Effects / Buff-Debuff System
// ─────────────────────────────────────────────────────────────

void AMOBAMMOPlayerState::ApplyStatusEffect(const FMOBAMMOStatusEffect& Effect)
{
    if (!HasAuthority())
    {
        return;
    }

    if (Effect.Type == EMOBAMMOStatusEffectType::Shield)
    {
        // Shield stacks: add magnitude to an existing shield rather than replace.
        for (FMOBAMMOStatusEffect& Existing : ActiveStatusEffects)
        {
            if (Existing.Type == EMOBAMMOStatusEffectType::Shield)
            {
                Existing.Magnitude        += Effect.Magnitude;
                Existing.RemainingDuration = FMath::Max(Existing.RemainingDuration, Effect.RemainingDuration);
                ForceNetUpdate();
                BroadcastStateUpdated();
                return;
            }
        }
    }
    else
    {
        // All other effect types replace an existing instance of the same type.
        for (FMOBAMMOStatusEffect& Existing : ActiveStatusEffects)
        {
            if (Existing.Type == Effect.Type)
            {
                Existing = Effect;
                ForceNetUpdate();
                BroadcastStateUpdated();
                return;
            }
        }
    }

    // No existing effect of this type — add a new entry.
    ActiveStatusEffects.Add(Effect);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::RemoveStatusEffect(EMOBAMMOStatusEffectType Type)
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 Removed = ActiveStatusEffects.RemoveAll(
        [Type](const FMOBAMMOStatusEffect& E) { return E.Type == Type; });

    if (Removed > 0)
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }
}

bool AMOBAMMOPlayerState::HasStatusEffect(EMOBAMMOStatusEffectType Type) const
{
    for (const FMOBAMMOStatusEffect& E : ActiveStatusEffects)
    {
        if (E.Type == Type)
        {
            return true;
        }
    }
    return false;
}

float AMOBAMMOPlayerState::AbsorbShieldDamage(float InDamage)
{
    if (!HasAuthority() || InDamage <= 0.0f)
    {
        return InDamage;
    }

    for (int32 i = ActiveStatusEffects.Num() - 1; i >= 0; --i)
    {
        FMOBAMMOStatusEffect& E = ActiveStatusEffects[i];
        if (E.Type != EMOBAMMOStatusEffectType::Shield)
        {
            continue;
        }

        if (E.Magnitude >= InDamage)
        {
            // Shield fully absorbs the hit.
            E.Magnitude -= InDamage;
            if (E.Magnitude <= 0.0f)
            {
                ActiveStatusEffects.RemoveAt(i);
            }
            ForceNetUpdate();
            BroadcastStateUpdated();
            return 0.0f;
        }
        else
        {
            // Shield partially absorbs; remainder passes through.
            const float Remainder = InDamage - E.Magnitude;
            ActiveStatusEffects.RemoveAt(i);
            ForceNetUpdate();
            BroadcastStateUpdated();
            return Remainder;
        }
    }

    return InDamage; // No shield active — full damage passes through.
}

void AMOBAMMOPlayerState::TickAndApplyStatusEffects(float DeltaTime,
                                                     float& OutHealApplied,
                                                     float& OutDamageApplied,
                                                     TArray<EMOBAMMOStatusEffectType>& OutExpiredTypes)
{
    OutHealApplied   = 0.0f;
    OutDamageApplied = 0.0f;
    OutExpiredTypes.Reset();

    if (!HasAuthority() || DeltaTime <= 0.0f)
    {
        return;
    }

    bool bDirty = false;

    for (int32 i = ActiveStatusEffects.Num() - 1; i >= 0; --i)
    {
        FMOBAMMOStatusEffect& E = ActiveStatusEffects[i];

        // Advance the effect's remaining duration.
        E.RemainingDuration -= DeltaTime;

        // Per-tick effects: Regeneration (HoT) and Poison (DoT).
        if (E.Type == EMOBAMMOStatusEffectType::Regeneration || E.Type == EMOBAMMOStatusEffectType::Poison)
        {
            E.TimeSinceLastTick += DeltaTime;
            if (E.TimeSinceLastTick >= E.TickInterval)
            {
                E.TimeSinceLastTick -= E.TickInterval;
                if (E.Type == EMOBAMMOStatusEffectType::Regeneration)
                {
                    OutHealApplied += ApplyHealing(E.Magnitude);
                }
                else // Poison bypasses shield (already inside the body)
                {
                    OutDamageApplied += ApplyDamage(E.Magnitude);
                }
                bDirty = true;
            }
        }

        // Remove expired effects.
        if (E.RemainingDuration <= 0.0f)
        {
            OutExpiredTypes.Add(E.Type);
            ActiveStatusEffects.RemoveAt(i);
            bDirty = true;
        }
    }

    if (bDirty)
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }
}

// ─────────────────────────────────────────────────────────────
// Skill Points / Ability Ranks
// ─────────────────────────────────────────────────────────────

int32 AMOBAMMOPlayerState::GetAbilityRank(int32 SlotIndex) const
{
    if (!AbilityRanks.IsValidIndex(SlotIndex))
    {
        return 1;
    }
    return AbilityRanks[SlotIndex];
}

bool AMOBAMMOPlayerState::CanUpgradeAbility(int32 SlotIndex) const
{
    return SkillPoints > 0
        && AbilityRanks.IsValidIndex(SlotIndex)
        && AbilityRanks[SlotIndex] < MaxAbilityRank;
}

bool AMOBAMMOPlayerState::SpendSkillPoint(int32 AbilitySlotIndex)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!CanUpgradeAbility(AbilitySlotIndex))
    {
        return false;
    }

    AbilityRanks[AbilitySlotIndex]++;
    SkillPoints--;

    ForceNetUpdate();
    BroadcastStateUpdated();
    return true;
}

void AMOBAMMOPlayerState::BroadcastStateUpdated()
{
    OnReplicatedStateUpdated.Broadcast();
}
