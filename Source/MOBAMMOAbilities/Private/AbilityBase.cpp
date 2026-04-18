#include "AbilityBase.h"
#include "AbilityComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"

UAbilityBase::UAbilityBase()
{
    bIsActive = false;
    ActiveTime = 0.0f;
    TicksRemaining = 0;
}

void UAbilityBase::Initialize(UAbilityComponent* InOwnerComponent)
{
    OwnerComponent = InOwnerComponent;
}

bool UAbilityBase::CanActivate(const FAbilityActivationInfo& Info) const
{
    if (!OwnerComponent.IsValid())
    {
        return false;
    }

    if (!OwnerComponent->IsValidLowLevel())
    {
        return false;
    }

    if (K2_CanActivate(Info))
    {
        return true;
    }

    if (Requirements.bRequiresTarget && !Info.TargetActor.IsValid())
    {
        return false;
    }

    if (Requirements.bRequiresGroundLocation && Info.TargetLocation.IsZero())
    {
        return false;
    }

    return true;
}

void UAbilityBase::Activate(const FAbilityActivationInfo& Info)
{
    bIsActive = true;
    ActiveTime = 0.0f;
    CurrentActivationInfo = Info;

    if (CastVFX && Info.OwnerActor.IsValid())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            Info.OwnerActor.Get()->GetWorld(),
            CastVFX,
            Info.OwnerActor->GetActorLocation(),
            Info.OwnerActor->GetActorRotation()
        );
    }

    if (CastSound && Info.OwnerActor.IsValid())
    {
        UGameplayStatics::SpawnSoundAtLocation(
            Info.OwnerActor.Get()->GetWorld(),
            CastSound,
            Info.OwnerActor->GetActorLocation()
        );
    }

    K2_OnActivated(Info);
}

void UAbilityBase::Tick(float DeltaTime)
{
    if (!bIsActive)
    {
        return;
    }

    ActiveTime += DeltaTime;

    if (AbilityType == EAbilityType::Channel && Channel.ChannelDuration > 0.0f)
    {
        if (ActiveTime >= Channel.ChannelDuration)
        {
            EndAbility(true);
        }
    }
}

void UAbilityBase::EndAbility(bool bWasSuccessful)
{
    bIsActive = false;

    if (LoopVFX && CurrentActivationInfo.OwnerActor.IsValid())
    {
        LoopVFX = nullptr;
    }

    K2_OnEnded(bWasSuccessful);
}

void UAbilityBase::CancelAbility()
{
    bIsActive = false;
    K2_OnCancelled();
}

void UAbilityBase::OnChannelTick()
{
    if (!OwnerComponent.IsValid())
    {
        return;
    }

    if (Channel.MaxTicks > 0 && TicksRemaining <= 0)
    {
        EndAbility(true);
        return;
    }

    if (Damage.BaseDamage > 0.0f)
    {
        switch (AbilityType)
        {
        case EAbilityType::Instant:
        case EAbilityType::Channel:
            if (CurrentActivationInfo.TargetActor.IsValid())
            {
                ApplyEffectsToTarget(CurrentActivationInfo.TargetActor.Get(), CurrentActivationInfo.TargetActor->GetActorLocation());
            }
            break;

        case EAbilityType::AoE:
            if (!CurrentActivationInfo.TargetLocation.IsZero())
            {
                TArray<AActor*> Targets = GetActorsInAoE(CurrentActivationInfo.TargetLocation);
                for (AActor* Target : Targets)
                {
                    ApplyEffectsToTarget(Target, Target->GetActorLocation());
                }
            }
            break;

        default:
            break;
        }
    }

    if (Channel.MaxTicks > 0)
    {
        TicksRemaining--;
    }
}

void UAbilityBase::ApplyEffectsToTarget(AActor* Target, const FVector& Location)
{
    if (!Target || !OwnerComponent.IsValid())
    {
        return;
    }

    if (ImpactVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            OwnerComponent->GetWorld(),
            ImpactVFX,
            Location,
            FRotator::ZeroRotator
        );
    }

    if (ImpactSound)
    {
        UGameplayStatics::SpawnSoundAtLocation(
            OwnerComponent->GetWorld(),
            ImpactSound,
            Location
        );
    }
}

TArray<AActor*> UAbilityBase::GetActorsInAoE(const FVector& Center, bool bIgnoreOwner) const
{
    TArray<AActor*> Results;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    if (bIgnoreOwner && CurrentActivationInfo.OwnerActor.IsValid())
    {
        ActorsToIgnore.Add(CurrentActivationInfo.OwnerActor.Get());
    }

    UKismetSystemLibrary::SphereOverlapActors(
        OwnerComponent.IsValid() ? OwnerComponent->GetWorld() : nullptr,
        Center,
        AoE.Radius,
        ObjectTypes,
        nullptr,
        ActorsToIgnore,
        Results
    );

    if (AoE.InnerRadius > 0.0f && AoE.bFullDamageInsideInner)
    {
    }

    return Results;
}

AActor* UAbilityBase::GetProjectileTarget(const FAbilityActivationInfo& Info) const
{
    if (Info.TargetActor.IsValid())
    {
        return Info.TargetActor.Get();
    }

    if (!Info.TargetLocation.IsZero() && OwnerComponent.IsValid())
    {
        FVector Start = Info.OwnerActor.IsValid() ? Info.OwnerActor->GetActorLocation() : FVector::ZeroVector;
        FVector Direction = (Info.TargetLocation - Start).GetSafeNormal();

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Info.OwnerActor.Get());

        if (OwnerComponent->GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * Projectile.MaxDistance, ECC_Visibility, Params))
        {
            return Hit.GetActor();
        }
    }

    return nullptr;
}
