#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MOBAMMOFactionTypes.h"
#include "MOBAMMOInventoryTypes.h"
#include "MOBAMMOQuestTypes.h"
#include "MOBAMMOStatusTypes.h"

#include "MOBAMMOPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBAMMOPlayerStateUpdatedSignature);

UCLASS()
class MOBAMMO_API AMOBAMMOPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AMOBAMMOPlayerState();

    UPROPERTY(BlueprintAssignable, Category="MOBAMMO|Replication")
    FMOBAMMOPlayerStateUpdatedSignature OnReplicatedStateUpdated;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ApplySessionIdentity(const FString& InAccountId, const FString& InCharacterId, const FString& InSessionId, const FString& InCharacterName, const FString& InClassId, int32 InLevel);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ApplyPersistentCharacterState(int32 InExperience, const FVector& InSavedWorldPosition, float InCurrentHealth, float InMaxHealth, float InCurrentMana, float InMaxMana, int32 InKillCount, int32 InDeathCount, int32 InGold = 0);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ApplyPersistentInventory(const TArray<FMOBAMMOInventoryItem>& InInventoryItems);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ApplyAppearanceSelection(int32 InPresetId, int32 InColorIndex, int32 InShade, int32 InTransparent, int32 InTextureDetail);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void InitializeAttributes(float InMaxHealth, float InMaxMana);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetCurrentHealth(float NewHealth);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetCurrentMana(float NewMana);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    float ApplyDamage(float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    float ApplyHealing(float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    bool ConsumeMana(float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    float RestoreMana(float Amount);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsAlive() const { return CurrentHealth > 0.0f; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetAccountId() const { return AccountId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetCharacterId() const { return CharacterId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetSessionId() const { return SessionId; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetPersistenceStatus(const FString& InStatus, const FString& InErrorMessage);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetPersistenceStatus() const { return PersistenceStatus; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetPersistenceErrorMessage() const { return PersistenceErrorMessage; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetCharacterName() const { return CharacterName; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetClassId() const { return ClassId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetCharacterLevel() const { return CharacterLevel; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetCharacterExperience() const { return CharacterExperience; }

    // ── Progression / Level-Up ────────────────────────────────────
    static constexpr int32 MaxCharacterLevel = 20;

    // Adds Amount XP. Returns the number of levels gained (0 if no level-up).
    // Authority-only; recalculates MaxHealth/MaxMana on level-up.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Progression")
    int32 GrantExperience(int32 Amount);

    // XP needed to advance from Level → Level+1 (e.g. Level 1 → 100 XP, Level 5 → 500 XP).
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Progression")
    int32 GetXPRequiredForLevel(int32 Level) const;

    // Cumulative total XP required to *reach* Level (e.g. Level 1 → 0, Level 2 → 100, Level 3 → 300).
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Progression")
    int32 GetTotalXPForLevel(int32 Level) const;

    // XP remaining until next level-up; 0 if at max level.
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Progression")
    int32 GetExperienceToNextLevel() const;

    // 0..1 fill fraction for the XP bar within the current level band.
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Progression")
    float GetExperienceProgressFraction() const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Progression")
    bool IsMaxLevel() const { return CharacterLevel >= MaxCharacterLevel; }

    // ── Currency / Gold ──────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Currency")
    int32 GetGold() const { return Gold; }

    // Server-only. Returns the actual amount granted (clamped to >=0).
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Currency")
    int32 GrantGold(int32 Amount);

    // Server-only. Returns true if the full Amount was deducted; false (no change) if insufficient.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Currency")
    bool SpendGold(int32 Amount);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FVector GetSavedWorldPosition() const { return SavedWorldPosition; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool HasPersistentCharacterSnapshot() const { return bHasPersistentCharacterSnapshot; }

    bool HasConsumedPersistentSpawnLocation() const { return bPersistentSpawnLocationConsumed; }
    void MarkPersistentSpawnLocationConsumed() { bPersistentSpawnLocationConsumed = true; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxHealth() const { return MaxHealth + EquipmentMaxHealthBonus; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetBaseMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentMana() const { return CurrentMana; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxMana() const { return MaxMana + EquipmentMaxManaBonus; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetBaseMaxMana() const { return MaxMana; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetEquipmentBonuses(float MaxHealthBonus, float MaxManaBonus);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetEquipmentMaxHealthBonus() const { return EquipmentMaxHealthBonus; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetEquipmentMaxManaBonus() const { return EquipmentMaxManaBonus; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void RecordKill();

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void RecordDeath();

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetKills() const { return KillCount; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetDeaths() const { return DeathCount; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetSelectedTargetIdentity(const FString& InCharacterId, const FString& InCharacterName);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ClearSelectedTarget();

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetSelectedTargetCharacterId() const { return SelectedTargetCharacterId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetSelectedTargetName() const { return SelectedTargetName; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartDamageCooldown(float CooldownDuration, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartHealCooldown(float CooldownDuration, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetDamageCooldownEndServerTime() const { return DamageCooldownEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetHealCooldownEndServerTime() const { return HealCooldownEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetDrainCooldownEndServerTime() const { return DrainCooldownEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetManaSurgeCooldownEndServerTime() const { return ManaSurgeCooldownEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetDamageCooldownRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetHealCooldownRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetDrainCooldownRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetManaSurgeCooldownRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsDamageSpellReady(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsHealSpellReady(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsDrainSpellReady(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsManaSurgeReady(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartDrainCooldown(float CooldownDuration, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartManaSurgeCooldown(float CooldownDuration, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartArcCharge(float DurationSeconds, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetArcChargeEndServerTime() const { return ArcChargeEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetArcChargeRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool HasArcCharge(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void StartRespawnCooldown(float CooldownDuration, float ServerWorldTimeSeconds);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetRespawnAvailableServerTime() const { return RespawnAvailableServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetRespawnCooldownRemaining(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool CanRespawn(float ServerWorldTimeSeconds) const;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void PushIncomingCombatFeedback(const FString& InFeedbackText, float DurationSeconds, bool bIsHealingFeedback);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetIncomingCombatFeedbackText() const { return IncomingCombatFeedbackText; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetIncomingCombatFeedbackEndServerTime() const { return IncomingCombatFeedbackEndServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool IsIncomingCombatFeedbackHealing() const { return bIncomingCombatFeedbackHealing; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    const TArray<FMOBAMMOInventoryItem>& GetInventoryItems() const;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void GrantItem(const FString& ItemId, int32 Quantity, int32 SlotIndex = -1);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void RemoveItem(const FString& ItemId, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    bool ConsumeInventoryItem(const FString& ItemId, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    bool EquipInventoryItem(const FString& ItemId, int32 EquipSlot);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    bool UnequipInventoryItem(const FString& ItemId);

    // ── Skill Points / Ability Ranks ─────────────────────────────
    static constexpr int32 MaxAbilityRank = 5;

    // Server-only: tries to spend 1 skill point on the given ability slot (0-indexed).
    // Returns true if the spend succeeded.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Skills")
    bool SpendSkillPoint(int32 AbilitySlotIndex);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Skills")
    int32 GetSkillPoints() const { return SkillPoints; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Skills")
    int32 GetAbilityRank(int32 SlotIndex) const;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Skills")
    bool CanUpgradeAbility(int32 SlotIndex) const;

    // ── Status Effects / Buff-Debuff System ──────────────────────
    // Server-only: applies (or refreshes) the given status effect.
    // Shield stacks magnitude with any existing shield; all others replace same type.
    UFUNCTION(BlueprintCallable, Category = "MOBAMMO|Status")
    void ApplyStatusEffect(const FMOBAMMOStatusEffect& Effect);

    // Server-only: removes all effects of the given type immediately.
    UFUNCTION(BlueprintCallable, Category = "MOBAMMO|Status")
    void RemoveStatusEffect(EMOBAMMOStatusEffectType Type);

    // Returns true if at least one effect of this type is active.
    UFUNCTION(BlueprintPure, Category = "MOBAMMO|Status")
    bool HasStatusEffect(EMOBAMMOStatusEffectType Type) const;

    // Returns true if a Silence effect is active (blocks ability use).
    UFUNCTION(BlueprintPure, Category = "MOBAMMO|Status")
    bool IsSilenced() const { return HasStatusEffect(EMOBAMMOStatusEffectType::Silence); }

    // Returns true if a Haste effect is active.
    UFUNCTION(BlueprintPure, Category = "MOBAMMO|Status")
    bool IsHasted() const { return HasStatusEffect(EMOBAMMOStatusEffectType::Haste); }

    // Cooldown multiplier: 0.70 while hasted, otherwise 1.0.
    UFUNCTION(BlueprintPure, Category = "MOBAMMO|Status")
    float GetCooldownMultiplier() const { return IsHasted() ? 0.70f : 1.0f; }

    // Server-only: absorbs InDamage through the shield bubble.
    // Depletes shield magnitude and returns the leftover damage that passes through.
    float AbsorbShieldDamage(float InDamage);

    // Read-only snapshot of all active effects (for HUD display).
    UFUNCTION(BlueprintPure, Category = "MOBAMMO|Status")
    const TArray<FMOBAMMOStatusEffect>& GetActiveStatusEffects() const { return ActiveStatusEffects; }

    // Server-only: advances all effect timers by DeltaTime.
    // Applies HoT/DoT ticks. Removes expired effects.
    // Populates OutHealApplied / OutDamageApplied and OutExpiredTypes for the caller to log.
    void TickAndApplyStatusEffects(float DeltaTime, float& OutHealApplied, float& OutDamageApplied,
                                   TArray<EMOBAMMOStatusEffectType>& OutExpiredTypes);

    // ── Zone tracking ─────────────────────────────────────────────
    // Which named zone the player is currently inside.  NAME_None = no zone /
    // default area.  Replicated so the HUD can show a zone label.
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Zone")
    FName GetCurrentZoneId() const { return CurrentZoneId; }

    // Server-only.  Called by GameMode when a ZoneVolume overlap fires.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Zone")
    void SetCurrentZoneId(FName NewZoneId);

    // ── Faction ───────────────────────────────────────────────────
    // Replicated to all clients so HUD and targeting can colour-code allies vs
    // enemies.  Default Allied — set to Hostile for adversarial game modes.
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Faction")
    EMOBAMMOFaction GetFaction() const { return Faction; }

    // Server-only.  Call from GameMode when a player changes side.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Faction")
    void SetFaction(EMOBAMMOFaction NewFaction);

    // ── Quest / Objective System ──────────────────────────────────
    // Server-only: assigns default session quests to this player.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Quest")
    void AssignStartingQuests();

    // Server-only: advances all active quests matching Type by Amount.
    // Returns QuestIds that were just newly completed (for reward dispatch).
    TArray<FString> AdvanceQuestProgress(EMOBAMMOQuestType Type, int32 Amount = 1);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Quest")
    const TArray<FMOBAMMOQuestProgress>& GetQuestProgress() const { return QuestProgress; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // Owner-only: zone label shown on local HUD (others don't need this).
    UPROPERTY(ReplicatedUsing=OnRep_ZoneId, BlueprintReadOnly, Category="MOBAMMO|Zone")
    FName CurrentZoneId = NAME_None;

    // Visible to all clients (HUD colour-codes allies vs enemies).
    UPROPERTY(ReplicatedUsing=OnRep_Faction, BlueprintReadOnly, Category="MOBAMMO|Faction")
    EMOBAMMOFaction Faction = EMOBAMMOFaction::Allied;

    UPROPERTY(ReplicatedUsing=OnRep_InventoryArray, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FMOBAMMOInventoryItemArray InventoryArray;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString AccountId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString CharacterId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString SessionId;

    UPROPERTY(ReplicatedUsing=OnRep_PersistenceStatus, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString PersistenceStatus = TEXT("Ready");

    UPROPERTY(ReplicatedUsing=OnRep_PersistenceStatus, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString PersistenceErrorMessage;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString CharacterName;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString ClassId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 CharacterLevel = 1;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 CharacterExperience = 0;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FVector SavedWorldPosition = FVector::ZeroVector;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 PresetId = 4;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 ColorIndex = 0;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 Shade = 58;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 Transparent = 18;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 TextureDetail = 88;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    bool bHasPersistentCharacterSnapshot = false;

    bool bPersistentSpawnLocationConsumed = false;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float CurrentHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float CurrentMana = 50.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float MaxMana = 50.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float EquipmentMaxHealthBonus = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float EquipmentMaxManaBonus = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float DamageCooldownEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float HealCooldownEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float DrainCooldownEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float ManaSurgeCooldownEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float ArcChargeEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 KillCount = 0;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 DeathCount = 0;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Currency")
    int32 Gold = 0;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float RespawnAvailableServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TargetSelection, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString SelectedTargetCharacterId;

    UPROPERTY(ReplicatedUsing=OnRep_TargetSelection, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString SelectedTargetName;

    UPROPERTY(ReplicatedUsing=OnRep_CombatFeedback, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString IncomingCombatFeedbackText;

    UPROPERTY(ReplicatedUsing=OnRep_CombatFeedback, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float IncomingCombatFeedbackEndServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_CombatFeedback, BlueprintReadOnly, Category="MOBAMMO|Replication")
    bool bIncomingCombatFeedbackHealing = false;

    // Active status effects — replicated to the owning client only.
    UPROPERTY(ReplicatedUsing = OnRep_StatusEffects, BlueprintReadOnly, Category = "MOBAMMO|Status")
    TArray<FMOBAMMOStatusEffect> ActiveStatusEffects;

    // Quest progress — replicated to the owning client only.
    UPROPERTY(ReplicatedUsing=OnRep_QuestProgress, BlueprintReadOnly, Category="MOBAMMO|Quest")
    TArray<FMOBAMMOQuestProgress> QuestProgress;

    // Skill points available to spend (earned on each level-up).
    UPROPERTY(ReplicatedUsing=OnRep_SkillProgress, BlueprintReadOnly, Category="MOBAMMO|Skills")
    int32 SkillPoints = 0;

    // Per-ability rank (index 0-3 → abilities 1-4). Starts at 1, max 5.
    UPROPERTY(ReplicatedUsing=OnRep_SkillProgress, BlueprintReadOnly, Category="MOBAMMO|Skills")
    TArray<int32> AbilityRanks;

private:
    UFUNCTION()
    void OnRep_ZoneId();

    UFUNCTION()
    void OnRep_Faction();

    UFUNCTION()
    void OnRep_PlayerIdentity();

    UFUNCTION()
    void OnRep_PersistenceStatus();

    UFUNCTION()
    void OnRep_Attributes();

    UFUNCTION()
    void OnRep_TargetSelection();

    UFUNCTION()
    void OnRep_CombatFeedback();

    UFUNCTION()
    void OnRep_InventoryArray();

    UFUNCTION()
    void OnRep_StatusEffects();

    UFUNCTION()
    void OnRep_QuestProgress();

    UFUNCTION()
    void OnRep_SkillProgress();

    void BroadcastStateUpdated();
    void RecalculateEquipmentBonusesFromInventory();
};
