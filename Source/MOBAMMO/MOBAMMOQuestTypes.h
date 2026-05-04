#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOQuestTypes.generated.h"

// ─────────────────────────────────────────────────────────────
// Quest type enum — determines which in-game event advances progress.
// ─────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EMOBAMMOQuestType : uint8
{
    KillTrainingDummy   UMETA(DisplayName="Kill Training Dummy"),
    KillTrainingMinion  UMETA(DisplayName="Kill Training Minion"),
    KillMob             UMETA(DisplayName="Kill Mob"),
    DealDamage          UMETA(DisplayName="Deal Damage"),
    BuyFromVendor       UMETA(DisplayName="Buy From Vendor"),
    UseAbility          UMETA(DisplayName="Use Ability"),
};

// ─────────────────────────────────────────────────────────────
// Static quest template (lives in the catalog, never replicated).
// ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOQuestDefinition
{
    GENERATED_BODY()

    // Unique identifier used as the key for progress tracking.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FString QuestId;

    // Short player-facing label shown in the HUD tracker.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FString DisplayName;

    // Event type that advances this quest.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    EMOBAMMOQuestType Type = EMOBAMMOQuestType::KillTrainingDummy;

    // Number of events (or cumulative damage) needed to complete.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    int32 TargetCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    int32 RewardXP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    int32 RewardGold = 0;
};

// ─────────────────────────────────────────────────────────────
// Per-player runtime progress — replicated owner-only.
// ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOQuestProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    FString QuestId;

    // Accumulated units (kills, damage points, ability casts, etc.).
    UPROPERTY(BlueprintReadOnly, Category="Quest")
    int32 CurrentCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    bool bCompleted = false;
};
