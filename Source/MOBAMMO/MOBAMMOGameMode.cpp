#include "MOBAMMOGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MOBAMMOAbilitySet.h"
#include "MOBAMMOCharacter.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOTrainingDummyActor.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"

namespace
{
    FString ReadOption(const FString& Options, const TCHAR* Key)
    {
        return UGameplayStatics::ParseOption(Options, Key);
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
    ApplyPlayerSessionData(NewPlayerController, Options);
    return ErrorMessage;
}

void AMOBAMMOGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UpdateConnectedPlayerCount();

    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        MOBAGameState->InitializeTrainingDummy();
    }

    EnsureTrainingDummyActor();
    EnsureTrainingMinionActor();

    if (AMOBAMMOPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AMOBAMMOPlayerState>() : nullptr)
    {
        if (PlayerState->GetMaxHealth() <= 0.0f || PlayerState->GetCurrentHealth() <= 0.0f)
        {
            InitializeDefaultAttributes(PlayerState);
        }
    }
}

void AMOBAMMOGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    UpdateConnectedPlayerCount();
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

    return true;
}

bool AMOBAMMOGameMode::ApplyDamageToPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    return PlayerState->ApplyDamage(Amount) > 0.0f;
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
    InitializeDefaultAttributes(PlayerState);
    PushCombatLog(FString::Printf(TEXT("%s used %s and respawned with full attributes."), *PlayerState->GetPlayerName(), *MOBAMMOAbilitySet::Reforge().Name));
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
    return ManaRecovered > 0.0f || DrainedMana > 0.0f;
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
        return;
    }

    const FString AccountId = ReadOption(Options, TEXT("AccountId"));
    const FString CharacterId = ReadOption(Options, TEXT("CharacterId"));
    const FString CharacterName = ReadOption(Options, TEXT("CharacterName"));
    const FString ClassId = ReadOption(Options, TEXT("ClassId"));
    const FString LevelString = ReadOption(Options, TEXT("Level"));
    const int32 CharacterLevel = LevelString.IsEmpty() ? 1 : FMath::Max(1, FCString::Atoi(*LevelString));

    MOBAPlayerState->ApplySessionIdentity(AccountId, CharacterId, CharacterName, ClassId, CharacterLevel);
    InitializeDefaultAttributes(MOBAPlayerState);
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
