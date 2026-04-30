#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MOBAMMOQuestTypes.h"

#include "MOBAMMOGameMode.generated.h"

class AMOBAMMOGameState;
class AMOBAMMOPlayerState;
class AMOBAMMOTrainingDummyActor;
class AMOBAMMOTrainingMinionActor;
class AMOBAMMOVendorActor;

UCLASS()
class MOBAMMO_API AMOBAMMOGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AMOBAMMOGameMode();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool ApplyDamageToPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool HealPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool ConsumeManaForPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool RestoreManaForPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool RespawnPlayer(AController* TargetController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool CastDebugDamageSpell(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool CastDebugHealSpell(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Debug")
    bool CastDebugDrainSpell(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Debug")
    bool CastDebugGrantItem(AController* InstigatorController, const FString& ItemId, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Inventory")
    bool UseFirstInventoryConsumable(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Inventory")
    bool ToggleEquipFirstInventoryItem(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Debug")
    bool TriggerDebugManaRestore(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Vendor")
    bool HandleVendorPurchase(AController* BuyerController, const FString& ItemId);

    // Skill point spending — called by server RPC in PlayerController.
    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Skills")
    bool HandleSpendSkillPoint(AController* Controller, int32 AbilitySlotIndex);

private:
    void UpdateConnectedPlayerCount();
    void ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options);
    void EnsureTrainingDummyActor();
    void EnsureTrainingMinionActor();
    void EnsureVendorActor();
    void ScheduleTrainingMinionRespawn();
    void RespawnTrainingMinionActor();
    void StartTrainingMinionAutoAttackLoop();
    void StopTrainingMinionAutoAttackLoop();
    void TickTrainingMinionAutoAttack();
    void TriggerTrainingMinionCounterAttack(AController* TargetController);
    bool ApplyTrainingMinionStrike(AController* TargetController, float DamageAmount, const FString& StrikeName);
    void GrantMinionLootToPlayer(AController* KillerController);
    bool HasActiveSession(AController* Controller) const;
    AMOBAMMOPlayerState* ResolveMOBAPlayerState(AController* Controller) const;
    AController* ResolveSelectedDebugTarget(const AMOBAMMOPlayerState* InstigatorState, AController* InstigatorController) const;
    bool IsTrainingDummySelected(const AMOBAMMOPlayerState* InstigatorState) const;
    bool IsTrainingMinionSelected(const AMOBAMMOPlayerState* InstigatorState) const;
    void InitializeDefaultAttributes(AMOBAMMOPlayerState* PlayerState) const;
    bool HasUsableSavedWorldPosition(const AMOBAMMOPlayerState* PlayerState) const;
    bool IsInsideArenaBounds(const FVector& Location) const;
    FVector ClampToArenaBounds(const FVector& Location) const;
    FVector GetSafeArenaReturnLocation() const;
    void ReturnControllerToArena(AController* Controller, const TCHAR* Reason);
    void RestorePlayerPawnToSavedLocation(AController* Controller);
    void StartPlayerAutoSaveLoop();
    void StartArenaSafetyLoop();
    void StartPlayerSessionHeartbeatLoop();
    void SaveAllActivePlayers();
    void EnforceArenaBoundsForAllPlayers();
    void SavePlayerProgress(AController* Controller) const;
    void HeartbeatAllActivePlayers();
    void HeartbeatPlayerSession(AController* Controller) const;
    AController* ResolveDebugTarget(AController* InstigatorController) const;
    bool IsControllerInAbilityRange(const AController* SourceController, const AController* TargetController, float AbilityRange) const;
    void PushCombatLog(const FString& CombatLog) const;
    void GrantKillExperience(AController* KillerController, int32 XPAmount, const FString& TargetName);
    void GrantKillReward(AController* KillerController, int32 XPAmount, int32 GoldAmount, const FString& TargetName);

    // Quest system
    void NotifyQuestEvent(AController* Controller, EMOBAMMOQuestType Type, int32 Amount = 1);
    void GrantQuestReward(AController* Controller, const FString& QuestId);

    // Rank-scaled ability stat helpers. Rank 1 = base, each extra rank adds 20% power / -10% cooldown.
    float RankScaledPower(float BasePower, int32 Rank) const;
    float RankScaledCooldown(float BaseCooldown, int32 Rank) const;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Progression")
    int32 KillXPTrainingDummy = 15;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Progression")
    int32 KillXPTrainingMinion = 40;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Progression")
    int32 KillXPPlayer = 60;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Currency")
    int32 KillGoldTrainingDummy = 2;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Currency")
    int32 KillGoldTrainingMinion = 8;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Currency")
    int32 KillGoldPlayer = 15;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DefaultMaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DefaultMaxMana = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDamageSpellManaCost = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDamageSpellPower = 18.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugHealSpellManaCost = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugHealSpellPower = 15.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDamageSpellRange = 1800.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugArcChargeDuration = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugArcChargeBonusDamage = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugArcChargeManaDiscount = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDrainSpellRange = 1600.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDrainSpellManaRestore = 12.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugManaRestoreAmount = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDamageCooldownDuration = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugHealCooldownDuration = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugDrainCooldownDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugManaSurgeCooldownDuration = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DebugRespawnDelayDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Persistence")
    float PlayerAutoSaveInterval = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Persistence")
    float PlayerSessionHeartbeatInterval = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Arena")
    FVector ArenaMinBounds = FVector(-3500.0f, -2500.0f, -500.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Arena")
    FVector ArenaMaxBounds = FVector(3500.0f, 2500.0f, 1500.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Arena")
    FVector ArenaSafeReturnLocation = FVector(0.0f, 0.0f, 220.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Arena")
    float ArenaSafetyCheckInterval = 0.25f;

    TMap<APlayerController*, FString> PendingSessionOptionsByController;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FVector TrainingDummySpawnLocation = FVector(520.0f, 360.0f, 140.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FRotator TrainingDummySpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Vendor")
    FVector VendorSpawnLocation = FVector(-520.0f, 360.0f, 140.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Vendor")
    FRotator VendorSpawnRotation = FRotator(0.0f, 0.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Vendor")
    float VendorPurchaseRange = 900.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FVector TrainingMinionSpawnLocation = FVector(520.0f, -360.0f, 140.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FRotator TrainingMinionSpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionRespawnDelay = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionCounterAttackDamage = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionCounterAttackRange = 1400.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionAutoAttackDamage = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionAutoAttackRange = 950.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionAutoAttackInterval = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    float TrainingMinionAggroDuration = 6.0f;

    UPROPERTY(Transient)
    TObjectPtr<AMOBAMMOTrainingDummyActor> TrainingDummyActor;

    UPROPERTY(Transient)
    TObjectPtr<AMOBAMMOTrainingMinionActor> TrainingMinionActor;

    UPROPERTY(Transient)
    TObjectPtr<AMOBAMMOVendorActor> VendorActor;

    FTimerHandle TrainingMinionRespawnTimerHandle;
    FTimerHandle TrainingMinionAutoAttackTimerHandle;
    FTimerHandle PlayerAutoSaveTimerHandle;
    FTimerHandle PlayerSessionHeartbeatTimerHandle;
    FTimerHandle ArenaSafetyTimerHandle;
};
