#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

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
    void ApplyPersistentCharacterState(int32 InExperience, const FVector& InSavedWorldPosition, float InCurrentHealth, float InMaxHealth, float InCurrentMana, float InMaxMana, int32 InKillCount, int32 InDeathCount);

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

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FVector GetSavedWorldPosition() const { return SavedWorldPosition; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    bool HasPersistentCharacterSnapshot() const { return bHasPersistentCharacterSnapshot; }

    bool HasConsumedPersistentSpawnLocation() const { return bPersistentSpawnLocationConsumed; }
    void MarkPersistentSpawnLocationConsumed() { bPersistentSpawnLocationConsumed = true; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentMana() const { return CurrentMana; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxMana() const { return MaxMana; }

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

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
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

private:
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

    void BroadcastStateUpdated();
};
