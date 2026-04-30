#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOBAMMOCharacter.generated.h"

class UAbilityComponent;
class UAnimationAsset;
class UCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMesh;
class USpringArmComponent;

UCLASS()
class MOBAMMO_API AMOBAMMOCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMOBAMMOCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Character")
    void ApplyClassVisualsFromPlayerState();

    UFUNCTION(BlueprintCallable, Category="Abilities")
    UAbilityComponent* GetAbilitySystemComponent() const { return AbilityComponent; }

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void ActivateAbility(int32 Slot);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void ActivateAbilityWithTarget(int32 Slot, AActor* Target);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Character")
    void PlayPrimaryAttackAnimation();

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Camera")
    void AdjustCameraZoom(float Value);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UAbilityComponent> AbilityComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UStaticMeshComponent> EquippedWeaponMeshComponent;

    UPROPERTY()
    TObjectPtr<USkeletalMesh> MageMeshAsset;

    UPROPERTY()
    TObjectPtr<UStaticMesh> PrototypeWeaponMeshAsset;

    UPROPERTY()
    TObjectPtr<UAnimationAsset> MageIdleAnimationAsset;

    UPROPERTY()
    TObjectPtr<UAnimationAsset> MageJogForwardAnimationAsset;

    UPROPERTY()
    TObjectPtr<UAnimationAsset> MagePrimaryAttackAnimationAsset;

    UPROPERTY()
    TArray<TObjectPtr<UAnimationAsset>> MageAbilityAnimationAssets;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void TurnAtRate(float Rate);
    void LookUpAtRate(float Rate);
    void CameraZoom(float Value);
    bool CanProcessGameplayInput() const;
    void ApplyAuroraVisuals();
    bool ApplyMageVisuals();
    bool IsMageCharacter() const;
    void PlayMageAbilityAnimation(int32 Slot);
    void RestoreMageIdleAnimation();
    void UpdateMageLocomotionAnimation(bool bForce = false);
    void PlayMageOneShotAnimation(UAnimationAsset* AnimationAsset);
    void ApplyEquipmentVisualsFromPlayerState();
    FString FindEquippedWeaponItemId() const;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    float MinCameraZoom = 280.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    float MaxCameraZoom = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    float CameraZoomStep = 80.0f;

    FString AppliedVisualClassId;
    FString AppliedWeaponItemId;
    FTimerHandle MageIdleTimerHandle;
    TWeakObjectPtr<UAnimationAsset> CurrentMageLoopAnimation;
    bool bMageOneShotAnimationPlaying = false;
    bool bUsingMageAnimBlueprint = false;
    bool bLoggedMissingMageVisuals = false;
};
