#include "MOBAMMOGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MOBAMMOAbilitySet.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOCharacter.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOTrainingDummyActor.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
    FString ReadOption(const FString& Options, const TCHAR* Key)
    {
        return UGameplayStatics::ParseOption(Options, Key);
    }

    bool TryReadIntOption(const FString& Options, const TCHAR* Key, int32& OutValue)
    {
        const FString Value = ReadOption(Options, Key);
        if (Value.IsEmpty())
        {
            return false;
        }

        OutValue = FCString::Atoi(*Value);
        return true;
    }

    bool TryReadFloatOption(const FString& Options, const TCHAR* Key, float& OutValue)
    {
        const FString Value = ReadOption(Options, Key);
        if (Value.IsEmpty())
        {
            return false;
        }

        OutValue = FCString::Atof(*Value);
        return true;
    }
}

AMOBAMMOGameMode::AMOBAMMOGameMode()
{
    GameStateClass = AMOBAMMOGameState::StaticClass();
    PlayerStateClass = AMOBAMMOPlayerState::StaticClass();
    PlayerControllerClass = AMOBAMMOPlayerController::StaticClass();

    DefaultPawnClass = AMOBAMMOCharacter::StaticClass();
}

FString AMOBAMMOGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    if (NewPlayerController)
    {
        PendingSessionOptionsByController.Add(NewPlayerController, Options);
    }

    ApplyPlayerSessionData(NewPlayerController, Options);
    return ErrorMessage;
}

void AMOBAMMOGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (const FString* PendingOptions = PendingSessionOptionsByController.Find(NewPlayer))
    {
        ApplyPlayerSessionData(NewPlayer, *PendingOptions);
        PendingSessionOptionsByController.Remove(NewPlayer);
    }

    UpdateConnectedPlayerCount();

    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        MOBAGameState->InitializeTrainingDummy();
    }

    EnsureTrainingDummyActor();
    EnsureTrainingMinionActor();

    if (AMOBAMMOPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AMOBAMMOPlayerState>() : nullptr)
    {
        if (!PlayerState->HasPersistentCharacterSnapshot() && (PlayerState->GetMaxHealth() <= 0.0f || PlayerState->GetCurrentHealth() <= 0.0f))
        {
            InitializeDefaultAttributes(PlayerState);
        }
    }

    RestorePlayerPawnToSavedLocation(NewPlayer);
    StartArenaSafetyLoop();
    StartPlayerAutoSaveLoop();
    StartPlayerSessionHeartbeatLoop();
}

void AMOBAMMOGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PlayerController = Cast<APlayerController>(Exiting))
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>())
            {
                BackendSubsystem->EndCurrentCharacterSession(PlayerController);
            }
        }

        PendingSessionOptionsByController.Remove(PlayerController);
    }

    Super::Logout(Exiting);
    UpdateConnectedPlayerCount();
}

void AMOBAMMOGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    RestorePlayerPawnToSavedLocation(NewPlayer);

    if (UWorld* World = GetWorld())
    {
        TWeakObjectPtr<AController> WeakController = NewPlayer;
        World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
        {
            if (WeakController.IsValid())
            {
                RestorePlayerPawnToSavedLocation(WeakController.Get());
            }
        }));

        FTimerHandle DeferredRestoreHandle;
        World->GetTimerManager().SetTimer(DeferredRestoreHandle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
        {
            if (WeakController.IsValid())
            {
                RestorePlayerPawnToSavedLocation(WeakController.Get());
            }
        }), 0.2f, false);
    }
}

void AMOBAMMOGameMode::EnsureTrainingDummyActor()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(TrainingDummyActor))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    TrainingDummyActor = World->SpawnActor<AMOBAMMOTrainingDummyActor>(
        AMOBAMMOTrainingDummyActor::StaticClass(),
        TrainingDummySpawnLocation,
        TrainingDummySpawnRotation,
        SpawnParams
    );
}

void AMOBAMMOGameMode::EnsureTrainingMinionActor()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(TrainingMinionActor))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    TrainingMinionActor = World->SpawnActor<AMOBAMMOTrainingMinionActor>(
        AMOBAMMOTrainingMinionActor::StaticClass(),
        TrainingMinionSpawnLocation,
        TrainingMinionSpawnRotation,
        SpawnParams
    );

    if (TrainingMinionActor)
    {
        PushCombatLog(FString::Printf(TEXT("%s entered the arena."), *AMOBAMMOTrainingMinionActor::GetTrainingMinionName()));
        StartTrainingMinionAutoAttackLoop();
    }
}

void AMOBAMMOGameMode::ScheduleTrainingMinionRespawn()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetTimerManager().IsTimerActive(TrainingMinionRespawnTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        TrainingMinionRespawnTimerHandle,
        this,
        &AMOBAMMOGameMode::RespawnTrainingMinionActor,
        FMath::Max(0.1f, TrainingMinionRespawnDelay),
        false
    );
}

void AMOBAMMOGameMode::RespawnTrainingMinionActor()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(TrainingMinionActor))
    {
        TrainingMinionActor->Destroy();
        TrainingMinionActor = nullptr;
    }

    EnsureTrainingMinionActor();
}

void AMOBAMMOGameMode::StartTrainingMinionAutoAttackLoop()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetTimerManager().IsTimerActive(TrainingMinionAutoAttackTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        TrainingMinionAutoAttackTimerHandle,
        this,
        &AMOBAMMOGameMode::TickTrainingMinionAutoAttack,
        FMath::Max(0.25f, TrainingMinionAutoAttackInterval),
        true
    );
}

void AMOBAMMOGameMode::StopTrainingMinionAutoAttackLoop()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(TrainingMinionAutoAttackTimerHandle);
    }
}

void AMOBAMMOGameMode::TickTrainingMinionAutoAttack()
{
    if (!HasAuthority() || !IsValid(TrainingMinionActor) || !TrainingMinionActor->IsAlive())
    {
        StopTrainingMinionAutoAttackLoop();
        return;
    }

    AController* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(TrainingMinionAutoAttackRange);
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(PlayerController);
        APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (!PlayerState || !PlayerState->IsAlive() || !PlayerPawn)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(TrainingMinionActor->GetActorLocation(), PlayerPawn->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = PlayerController;
        }
    }

    if (BestTarget)
    {
        ApplyTrainingMinionStrike(BestTarget, TrainingMinionAutoAttackDamage, TEXT("Minion Pulse"));
    }
}

void AMOBAMMOGameMode::TriggerTrainingMinionCounterAttack(AController* TargetController)
{
    if (!HasAuthority() || !IsValid(TrainingMinionActor) || !TrainingMinionActor->IsAlive())
    {
        return;
    }

    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    APawn* TargetPawn = TargetController ? TargetController->GetPawn() : nullptr;
    if (!TargetState || !TargetState->IsAlive() || !TargetPawn)
    {
        return;
    }

    const float DistanceSquared = FVector::DistSquared(TrainingMinionActor->GetActorLocation(), TargetPawn->GetActorLocation());
    if (DistanceSquared > FMath::Square(TrainingMinionCounterAttackRange))
    {
        return;
    }

    ApplyTrainingMinionStrike(TargetController, TrainingMinionCounterAttackDamage, TEXT("Minion Strike"));
}

bool AMOBAMMOGameMode::ApplyTrainingMinionStrike(AController* TargetController, float DamageAmount, const FString& StrikeName)
{
    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    if (!TargetState || !TargetState->IsAlive() || DamageAmount <= 0.0f)
    {
        return false;
    }

    const float DamageApplied = TargetState->ApplyDamage(DamageAmount);
    if (DamageApplied <= 0.0f)
    {
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float AggroEndServerTime = ServerWorldTimeSeconds + FMath::Max(0.25f, TrainingMinionAggroDuration);

    if (AMOBAMMOGameState* MOBAState = GetGameState<AMOBAMMOGameState>())
    {
        MOBAState->SetTrainingMinionThreat(TargetState->GetPlayerName(), StrikeName, ServerWorldTimeSeconds, AggroEndServerTime);
    }

    if (IsValid(TrainingMinionActor) && TrainingMinionActor->IsAlive())
    {
        TrainingMinionActor->SetAggroTarget(
            TargetController ? TargetController->GetPawn() : nullptr,
            TargetState->GetPlayerName(),
            TrainingMinionAggroDuration
        );
    }

    TargetState->PushIncomingCombatFeedback(FString::Printf(TEXT("-%.0f %s"), DamageApplied, *StrikeName), 1.1f, false);
    PushCombatLog(FString::Printf(
        TEXT("%s hit %s with %s for %.0f damage. Player HP %.0f/%.0f."),
        *AMOBAMMOTrainingMinionActor::GetTrainingMinionName(),
        *TargetState->GetPlayerName(),
        *StrikeName,
        DamageApplied,
        TargetState->GetCurrentHealth(),
        TargetState->GetMaxHealth()
    ));

    if (!TargetState->IsAlive())
    {
        TargetState->RecordDeath();
        TargetState->StartRespawnCooldown(DebugRespawnDelayDuration, ServerWorldTimeSeconds);
        PushCombatLog(FString::Printf(
            TEXT("%s was defeated by %s. Reforge unlocks in %.1fs."),
            *TargetState->GetPlayerName(),
            *AMOBAMMOTrainingMinionActor::GetTrainingMinionName(),
            DebugRespawnDelayDuration
        ));
    }

    SavePlayerProgress(TargetController);
    return true;
}

bool AMOBAMMOGameMode::ApplyDamageToPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    const bool bApplied = PlayerState->ApplyDamage(Amount) > 0.0f;
    if (bApplied)
    {
        SavePlayerProgress(TargetController);
    }

    return bApplied;
}

bool AMOBAMMOGameMode::HealPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    const float AppliedHealing = PlayerState->ApplyHealing(Amount);
    if (AppliedHealing > 0.0f)
    {
        PushCombatLog(FString::Printf(TEXT("%s recovered %.0f health."), *PlayerState->GetPlayerName(), AppliedHealing));
        SavePlayerProgress(TargetController);
    }

    return AppliedHealing > 0.0f;
}

bool AMOBAMMOGameMode::ConsumeManaForPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount < 0.0f)
    {
        return false;
    }

    const bool bConsumed = PlayerState->ConsumeMana(Amount);
    if (bConsumed)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s spent %.0f mana. MP %.0f/%.0f."),
            *PlayerState->GetPlayerName(),
            Amount,
            PlayerState->GetCurrentMana(),
            PlayerState->GetMaxMana()
        ));
        SavePlayerProgress(TargetController);
    }

    return bConsumed;
}

bool AMOBAMMOGameMode::RestoreManaForPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    const float RestoredMana = PlayerState->RestoreMana(Amount);
    if (RestoredMana > 0.0f)
    {
        PushCombatLog(FString::Printf(TEXT("%s recovered %.0f mana."), *PlayerState->GetPlayerName(), RestoredMana));
        SavePlayerProgress(TargetController);
    }

    return RestoredMana > 0.0f;
}

bool AMOBAMMOGameMode::RespawnPlayer(AController* TargetController)
{
    if (!HasAuthority() || !IsValid(TargetController))
    {
        return false;
    }

    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState)
    {
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float RespawnRemaining = PlayerState->GetRespawnCooldownRemaining(ServerWorldTimeSeconds);
    if (!PlayerState->CanRespawn(ServerWorldTimeSeconds))
    {
        PushCombatLog(FString::Printf(
            TEXT("%s reached for %s too early. Reforge available in %.1fs."),
            *PlayerState->GetPlayerName(),
            *MOBAMMOAbilitySet::Reforge().Name,
            RespawnRemaining
        ));
        return false;
    }

    if (APawn* ExistingPawn = TargetController->GetPawn())
    {
        TargetController->UnPossess();
        ExistingPawn->Destroy();
    }

    RestartPlayer(TargetController);
    ReturnControllerToArena(TargetController, TEXT("respawn"));
    InitializeDefaultAttributes(PlayerState);
    PushCombatLog(FString::Printf(TEXT("%s used %s and respawned with full attributes."), *PlayerState->GetPlayerName(), *MOBAMMOAbilitySet::Reforge().Name));
    SavePlayerProgress(TargetController);
    return true;
}

bool AMOBAMMOGameMode::CastDebugDamageSpell(AController* InstigatorController)
{
    const FMOBAMMOAbilityDefinition DamageAbility = MOBAMMOAbilitySet::ArcBurst();
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    if (IsTrainingDummySelected(InstigatorState))
    {
        AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>();
        if (!MOBAGameState)
        {
            return false;
        }

        const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        const bool bHasArcCharge = InstigatorState->HasArcCharge(ServerWorldTimeSeconds);
        const float DamageManaCost = FMath::Max(0.0f, DebugDamageSpellManaCost - (bHasArcCharge ? DebugArcChargeManaDiscount : 0.0f));
        const float DamagePower = DebugDamageSpellPower + (bHasArcCharge ? DebugArcChargeBonusDamage : 0.0f);
        const float DamageCooldownRemaining = InstigatorState->GetDamageCooldownRemaining(ServerWorldTimeSeconds);
        if (DamageCooldownRemaining > KINDA_SMALL_NUMBER)
        {
            PushCombatLog(FString::Printf(TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."), *InstigatorState->GetPlayerName(), *DamageAbility.Name, DamageCooldownRemaining));
            return false;
        }

        if (!InstigatorState->ConsumeMana(DamageManaCost))
        {
            PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *DamageAbility.Name));
            return false;
        }

        const float DamageApplied = MOBAGameState->ApplyDamageToTrainingDummy(DamagePower);
        InstigatorState->StartDamageCooldown(DebugDamageCooldownDuration, ServerWorldTimeSeconds);
        if (bHasArcCharge)
        {
            InstigatorState->StartArcCharge(0.0f, ServerWorldTimeSeconds);
        }

        PushCombatLog(FString::Printf(
            TEXT("%s hit %s with %s%s for %.0f damage. Dummy HP %.0f/%.0f."),
            *InstigatorState->GetPlayerName(),
            *MOBAGameState->GetTrainingDummyName(),
            *DamageAbility.Name,
            bHasArcCharge ? TEXT(" [Charged]") : TEXT(""),
            DamageApplied,
            MOBAGameState->GetTrainingDummyHealth(),
            MOBAGameState->GetTrainingDummyMaxHealth()
        ));

        if (!MOBAGameState->IsTrainingDummyAlive())
        {
            InstigatorState->RecordKill();
            PushCombatLog(FString::Printf(TEXT("%s disabled the Training Dummy. It will reset on the next hit."), *InstigatorState->GetPlayerName()));
        }

        SavePlayerProgress(InstigatorController);
        return DamageApplied > 0.0f;
    }

    if (IsTrainingMinionSelected(InstigatorState))
    {
        EnsureTrainingMinionActor();
        if (!IsValid(TrainingMinionActor))
        {
            PushCombatLog(TEXT("Sparring Minion is not available yet."));
            return false;
        }

        const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        const bool bHasArcCharge = InstigatorState->HasArcCharge(ServerWorldTimeSeconds);
        const float DamageManaCost = FMath::Max(0.0f, DebugDamageSpellManaCost - (bHasArcCharge ? DebugArcChargeManaDiscount : 0.0f));
        const float DamagePower = DebugDamageSpellPower + (bHasArcCharge ? DebugArcChargeBonusDamage : 0.0f);
        const float DamageCooldownRemaining = InstigatorState->GetDamageCooldownRemaining(ServerWorldTimeSeconds);
        if (DamageCooldownRemaining > KINDA_SMALL_NUMBER)
        {
            PushCombatLog(FString::Printf(TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."), *InstigatorState->GetPlayerName(), *DamageAbility.Name, DamageCooldownRemaining));
            return false;
        }

        if (!InstigatorState->ConsumeMana(DamageManaCost))
        {
            PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *DamageAbility.Name));
            return false;
        }

        const float PreviousHealth = TrainingMinionActor->GetCurrentHealth();
        UGameplayStatics::ApplyDamage(TrainingMinionActor, DamagePower, InstigatorController, InstigatorController ? InstigatorController->GetPawn() : nullptr, nullptr);
        const float DamageApplied = FMath::Max(0.0f, PreviousHealth - TrainingMinionActor->GetCurrentHealth());
        InstigatorState->StartDamageCooldown(DebugDamageCooldownDuration, ServerWorldTimeSeconds);
        if (bHasArcCharge)
        {
            InstigatorState->StartArcCharge(0.0f, ServerWorldTimeSeconds);
        }

        PushCombatLog(FString::Printf(
            TEXT("%s hit %s with %s%s for %.0f damage. Minion HP %.0f/%.0f."),
            *InstigatorState->GetPlayerName(),
            *AMOBAMMOTrainingMinionActor::GetTrainingMinionName(),
            *DamageAbility.Name,
            bHasArcCharge ? TEXT(" [Charged]") : TEXT(""),
            DamageApplied,
            TrainingMinionActor->GetCurrentHealth(),
            TrainingMinionActor->GetMaxHealth()
        ));

        if (!TrainingMinionActor->IsAlive())
        {
            InstigatorState->RecordKill();
            StopTrainingMinionAutoAttackLoop();
            ScheduleTrainingMinionRespawn();
            PushCombatLog(FString::Printf(
                TEXT("%s defeated %s. Reinforcements arrive in %.0fs."),
                *InstigatorState->GetPlayerName(),
                *AMOBAMMOTrainingMinionActor::GetTrainingMinionName(),
                TrainingMinionRespawnDelay
            ));
        }
        else
        {
            TriggerTrainingMinionCounterAttack(InstigatorController);
        }

        SavePlayerProgress(InstigatorController);
        return DamageApplied > 0.0f;
    }

    AController* TargetController = ResolveSelectedDebugTarget(InstigatorState, InstigatorController);
    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    if (!InstigatorState || !TargetState)
    {
        return false;
    }

    if (!IsControllerInAbilityRange(InstigatorController, TargetController, DebugDamageSpellRange))
    {
        PushCombatLog(FString::Printf(
            TEXT("%s tried to cast %s, but the target was out of range (%.0f)."),
            *InstigatorState->GetPlayerName(),
            *DamageAbility.Name,
            DebugDamageSpellRange
        ));
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bHasArcCharge = InstigatorState->HasArcCharge(ServerWorldTimeSeconds);
    const float DamageManaCost = FMath::Max(0.0f, DebugDamageSpellManaCost - (bHasArcCharge ? DebugArcChargeManaDiscount : 0.0f));
    const float DamagePower = DebugDamageSpellPower + (bHasArcCharge ? DebugArcChargeBonusDamage : 0.0f);
    const float DamageCooldownRemaining = InstigatorState->GetDamageCooldownRemaining(ServerWorldTimeSeconds);
    if (DamageCooldownRemaining > KINDA_SMALL_NUMBER)
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."), *InstigatorState->GetPlayerName(), *DamageAbility.Name, DamageCooldownRemaining));
        return false;
    }

    if (!InstigatorState->ConsumeMana(DamageManaCost))
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *DamageAbility.Name));
        return false;
    }

    const float DamageApplied = TargetState->ApplyDamage(DamagePower);
    if (DamageApplied <= 0.0f)
    {
        PushCombatLog(FString::Printf(TEXT("%s cast %s but nothing changed."), *InstigatorState->GetPlayerName(), *DamageAbility.Name));
        return false;
    }

    const FString CombatEntry = FString::Printf(
        TEXT("%s hit %s with %s%s for %.0f damage. Target HP %.0f/%.0f."),
        *InstigatorState->GetPlayerName(),
        *TargetState->GetPlayerName(),
        *DamageAbility.Name,
        bHasArcCharge ? TEXT(" [Charged]") : TEXT(""),
        DamageApplied,
        TargetState->GetCurrentHealth(),
        TargetState->GetMaxHealth()
    );
    TargetState->PushIncomingCombatFeedback(FString::Printf(TEXT("-%.0f %s"), DamageApplied, *DamageAbility.Name), 1.25f, false);
    PushCombatLog(CombatEntry);
    if (!TargetState->IsAlive())
    {
        InstigatorState->RecordKill();
        TargetState->RecordDeath();
        TargetState->StartRespawnCooldown(DebugRespawnDelayDuration, ServerWorldTimeSeconds);
        PushCombatLog(FString::Printf(
            TEXT("%s was defeated by %s. Reforge unlocks in %.1fs."),
            *TargetState->GetPlayerName(),
            *InstigatorState->GetPlayerName(),
            DebugRespawnDelayDuration
        ));
    }
    InstigatorState->StartDamageCooldown(DebugDamageCooldownDuration, ServerWorldTimeSeconds);
    if (bHasArcCharge)
    {
        InstigatorState->StartArcCharge(0.0f, ServerWorldTimeSeconds);
    }
    SavePlayerProgress(InstigatorController);
    SavePlayerProgress(TargetController);
    return true;
}

bool AMOBAMMOGameMode::CastDebugHealSpell(AController* InstigatorController)
{
    const FMOBAMMOAbilityDefinition HealAbility = MOBAMMOAbilitySet::Renew();
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float HealCooldownRemaining = InstigatorState->GetHealCooldownRemaining(ServerWorldTimeSeconds);
    if (HealCooldownRemaining > KINDA_SMALL_NUMBER)
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."), *InstigatorState->GetPlayerName(), *HealAbility.Name, HealCooldownRemaining));
        return false;
    }

    if (!InstigatorState->ConsumeMana(DebugHealSpellManaCost))
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *HealAbility.Name));
        return false;
    }

    const float HealingApplied = InstigatorState->ApplyHealing(DebugHealSpellPower);
    if (HealingApplied > 0.0f)
    {
        InstigatorState->PushIncomingCombatFeedback(FString::Printf(TEXT("+%.0f %s"), HealingApplied, *HealAbility.Name), 1.25f, true);
    }
    PushCombatLog(FString::Printf(
        TEXT("%s cast %s for %.0f healing. HP %.0f/%.0f."),
        *InstigatorState->GetPlayerName(),
        *HealAbility.Name,
        HealingApplied,
        InstigatorState->GetCurrentHealth(),
        InstigatorState->GetMaxHealth()
    ));
    InstigatorState->StartHealCooldown(DebugHealCooldownDuration, ServerWorldTimeSeconds);
    SavePlayerProgress(InstigatorController);
    return HealingApplied > 0.0f;
}

bool AMOBAMMOGameMode::TriggerDebugManaRestore(AController* InstigatorController)
{
    const FMOBAMMOAbilityDefinition UtilityAbility = MOBAMMOAbilitySet::ManaSurge();
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float CooldownRemaining = InstigatorState->GetManaSurgeCooldownRemaining(ServerWorldTimeSeconds);
    if (CooldownRemaining > KINDA_SMALL_NUMBER)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."),
            *InstigatorState->GetPlayerName(),
            *UtilityAbility.Name,
            CooldownRemaining
        ));
        return false;
    }

    const float ManaRestored = InstigatorState->RestoreMana(DebugManaRestoreAmount);
    if (ManaRestored > 0.0f)
    {
        InstigatorState->PushIncomingCombatFeedback(FString::Printf(TEXT("+%.0f %s"), ManaRestored, *UtilityAbility.Name), 1.1f, true);
    }
    PushCombatLog(FString::Printf(
        TEXT("%s cast %s and restored %.0f mana. MP %.0f/%.0f."),
        *InstigatorState->GetPlayerName(),
        *UtilityAbility.Name,
        ManaRestored,
        InstigatorState->GetCurrentMana(),
        InstigatorState->GetMaxMana()
    ));
    InstigatorState->StartManaSurgeCooldown(DebugManaSurgeCooldownDuration, ServerWorldTimeSeconds);
    SavePlayerProgress(InstigatorController);
    return ManaRestored > 0.0f;
}

bool AMOBAMMOGameMode::CastDebugDrainSpell(AController* InstigatorController)
{
    const FMOBAMMOAbilityDefinition DrainAbility = MOBAMMOAbilitySet::DrainPulse();
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    if (IsTrainingDummySelected(InstigatorState))
    {
        AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>();
        if (!MOBAGameState)
        {
            return false;
        }

        const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        const float CooldownRemaining = InstigatorState->GetDrainCooldownRemaining(ServerWorldTimeSeconds);
        if (CooldownRemaining > KINDA_SMALL_NUMBER)
        {
            PushCombatLog(FString::Printf(
                TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."),
                *InstigatorState->GetPlayerName(),
                *DrainAbility.Name,
                CooldownRemaining
            ));
            return false;
        }

        if (!InstigatorState->ConsumeMana(DrainAbility.ManaCost))
        {
            PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *DrainAbility.Name));
            return false;
        }

        const float DrainedMana = MOBAGameState->DrainTrainingDummyMana(DebugDrainSpellManaRestore);
        const float ManaRecovered = InstigatorState->RestoreMana(DrainedMana);
        InstigatorState->PushIncomingCombatFeedback(FString::Printf(TEXT("+%.0f MP %s"), ManaRecovered, *DrainAbility.Name), 1.1f, true);
        InstigatorState->StartArcCharge(DebugArcChargeDuration, ServerWorldTimeSeconds);
        InstigatorState->StartDrainCooldown(DebugDrainCooldownDuration, ServerWorldTimeSeconds);
        PushCombatLog(FString::Printf(
            TEXT("%s cast %s on %s, draining %.0f mana, restoring %.0f mana, and empowering Arc Burst."),
            *InstigatorState->GetPlayerName(),
            *DrainAbility.Name,
            *MOBAGameState->GetTrainingDummyName(),
            DrainedMana,
            ManaRecovered
        ));
        SavePlayerProgress(InstigatorController);
        return ManaRecovered > 0.0f || DrainedMana > 0.0f;
    }

    AController* TargetController = ResolveSelectedDebugTarget(InstigatorState, InstigatorController);
    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    if (!InstigatorState || !TargetState)
    {
        return false;
    }

    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float CooldownRemaining = InstigatorState->GetDrainCooldownRemaining(ServerWorldTimeSeconds);
    if (CooldownRemaining > KINDA_SMALL_NUMBER)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s tried to cast %s, but it is on cooldown for %.1fs."),
            *InstigatorState->GetPlayerName(),
            *DrainAbility.Name,
            CooldownRemaining
        ));
        return false;
    }

    if (!IsControllerInAbilityRange(InstigatorController, TargetController, DebugDrainSpellRange))
    {
        PushCombatLog(FString::Printf(
            TEXT("%s tried to cast %s, but the target was out of range (%.0f)."),
            *InstigatorState->GetPlayerName(),
            *DrainAbility.Name,
            DebugDrainSpellRange
        ));
        return false;
    }

    if (!InstigatorState->ConsumeMana(DrainAbility.ManaCost))
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but lacked mana."), *InstigatorState->GetPlayerName(), *DrainAbility.Name));
        return false;
    }

    const float DrainedMana = FMath::Min(DebugDrainSpellManaRestore, TargetState->GetCurrentMana());
    TargetState->ConsumeMana(DrainedMana);
    const float ManaRecovered = InstigatorState->RestoreMana(DrainedMana);
    TargetState->PushIncomingCombatFeedback(FString::Printf(TEXT("-%.0f MP %s"), DrainedMana, *DrainAbility.Name), 1.1f, false);
    InstigatorState->PushIncomingCombatFeedback(FString::Printf(TEXT("+%.0f MP %s"), ManaRecovered, *DrainAbility.Name), 1.1f, true);
    InstigatorState->StartArcCharge(DebugArcChargeDuration, ServerWorldTimeSeconds);
    PushCombatLog(FString::Printf(
        TEXT("%s cast %s on %s, draining %.0f mana, restoring %.0f mana, and empowering Arc Burst."),
        *InstigatorState->GetPlayerName(),
        *DrainAbility.Name,
        *TargetState->GetPlayerName(),
        DrainedMana,
        ManaRecovered
    ));
    InstigatorState->StartDrainCooldown(DebugDrainCooldownDuration, ServerWorldTimeSeconds);
    SavePlayerProgress(InstigatorController);
    SavePlayerProgress(TargetController);
    return ManaRecovered > 0.0f || DrainedMana > 0.0f;
}

bool AMOBAMMOGameMode::CastDebugGrantItem(AController* InstigatorController, const FString& ItemId, int32 Quantity)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState || Quantity <= 0 || ItemId.IsEmpty())
    {
        return false;
    }

    InstigatorState->GrantItem(ItemId, Quantity);
    PushCombatLog(FString::Printf(TEXT("%s gained %dx %s."), *InstigatorState->GetPlayerName(), Quantity, *ItemId));
    SavePlayerProgress(InstigatorController);
    return true;
}

void AMOBAMMOGameMode::UpdateConnectedPlayerCount()
{
    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        int32 ValidPlayers = 0;
        for (APlayerState* PlayerState : GameState->PlayerArray)
        {
            if (IsValid(PlayerState))
            {
                ++ValidPlayers;
            }
        }

        MOBAGameState->SetConnectedPlayers(ValidPlayers);
    }
}

void AMOBAMMOGameMode::ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options)
{
    if (!NewPlayerController)
    {
        return;
    }

    AMOBAMMOPlayerState* MOBAPlayerState = NewPlayerController->GetPlayerState<AMOBAMMOPlayerState>();
    if (!MOBAPlayerState)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Persistence] Session data skipped because PlayerState is not ready."));
        return;
    }

    const FString AccountId = ReadOption(Options, TEXT("AccountId"));
    const FString CharacterId = ReadOption(Options, TEXT("CharacterId"));
    const FString SessionId = ReadOption(Options, TEXT("SessionId"));
    if (AccountId.IsEmpty() || CharacterId.IsEmpty())
    {
        return;
    }

    const FString CharacterName = ReadOption(Options, TEXT("CharacterName"));
    const FString ClassId = ReadOption(Options, TEXT("ClassId"));
    const FString LevelString = ReadOption(Options, TEXT("Level"));
    const int32 CharacterLevel = LevelString.IsEmpty() ? 1 : FMath::Max(1, FCString::Atoi(*LevelString));
    int32 CharacterExperience = 0;
    FVector SavedWorldPosition = FVector::ZeroVector;
    int32 PresetId = 4;
    int32 ColorIndex = 0;
    int32 Shade = 58;
    int32 Transparent = 18;
    int32 TextureDetail = 88;
    int32 KillCount = 0;
    int32 DeathCount = 0;
    float CurrentHealth = DefaultMaxHealth;
    float MaxHealth = DefaultMaxHealth;
    float CurrentMana = DefaultMaxMana;
    float MaxMana = DefaultMaxMana;
    float SavedPositionX = 0.0f;
    float SavedPositionY = 0.0f;
    float SavedPositionZ = 0.0f;

    TryReadIntOption(Options, TEXT("Experience"), CharacterExperience);
    TryReadFloatOption(Options, TEXT("PositionX"), SavedPositionX);
    TryReadFloatOption(Options, TEXT("PositionY"), SavedPositionY);
    TryReadFloatOption(Options, TEXT("PositionZ"), SavedPositionZ);
    TryReadIntOption(Options, TEXT("PresetId"), PresetId);
    TryReadIntOption(Options, TEXT("ColorIndex"), ColorIndex);
    TryReadIntOption(Options, TEXT("Shade"), Shade);
    TryReadIntOption(Options, TEXT("Transparent"), Transparent);
    TryReadIntOption(Options, TEXT("TextureDetail"), TextureDetail);
    TryReadFloatOption(Options, TEXT("Health"), CurrentHealth);
    TryReadFloatOption(Options, TEXT("MaxHealth"), MaxHealth);
    TryReadFloatOption(Options, TEXT("Mana"), CurrentMana);
    TryReadFloatOption(Options, TEXT("MaxMana"), MaxMana);
    TryReadIntOption(Options, TEXT("KillCount"), KillCount);
    TryReadIntOption(Options, TEXT("DeathCount"), DeathCount);
    SavedWorldPosition = ClampToArenaBounds(FVector(SavedPositionX, SavedPositionY, SavedPositionZ));

    UE_LOG(LogTemp, Log, TEXT("[Persistence] Applying session data. CharacterId=%s SessionId=%s Position=%s Health=%.1f/%.1f Mana=%.1f/%.1f"),
        *CharacterId,
        *SessionId,
        *SavedWorldPosition.ToCompactString(),
        CurrentHealth,
        MaxHealth,
        CurrentMana,
        MaxMana);

    MOBAPlayerState->ApplySessionIdentity(AccountId, CharacterId, SessionId, CharacterName, ClassId, CharacterLevel);
    MOBAPlayerState->ApplyAppearanceSelection(PresetId, ColorIndex, Shade, Transparent, TextureDetail);
    MOBAPlayerState->ApplyPersistentCharacterState(CharacterExperience, SavedWorldPosition, CurrentHealth, MaxHealth, CurrentMana, MaxMana, KillCount, DeathCount);

    const FString InventoryString = ReadOption(Options, TEXT("Inventory"));
    if (!InventoryString.IsEmpty())
    {
        TArray<FString> ItemTokens;
        InventoryString.ParseIntoArray(ItemTokens, TEXT(","), true);

        TArray<FMOBAMMOInventoryItem> ParsedInventory;
        for (const FString& Token : ItemTokens)
        {
            TArray<FString> Parts;
            Token.ParseIntoArray(Parts, TEXT(":"), false);
            if (Parts.Num() == 3)
            {
                FMOBAMMOInventoryItem NewItem;
                NewItem.ItemId = Parts[0];
                NewItem.Quantity = FCString::Atoi(*Parts[1]);
                NewItem.SlotIndex = FCString::Atoi(*Parts[2]);
                ParsedInventory.Add(NewItem);
            }
        }
        MOBAPlayerState->ApplyPersistentInventory(ParsedInventory);
    }
}

AMOBAMMOPlayerState* AMOBAMMOGameMode::ResolveMOBAPlayerState(AController* Controller) const
{
    if (!HasAuthority() || !IsValid(Controller))
    {
        return nullptr;
    }

    return Controller->GetPlayerState<AMOBAMMOPlayerState>();
}

AController* AMOBAMMOGameMode::ResolveSelectedDebugTarget(const AMOBAMMOPlayerState* InstigatorState, AController* InstigatorController) const
{
    if (InstigatorState)
    {
        const FString SelectedTargetCharacterId = InstigatorState->GetSelectedTargetCharacterId();
        if (!SelectedTargetCharacterId.IsEmpty())
        {
            for (APlayerState* IteratedPlayerState : GameState->PlayerArray)
            {
                const AMOBAMMOPlayerState* CandidateState = Cast<AMOBAMMOPlayerState>(IteratedPlayerState);
                if (!CandidateState || CandidateState == InstigatorState)
                {
                    continue;
                }

                if (CandidateState->GetCharacterId() == SelectedTargetCharacterId)
                {
                    if (AController* CandidateController = Cast<AController>(CandidateState->GetOwner()))
                    {
                        return CandidateController;
                    }
                }
            }
        }
    }

    return ResolveDebugTarget(InstigatorController);
}

bool AMOBAMMOGameMode::IsTrainingDummySelected(const AMOBAMMOPlayerState* InstigatorState) const
{
    const AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>();
    return InstigatorState
        && MOBAGameState
        && !InstigatorState->GetSelectedTargetCharacterId().IsEmpty()
        && InstigatorState->GetSelectedTargetCharacterId() == MOBAGameState->GetTrainingDummyCharacterId();
}

bool AMOBAMMOGameMode::IsTrainingMinionSelected(const AMOBAMMOPlayerState* InstigatorState) const
{
    return InstigatorState
        && !InstigatorState->GetSelectedTargetCharacterId().IsEmpty()
        && InstigatorState->GetSelectedTargetCharacterId() == AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId();
}

void AMOBAMMOGameMode::InitializeDefaultAttributes(AMOBAMMOPlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return;
    }

    PlayerState->InitializeAttributes(DefaultMaxHealth, DefaultMaxMana);
}

bool AMOBAMMOGameMode::HasUsableSavedWorldPosition(const AMOBAMMOPlayerState* PlayerState) const
{
    return PlayerState
        && PlayerState->HasPersistentCharacterSnapshot()
        && !PlayerState->HasConsumedPersistentSpawnLocation()
        && !PlayerState->GetSavedWorldPosition().IsNearlyZero(1.0);
}

bool AMOBAMMOGameMode::IsInsideArenaBounds(const FVector& Location) const
{
    return Location.X >= ArenaMinBounds.X && Location.X <= ArenaMaxBounds.X
        && Location.Y >= ArenaMinBounds.Y && Location.Y <= ArenaMaxBounds.Y
        && Location.Z >= ArenaMinBounds.Z && Location.Z <= ArenaMaxBounds.Z;
}

FVector AMOBAMMOGameMode::ClampToArenaBounds(const FVector& Location) const
{
    if (IsInsideArenaBounds(Location))
    {
        return Location;
    }

    if (Location.Z < ArenaMinBounds.Z)
    {
        return GetSafeArenaReturnLocation();
    }

    return FVector(
        FMath::Clamp(Location.X, ArenaMinBounds.X, ArenaMaxBounds.X),
        FMath::Clamp(Location.Y, ArenaMinBounds.Y, ArenaMaxBounds.Y),
        FMath::Clamp(Location.Z, ArenaMinBounds.Z, ArenaMaxBounds.Z)
    );
}

FVector AMOBAMMOGameMode::GetSafeArenaReturnLocation() const
{
    return FVector(
        FMath::Clamp(ArenaSafeReturnLocation.X, ArenaMinBounds.X, ArenaMaxBounds.X),
        FMath::Clamp(ArenaSafeReturnLocation.Y, ArenaMinBounds.Y, ArenaMaxBounds.Y),
        FMath::Clamp(ArenaSafeReturnLocation.Z, ArenaMinBounds.Z, ArenaMaxBounds.Z)
    );
}

void AMOBAMMOGameMode::ReturnControllerToArena(AController* Controller, const TCHAR* Reason)
{
    if (!HasAuthority())
    {
        return;
    }

    APawn* PlayerPawn = Controller ? Controller->GetPawn() : nullptr;
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(Controller);
    if (!PlayerPawn || !PlayerState)
    {
        return;
    }

    const FVector CurrentLocation = PlayerPawn->GetActorLocation();
    if (IsInsideArenaBounds(CurrentLocation))
    {
        return;
    }

    const FVector SafeLocation = ClampToArenaBounds(CurrentLocation);
    PlayerPawn->SetActorLocation(SafeLocation, false);
    if (ACharacter* CharacterPawn = Cast<ACharacter>(PlayerPawn))
    {
        if (UCharacterMovementComponent* MovementComponent = CharacterPawn->GetCharacterMovement())
        {
            MovementComponent->StopMovementImmediately();
        }
    }
    PlayerState->ApplyPersistentCharacterState(
        PlayerState->GetCharacterExperience(),
        SafeLocation,
        PlayerState->GetCurrentHealth(),
        PlayerState->GetMaxHealth(),
        PlayerState->GetCurrentMana(),
        PlayerState->GetMaxMana(),
        PlayerState->GetKills(),
        PlayerState->GetDeaths()
    );

    PushCombatLog(FString::Printf(TEXT("%s returned to the arena."), *PlayerState->GetPlayerName()));
    UE_LOG(LogTemp, Warning, TEXT("[Arena] Returned %s to safe bounds. Reason=%s From=%s To=%s"),
        *PlayerState->GetPlayerName(),
        Reason ? Reason : TEXT("unknown"),
        *CurrentLocation.ToCompactString(),
        *SafeLocation.ToCompactString());
    SavePlayerProgress(Controller);
}

void AMOBAMMOGameMode::RestorePlayerPawnToSavedLocation(AController* Controller)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(Controller);
    APawn* PlayerPawn = Controller ? Controller->GetPawn() : nullptr;
    if (!PlayerPawn)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Persistence] Restore skipped because pawn is not ready."));
        return;
    }

    if (!HasUsableSavedWorldPosition(PlayerState))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Persistence] Restore skipped because saved position is not usable."));
        return;
    }

    const FVector SavedPosition = ClampToArenaBounds(PlayerState->GetSavedWorldPosition());
    PlayerPawn->SetActorLocation(SavedPosition, false);
    PlayerState->MarkPersistentSpawnLocationConsumed();
    UE_LOG(LogTemp, Log, TEXT("[Persistence] Restored player pawn to saved position %s."), *SavedPosition.ToCompactString());
}

void AMOBAMMOGameMode::StartArenaSafetyLoop()
{
    if (!HasAuthority() || ArenaSafetyCheckInterval <= 0.0f)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetTimerManager().IsTimerActive(ArenaSafetyTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        ArenaSafetyTimerHandle,
        this,
        &AMOBAMMOGameMode::EnforceArenaBoundsForAllPlayers,
        FMath::Max(0.1f, ArenaSafetyCheckInterval),
        true
    );
}

void AMOBAMMOGameMode::StartPlayerAutoSaveLoop()
{
    if (!HasAuthority() || PlayerAutoSaveInterval <= 0.0f)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetTimerManager().IsTimerActive(PlayerAutoSaveTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        PlayerAutoSaveTimerHandle,
        this,
        &AMOBAMMOGameMode::SaveAllActivePlayers,
        FMath::Max(1.0f, PlayerAutoSaveInterval),
        true
    );
}

void AMOBAMMOGameMode::StartPlayerSessionHeartbeatLoop()
{
    if (!HasAuthority() || PlayerSessionHeartbeatInterval <= 0.0f)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetTimerManager().IsTimerActive(PlayerSessionHeartbeatTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        PlayerSessionHeartbeatTimerHandle,
        this,
        &AMOBAMMOGameMode::HeartbeatAllActivePlayers,
        FMath::Max(2.0f, PlayerSessionHeartbeatInterval),
        true
    );
}

void AMOBAMMOGameMode::SaveAllActivePlayers()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        SavePlayerProgress(It->Get());
    }
}

void AMOBAMMOGameMode::HeartbeatAllActivePlayers()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        HeartbeatPlayerSession(It->Get());
    }
}

void AMOBAMMOGameMode::EnforceArenaBoundsForAllPlayers()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ReturnControllerToArena(It->Get(), TEXT("bounds"));
    }
}

void AMOBAMMOGameMode::SavePlayerProgress(AController* Controller) const
{
    if (!HasAuthority())
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance ? GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>() : nullptr;
    if (!BackendSubsystem)
    {
        return;
    }

    BackendSubsystem->SaveCurrentCharacterProgress(PlayerController);
}

void AMOBAMMOGameMode::HeartbeatPlayerSession(AController* Controller) const
{
    if (!HasAuthority())
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance ? GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>() : nullptr;
    if (!BackendSubsystem)
    {
        return;
    }

    BackendSubsystem->HeartbeatCurrentCharacterSession(PlayerController);
}

AController* AMOBAMMOGameMode::ResolveDebugTarget(AController* InstigatorController) const
{
    for (APlayerState* IteratedPlayerState : GameState->PlayerArray)
    {
        if (!IsValid(IteratedPlayerState))
        {
            continue;
        }

        if (AController* CandidateController = Cast<AController>(IteratedPlayerState->GetOwner()))
        {
            if (CandidateController != InstigatorController)
            {
                return CandidateController;
            }
        }
    }

    return InstigatorController;
}

bool AMOBAMMOGameMode::IsControllerInAbilityRange(const AController* SourceController, const AController* TargetController, float AbilityRange) const
{
    if (!IsValid(SourceController) || !IsValid(TargetController) || AbilityRange <= 0.0f)
    {
        return false;
    }

    const APawn* SourcePawn = SourceController->GetPawn();
    const APawn* TargetPawn = TargetController->GetPawn();
    if (!IsValid(SourcePawn) || !IsValid(TargetPawn))
    {
        return false;
    }

    return FVector::DistSquared(SourcePawn->GetActorLocation(), TargetPawn->GetActorLocation()) <= FMath::Square(AbilityRange);
}

void AMOBAMMOGameMode::PushCombatLog(const FString& CombatLog) const
{
    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        MOBAGameState->SetLastCombatLog(CombatLog);
        MOBAGameState->PushCombatFeedEntry(CombatLog);
    }
}
