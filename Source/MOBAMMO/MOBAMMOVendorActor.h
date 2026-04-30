#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBAMMOVendorActor.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class MOBAMMO_API AMOBAMMOVendorActor : public AActor
{
    GENERATED_BODY()

public:
    AMOBAMMOVendorActor();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void FaceTextTowardLocalCamera() const;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Vendor")
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Vendor")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Vendor")
    TObjectPtr<UStaticMeshComponent> GlowMesh;

    UPROPERTY(VisibleAnywhere, Category="MOBAMMO|Vendor")
    TObjectPtr<UTextRenderComponent> NameplateText;

    FVector BaseBodyScale  = FVector(0.75f, 0.75f, 1.85f);
    FVector BaseGlowScale  = FVector(2.2f, 2.2f, 0.04f);
};
