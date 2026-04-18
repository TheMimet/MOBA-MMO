#include "AoEAbility.h"
#include "AbilityComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

UAoEAbility::UAoEAbility()
{
    AbilityType = EAbilityType::AoE;
}

void UAoEAbility::Activate(const FAbilityActivationInfo& Info)
{
    Super::Activate(Info);

    if (Info.TargetLocation.IsZero())
    {
        EndAbility(false);
        return;
    }

    K2_OnAoEActivated(Info.TargetLocation, AoE.Radius);

    if (LoopVFX && OwnerComponent.IsValid())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            OwnerComponent->GetWorld(),
            LoopVFX,
            Info.TargetLocation,
            FRotator::ZeroRotator
        );
    }

    ApplyAoEEffect(Info.TargetLocation);

    FTimerHandle Handle;
    OwnerComponent->GetWorld()->GetTimerManager().SetTimer(Handle, [this]()
    {
        EndAbility(true);
    }, 0.5f, false);
}

bool UAoEAbility::CanActivate(const FAbilityActivationInfo& Info) const
{
    if (!Super::CanActivate(Info))
    {
        return false;
    }

    if (Requirements.bRequiresGroundLocation && Info.TargetLocation.IsZero())
    {
        return false;
    }

    return true;
}

void UAoEAbility::ApplyAoEEffect(const FVector& Center)
{
    TArray<AActor*> Targets = GetActorsInAoE(Center, true);

    for (AActor* Target : Targets)
    {
        ApplyEffectToActorInAoE(Target, Center);
    }
}

void UAoEAbility::ApplyEffectToActorInAoE(AActor* Target, const FVector& Center)
{
    if (!Target)
    {
        return;
    }

    FVector ImpactLocation = Target->GetActorLocation();

    if (ImpactVFX && OwnerComponent.IsValid())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            OwnerComponent->GetWorld(),
            ImpactVFX,
            ImpactLocation,
            FRotator::ZeroRotator
        );
    }

    if (ImpactSound)
    {
        UGameplayStatics::SpawnSoundAtLocation(
            OwnerComponent.IsValid() ? OwnerComponent->GetWorld() : nullptr,
            ImpactSound,
            ImpactLocation
        );
    }

    ApplyEffectsToTarget(Target, ImpactLocation);
}
