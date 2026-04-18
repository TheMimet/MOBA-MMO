#include "MOBAMMOTrainingDummyActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerState.h"
#include "UObject/ConstructorHelpers.h"

AMOBAMMOTrainingDummyActor::AMOBAMMOTrainingDummyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

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

    TargetRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetRingMesh"));
    TargetRingMesh->SetupAttachment(RootComponent);
    TargetRingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
    TargetRingMesh->SetRelativeScale3D(BaseRingScale);
    TargetRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (CylinderMesh.Succeeded())
    {
        TargetRingMesh->SetStaticMesh(CylinderMesh.Object);
    }

    BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
    BeaconMesh->SetupAttachment(RootComponent);
    BeaconMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 235.0f));
    BeaconMesh->SetRelativeScale3D(BaseBeaconScale);
    BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        BeaconMesh->SetStaticMesh(SphereMesh.Object);
    }

    NameplateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameplateText"));
    NameplateText->SetupAttachment(RootComponent);
    NameplateText->SetRelativeLocation(FVector(0.0f, 0.0f, 255.0f));
    NameplateText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    NameplateText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    NameplateText->SetTextRenderColor(FColor(255, 220, 120));
    NameplateText->SetWorldSize(22.0f);
    NameplateText->SetText(FText::FromString(TEXT("Training Dummy")));

    FloatingFeedbackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FloatingFeedbackText"));
    FloatingFeedbackText->SetupAttachment(RootComponent);
    FloatingFeedbackText->SetRelativeLocation(FloatingFeedbackBaseLocation);
    FloatingFeedbackText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    FloatingFeedbackText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    FloatingFeedbackText->SetTextRenderColor(FColor::Transparent);
    FloatingFeedbackText->SetWorldSize(32.0f);
    FloatingFeedbackText->SetText(FText::GetEmpty());
}

void AMOBAMMOTrainingDummyActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshFromGameState(0.0f);
}

void AMOBAMMOTrainingDummyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshFromGameState(DeltaSeconds);
}

void AMOBAMMOTrainingDummyActor::RefreshFromGameState(float DeltaSeconds)
{
    const UWorld* World = GetWorld();
    const AMOBAMMOGameState* MOBAGameState = World ? World->GetGameState<AMOBAMMOGameState>() : nullptr;
    if (!MOBAGameState || !NameplateText)
    {
        return;
    }

    const float Health = MOBAGameState->GetTrainingDummyHealth();
    const float MaxHealth = FMath::Max(1.0f, MOBAGameState->GetTrainingDummyMaxHealth());
    const float Mana = MOBAGameState->GetTrainingDummyMana();
    const float MaxMana = FMath::Max(1.0f, MOBAGameState->GetTrainingDummyMaxMana());
    const bool bDead = Health <= 0.0f;
    bool bSelectedByLocalPlayer = false;

    if (LastObservedHealth >= 0.0f)
    {
        if (Health < LastObservedHealth)
        {
            HitPulseRemaining = 0.35f;
            ShowFloatingFeedback(FString::Printf(TEXT("-%.0f HP"), LastObservedHealth - Health), FColor(255, 85, 65));
        }
        else if (Health > LastObservedHealth)
        {
            RestorePulseRemaining = 0.35f;
            ShowFloatingFeedback(FString::Printf(TEXT("+%.0f HP"), Health - LastObservedHealth), FColor(90, 255, 150));
        }
    }

    if (LastObservedMana >= 0.0f)
    {
        if (Mana < LastObservedMana)
        {
            DrainPulseRemaining = 0.35f;
            ShowFloatingFeedback(FString::Printf(TEXT("-%.0f MP"), LastObservedMana - Mana), FColor(90, 170, 255));
        }
        else if (Mana > LastObservedMana)
        {
            RestorePulseRemaining = 0.35f;
            ShowFloatingFeedback(FString::Printf(TEXT("+%.0f MP"), Mana - LastObservedMana), FColor(90, 220, 255));
        }
    }

    LastObservedHealth = Health;
    LastObservedMana = Mana;
    HitPulseRemaining = FMath::Max(0.0f, HitPulseRemaining - DeltaSeconds);
    DrainPulseRemaining = FMath::Max(0.0f, DrainPulseRemaining - DeltaSeconds);
    RestorePulseRemaining = FMath::Max(0.0f, RestorePulseRemaining - DeltaSeconds);
    FloatingFeedbackRemaining = FMath::Max(0.0f, FloatingFeedbackRemaining - DeltaSeconds);

    if (World)
    {
        if (const APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            const AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>();
            bSelectedByLocalPlayer = PlayerState
                && PlayerState->GetSelectedTargetCharacterId() == MOBAGameState->GetTrainingDummyCharacterId();
        }
    }

    NameplateText->SetText(FText::FromString(FString::Printf(
        TEXT("%s%s%s\nHP %.0f/%.0f | MP %.0f/%.0f"),
        bSelectedByLocalPlayer ? TEXT("[TARGET] ") : TEXT(""),
        bDead ? TEXT("[DOWN] ") : TEXT(""),
        *MOBAGameState->GetTrainingDummyName(),
        Health,
        MaxHealth,
        Mana,
        MaxMana
    )));

    FColor NameplateColor = bSelectedByLocalPlayer ? FColor(80, 255, 180) : FColor(255, 220, 120);
    if (bDead)
    {
        NameplateColor = FColor(255, 80, 80);
    }
    else if (HitPulseRemaining > 0.0f)
    {
        NameplateColor = FColor(255, 90, 70);
    }
    else if (DrainPulseRemaining > 0.0f)
    {
        NameplateColor = FColor(90, 170, 255);
    }
    else if (RestorePulseRemaining > 0.0f)
    {
        NameplateColor = FColor(90, 255, 150);
    }
    NameplateText->SetTextRenderColor(NameplateColor);

    if (BodyMesh)
    {
        const float HealthRatio = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
        float PulseScale = 1.0f;
        if (HitPulseRemaining > 0.0f)
        {
            PulseScale += 0.12f * (HitPulseRemaining / 0.35f);
        }
        else if (DrainPulseRemaining > 0.0f)
        {
            PulseScale += 0.08f * (DrainPulseRemaining / 0.35f);
        }
        else if (RestorePulseRemaining > 0.0f)
        {
            PulseScale += 0.06f * (RestorePulseRemaining / 0.35f);
        }

        BodyMesh->SetRelativeScale3D(BaseBodyScale * (bDead ? 0.82f : PulseScale));
        BodyMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(1.0f, 0.15f + (0.55f * HealthRatio), 0.08f));
        BodyMesh->SetRenderCustomDepth(bSelectedByLocalPlayer);
    }

    if (TargetRingMesh)
    {
        const float RingPulse = bSelectedByLocalPlayer ? (1.0f + 0.06f * FMath::Sin(World ? World->GetTimeSeconds() * 7.0f : 0.0f)) : 1.0f;
        TargetRingMesh->SetRelativeScale3D(BaseRingScale * RingPulse);
        TargetRingMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), bSelectedByLocalPlayer ? FVector(0.05f, 1.0f, 0.6f) : FVector(1.0f, 0.78f, 0.18f));
        TargetRingMesh->SetRenderCustomDepth(bSelectedByLocalPlayer);
    }

    if (BeaconMesh)
    {
        const float BeaconPulse = bDead ? 0.65f : (1.0f + 0.12f * FMath::Sin(World ? World->GetTimeSeconds() * 5.5f : 0.0f));
        BeaconMesh->SetRelativeScale3D(BaseBeaconScale * BeaconPulse);
        BeaconMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), bDead ? FVector(1.0f, 0.05f, 0.05f) : FVector(0.05f, 0.95f, 1.0f));
        BeaconMesh->SetRenderCustomDepth(bSelectedByLocalPlayer);
    }

    if (FloatingFeedbackText)
    {
        if (FloatingFeedbackRemaining > 0.0f)
        {
            const float LifeAlpha = FloatingFeedbackRemaining / 1.1f;
            FloatingFeedbackText->SetRelativeLocation(FloatingFeedbackBaseLocation + FVector(0.0f, 0.0f, 36.0f * (1.0f - LifeAlpha)));
            FloatingFeedbackText->SetWorldSize(26.0f + 8.0f * LifeAlpha);
        }
        else
        {
            FloatingFeedbackText->SetText(FText::GetEmpty());
            FloatingFeedbackText->SetTextRenderColor(FColor::Transparent);
        }
    }

    FaceTextTowardLocalCamera();
}

void AMOBAMMOTrainingDummyActor::FaceTextTowardLocalCamera() const
{
    const UWorld* World = GetWorld();
    const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    if (!PlayerController || !PlayerController->PlayerCameraManager)
    {
        return;
    }

    const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    auto FaceComponent = [CameraLocation](UTextRenderComponent* TextComponent)
    {
        if (!TextComponent)
        {
            return;
        }

        const FVector ToCamera = CameraLocation - TextComponent->GetComponentLocation();
        if (!ToCamera.IsNearlyZero())
        {
            TextComponent->SetWorldRotation(ToCamera.Rotation());
        }
    };

    FaceComponent(NameplateText);
    FaceComponent(FloatingFeedbackText);
}

void AMOBAMMOTrainingDummyActor::ShowFloatingFeedback(const FString& Message, const FColor& Color)
{
    if (!FloatingFeedbackText || Message.IsEmpty())
    {
        return;
    }

    FloatingFeedbackRemaining = 1.1f;
    FloatingFeedbackText->SetRelativeLocation(FloatingFeedbackBaseLocation);
    FloatingFeedbackText->SetText(FText::FromString(Message));
    FloatingFeedbackText->SetTextRenderColor(Color);
}
