#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBAMMOSpawnPoint.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// AMOBAMMOSpawnPoint
//
// Place in the level to mark a valid player spawn location.
// GameMode collects all live instances on BeginPlay and picks the best one
// for a given zone when restarting a player.
//
// Priority: lower number = picked first when multiple points share a zone.
// ZoneId    = NAME_None means "any zone / default spawn".
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class MOBAMMO_API AMOBAMMOSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOSpawnPoint();

    // Zone this spawn point belongs to.  NAME_None = global default.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Zone")
    FName ZoneId = NAME_None;

    // Lower priority value → preferred first.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Zone", meta=(ClampMin=0))
    int32 Priority = 0;

    // If true this point is temporarily disabled and GameMode will skip it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Zone")
    bool bDisabled = false;
};
