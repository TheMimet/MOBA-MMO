#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOBAMMOFactionTypes.h"
#include "MOBAMMOAICharacter.generated.h"

class UHealthComponent;

// ── Death broadcast ───────────────────────────────────────────────────────────
// Fires in AMOBAMMOAICharacter::OnDeath so listeners (e.g. AMOBAMMOMobSpawner)
// can react without coupling to the delegate on UHealthComponent.
// InstigatorActor is the pawn / actor that dealt the killing blow.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAICharacterDeath,
    class AMOBAMMOAICharacter* /* Enemy */,
    AActor*                     /* InstigatorActor */);

UCLASS()
class MOBAMMOAI_API AMOBAMMOAICharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMOBAMMOAICharacter();

    UFUNCTION(BlueprintCallable, Category="Health")
    UHealthComponent* GetHealthComponent() const { return HealthComponent; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    class UBehaviorTree* BehaviorTree;

    // ── Faction — determines combat relationship with players / other actors ──
    // Defaults to Hostile so spawner enemies attack players automatically.
    // Override in Blueprint or AMOBAMMOMobSpawner to change per-group faction.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Faction")
    EMOBAMMOFaction Faction = EMOBAMMOFaction::Hostile;

    // ── Reward values (set per-enemy Blueprint or subclass) ───────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Reward")
    int32 XPReward = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Reward")
    int32 GoldReward = 5;

    // ── Identity helpers (override in concrete subclasses) ────────────────────
    virtual FString GetEnemyTypeId()      const { return TEXT("enemy"); }
    virtual FString GetEnemyDisplayName() const { return TEXT("Enemy"); }

    // ── Non-dynamic multicast — bound by AMOBAMMOMobSpawner and other systems ─
    FOnAICharacterDeath OnAIDied;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
    UHealthComponent* HealthComponent;

    UFUNCTION()
    virtual void OnDeath(UHealthComponent* HealthComp, AActor* InstigatorActor);
};
