#include "MOBAMMOLootDropActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "MOBAMMOGameMode.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMOBAMMOLootDropActor::AMOBAMMOLootDropActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    // ── Pickup sphere (server triggers overlap) ───────────────
    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetSphereRadius(PickupRadius);
    PickupSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    PickupSphere->SetGenerateOverlapEvents(true);
    RootComponent = PickupSphere;

    // ── Gem visual ────────────────────────────────────────────
    GemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GemMesh"));
    GemMesh->SetupAttachment(RootComponent);
    GemMesh->SetRelativeScale3D(FVector(0.22f));
    GemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshFinder.Succeeded())
    {
        GemMesh->SetStaticMesh(SphereMeshFinder.Object);
    }

    // ── Floating label ────────────────────────────────────────
    LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
    LabelText->SetupAttachment(RootComponent);
    LabelText->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
    LabelText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    LabelText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    LabelText->SetWorldSize(18.0f);
    LabelText->SetTextRenderColor(FColor(255, 215, 0)); // gold
    LabelText->SetText(FText::FromString(TEXT("Item")));
}

void AMOBAMMOLootDropActor::BeginPlay()
{
    Super::BeginPlay();

    BaseLocation = GetActorLocation();
    AnimTime = FMath::FRandRange(0.0f, 6.28f); // random phase so drops don't bob in sync

    SetLifeSpan(LifeSpan);
    UpdateLabel();

    // Only the server processes pickups.
    if (HasAuthority())
    {
        PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AMOBAMMOLootDropActor::HandlePickupOverlap);
    }
}

void AMOBAMMOLootDropActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bPickedUp)
    {
        return;
    }

    // Gentle bob + slow spin — runs on all clients.
    AnimTime += DeltaSeconds;
    const float BobZ  = FMath::Sin(AnimTime * 2.2f) * 12.0f;
    SetActorLocation(BaseLocation + FVector(0.0f, 0.0f, BobZ + 30.0f));
    AddActorWorldRotation(FRotator(0.0f, DeltaSeconds * 90.0f, 0.0f));
}

void AMOBAMMOLootDropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMOBAMMOLootDropActor, ItemId);
    DOREPLIFETIME(AMOBAMMOLootDropActor, Quantity);
    DOREPLIFETIME(AMOBAMMOLootDropActor, DisplayName);
}

// ─────────────────────────────────────────────────────────────
// Server: configure payload immediately after spawning.
// ─────────────────────────────────────────────────────────────

void AMOBAMMOLootDropActor::InitializeDrop(const FString& InItemId, int32 InQuantity,
                                           const FString& InDisplayName)
{
    ItemId      = InItemId;
    Quantity    = FMath::Max(1, InQuantity);
    DisplayName = InDisplayName.IsEmpty() ? InItemId : InDisplayName;
    UpdateLabel();
}

// ─────────────────────────────────────────────────────────────
// Replication rep-notify — update label on clients.
// ─────────────────────────────────────────────────────────────

void AMOBAMMOLootDropActor::OnRep_DropPayload()
{
    UpdateLabel();
}

void AMOBAMMOLootDropActor::UpdateLabel()
{
    if (!LabelText)
    {
        return;
    }

    const FString Label = (Quantity > 1)
        ? FString::Printf(TEXT("%s x%d"), *DisplayName, Quantity)
        : DisplayName;

    LabelText->SetText(FText::FromString(Label));
}

// ─────────────────────────────────────────────────────────────
// Server: player enters pickup radius — grant and destroy.
// ─────────────────────────────────────────────────────────────

void AMOBAMMOLootDropActor::HandlePickupOverlap(UPrimitiveComponent* /*OverlappedComp*/,
                                                AActor* OtherActor,
                                                UPrimitiveComponent* /*OtherComp*/,
                                                int32 /*OtherBodyIndex*/,
                                                bool /*bFromSweep*/,
                                                const FHitResult& /*SweepResult*/)
{
    if (bPickedUp || !HasAuthority() || ItemId.IsEmpty())
    {
        return;
    }

    // Only player characters trigger pickup.
    const ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
    if (!PlayerCharacter)
    {
        return;
    }

    AController* Controller = PlayerCharacter->GetController();
    if (!Controller)
    {
        return;
    }

    if (AMOBAMMOGameMode* GameMode = GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>())
    {
        if (GameMode->GrantLootPickup(Controller, ItemId, Quantity, DisplayName))
        {
            bPickedUp = true;
            // Hide immediately; the actor will be cleaned up by SetLifeSpan or Destroy.
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            Destroy();
        }
    }
}
