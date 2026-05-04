#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOFactionTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EMOBAMMOFaction
//
// Faction tag applied to every combat participant (players, AI enemies, neutral
// objects).  Damage and ability targeting decisions are gated on faction
// relationships so that friendly-fire is impossible between Allied actors and
// hostile interactions are enforced between Allied and Hostile actors.
//
// Neutral actors (training dummies, environmental hazards) can be hit by any
// faction.  Two Hostile actors do not damage each other by default — they only
// engage Allied targets.
//
//   Allied  → player characters (default)
//   Hostile → spawner enemies (default)
//   Neutral → training dummies, destructible props, loot actors
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EMOBAMMOFaction : uint8
{
    Allied  UMETA(DisplayName = "Allied"),
    Hostile UMETA(DisplayName = "Hostile"),
    Neutral UMETA(DisplayName = "Neutral"),
};

// Returns true when ActorA and ActorB should be able to harm one another.
// Allied–Allied  → false (no friendly fire)
// Hostile–Allied → true  (enemy attacks player)
// Neutral–*      → true  (neutral can always be targeted)
// *–Neutral      → true  (anyone can hit neutral objects)
FORCEINLINE bool AreFactionsHostile(EMOBAMMOFaction A, EMOBAMMOFaction B)
{
    if (A == EMOBAMMOFaction::Neutral || B == EMOBAMMOFaction::Neutral)
    {
        return true;
    }
    return A != B;
}
