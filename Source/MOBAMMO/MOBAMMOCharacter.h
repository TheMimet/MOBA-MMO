#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOBAMMOCharacter.generated.h"

class UAbilityComponent;
class UCameraComponent;
class USpringArmComponent;

UCLASS()
class MOBAMMO_API AMOBAMMOCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMOBAMMOCharacter();

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    UAbilityComponent* GetAbilitySystemComponent() const { return AbilityComponent; }

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void ActivateAbility(int32 Slot);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void ActivateAbilityWithTarget(int32 Slot, AActor* Target);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UAbilityComponent> AbilityComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void TurnAtRate(float Rate);
    void LookUpAtRate(float Rate);

    UFUNCTION()
    void InputAbility1();

    UFUNCTION()
    void InputAbility2();

    UFUNCTION()
    void InputAbility3();

    UFUNCTION()
    void InputAbility4();

    UFUNCTION()
    void InputAbility5();

    UFUNCTION()
    void InputAbility6();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    float TurnRate = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    float LookUpRate = 45.0f;
};
