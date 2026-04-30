#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOStatusTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Status effect types available in the game
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EMOBAMMOStatusEffectType : uint8
{
    Shield        UMETA(DisplayName = "Shield"),        // Absorbs incoming damage
    Regeneration  UMETA(DisplayName = "Regeneration"),  // Heals HP over time
    Poison        UMETA(DisplayName = "Poison"),        // Deals damage over time
    Haste         UMETA(DisplayName = "Haste"),         // Reduces ability cooldowns
    Silence       UMETA(DisplayName = "Silence"),       // Prevents ability use
};

// ─────────────────────────────────────────────────────────────────────────────
// A single active status effect on a player
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOStatusEffect
{
    GENERATED_BODY()

    // Which kind of effect this is
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    EMOBAMMOStatusEffectType Type = EMOBAMMOStatusEffectType::Shield;

    // Seconds remaining before this effect expires
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float RemainingDuration = 0.0f;

    // For Shield: HP remaining in the shield bubble
    // For Regen/Poison: HP healed/dealt per tick
    // For Haste: cooldown multiplier reduction (e.g. 0.30 = 30% faster)
    // For Silence: unused (effect is presence/absence)
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float Magnitude = 0.0f;

    // Seconds between ticks (for Regen and Poison)
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float TickInterval = 1.0f;

    // Seconds since last tick fired (server-side only; not replicated)
    UPROPERTY(NotReplicated)
    float TimeSinceLastTick = 0.0f;
};
