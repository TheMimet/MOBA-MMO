#include "ChannelAbility.h"
#include "AbilityComponent.h"

UChannelAbility::UChannelAbility()
{
    AbilityType = EAbilityType::Channel;
    bIsActive = false;
}

void UChannelAbility::Activate(const FAbilityActivationInfo& Info)
{
    Super::Activate(Info);

    CurrentChannelTime = 0.0f;
    TimeSinceLastTick = 0.0f;

    K2_OnChannelStarted(Info);

    if (OwnerComponent.IsValid())
    {
        OwnerComponent->SetComponentTickEnabled(true);
    }
}

void UChannelAbility::Tick(float DeltaTime)
{
    if (!bIsActive)
    {
        return;
    }

    Super::Tick(DeltaTime);

    CurrentChannelTime += DeltaTime;
    TimeSinceLastTick += DeltaTime;

    K2_OnChannelTick(CurrentChannelTime, TotalChannelDuration);

    if (CurrentChannelTime >= TotalChannelDuration)
    {
        EndAbility(true);
        return;
    }

    if (TimeSinceLastTick >= TickInterval)
    {
        TimeSinceLastTick = 0.0f;
        ExecuteChannelTick();
    }
}

void UChannelAbility::EndAbility(bool bWasSuccessful)
{
    K2_OnChannelEnded(bWasSuccessful);
    Super::EndAbility(bWasSuccessful);

    CurrentChannelTime = 0.0f;
    TimeSinceLastTick = 0.0f;
}

bool UChannelAbility::CanActivate(const FAbilityActivationInfo& Info) const
{
    if (!Super::CanActivate(Info))
    {
        return false;
    }

    if (Requirements.bRequiresTarget && !Info.TargetActor.IsValid())
    {
        return false;
    }

    return true;
}

void UChannelAbility::ExecuteChannelTick()
{
    if (DamagePerTick > 0.0f && CurrentActivationInfo.TargetActor.IsValid())
    {
        ApplyEffectsToTarget(CurrentActivationInfo.TargetActor.Get(), CurrentActivationInfo.TargetActor->GetActorLocation());
    }

    if (HealPerTick > 0.0f && CurrentActivationInfo.OwnerActor.IsValid())
    {
        ApplyEffectsToTarget(CurrentActivationInfo.OwnerActor.Get(), CurrentActivationInfo.OwnerActor->GetActorLocation());
    }
}
