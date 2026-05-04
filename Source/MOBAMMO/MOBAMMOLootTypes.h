#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOLootTypes.generated.h"

// ─────────────────────────────────────────────────────────────
// One row in a loot table.
// Roll FMath::FRand() < DropChance to decide whether this row
// fires; quantity is then RandRange(Min, Max).
// ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOLootEntry
{
    GENERATED_BODY()

    // Inventory item ID (must match the vendor / item catalog).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot")
    FString ItemId;

    // Player-facing name shown on the pickup actor ("Health Potion", "Gold Coin" …).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot")
    FString DisplayName;

    // 0 = never drops; 1 = always drops.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    float DropChance = 1.0f;

    // Inclusive quantity range rolled on a successful drop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin="1"))
    int32 MinQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin="1"))
    int32 MaxQuantity = 1;
};
