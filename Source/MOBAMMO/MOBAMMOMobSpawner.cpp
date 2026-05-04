#include "MOBAMMOMobSpawner.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "MOBAMMOAICharacter.h"
#include "MOBAMMOGameMode.h"
#include "MOBAMMOLootDropActor.h"

AMOBAMMOMobSpawner::AMOBAMMOMobSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    // Spawner itself is not replicated — only the enemies it spawns are.
    bReplicates = false;
}

// ─────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────

void AMOBAMMOMobSpawner::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        SpawnInitialEnemies();
    }
}

void AMOBAMMOMobSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        for (FTimerHandle& Handle : RespawnHandles)
        {
            World->GetTimerManager().ClearTimer(Handle);
        }
    }
    RespawnHandles.Empty();

    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────
// Spawning
// ─────────────────────────────────────────────────────────────

void AMOBAMMOMobSpawner::SpawnInitialEnemies()
{
    for (int32 i = 0; i < MaxActive; ++i)
    {
        SpawnOneEnemy();
    }
}

void AMOBAMMOMobSpawner::SpawnOneEnemy()
{
    UWorld* World = GetWorld();
    if (!World || !EnemyClass)
    {
        return;
    }

    // Scatter enemies randomly within SpawnRadius on the XY plane.
    const FVector2D RandOffset  = FMath::RandPointInCircle(SpawnRadius);
    const FVector   SpawnOrigin = GetActorLocation();
    const FVector   SpawnLoc    = SpawnOrigin + FVector(RandOffset.X, RandOffset.Y, 0.0f);
    const FRotator  SpawnRot    = FRotator(0.0f, FMath::FRandRange(0.0f, 359.9f), 0.0f);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.Owner = this;

    AMOBAMMOAICharacter* Enemy =
        World->SpawnActor<AMOBAMMOAICharacter>(EnemyClass, SpawnLoc, SpawnRot, Params);

    if (!Enemy)
    {
        return;
    }

    // Propagate spawner faction so GameMode faction checks work correctly.
    Enemy->Faction = SpawnerFaction;

    SpawnedEnemies.Add(Enemy);

    // Bind to the death event so we can award kills and respawn.
    Enemy->OnAIDied.AddUObject(this, &AMOBAMMOMobSpawner::HandleEnemyDied);
}

// ─────────────────────────────────────────────────────────────
// Death callback
// ─────────────────────────────────────────────────────────────

void AMOBAMMOMobSpawner::HandleEnemyDied(AMOBAMMOAICharacter* Enemy, AActor* InstigatorActor)
{
    SpawnedEnemies.Remove(Enemy);

    // Resolve the killer's controller so GameMode can grant rewards.
    AController* KillerController = nullptr;
    if (const APawn* Pawn = Cast<APawn>(InstigatorActor))
    {
        KillerController = Pawn->GetController();
    }
    else if (AController* Ctrl = Cast<AController>(InstigatorActor))
    {
        KillerController = Ctrl;
    }

    if (KillerController && Enemy)
    {
        if (AMOBAMMOGameMode* GameMode = GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>())
        {
            GameMode->NotifyEnemyKilled(
                KillerController,
                Enemy->XPReward,
                Enemy->GoldReward,
                Enemy->GetEnemyDisplayName(),
                KillQuestType
            );
        }
    }

    // ── Loot drops ────────────────────────────────────────────
    if (!LootTable.IsEmpty() && Enemy && GetWorld())
    {
        const FVector DropBase = Enemy->GetActorLocation();
        for (const FMOBAMMOLootEntry& Entry : LootTable)
        {
            if (Entry.ItemId.IsEmpty() || Entry.DropChance <= 0.0f)
            {
                continue;
            }
            if (FMath::FRand() > Entry.DropChance)
            {
                continue; // Roll failed — skip this entry
            }

            const int32 DroppedQty = FMath::RandRange(
                FMath::Max(1, Entry.MinQuantity),
                FMath::Max(Entry.MinQuantity, Entry.MaxQuantity));

            // Scatter drops slightly so stacked items don't overlap.
            const FVector2D ScatterXY = FMath::RandPointInCircle(60.0f);
            const FVector DropLoc = DropBase + FVector(ScatterXY.X, ScatterXY.Y, 20.0f);

            FActorSpawnParameters DropParams;
            DropParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            if (AMOBAMMOLootDropActor* Drop = GetWorld()->SpawnActor<AMOBAMMOLootDropActor>(
                    AMOBAMMOLootDropActor::StaticClass(), DropLoc, FRotator::ZeroRotator, DropParams))
            {
                Drop->InitializeDrop(Entry.ItemId, DroppedQty, Entry.DisplayName);
            }
        }
    }

    if (bRespawn)
    {
        ScheduleRespawn();
    }
}

// ─────────────────────────────────────────────────────────────
// Respawn scheduling
// ─────────────────────────────────────────────────────────────

void AMOBAMMOMobSpawner::ScheduleRespawn()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTimerHandle Handle;
    FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        SpawnOneEnemy();
    });

    World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(0.1f, RespawnDelay), false);
    RespawnHandles.Add(Handle);
}
