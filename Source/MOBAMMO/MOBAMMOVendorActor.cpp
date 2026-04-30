#include "MOBAMMOVendorActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AMOBAMMOVendorActor::AMOBAMMOVendorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
    CapsuleComponent->InitCapsuleSize(52.0f, 120.0f);
    CapsuleComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    RootComponent = CapsuleComponent;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(RootComponent);
    BodyMesh->SetRelativeScale3D(BaseBodyScale);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(CylinderMesh.Object);
    }

    GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
    GlowMesh->SetupAttachment(RootComponent);
    GlowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
    GlowMesh->SetRelativeScale3D(BaseGlowScale);
    GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (CylinderMesh.Succeeded())
    {
        GlowMesh->SetStaticMesh(CylinderMesh.Object);
    }

    NameplateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameplateText"));
    NameplateText->SetupAttachment(RootComponent);
    NameplateText->SetRelativeLocation(FVector(0.0f, 0.0f, 265.0f));
    NameplateText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    NameplateText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    NameplateText->SetTextRenderColor(FColor(120, 220, 255));
    NameplateText->SetWorldSize(22.0f);
    NameplateText->SetText(FText::FromString(TEXT("Vendor\n[Press V to shop]")));
}

void AMOBAMMOVendorActor::BeginPlay()
{
    Super::BeginPlay();
}

void AMOBAMMOVendorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Gently pulse the vendor's glow ring and body color on clients
    const UWorld* World = GetWorld();
    const float T = World ? World->GetTimeSeconds() : 0.0f;

    if (BodyMesh)
    {
        const float PulseScale = 1.0f + 0.04f * FMath::Sin(T * 3.2f);
        BodyMesh->SetRelativeScale3D(BaseBodyScale * PulseScale);
        // Gold/amber tint
        BodyMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(1.0f, 0.82f, 0.1f));
    }

    if (GlowMesh)
    {
        const float GlowPulse = 1.0f + 0.08f * FMath::Sin(T * 4.5f);
        GlowMesh->SetRelativeScale3D(BaseGlowScale * GlowPulse);
        // Cyan / arcane hue
        GlowMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.05f, 0.8f, 1.0f));
    }

    FaceTextTowardLocalCamera();
}

void AMOBAMMOVendorActor::FaceTextTowardLocalCamera() const
{
    const UWorld* World = GetWorld();
    const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    if (!PlayerController || !PlayerController->PlayerCameraManager || !NameplateText)
    {
        return;
    }

    const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    const FVector ToCamera = CameraLocation - NameplateText->GetComponentLocation();
    if (!ToCamera.IsNearlyZero())
    {
        NameplateText->SetWorldRotation(ToCamera.Rotation());
    }
}
