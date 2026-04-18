#include "ProjectileAbility.h"
#include "AbilityComponent.h"

UProjectileAbility::UProjectileAbility()
{
    AbilityType = EAbilityType::Projectile;
}

void UProjectileAbility::Activate(const FAbilityActivationInfo& Info)
{
    Super::Activate(Info);
    SpawnProjectile(Info);
}

bool UProjectileAbility::CanActivate(const FAbilityActivationInfo& Info) const
{
    if (!Super::CanActivate(Info))
    {
        return false;
    }

    return true;
}

void UProjectileAbility::SpawnProjectile(const FAbilityActivationInfo& Info)
{
    if (!ProjectileClass || !OwnerComponent.IsValid())
    {
        EndAbility(false);
        return;
    }

    AActor* SpawnedProjectile = OwnerComponent->GetWorld()->SpawnActor<AActor>(
        ProjectileClass,
        Info.StartLocation,
        FRotator(Info.LookDirection.Pitch, Info.LookDirection.Yaw, 0.0f)
    );

    if (SpawnedProjectile)
    {
        K2_OnProjectileSpawned(SpawnedProjectile);
    }
    else
    {
        EndAbility(false);
    }
}
