#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    MaxHealth = 100.0f;
    Health = MaxHealth;
    bIsDead = false;
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHealthComponent, Health);
    DOREPLIFETIME(UHealthComponent, MaxHealth);
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    bIsDead = Health <= 0.0f;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
    }
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
    const bool bWasDead = OldHealth <= 0.0f;
    bIsDead = Health <= 0.0f;
    OnHealthChanged.Broadcast(this, Health, MaxHealth);

    if (!bWasDead && bIsDead)
    {
        OnDeath.Broadcast(this, nullptr);
    }
}

void UHealthComponent::Heal(float HealAmount)
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || HealAmount <= 0.0f || Health <= 0.0f)
    {
        return;
    }

    Health = FMath::Clamp(Health + HealAmount, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(this, Health, MaxHealth);

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }
}

void UHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || Damage <= 0.0f || bIsDead)
    {
        return;
    }

    Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(this, Health, MaxHealth);

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }

    if (Health <= 0.0f)
    {
        bIsDead = true;
        OnDeath.Broadcast(this, DamageCauser);
    }
}
