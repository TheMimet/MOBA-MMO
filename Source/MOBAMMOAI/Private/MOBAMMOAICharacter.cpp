#include "MOBAMMOAICharacter.h"
#include "HealthComponent.h"
#include "Components/CapsuleComponent.h"

AMOBAMMOAICharacter::AMOBAMMOAICharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bReplicateUsingRegisteredSubObjectList = true;

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AMOBAMMOAICharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddDynamic(this, &AMOBAMMOAICharacter::OnDeath);
    }
}

void AMOBAMMOAICharacter::OnDeath(UHealthComponent* HealthComp, AActor* InstigatorActor)
{
    // Basic death: disable collision, ragdoll the mesh, then self-destruct.
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
    }

    SetLifeSpan(5.0f);

    // Broadcast so MobSpawner (and any other listener) can award kills / respawn.
    OnAIDied.Broadcast(this, InstigatorActor);
}
