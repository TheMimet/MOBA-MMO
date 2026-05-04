#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBAMMOLootDropActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

// ─────────────────────────────────────────────────────────────
// A short-lived world actor placed at an enemy's death position.
// Any player who enters the pickup radius (server-side overlap)
// automatically collects the item via GameMode::GrantLootPickup.
//
// The actor self-destructs after LifeSpan seconds so the world
// doesn't fill with uncollected loot.
// ─────────────────────────────────────────────────────────────
UCLASS()
class MOBAMMO_API AMOBAMMOLootDropActor : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOLootDropActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Server-side initialisation ───────────────────────────
    // Call immediately after spawning to configure the drop.
    void InitializeDrop(const FString& InItemId, int32 InQuantity, const FString& InDisplayName);

    // ── Configuration ────────────────────────────────────────
    // Seconds before the actor self-destructs if uncollected.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Loot")
    float LifeSpan = 60.0f;

    // Radius (cm) within which a player automatically picks up the drop.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Loot")
    float PickupRadius = 150.0f;

private:
    // ── Components ───────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Loot")
    TObjectPtr<USphereComponent> PickupSphere;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Loot")
    TObjectPtr<UStaticMeshComponent> GemMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Loot")
    TObjectPtr<UTextRenderComponent> LabelText;

    // ── Replicated drop payload ───────────────────────────────
    UPROPERTY(ReplicatedUsing=OnRep_DropPayload)
    FString ItemId;

    UPROPERTY(Replicated)
    int32 Quantity = 1;

    UPROPERTY(ReplicatedUsing=OnRep_DropPayload)
    FString DisplayName;

    UFUNCTION()
    void OnRep_DropPayload();

    // ── Pickup overlap ────────────────────────────────────────
    UFUNCTION()
    void HandlePickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                             bool bFromSweep, const FHitResult& SweepResult);

    void UpdateLabel();

    bool bPickedUp = false;

    // Bob / spin animation state (clients).
    float AnimTime = 0.0f;
    FVector BaseLocation = FVector::ZeroVector;
};
