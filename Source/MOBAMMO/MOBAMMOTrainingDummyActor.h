#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MOBAMMOTrainingDummyActor.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class MOBAMMO_API AMOBAMMOTrainingDummyActor : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOTrainingDummyActor();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void RefreshFromGameState(float DeltaSeconds);
    void FaceTextTowardLocalCamera() const;
    void ShowFloatingFeedback(const FString& Message, const FColor& Color);

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UStaticMeshComponent> TargetRingMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UStaticMeshComponent> BeaconMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UTextRenderComponent> NameplateText;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Training")
    TObjectPtr<UTextRenderComponent> FloatingFeedbackText;

    FVector BaseBodyScale = FVector(0.75f, 0.75f, 1.85f);
    FVector BaseRingScale = FVector(2.4f, 2.4f, 0.04f);
    FVector BaseBeaconScale = FVector(0.35f, 0.35f, 0.35f);
    float LastObservedHealth = -1.0f;
    float LastObservedMana = -1.0f;
    float HitPulseRemaining = 0.0f;
    float DrainPulseRemaining = 0.0f;
    float RestorePulseRemaining = 0.0f;
    float FloatingFeedbackRemaining = 0.0f;
    FVector FloatingFeedbackBaseLocation = FVector(0.0f, 0.0f, 310.0f);
};
