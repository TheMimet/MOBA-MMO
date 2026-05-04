#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOZoneTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// FMOBAMMOZoneInfo
//
// Lightweight descriptor attached to a zone volume and broadcast whenever a
// player enters / exits the zone.  Kept as a plain struct so it can travel
// through multicast delegates without UObject overhead.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOZoneInfo
{
    GENERATED_BODY()

    // Unique machine-readable identifier, e.g. "arena", "forest", "dungeon".
    // Used by GameMode to look up spawn points and quest triggers.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    FName ZoneId = NAME_None;

    // Human-readable name shown on HUD when entering the zone.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    FText DisplayName;

    // Whether the zone allows PvP combat (overrides faction rules when false).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    bool bAllowPvP = false;

    // Optional safe-return location used if a player dies with no spawn point.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    FVector SafeLocation = FVector::ZeroVector;

    bool IsValid() const { return ZoneId != NAME_None; }
};
