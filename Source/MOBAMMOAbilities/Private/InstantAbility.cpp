#include "InstantAbility.h"
#include "AbilityComponent.h"

UInstantAbility::UInstantAbility()
{
    AbilityType = EAbilityType::Instant;
}

void UInstantAbility::Activate(const FAbilityActivationInfo& Info)
{
    Super::Activate(Info);

    switch (TargetType)
    {
    case EAbilityTargetType::SingleTarget:
        if (Info.TargetActor.IsValid())
        {
            ApplyInstantEffect(Info.TargetActor.Get(), Info.TargetActor->GetActorLocation());
        }
        break;

    case EAbilityTargetType::Self:
        if (Info.OwnerActor.IsValid())
        {
            ApplyInstantEffect(Info.OwnerActor.Get(), Info.OwnerActor->GetActorLocation());
        }
        break;

    case EAbilityTargetType::GroundLocation:
        if (!Info.TargetLocation.IsZero())
        {
            TArray<AActor*> Actors = GetActorsInAoE(Info.TargetLocation);
            for (AActor* Actor : Actors)
            {
                ApplyInstantEffect(Actor, Actor->GetActorLocation());
            }
        }
        break;

    case EAbilityTargetType::Direction:
        break;

    default:
        break;
    }

    EndAbility(true);
}

bool UInstantAbility::CanActivate(const FAbilityActivationInfo& Info) const
{
    if (!Super::CanActivate(Info))
    {
        return false;
    }

    if (TargetType == EAbilityTargetType::SingleTarget && Requirements.bRequiresTarget)
    {
        if (!Info.TargetActor.IsValid())
        {
            return false;
        }
    }

    return true;
}

void UInstantAbility::ApplyInstantEffect(AActor* Target, const FVector& ImpactLocation)
{
    if (!Target || !OwnerComponent.IsValid())
    {
        return;
    }

    K2_ApplyInstantEffect(CurrentActivationInfo, Target);

    ApplyEffectsToTarget(Target, ImpactLocation);
}
