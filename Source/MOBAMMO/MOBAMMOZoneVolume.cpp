#include "MOBAMMOZoneVolume.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "MOBAMMOGameMode.h"

AMOBAMMOZoneVolume::AMOBAMMOZoneVolume()
{
    PrimaryActorTick.bCanEverTick = false;
    // Zone volumes only matter on the server; clients use PlayerState.CurrentZoneId
    // (replicated) for any UI that needs to know the current zone.
    bReplicates = false;

    ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
    ZoneBox->SetBoxExtent(FVector(500.0f));
    ZoneBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    ZoneBox->SetGenerateOverlapEvents(true);
    RootComponent = ZoneBox;
}

void AMOBAMMOZoneVolume::BeginPlay()
{
    Super::BeginPlay();

    // Only the server processes zone transitions.
    if (HasAuthority())
    {
        ZoneBox->OnComponentBeginOverlap.AddDynamic(this, &AMOBAMMOZoneVolume::HandleBeginOverlap);
        ZoneBox->OnComponentEndOverlap.AddDynamic(this, &AMOBAMMOZoneVolume::HandleEndOverlap);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlap callbacks
// ─────────────────────────────────────────────────────────────────────────────

void AMOBAMMOZoneVolume::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/,
                                            AActor*              OtherActor,
                                            UPrimitiveComponent* /*OtherComp*/,
                                            int32                /*OtherBodyIndex*/,
                                            bool                 /*bFromSweep*/,
                                            const FHitResult&    /*SweepResult*/)
{
    if (!ZoneInfo.IsValid())
    {
        return;
    }

    const ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
    {
        return;
    }

    AController* Controller = Character->GetController();
    if (!Controller)
    {
        return;
    }

    if (AMOBAMMOGameMode* GM = GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>())
    {
        GM->HandlePlayerEnteredZone(Controller, ZoneInfo);
    }
}

void AMOBAMMOZoneVolume::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComp*/,
                                          AActor*              OtherActor,
                                          UPrimitiveComponent* /*OtherComp*/,
                                          int32                /*OtherBodyIndex*/)
{
    const ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
    {
        return;
    }

    AController* Controller = Character->GetController();
    if (!Controller)
    {
        return;
    }

    if (AMOBAMMOGameMode* GM = GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>())
    {
        GM->HandlePlayerExitedZone(Controller, ZoneInfo.ZoneId);
    }
}
