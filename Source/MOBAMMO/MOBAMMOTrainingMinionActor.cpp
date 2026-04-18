#include "MOBAMMOTrainingMinionActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FString TrainingMinionCharacterId(TEXT("training-minion"));
    const FString TrainingMinionName(TEXT("Sparring Minion"));
}

AMOBAMMOTrainingMinionActor::AMOBAMMOTrainingMinionActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(GetCapsuleComponent());
    BodyMesh->SetRelativeScale3D(BodyBaseScale);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        BodyMesh->SetStaticMesh(CylinderMesh.Object);
    }

    TargetRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetRingMesh"));
    TargetRingMesh->SetupAttachment(GetCapsuleComponent());
    TargetRingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -94.0f));
    TargetRingMesh->SetRelativeScale3D(RingBaseScale);
    TargetRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (CylinderMesh.Succeeded())
    {
        TargetRingMesh->SetStaticMesh(CylinderMesh.Object);
    }

    NameplateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameplateText"));
    NameplateText->SetupAttachment(GetCapsuleComponent());
    NameplateText->SetRelativeLocation(FVector(0.0f, 0.0f, 178.0f));
    NameplateText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    NameplateText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    NameplateText->SetTextRenderColor(FColor(120, 210, 255));
    NameplateText->SetWorldSize(20.0f);
    NameplateText->SetText(FText::FromString(TrainingMinionName));

    FloatingFeedbackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FloatingFeedbackText"));
    FloatingFeedbackText->SetupAttachment(GetCapsuleComponent());
    FloatingFeedbackText->SetRelativeLocation(FVector(0.0f, 0.0f, 224.0f));
    FloatingFeedbackText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    FloatingFeedbackText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    FloatingFeedbackText->SetTextRenderColor(FColor::Transparent);
    FloatingFeedbackText->SetWorldSize(26.0f);
    FloatingFeedbackText->SetText(FText::GetEmpty());
}

void AMOBAMMOTrainingMinionActor::BeginPlay()
{
    Super::BeginPlay();
    SpawnLocation = GetActorLocation();
    LastObservedHealth = GetCurrentHealth();
    UpdatePresentation(0.0f);
}

void AMOBAMMOTrainingMinionActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdatePatrol(DeltaSeconds);
    UpdatePresentation(DeltaSeconds);
}

float AMOBAMMOTrainingMinionActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float PreviousHealth = GetCurrentHealth();
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    const float HealthDelta = FMath::Max(0.0f, PreviousHealth - GetCurrentHealth());

    if (HealthDelta > 0.0f)
    {
        HitPulseRemaining = 0.35f;
        ShowFloatingFeedback(HealthDelta);
    }

    return AppliedDamage;
}

const FString& AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId()
{
    return TrainingMinionCharacterId;
}

const FString& AMOBAMMOTrainingMinionActor::GetTrainingMinionName()
{
    return TrainingMinionName;
}

void AMOBAMMOTrainingMinionActor::SetAggroTarget(AActor* TargetActor, const FString& TargetName, float DurationSeconds)
{
    if (!HasAuthority() || !IsAlive())
    {
        return;
    }

    AggroTargetActor = TargetActor;
    AggroTargetName = TargetName;
    AggroEndServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::Max(0.25f, DurationSeconds) : 0.0f;
    ForceNetUpdate();
}

float AMOBAMMOTrainingMinionActor::GetCurrentHealth() const
{
    return HealthComponent ? HealthComponent->GetHealth() : 0.0f;
}

float AMOBAMMOTrainingMinionActor::GetMaxHealth() const
{
    return HealthComponent ? HealthComponent->GetMaxHealth() : 0.0f;
}

bool AMOBAMMOTrainingMinionActor::IsAlive() const
{
    return HealthComponent && !HealthComponent->IsDead();
}

void AMOBAMMOTrainingMinionActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOTrainingMinionActor, AggroTargetName);
    DOREPLIFETIME(AMOBAMMOTrainingMinionActor, AggroEndServerTime);
}

void AMOBAMMOTrainingMinionActor::OnDeath(UHealthComponent* HealthComp, AActor* InstigatorActor)
{
    AggroTargetActor.Reset();
    AggroTargetName.Reset();
    AggroEndServerTime = 0.0f;
    ForceNetUpdate();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetLifeSpan(8.0f);
}

void AMOBAMMOTrainingMinionActor::UpdatePresentation(float DeltaSeconds)
{
    const float Health = GetCurrentHealth();
    const float MaxHealth = FMath::Max(1.0f, GetMaxHealth());
    const bool bDead = Health <= 0.0f;
    const bool bAggroActive = IsAggroActive();

    if (LastObservedHealth >= 0.0f && Health < LastObservedHealth)
    {
        HitPulseRemaining = 0.35f;
        ShowFloatingFeedback(LastObservedHealth - Health);
    }
    LastObservedHealth = Health;

    HitPulseRemaining = FMath::Max(0.0f, HitPulseRemaining - DeltaSeconds);
    FloatingFeedbackRemaining = FMath::Max(0.0f, FloatingFeedbackRemaining - DeltaSeconds);

    if (NameplateText)
    {
        NameplateText->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s%s\nHP %.0f/%.0f"),
            bDead ? TEXT("[DOWN] ") : TEXT(""),
            bAggroActive ? TEXT("[AGGRO] ") : TEXT(""),
            *TrainingMinionName,
            Health,
            MaxHealth
        )));
        NameplateText->SetTextRenderColor(bDead ? FColor(255, 90, 90) : (bAggroActive ? FColor(255, 165, 75) : FColor(120, 210, 255)));
    }

    if (BodyMesh)
    {
        const float HealthRatio = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
        const float PulseScale = 1.0f + (HitPulseRemaining > 0.0f ? 0.12f * (HitPulseRemaining / 0.35f) : 0.0f);
        BodyMesh->SetRelativeScale3D(BodyBaseScale * (bDead ? 0.7f : PulseScale));
        BodyMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), bDead ? FVector(0.55f, 0.05f, 0.05f) : FVector(0.05f, 0.35f + 0.55f * HealthRatio, 1.0f));
    }

    if (TargetRingMesh)
    {
        const float RingPulse = 1.0f + 0.05f * FMath::Sin(GetWorld() ? GetWorld()->GetTimeSeconds() * 4.0f : 0.0f);
        TargetRingMesh->SetRelativeScale3D(RingBaseScale * (bAggroActive ? RingPulse * 1.12f : RingPulse));
        TargetRingMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), bDead ? FVector(1.0f, 0.1f, 0.1f) : (bAggroActive ? FVector(1.0f, 0.35f, 0.08f) : FVector(0.1f, 0.75f, 1.0f)));
    }

    if (FloatingFeedbackText)
    {
        if (FloatingFeedbackRemaining > 0.0f)
        {
            const float LifeAlpha = FloatingFeedbackRemaining / 1.1f;
            FloatingFeedbackText->SetRelativeLocation(FVector(0.0f, 0.0f, 224.0f + 34.0f * (1.0f - LifeAlpha)));
            FloatingFeedbackText->SetWorldSize(22.0f + 6.0f * LifeAlpha);
        }
        else
        {
            FloatingFeedbackText->SetText(FText::GetEmpty());
            FloatingFeedbackText->SetTextRenderColor(FColor::Transparent);
        }
    }

    FaceTextTowardLocalCamera();
}

void AMOBAMMOTrainingMinionActor::UpdatePatrol(float DeltaSeconds)
{
    if (!HasAuthority() || !IsAlive())
    {
        return;
    }

    if (IsAggroActive())
    {
        if (AActor* TargetActor = AggroTargetActor.Get())
        {
            const FVector CurrentLocation = GetActorLocation();
            const FVector TargetLocation = TargetActor->GetActorLocation();
            FVector ToTarget = TargetLocation - CurrentLocation;
            ToTarget.Z = 0.0f;

            if (!ToTarget.IsNearlyZero())
            {
                SetActorRotation(ToTarget.Rotation());

                const float Distance = ToTarget.Size();
                if (Distance > AggroDesiredDistance)
                {
                    const FVector Direction = ToTarget / Distance;
                    const FVector NewLocation = CurrentLocation + Direction * AggroChaseSpeed * DeltaSeconds;
                    SetActorLocation(NewLocation, true);
                }
            }

            return;
        }

        AggroTargetName.Reset();
        AggroEndServerTime = 0.0f;
        ForceNetUpdate();
    }

    PatrolTime += DeltaSeconds;
    const FVector NewLocation = SpawnLocation + PatrolOffset * FMath::Sin(PatrolTime * 0.75f);
    SetActorLocation(NewLocation, true);
}

bool AMOBAMMOTrainingMinionActor::IsAggroActive() const
{
    const UWorld* World = GetWorld();
    return IsAlive()
        && !AggroTargetName.IsEmpty()
        && World
        && World->GetTimeSeconds() <= AggroEndServerTime;
}

void AMOBAMMOTrainingMinionActor::FaceTextTowardLocalCamera() const
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

void AMOBAMMOTrainingMinionActor::ShowFloatingFeedback(float DamageAmount)
{
    if (!FloatingFeedbackText || DamageAmount <= 0.0f)
    {
        return;
    }

    FloatingFeedbackRemaining = 1.1f;
    FloatingFeedbackText->SetRelativeLocation(FVector(0.0f, 0.0f, 224.0f));
    FloatingFeedbackText->SetText(FText::FromString(FString::Printf(TEXT("-%.0f HP"), DamageAmount)));
    FloatingFeedbackText->SetTextRenderColor(FColor(255, 90, 70));
}
