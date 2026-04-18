#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"

#include "MOBAMMOGameMode.generated.h"

class AMOBAMMOGameState;
class AMOBAMMOPlayerState;
class AMOBAMMOTrainingDummyActor;
class AMOBAMMOTrainingMinionActor;

UCLASS()
class MOBAMMO_API AMOBAMMOGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AMOBAMMOGameMode();

    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

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

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool CastDebugDrainSpell(AController* InstigatorController);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool TriggerDebugManaRestore(AController* InstigatorController);

private:
    void UpdateConnectedPlayerCount();
    void ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options);
    void EnsureTrainingDummyActor();
    void EnsureTrainingMinionActor();
    void ScheduleTrainingMinionRespawn();
    void RespawnTrainingMinionActor();
    void StartTrainingMinionAutoAttackLoop();
    void StopTrainingMinionAutoAttackLoop();
    void TickTrainingMinionAutoAttack();
    void TriggerTrainingMinionCounterAttack(AController* TargetController);
    bool ApplyTrainingMinionStrike(AController* TargetController, float DamageAmount, const FString& StrikeName);
    AMOBAMMOPlayerState* ResolveMOBAPlayerState(AController* Controller) const;
    AController* ResolveSelectedDebugTarget(const AMOBAMMOPlayerState* InstigatorState, AController* InstigatorController) const;
    bool IsTrainingDummySelected(const AMOBAMMOPlayerState* InstigatorState) const;
    bool IsTrainingMinionSelected(const AMOBAMMOPlayerState* InstigatorState) const;
    void InitializeDefaultAttributes(AMOBAMMOPlayerState* PlayerState) const;
    AController* ResolveDebugTarget(AController* InstigatorController) const;
    bool IsControllerInAbilityRange(const AController* SourceController, const AController* TargetController, float AbilityRange) const;
    void PushCombatLog(const FString& CombatLog) const;

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

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FVector TrainingDummySpawnLocation = FVector(520.0f, 360.0f, 140.0f);

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Training")
    FRotator TrainingDummySpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

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

    FTimerHandle TrainingMinionRespawnTimerHandle;
    FTimerHandle TrainingMinionAutoAttackTimerHandle;
};
