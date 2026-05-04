#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBAMMOFactionTypes.h"
#include "MOBAMMOLootTypes.h"
#include "MOBAMMOQuestTypes.h"
#include "MOBAMMOMobSpawner.generated.h"

class AMOBAMMOAICharacter;

// ─────────────────────────────────────────────────────────────
// Place in the level to manage a group of AI enemies.
// The spawner fills MaxActive slots on BeginPlay and respawns
// each dead enemy after RespawnDelay seconds.
//
// Set EnemyClass to any AMOBAMMOAICharacter subclass.
// The spawner automatically routes kill rewards through
// AMOBAMMOGameMode::NotifyEnemyKilled().
// ─────────────────────────────────────────────────────────────
UCLASS()
class MOBAMMO_API AMOBAMMOMobSpawner : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOMobSpawner();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Configuration ─────────────────────────────────────────
    // Concrete AMOBAMMOAICharacter subclass to spawn.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner")
    TSubclassOf<AMOBAMMOAICharacter> EnemyClass;

    // How many live enemies this spawner maintains at once.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner", meta=(ClampMin=1, ClampMax=20))
    int32 MaxActive = 3;

    // Random XY radius (cm) around the spawner's origin for each spawn point.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner", meta=(ClampMin=0))
    float SpawnRadius = 400.0f;

    // Seconds before a dead enemy slot is refilled.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner", meta=(ClampMin=0))
    float RespawnDelay = 10.0f;

    // If false the spawner does not refill dead slots.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner")
    bool bRespawn = true;

    // Quest type reported to GameMode when any enemy from this spawner is killed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Spawner")
    EMOBAMMOQuestType KillQuestType = EMOBAMMOQuestType::KillMob;

    // Faction assigned to every enemy this spawner creates.
    // Hostile (default) means spawned enemies attack Allied player characters.
    // Change to Allied for friendly escort NPCs, Neutral for passive creatures.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Faction")
    EMOBAMMOFaction SpawnerFaction = EMOBAMMOFaction::Hostile;

    // Items this spawner's enemies can drop.  Each entry is rolled independently.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Loot")
    TArray<FMOBAMMOLootEntry> LootTable;

private:
    void SpawnInitialEnemies();
    void SpawnOneEnemy();
    void HandleEnemyDied(AMOBAMMOAICharacter* Enemy, AActor* InstigatorActor);
    void ScheduleRespawn();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AMOBAMMOAICharacter>> SpawnedEnemies;

    // One handle per pending respawn; cleared in EndPlay.
    TArray<FTimerHandle> RespawnHandles;
};
