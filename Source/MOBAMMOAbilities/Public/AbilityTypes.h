#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.generated.h"

class AActor;
class UGameplayAbility;

UENUM(BlueprintType)
enum class EAbilityType : uint8
{
    Instant    UMETA(DisplayName="Instant"),
    Channel    UMETA(DisplayName="Channel"),
    Projectile UMETA(DisplayName="Projectile"),
    AoE        UMETA(DisplayName="Area of Effect")
};

UENUM(BlueprintType)
enum class EAbilityTargetType : uint8
{
    None          UMETA(DisplayName="None"),
    Self          UMETA(DisplayName="Self"),
    SingleTarget  UMETA(DisplayName="Single Target"),
    GroundLocation UMETA(DisplayName="Ground Location"),
    Direction     UMETA(DisplayName="Direction")
};

UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
    None          UMETA(DisplayName="None"),
    Damage        UMETA(DisplayName="Damage"),
    Heal          UMETA(DisplayName="Heal"),
    Buff          UMETA(DisplayName="Buff"),
    Debuff        UMETA(DisplayName="Debuff"),
    Shield        UMETA(DisplayName="Shield"),
    CrowdControl  UMETA(DisplayName="Crowd Control"),
    Teleport      UMETA(DisplayName="Teleport"),
    Dash          UMETA(DisplayName="Dash")
};

USTRUCT(BlueprintType)
struct FAbilityCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ManaCost = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HealthCost = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Cooldown = 0.0f;
};

USTRUCT(BlueprintType)
struct FAbilityDamage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ScalingFactor = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DamageType = NAME_None;
};

USTRUCT(BlueprintType)
struct FAbilityAoE
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InnerRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bFullDamageInsideInner = true;
};

USTRUCT(BlueprintType)
struct FAbilityProjectile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Speed = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxDistance = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHomingOnTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileRadius = 20.0f;
};

USTRUCT(BlueprintType)
struct FAbilityChannel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChannelDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TickInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxTicks = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInterruptible = true;
};

USTRUCT(BlueprintType)
struct FAbilityRequirement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinManaCost = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinHealthPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequiresGroundLocation = false;
};

USTRUCT()
struct FActiveAbility
{
    GENERATED_BODY()

    int32 Slot = 0;
    float StartTime = 0.0f;
    float EndTime = 0.0f;
    float CooldownRemaining = 0.0f;
    bool bIsChanneling = false;
    TWeakObjectPtr<AActor> TargetActor;
    FVector TargetLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FAbilityActivationInfo
{
    GENERATED_BODY()

    TWeakObjectPtr<AActor> OwnerActor;
    TWeakObjectPtr<AActor> TargetActor;
    FVector TargetLocation = FVector::ZeroVector;
    FVector StartLocation = FVector::ZeroVector;
    FRotator LookDirection = FRotator::ZeroRotator;
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAbilityActivated, int32, Slot, const FAbilityActivationInfo&, Info);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAbilityEnded, int32, Slot, bool, bWasSuccessful);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAbilityCooldownReady, int32, Slot, float, CooldownDuration, float, ManaCost);
