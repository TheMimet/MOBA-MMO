#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChangedDelegate, class UHealthComponent*, HealthComp, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathDelegate, class UHealthComponent*, HealthComp, class AActor*, InstigatorActor);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MOBAMMOAI_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    UFUNCTION(BlueprintCallable, Category="Health")
    float GetHealth() const { return Health; }

    UFUNCTION(BlueprintCallable, Category="Health")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintCallable, Category="Health")
    bool IsDead() const { return Health <= 0.0f; }

    UFUNCTION(BlueprintCallable, Category="Health")
    void Heal(float HealAmount);

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHealthChangedDelegate OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnDeathDelegate OnDeath;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Health, Category="Health")
    float Health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Health")
    float MaxHealth;

    UFUNCTION()
    void OnRep_Health(float OldHealth);

    UFUNCTION()
    void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

    bool bIsDead;
};
