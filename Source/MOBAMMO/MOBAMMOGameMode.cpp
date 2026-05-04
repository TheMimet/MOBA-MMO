#include "MOBAMMOGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MOBAMMOAbilitySet.h"
#include "MOBAMMOAICharacter.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOCharacter.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOSpawnPoint.h"
#include "MOBAMMOTrainingDummyActor.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "MOBAMMOVendorActor.h"
#include "MOBAMMOVendorCatalog.h"
#include "MOBAMMOQuestCatalog.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

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

    FString GetInventoryItemDisplayName(const FString& ItemId)
    {
        if (ItemId == TEXT("health_potion_small")) return TEXT("Small Health Potion");
        if (ItemId == TEXT("mana_potion_small")) return TEXT("Small Mana Potion");
        if (ItemId == TEXT("iron_sword")) return TEXT("Iron Sword");
        if (ItemId == TEXT("crystal_staff")) return TEXT("Crystal Staff");
        if (ItemId == TEXT("leather_helm")) return TEXT("Leather Helm");
        if (ItemId == TEXT("chain_body")) return TEXT("Chain Body");
        if (ItemId == TEXT("swift_boots")) return TEXT("Swift Boots");
        if (ItemId == TEXT("mana_ring")) return TEXT("Mana Ring");
        if (ItemId == TEXT("sparring_token")) return TEXT("Sparring Token");

        FString Name = ItemId;
        Name.ReplaceInline(TEXT("_"), TEXT(" "));
        return Name;
    }

    bool TryGetConsumableUseForItem(const FString& ItemId, float& OutHealthRestore, float& OutManaRestore)
    {
        OutHealthRestore = 0.0f;
        OutManaRestore = 0.0f;

        if (ItemId == TEXT("health_potion_small"))
        {
            OutHealthRestore = 25.0f;
            return true;
        }

        if (ItemId == TEXT("mana_potion_small"))
        {
            OutManaRestore = 20.0f;
            return true;
        }

        return false;
    }

    bool TryGetEquipSlotForItem(const FString& ItemId, int32& OutEquipSlot)
    {
        if (ItemId == TEXT("iron_sword") || ItemId == TEXT("crystal_staff"))
        {
            OutEquipSlot = 0;
            return true;
        }
        if (ItemId == TEXT("leather_helm"))
        {
            OutEquipSlot = 2;
            return true;
        }
        if (ItemId == TEXT("chain_body"))
        {
            OutEquipSlot = 3;
            return true;
        }
        if (ItemId == TEXT("swift_boots"))
        {
            OutEquipSlot = 4;
            return true;
        }
        if (ItemId == TEXT("mana_ring"))
        {
            OutEquipSlot = 5;
            return true;
        }

        return false;
    }

    TArray<FMOBAMMOAbilityDefinition> GetAbilityDefinitionsForState(const AMOBAMMOPlayerState* PlayerState)
    {
        return MOBAMMOAbilitySet::ForClass(PlayerState ? PlayerState->GetClassId() : FString());
    }

    FMOBAMMOAbilityDefinition GetAbilityDefinitionForState(const AMOBAMMOPlayerState* PlayerState, int32 AbilityIndex, const FMOBAMMOAbilityDefinition& Fallback)
    {
        const TArray<FMOBAMMOAbilityDefinition> Definitions = GetAbilityDefinitionsForState(PlayerState);
        return Definitions.IsValidIndex(AbilityIndex) ? Definitions[AbilityIndex] : Fallback;
    }
}

AMOBAMMOGameMode::AMOBAMMOGameMode()
{
    GameStateClass = AMOBAMMOGameState::StaticClass();
    PlayerStateClass = AMOBAMMOPlayerState::StaticClass();
    PlayerControllerClass = AMOBAMMOPlayerController::StaticClass();

    DefaultPawnClass = AMOBAMMOCharacter::StaticClass();
}

void AMOBAMMOGameMode::BeginPlay()
{
    Super::BeginPlay();
    CollectSpawnPoints();
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

    if (!HasActiveSession(NewPlayer))
    {
        return;
    }

    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        MOBAGameState->InitializeTrainingDummy();
    }

    EnsureTrainingDummyActor();
    EnsureTrainingMinionActor();
    EnsureVendorActor();

    if (AMOBAMMOPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AMOBAMMOPlayerState>() : nullptr)
    {
        if (!PlayerState->HasPersistentCharacterSnapshot() && (PlayerState->GetMaxHealth() <= 0.0f || PlayerState->GetCurrentHealth() <= 0.0f))
        {
            InitializeDefaultAttributes(PlayerState);
        }

        // Assign fresh session quests every time a player joins.
        PlayerState->AssignStartingQuests();
    }

    RestorePlayerPawnToSavedLocation(NewPlayer);
    StartArenaSafetyLoop();
    StartPlayerAutoSaveLoop();
    StartPlayerSessionHeartbeatLoop();
    StartStatusEffectTickLoop();
}

void AMOBAMMOGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Fire final-save for any players still connected when the server world ends.
    // In normal operation Logout() already handles per-player disconnect; this
    // catches graceful server shutdown where Logout may not be called first.
    if (HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>())
                {
                    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                    {
                        if (APlayerController* PC = Cast<APlayerController>(It->Get()))
                        {
                            BackendSubsystem->EndCurrentCharacterSession(PC);
                        }
                    }
                }
            }
        }
    }

    Super::EndPlay(EndPlayReason);
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
    if (!HasActiveSession(NewPlayer))
    {
        return;
    }

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

void AMOBAMMOGameMode::EnsureVendorActor()
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(VendorActor))
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

    VendorActor = World->SpawnActor<AMOBAMMOVendorActor>(
        AMOBAMMOVendorActor::StaticClass(),
        VendorSpawnLocation,
        VendorSpawnRotation,
        SpawnParams
    );

    if (VendorActor)
    {
        PushCombatLog(TEXT("The Vendor has set up shop. Press V to browse wares."));
    }
}

bool AMOBAMMOGameMode::HandleVendorPurchase(AController* BuyerController, const FString& ItemId)
{
    if (!BuyerController)
    {
        return false;
    }

    AMOBAMMOPlayerState* BuyerState = ResolveMOBAPlayerState(BuyerController);
    if (!BuyerState)
    {
        return false;
    }

    // Validate item exists in catalog
    FMOBAMMOVendorItem CatalogEntry;
    if (!UMOBAMMOVendorCatalog::FindItem(ItemId, CatalogEntry))
    {
        PushCombatLog(FString::Printf(TEXT("%s: unknown item '%s'."), *BuyerState->GetCharacterName(), *ItemId));
        return false;
    }

    // Proximity check
    if (IsValid(VendorActor))
    {
        const APawn* Pawn = BuyerController->GetPawn();
        if (Pawn)
        {
            const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), VendorActor->GetActorLocation());
            if (DistSq > VendorPurchaseRange * VendorPurchaseRange)
            {
                PushCombatLog(FString::Printf(
                    TEXT("%s is too far from the Vendor to shop (range %.0f)."),
                    *BuyerState->GetCharacterName(),
                    FMath::Sqrt(DistSq)
                ));
                return false;
            }
        }
    }

    // Afford check
    if (BuyerState->GetGold() < CatalogEntry.Price)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s cannot afford %s (%d g needed, has %d g)."),
            *BuyerState->GetCharacterName(),
            *CatalogEntry.DisplayName,
            CatalogEntry.Price,
            BuyerState->GetGold()
        ));
        return false;
    }

    // Deduct gold
    const bool bSpent = BuyerState->SpendGold(CatalogEntry.Price);
    if (!bSpent)
    {
        return false;
    }

    // Grant item
    BuyerState->GrantItem(ItemId, 1);

    // Feedback
    PushCombatLog(FString::Printf(
        TEXT("%s purchased %s for %d g. Remaining gold: %d g."),
        *BuyerState->GetCharacterName(),
        *CatalogEntry.DisplayName,
        CatalogEntry.Price,
        BuyerState->GetGold()
    ));

    // Quest: buying from vendor
    NotifyQuestEvent(BuyerController, EMOBAMMOQuestType::BuyFromVendor);

    return true;
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

bool AMOBAMMOGameMode::HasActiveSession(AController* Controller) const
{
    const AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(Controller);
    return PlayerState && !PlayerState->GetSessionId().TrimStartAndEnd().IsEmpty() && !PlayerState->GetCharacterId().TrimStartAndEnd().IsEmpty();
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

    // Let the player's shield absorb what it can before raw damage lands.
    const float EffectiveAmount = PlayerState->AbsorbShieldDamage(Amount);
    if (EffectiveAmount <= 0.0f)
    {
        // Fully absorbed by shield — still report success so callers log the event.
        PushCombatLog(FString::Printf(TEXT("%s's shield absorbed %.0f damage!"),
            *PlayerState->GetPlayerName(), Amount));
        return true;
    }

    const bool bApplied = PlayerState->ApplyDamage(EffectiveAmount) > 0.0f;
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
    const FMOBAMMOAbilityDefinition RespawnAbility = GetAbilityDefinitionForState(PlayerState, 4, MOBAMMOAbilitySet::Reforge());
    PushCombatLog(FString::Printf(TEXT("%s used %s and respawned with full attributes."), *PlayerState->GetPlayerName(), *RespawnAbility.Name));
    SavePlayerProgress(TargetController);
    return true;
}

bool AMOBAMMOGameMode::CastDebugDamageSpell(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }
    const FMOBAMMOAbilityDefinition DamageAbility = GetAbilityDefinitionForState(InstigatorState, 0, MOBAMMOAbilitySet::ArcBurst());

    // Silence prevents ALL ability use.
    if (InstigatorState->IsSilenced())
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but is Silenced!"),
            *InstigatorState->GetPlayerName(), *DamageAbility.Name));
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
        const int32 DmgRank = InstigatorState->GetAbilityRank(0);
        const float ScaledDmgPower    = RankScaledPower(DebugDamageSpellPower, DmgRank);
        const float ScaledDmgCooldown = RankScaledCooldown(DebugDamageCooldownDuration, DmgRank);
        const float DamageManaCost = FMath::Max(0.0f, DebugDamageSpellManaCost - (bHasArcCharge ? DebugArcChargeManaDiscount : 0.0f));
        const float DamagePower = ScaledDmgPower + (bHasArcCharge ? DebugArcChargeBonusDamage : 0.0f);
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
        InstigatorState->StartDamageCooldown(ScaledDmgCooldown, ServerWorldTimeSeconds);
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
            GrantKillReward(InstigatorController, KillXPTrainingDummy, KillGoldTrainingDummy, MOBAGameState->GetTrainingDummyName());
            NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::KillTrainingDummy);
        }

        // DealDamage + UseAbility quest events
        if (DamageApplied > 0.0f)
        {
            NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::DealDamage, FMath::RoundToInt(DamageApplied));
        }
        NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::UseAbility);

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
        const int32 DmgRankM = InstigatorState->GetAbilityRank(0);
        const float ScaledDmgPowerM    = RankScaledPower(DebugDamageSpellPower, DmgRankM);
        const float ScaledDmgCooldownM = RankScaledCooldown(DebugDamageCooldownDuration, DmgRankM);
        const float DamageManaCost = FMath::Max(0.0f, DebugDamageSpellManaCost - (bHasArcCharge ? DebugArcChargeManaDiscount : 0.0f));
        const float DamagePower = ScaledDmgPowerM + (bHasArcCharge ? DebugArcChargeBonusDamage : 0.0f);
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
        InstigatorState->StartDamageCooldown(ScaledDmgCooldownM, ServerWorldTimeSeconds);
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
            GrantMinionLootToPlayer(InstigatorController);
            GrantKillReward(InstigatorController, KillXPTrainingMinion, KillGoldTrainingMinion, AMOBAMMOTrainingMinionActor::GetTrainingMinionName());
            NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::KillTrainingMinion);
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

        // DealDamage + UseAbility quest events
        if (DamageApplied > 0.0f)
        {
            NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::DealDamage, FMath::RoundToInt(DamageApplied));
        }
        NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::UseAbility);

        SavePlayerProgress(InstigatorController);
        return DamageApplied > 0.0f;
    }

    AController* TargetController = ResolveSelectedDebugTarget(InstigatorState, InstigatorController);
    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    if (!InstigatorState || !TargetState)
    {
        return false;
    }

    // ── Faction guard: block friendly-fire between Allied players ─
    if (!IsValidDamageTarget(InstigatorController, TargetController ? TargetController->GetPawn() : nullptr))
    {
        PushCombatLog(FString::Printf(
            TEXT("%s cannot target %s — they are on the same faction."),
            *InstigatorState->GetPlayerName(),
            *TargetState->GetPlayerName()
        ));
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
        GrantKillReward(InstigatorController, KillXPPlayer, KillGoldPlayer, TargetState->GetPlayerName());
    }
    InstigatorState->StartDamageCooldown(DebugDamageCooldownDuration, ServerWorldTimeSeconds);
    if (bHasArcCharge)
    {
        InstigatorState->StartArcCharge(0.0f, ServerWorldTimeSeconds);
    }

    // Rank 2+: apply Poison to the player target (3 damage/s for 6 s, scales with rank).
    const int32 DmgRankPvP = InstigatorState->GetAbilityRank(0);
    if (DmgRankPvP >= 2 && TargetState->IsAlive())
    {
        FMOBAMMOStatusEffect PoisonEffect;
        PoisonEffect.Type              = EMOBAMMOStatusEffectType::Poison;
        PoisonEffect.Magnitude         = 3.0f * (1.0f + (DmgRankPvP - 1) * 0.20f);
        PoisonEffect.TickInterval      = 1.0f;
        PoisonEffect.RemainingDuration = 6.0f;
        TargetState->ApplyStatusEffect(PoisonEffect);
        PushCombatLog(FString::Printf(TEXT("%s is poisoned! (%.0f dmg/s for 6s)"),
            *TargetState->GetPlayerName(), PoisonEffect.Magnitude));
    }

    SavePlayerProgress(InstigatorController);
    SavePlayerProgress(TargetController);
    return true;
}

bool AMOBAMMOGameMode::CastDebugHealSpell(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }
    const FMOBAMMOAbilityDefinition HealAbility = GetAbilityDefinitionForState(InstigatorState, 1, MOBAMMOAbilitySet::Renew());

    // Silence prevents ALL ability use.
    if (InstigatorState->IsSilenced())
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to cast %s but is Silenced!"),
            *InstigatorState->GetPlayerName(), *HealAbility.Name));
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

    const int32 HealRank = InstigatorState->GetAbilityRank(1);
    const float ScaledHealPower    = RankScaledPower(DebugHealSpellPower, HealRank);
    const float ScaledHealCooldown = RankScaledCooldown(DebugHealCooldownDuration, HealRank);
    const float HealingApplied = InstigatorState->ApplyHealing(ScaledHealPower);
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
    InstigatorState->StartHealCooldown(ScaledHealCooldown, ServerWorldTimeSeconds);

    // Rank 2+: also apply Regeneration (5 HP/s for 8 s scaled by rank).
    if (HealRank >= 2)
    {
        FMOBAMMOStatusEffect RegenEffect;
        RegenEffect.Type             = EMOBAMMOStatusEffectType::Regeneration;
        RegenEffect.Magnitude        = 5.0f * (1.0f + (HealRank - 1) * 0.20f); // scales with rank
        RegenEffect.TickInterval     = 1.0f;
        RegenEffect.RemainingDuration = 8.0f;
        InstigatorState->ApplyStatusEffect(RegenEffect);
        PushCombatLog(FString::Printf(TEXT("%s gained Regeneration (%.0f HP/s for 8s)."),
            *InstigatorState->GetPlayerName(), RegenEffect.Magnitude));
    }

    // Rank 4+: also apply a Shield bubble (absorbs 20 + 5*(rank-4) damage).
    if (HealRank >= 4)
    {
        FMOBAMMOStatusEffect ShieldEffect;
        ShieldEffect.Type             = EMOBAMMOStatusEffectType::Shield;
        ShieldEffect.Magnitude        = 20.0f + 5.0f * (HealRank - 4);
        ShieldEffect.RemainingDuration = 15.0f;
        InstigatorState->ApplyStatusEffect(ShieldEffect);
        PushCombatLog(FString::Printf(TEXT("%s gained a Shield (absorbs %.0f damage for 15s)."),
            *InstigatorState->GetPlayerName(), ShieldEffect.Magnitude));
    }

    NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::UseAbility);
    SavePlayerProgress(InstigatorController);
    return HealingApplied > 0.0f;
}

bool AMOBAMMOGameMode::TriggerDebugManaRestore(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }
    const FMOBAMMOAbilityDefinition UtilityAbility = GetAbilityDefinitionForState(InstigatorState, 3, MOBAMMOAbilitySet::ManaSurge());

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

    const int32 SurgeRank = InstigatorState->GetAbilityRank(3);
    const float ScaledSurgePower    = RankScaledPower(DebugManaRestoreAmount, SurgeRank);
    const float ScaledSurgeCooldown = RankScaledCooldown(DebugManaSurgeCooldownDuration, SurgeRank);
    const float ManaRestored = InstigatorState->RestoreMana(ScaledSurgePower);
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
    InstigatorState->StartManaSurgeCooldown(ScaledSurgeCooldown, ServerWorldTimeSeconds);
    NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::UseAbility);
    SavePlayerProgress(InstigatorController);
    return ManaRestored > 0.0f;
}

bool AMOBAMMOGameMode::CastDebugDrainSpell(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }
    const FMOBAMMOAbilityDefinition DrainAbility = GetAbilityDefinitionForState(InstigatorState, 2, MOBAMMOAbilitySet::DrainPulse());
    const FMOBAMMOAbilityDefinition DamageAbility = GetAbilityDefinitionForState(InstigatorState, 0, MOBAMMOAbilitySet::ArcBurst());

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
            TEXT("%s cast %s on %s, draining %.0f mana, restoring %.0f mana, and empowering %s."),
            *InstigatorState->GetPlayerName(),
            *DrainAbility.Name,
            *MOBAGameState->GetTrainingDummyName(),
            DrainedMana,
            ManaRecovered,
            *DamageAbility.Name
        ));
        NotifyQuestEvent(InstigatorController, EMOBAMMOQuestType::UseAbility);
        SavePlayerProgress(InstigatorController);
        return ManaRecovered > 0.0f || DrainedMana > 0.0f;
    }

    AController* TargetController = ResolveSelectedDebugTarget(InstigatorState, InstigatorController);
    AMOBAMMOPlayerState* TargetState = ResolveMOBAPlayerState(TargetController);
    if (!InstigatorState || !TargetState)
    {
        return false;
    }

    // ── Faction guard: drain only works on hostile targets ────────
    if (!IsValidDamageTarget(InstigatorController, TargetController ? TargetController->GetPawn() : nullptr))
    {
        PushCombatLog(FString::Printf(
            TEXT("%s cannot drain %s — they are on the same faction."),
            *InstigatorState->GetPlayerName(),
            *TargetState->GetPlayerName()
        ));
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
        TEXT("%s cast %s on %s, draining %.0f mana, restoring %.0f mana, and empowering %s."),
        *InstigatorState->GetPlayerName(),
        *DrainAbility.Name,
        *TargetState->GetPlayerName(),
        DrainedMana,
        ManaRecovered,
        *DamageAbility.Name
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

bool AMOBAMMOGameMode::UseFirstInventoryConsumable(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    FString SelectedItemId;
    float SelectedHealthRestore = 0.0f;
    float SelectedManaRestore = 0.0f;

    for (const FMOBAMMOInventoryItem& Item : InstigatorState->GetInventoryItems())
    {
        float HealthRestore = 0.0f;
        float ManaRestore = 0.0f;
        if (!TryGetConsumableUseForItem(Item.ItemId, HealthRestore, ManaRestore))
        {
            continue;
        }

        const bool bCanRestoreHealth = HealthRestore > 0.0f && InstigatorState->GetCurrentHealth() < InstigatorState->GetMaxHealth();
        const bool bCanRestoreMana = ManaRestore > 0.0f && InstigatorState->GetCurrentMana() < InstigatorState->GetMaxMana();
        if (!bCanRestoreHealth && !bCanRestoreMana)
        {
            continue;
        }

        SelectedItemId = Item.ItemId;
        SelectedHealthRestore = HealthRestore;
        SelectedManaRestore = ManaRestore;
        break;
    }

    if (SelectedItemId.IsEmpty())
    {
        PushCombatLog(FString::Printf(TEXT("%s has no useful consumable right now."), *InstigatorState->GetPlayerName()));
        return false;
    }

    const float HealthRecovered = SelectedHealthRestore > 0.0f ? InstigatorState->ApplyHealing(SelectedHealthRestore) : 0.0f;
    const float ManaRecovered = SelectedManaRestore > 0.0f ? InstigatorState->RestoreMana(SelectedManaRestore) : 0.0f;
    if (HealthRecovered <= 0.0f && ManaRecovered <= 0.0f)
    {
        return false;
    }

    if (!InstigatorState->ConsumeInventoryItem(SelectedItemId, 1))
    {
        return false;
    }

    const FString RestoreText = HealthRecovered > 0.0f
        ? FString::Printf(TEXT("+%.0f HP"), HealthRecovered)
        : FString::Printf(TEXT("+%.0f MP"), ManaRecovered);
    PushCombatLog(FString::Printf(TEXT("%s used %s (%s)."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(SelectedItemId), *RestoreText));
    SavePlayerProgress(InstigatorController);
    return true;
}

bool AMOBAMMOGameMode::UseInventoryItem(AController* InstigatorController, const FString& ItemId)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState || ItemId.IsEmpty())
    {
        return false;
    }

    const FMOBAMMOInventoryItem* InventoryItem = InstigatorState->GetInventoryItems().FindByPredicate([&ItemId](const FMOBAMMOInventoryItem& Item)
    {
        return Item.ItemId == ItemId && Item.Quantity > 0;
    });

    if (!InventoryItem)
    {
        PushCombatLog(FString::Printf(TEXT("%s tried to use an item that is no longer in inventory."), *InstigatorState->GetPlayerName()));
        return false;
    }

    float HealthRestore = 0.0f;
    float ManaRestore = 0.0f;
    if (TryGetConsumableUseForItem(ItemId, HealthRestore, ManaRestore))
    {
        const bool bCanRestoreHealth = HealthRestore > 0.0f && InstigatorState->GetCurrentHealth() < InstigatorState->GetMaxHealth();
        const bool bCanRestoreMana = ManaRestore > 0.0f && InstigatorState->GetCurrentMana() < InstigatorState->GetMaxMana();
        if (!bCanRestoreHealth && !bCanRestoreMana)
        {
            PushCombatLog(FString::Printf(TEXT("%s cannot use %s right now."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(ItemId)));
            return false;
        }

        const float HealthRecovered = HealthRestore > 0.0f ? InstigatorState->ApplyHealing(HealthRestore) : 0.0f;
        const float ManaRecovered = ManaRestore > 0.0f ? InstigatorState->RestoreMana(ManaRestore) : 0.0f;
        if (HealthRecovered <= 0.0f && ManaRecovered <= 0.0f)
        {
            return false;
        }

        if (!InstigatorState->ConsumeInventoryItem(ItemId, 1))
        {
            return false;
        }

        const FString RestoreText = HealthRecovered > 0.0f
            ? FString::Printf(TEXT("+%.0f HP"), HealthRecovered)
            : FString::Printf(TEXT("+%.0f MP"), ManaRecovered);
        PushCombatLog(FString::Printf(TEXT("%s used %s (%s)."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(ItemId), *RestoreText));
        SavePlayerProgress(InstigatorController);
        return true;
    }

    int32 EquipSlot = -1;
    if (TryGetEquipSlotForItem(ItemId, EquipSlot))
    {
        if (InventoryItem->SlotIndex == EquipSlot)
        {
            if (InstigatorState->UnequipInventoryItem(ItemId))
            {
                PushCombatLog(FString::Printf(TEXT("%s unequipped %s."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(ItemId)));
                SavePlayerProgress(InstigatorController);
                return true;
            }
            return false;
        }

        if (InstigatorState->EquipInventoryItem(ItemId, EquipSlot))
        {
            PushCombatLog(FString::Printf(TEXT("%s equipped %s."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(ItemId)));
            SavePlayerProgress(InstigatorController);
            return true;
        }
        return false;
    }

    PushCombatLog(FString::Printf(TEXT("%s cannot use %s."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(ItemId)));
    return false;
}

bool AMOBAMMOGameMode::ToggleEquipFirstInventoryItem(AController* InstigatorController)
{
    AMOBAMMOPlayerState* InstigatorState = ResolveMOBAPlayerState(InstigatorController);
    if (!InstigatorState)
    {
        return false;
    }

    for (const FMOBAMMOInventoryItem& Item : InstigatorState->GetInventoryItems())
    {
        int32 EquipSlot = -1;
        if (TryGetEquipSlotForItem(Item.ItemId, EquipSlot) && Item.SlotIndex != EquipSlot)
        {
            if (InstigatorState->EquipInventoryItem(Item.ItemId, EquipSlot))
            {
                PushCombatLog(FString::Printf(TEXT("%s equipped %s."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(Item.ItemId)));
                SavePlayerProgress(InstigatorController);
                return true;
            }
        }
    }

    for (const FMOBAMMOInventoryItem& Item : InstigatorState->GetInventoryItems())
    {
        int32 EquipSlot = -1;
        if (TryGetEquipSlotForItem(Item.ItemId, EquipSlot) && Item.SlotIndex == EquipSlot)
        {
            if (InstigatorState->UnequipInventoryItem(Item.ItemId))
            {
                PushCombatLog(FString::Printf(TEXT("%s unequipped %s."), *InstigatorState->GetPlayerName(), *GetInventoryItemDisplayName(Item.ItemId)));
                SavePlayerProgress(InstigatorController);
                return true;
            }
        }
    }

    PushCombatLog(FString::Printf(TEXT("%s has no equippable item."), *InstigatorState->GetPlayerName()));
    return false;
}

void AMOBAMMOGameMode::GrantKillExperience(AController* KillerController, int32 XPAmount, const FString& TargetName)
{
    GrantKillReward(KillerController, XPAmount, 0, TargetName);
}

void AMOBAMMOGameMode::GrantKillReward(AController* KillerController, int32 XPAmount, int32 GoldAmount, const FString& TargetName)
{
    AMOBAMMOPlayerState* KillerState = ResolveMOBAPlayerState(KillerController);
    if (!KillerState || (XPAmount <= 0 && GoldAmount <= 0))
    {
        return;
    }

    int32 LevelsGained = 0;
    if (XPAmount > 0 && !KillerState->IsMaxLevel())
    {
        LevelsGained = KillerState->GrantExperience(XPAmount);
    }

    int32 GoldGranted = 0;
    if (GoldAmount > 0)
    {
        GoldGranted = KillerState->GrantGold(GoldAmount);
    }

    // Combined reward feed entry
    if (XPAmount > 0 && GoldGranted > 0)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s defeated %s. (+%d XP, +%d gold)"),
            *KillerState->GetPlayerName(), *TargetName, XPAmount, GoldGranted));
    }
    else if (XPAmount > 0)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s gained %d XP for defeating %s."),
            *KillerState->GetPlayerName(), XPAmount, *TargetName));
    }
    else if (GoldGranted > 0)
    {
        PushCombatLog(FString::Printf(
            TEXT("%s looted %d gold from %s."),
            *KillerState->GetPlayerName(), GoldGranted, *TargetName));
    }

    if (LevelsGained > 0)
    {
        PushCombatLog(FString::Printf(
            TEXT("[LEVEL UP] %s reached Level %d! (+%d HP, +%d MP)"),
            *KillerState->GetPlayerName(),
            KillerState->GetCharacterLevel(),
            int32(LevelsGained * 15),
            int32(LevelsGained * 8)));
    }
}

void AMOBAMMOGameMode::GrantMinionLootToPlayer(AController* KillerController)
{
    AMOBAMMOPlayerState* KillerState = ResolveMOBAPlayerState(KillerController);
    if (!KillerState)
    {
        return;
    }

    KillerState->GrantItem(TEXT("sparring_token"), 1);
    PushCombatLog(FString::Printf(TEXT("%s looted 1x Sparring Token."), *KillerState->GetPlayerName()));

    if (FMath::RandRange(1, 100) <= 30)
    {
        KillerState->GrantItem(TEXT("health_potion_small"), 1);
        PushCombatLog(FString::Printf(TEXT("%s found 1x Small Health Potion."), *KillerState->GetPlayerName()));
    }
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
    int32 Gold = 0;
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
    TryReadIntOption(Options, TEXT("Gold"), Gold);
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
    MOBAPlayerState->ApplyPersistentCharacterState(CharacterExperience, SavedWorldPosition, CurrentHealth, MaxHealth, CurrentMana, MaxMana, KillCount, DeathCount, Gold);

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

// ─────────────────────────────────────────────────────────────
// Quest System
// ─────────────────────────────────────────────────────────────

void AMOBAMMOGameMode::NotifyQuestEvent(AController* Controller, EMOBAMMOQuestType Type, int32 Amount)
{
    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return;
    }

    const TArray<FString> Completed = PS->AdvanceQuestProgress(Type, Amount);
    for (const FString& QuestId : Completed)
    {
        GrantQuestReward(Controller, QuestId);
    }
}

// ─────────────────────────────────────────────────────────────
// Skill Point System
// ─────────────────────────────────────────────────────────────

float AMOBAMMOGameMode::RankScaledPower(float BasePower, int32 Rank) const
{
    // +20% of base power per rank above 1 (rank 5 → 180% of base).
    const int32 ClampedRank = FMath::Clamp(Rank, 1, AMOBAMMOPlayerState::MaxAbilityRank);
    return BasePower * (1.0f + (ClampedRank - 1) * 0.20f);
}

float AMOBAMMOGameMode::RankScaledCooldown(float BaseCooldown, int32 Rank) const
{
    // -10% cooldown per rank above 1 (rank 5 → 60% of base, min 0.5s).
    const int32 ClampedRank = FMath::Clamp(Rank, 1, AMOBAMMOPlayerState::MaxAbilityRank);
    return FMath::Max(0.5f, BaseCooldown * (1.0f - (ClampedRank - 1) * 0.10f));
}

bool AMOBAMMOGameMode::HandleSpendSkillPoint(AController* Controller, int32 AbilitySlotIndex)
{
    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return false;
    }

    if (!PS->CanUpgradeAbility(AbilitySlotIndex))
    {
        const FString Reason = (PS->GetSkillPoints() <= 0)
            ? TEXT("no skill points available")
            : TEXT("ability is already at max rank");
        PushCombatLog(FString::Printf(TEXT("%s tried to upgrade ability %d but %s."),
            *PS->GetPlayerName(), AbilitySlotIndex + 1, *Reason));
        return false;
    }

    PS->SpendSkillPoint(AbilitySlotIndex);

    PushCombatLog(FString::Printf(
        TEXT("[UPGRADE] %s upgraded Ability %d to Rank %d! (%d SP remaining)"),
        *PS->GetPlayerName(),
        AbilitySlotIndex + 1,
        PS->GetAbilityRank(AbilitySlotIndex),
        PS->GetSkillPoints()
    ));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Zone System
// ─────────────────────────────────────────────────────────────────────────────

void AMOBAMMOGameMode::CollectSpawnPoints()
{
    RegisteredSpawnPoints.Empty();

    for (TActorIterator<AMOBAMMOSpawnPoint> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && !It->bDisabled)
        {
            RegisteredSpawnPoints.Add(*It);
        }
    }

    // Sort ascending by Priority so FindBestSpawnPointForZone always picks
    // the lowest-priority-number (most preferred) first.
    RegisteredSpawnPoints.Sort([](const AMOBAMMOSpawnPoint& A, const AMOBAMMOSpawnPoint& B)
    {
        return A.Priority < B.Priority;
    });

    UE_LOG(LogTemp, Log, TEXT("[ZoneSystem] Collected %d spawn point(s)."),
        RegisteredSpawnPoints.Num());
}

void AMOBAMMOGameMode::HandlePlayerEnteredZone(AController* Controller,
                                               const FMOBAMMOZoneInfo& ZoneInfo)
{
    if (!Controller || !ZoneInfo.IsValid())
    {
        return;
    }

    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return;
    }

    // Skip redundant enter events (e.g. overlapping two volumes of the same zone).
    if (PS->GetCurrentZoneId() == ZoneInfo.ZoneId)
    {
        return;
    }

    PS->SetCurrentZoneId(ZoneInfo.ZoneId);

    const FString ZoneName = ZoneInfo.DisplayName.IsEmpty()
        ? ZoneInfo.ZoneId.ToString()
        : ZoneInfo.DisplayName.ToString();

    PushCombatLog(FString::Printf(
        TEXT("[Zone] %s entered %s."),
        *PS->GetPlayerName(),
        *ZoneName
    ));

    UE_LOG(LogTemp, Log, TEXT("[ZoneSystem] %s entered zone '%s'."),
        *PS->GetPlayerName(), *ZoneInfo.ZoneId.ToString());
}

void AMOBAMMOGameMode::HandlePlayerExitedZone(AController* Controller, FName ZoneId)
{
    if (!Controller)
    {
        return;
    }

    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return;
    }

    // Only clear if we're actually leaving the zone we think we're in.
    if (PS->GetCurrentZoneId() != ZoneId)
    {
        return;
    }

    PS->SetCurrentZoneId(NAME_None);

    UE_LOG(LogTemp, Log, TEXT("[ZoneSystem] %s exited zone '%s'."),
        *PS->GetPlayerName(), *ZoneId.ToString());
}

AActor* AMOBAMMOGameMode::FindBestSpawnPointForZone(FName ZoneId) const
{
    // First pass: exact zone match.
    for (AMOBAMMOSpawnPoint* SP : RegisteredSpawnPoints)
    {
        if (IsValid(SP) && !SP->bDisabled && SP->ZoneId == ZoneId)
        {
            return SP;
        }
    }

    // Second pass: global fallback (ZoneId == NAME_None).
    for (AMOBAMMOSpawnPoint* SP : RegisteredSpawnPoints)
    {
        if (IsValid(SP) && !SP->bDisabled && SP->ZoneId == NAME_None)
        {
            return SP;
        }
    }

    return nullptr;
}

FName AMOBAMMOGameMode::GetControllerCurrentZone(const AController* Controller) const
{
    if (!Controller)
    {
        return NAME_None;
    }
    if (const AMOBAMMOPlayerState* PS = Controller->GetPlayerState<AMOBAMMOPlayerState>())
    {
        return PS->GetCurrentZoneId();
    }
    return NAME_None;
}

AActor* AMOBAMMOGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    // Try to find a spawn point matching the player's current zone.
    const FName ZoneId = GetControllerCurrentZone(Player);
    if (AActor* ZoneSpawn = FindBestSpawnPointForZone(ZoneId))
    {
        return ZoneSpawn;
    }

    // No custom spawn points registered — fall back to engine default
    // (PlayerStart actors in the level).
    return Super::ChoosePlayerStart_Implementation(Player);
}

// ─────────────────────────────────────────────────────────────────────────────
// Faction Helpers
// ─────────────────────────────────────────────────────────────────────────────

EMOBAMMOFaction AMOBAMMOGameMode::GetActorFaction(const AActor* Actor) const
{
    if (!Actor)
    {
        return EMOBAMMOFaction::Neutral;
    }

    // Player characters: read from PlayerState (replicated).
    if (const ACharacter* Character = Cast<ACharacter>(Actor))
    {
        if (const AController* Ctrl = Character->GetController())
        {
            if (const AMOBAMMOPlayerState* PS = Ctrl->GetPlayerState<AMOBAMMOPlayerState>())
            {
                return PS->GetFaction();
            }
        }
    }

    // AI enemies: faction stored directly on the character.
    if (const AMOBAMMOAICharacter* AIChar = Cast<AMOBAMMOAICharacter>(Actor))
    {
        return AIChar->Faction;
    }

    // Fallback — training dummies, props, loot actors, etc.
    return EMOBAMMOFaction::Neutral;
}

bool AMOBAMMOGameMode::AreActorsHostile(const AActor* ActorA, const AActor* ActorB) const
{
    return AreFactionsHostile(GetActorFaction(ActorA), GetActorFaction(ActorB));
}

bool AMOBAMMOGameMode::IsValidDamageTarget(const AController* InstigatorController,
                                           const AActor* Target) const
{
    if (!InstigatorController || !Target)
    {
        return false;
    }
    const APawn* InstigatorPawn = InstigatorController->GetPawn();
    return AreActorsHostile(InstigatorPawn, Target);
}

void AMOBAMMOGameMode::NotifyEnemyKilled(AController* KillerController, int32 XP, int32 Gold,
                                         const FString& EnemyName, EMOBAMMOQuestType QuestType)
{
    if (!KillerController)
    {
        return;
    }

    GrantKillReward(KillerController, XP, Gold, EnemyName);
    NotifyQuestEvent(KillerController, QuestType, 1);
}

bool AMOBAMMOGameMode::GrantLootPickup(AController* PlayerController, const FString& ItemId,
                                       int32 Quantity, const FString& SourceName)
{
    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(PlayerController);
    if (!PS || ItemId.IsEmpty() || Quantity <= 0)
    {
        return false;
    }

    PS->GrantItem(ItemId, Quantity);

    const FString Label = SourceName.IsEmpty() ? ItemId : SourceName;
    PushCombatLog(FString::Printf(TEXT("[LOOT] %s picked up %dx %s."),
        *PS->GetPlayerName(), Quantity, *Label));

    // Persist immediately so the item survives a disconnect.
    SavePlayerProgress(PlayerController);
    return true;
}

void AMOBAMMOGameMode::GrantQuestReward(AController* Controller, const FString& QuestId)
{
    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return;
    }

    FMOBAMMOQuestDefinition Def;
    if (!UMOBAMMOQuestCatalog::FindQuest(QuestId, Def))
    {
        return;
    }

    int32 LevelsGained = 0;
    if (Def.RewardXP > 0 && !PS->IsMaxLevel())
    {
        LevelsGained = PS->GrantExperience(Def.RewardXP);
    }

    if (Def.RewardGold > 0)
    {
        PS->GrantGold(Def.RewardGold);
    }

    FString RewardMsg = FString::Printf(
        TEXT("[QUEST] %s completed \"%s\"!  +%d XP"),
        *PS->GetPlayerName(), *Def.DisplayName, Def.RewardXP);

    if (Def.RewardGold > 0)
    {
        RewardMsg += FString::Printf(TEXT("  +%d Gold"), Def.RewardGold);
    }

    if (LevelsGained > 0)
    {
        RewardMsg += FString::Printf(TEXT("  [LEVEL UP → %d]"), PS->GetCharacterLevel());
    }

    PushCombatLog(RewardMsg);
}

// ─────────────────────────────────────────────────────────────
// Status Effect Tick Loop
// ─────────────────────────────────────────────────────────────

void AMOBAMMOGameMode::StartStatusEffectTickLoop()
{
    if (!HasAuthority())
    {
        return;
    }

    // Guard: don't start a second timer if one is already running.
    if (GetWorldTimerManager().IsTimerActive(StatusEffectTickTimerHandle))
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        StatusEffectTickTimerHandle,
        this,
        &AMOBAMMOGameMode::TickAllStatusEffects,
        StatusEffectTickInterval,
        /*bLoop=*/true
    );
}

void AMOBAMMOGameMode::TickAllStatusEffects()
{
    if (!HasAuthority())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AController* Controller = It->Get();
        if (Controller && HasActiveSession(Controller))
        {
            TickStatusEffectsForPlayer(Controller, StatusEffectTickInterval);
        }
    }
}

void AMOBAMMOGameMode::TickStatusEffectsForPlayer(AController* Controller, float DeltaTime)
{
    AMOBAMMOPlayerState* PS = ResolveMOBAPlayerState(Controller);
    if (!PS)
    {
        return;
    }

    // If dead, clear all effects so they don't carry over to respawn.
    if (!PS->IsAlive())
    {
        const TArray<FMOBAMMOStatusEffect> EffectsCopy = PS->GetActiveStatusEffects();
        for (const FMOBAMMOStatusEffect& E : EffectsCopy)
        {
            PS->RemoveStatusEffect(E.Type);
        }
        return;
    }

    if (PS->GetActiveStatusEffects().Num() == 0)
    {
        return;
    }

    float HealApplied   = 0.0f;
    float DamageApplied = 0.0f;
    TArray<EMOBAMMOStatusEffectType> ExpiredTypes;

    PS->TickAndApplyStatusEffects(DeltaTime, HealApplied, DamageApplied, ExpiredTypes);

    // Log regeneration tick.
    if (HealApplied > 0.0f)
    {
        PushCombatLog(FString::Printf(TEXT("[Regen] %s regenerated %.0f HP. HP %.0f/%.0f."),
            *PS->GetPlayerName(), HealApplied,
            PS->GetCurrentHealth(), PS->GetMaxHealth()));
    }

    // Log poison tick and handle possible death.
    if (DamageApplied > 0.0f)
    {
        PushCombatLog(FString::Printf(TEXT("[Poison] %s took %.0f poison damage. HP %.0f/%.0f."),
            *PS->GetPlayerName(), DamageApplied,
            PS->GetCurrentHealth(), PS->GetMaxHealth()));

        if (!PS->IsAlive())
        {
            PS->RecordDeath();
            const float ServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            PS->StartRespawnCooldown(DebugRespawnDelayDuration, ServerTime);
            PushCombatLog(FString::Printf(TEXT("%s succumbed to poison. Reforge in %.1fs."),
                *PS->GetPlayerName(), DebugRespawnDelayDuration));

            // Clear remaining effects on death.
            const TArray<FMOBAMMOStatusEffect> EffectsOnDeath = PS->GetActiveStatusEffects();
            for (const FMOBAMMOStatusEffect& E : EffectsOnDeath)
            {
                PS->RemoveStatusEffect(E.Type);
            }

            SavePlayerProgress(Controller);
            return;
        }
    }

    // Log expired effects.
    for (const EMOBAMMOStatusEffectType ExpiredType : ExpiredTypes)
    {
        const TCHAR* EffectName = TEXT("effect");
        switch (ExpiredType)
        {
            case EMOBAMMOStatusEffectType::Shield:       EffectName = TEXT("Shield");  break;
            case EMOBAMMOStatusEffectType::Regeneration: EffectName = TEXT("Regen");   break;
            case EMOBAMMOStatusEffectType::Poison:       EffectName = TEXT("Poison");  break;
            case EMOBAMMOStatusEffectType::Haste:        EffectName = TEXT("Haste");   break;
            case EMOBAMMOStatusEffectType::Silence:      EffectName = TEXT("Silence"); break;
        }
        PushCombatLog(FString::Printf(TEXT("[Status] %s's %s wore off."),
            *PS->GetPlayerName(), EffectName));
    }

    if (HealApplied > 0.0f || DamageApplied > 0.0f || ExpiredTypes.Num() > 0)
    {
        SavePlayerProgress(Controller);
    }
}
