#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBAMMOZoneTypes.h"
#include "MOBAMMOZoneVolume.generated.h"

class UBoxComponent;

// ─────────────────────────────────────────────────────────────────────────────
// AMOBAMMOZoneVolume
//
// Server-side trigger volume.  When an ACharacter overlaps it the volume
// notifies AMOBAMMOGameMode which updates the player's CurrentZoneId in
// AMOBAMMOPlayerState.
//
// Usage: place in the level, set ZoneInfo.ZoneId (e.g. "forest") and resize
// the box.  Overlapping player characters trigger GameMode::HandlePlayerEnteredZone.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class MOBAMMO_API AMOBAMMOZoneVolume : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOZoneVolume();

    // All zone metadata in one block — edit in the Details panel.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MOBAMMO|Zone")
    FMOBAMMOZoneInfo ZoneInfo;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MOBAMMO|Zone")
    TObjectPtr<UBoxComponent> ZoneBox;

private:
    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
                            AActor*              OtherActor,
                            UPrimitiveComponent* OtherComp,
                            int32                OtherBodyIndex,
                            bool                 bFromSweep,
                            const FHitResult&    SweepResult);

    UFUNCTION()
    void HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
                          AActor*              OtherActor,
                          UPrimitiveComponent* OtherComp,
                          int32                OtherBodyIndex);
};
