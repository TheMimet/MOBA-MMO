#include "MOBAMMOCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MOBAMMOAbilities/Public/AbilityComponent.h"
#include "MOBAMMOPlayerState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const TCHAR* AuroraMeshPath = TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Skins/MoonCrystal/Meshes/Aurora_MoonCrystal.Aurora_MoonCrystal");
    const TCHAR* AuroraAnimBlueprintPath = TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Aurora_AnimBlueprint");
    const TCHAR* AuroraAnimBlueprintClassPath = TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Aurora_AnimBlueprint.Aurora_AnimBlueprint_C");
    const TCHAR* MageMeshPath = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Meshes/Wukong.Wukong");
    const TCHAR* MageAnimBlueprintClassPath = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/AnimBP_Wukong_Rigging.AnimBP_Wukong_Rigging_C");
    const TCHAR* MageIdleAnimationPath = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Idle.Idle");
    const TCHAR* MageJogForwardAnimationPath = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Jog_Fwd.Jog_Fwd");
    const TCHAR* MagePrimaryAttackAnimationPath = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Primary_Melee_A_Slow.Primary_Melee_A_Slow");
    const TCHAR* PrototypeWeaponMeshPath = TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder");
    const TCHAR* MageAbilityAnimationPaths[] = {
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Q_Slam.Q_Slam"),
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Cast.Cast"),
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/RMB_Push.RMB_Push"),
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Emote_StaffSpin.Emote_StaffSpin"),
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Primary_Melee_E_Slow.Primary_Melee_E_Slow"),
        TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/Primary_Melee_Air.Primary_Melee_Air")
    };
}

AMOBAMMOCharacter::AMOBAMMOCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bReplicateUsingRegisteredSubObjectList = true;
    SetReplicateMovement(true);

    // Replicate movement/state at up to 20 Hz; allow the engine to drop to
    // 5 Hz when the actor hasn't moved (saves bandwidth for idle players).
    SetNetUpdateFrequency(20.0f);
    SetMinNetUpdateFrequency(5.0f);

    // Stop replicating to clients beyond 15 000 UU (~150 m).
    // Characters outside this radius are irrelevant to gameplay and don't
    // need to consume bandwidth. (15000² = 225 000 000)
    SetNetCullDistanceSquared(225000000.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 600.0f;
    GetCharacterMovement()->AirControl = 0.2f;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 420.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 55.0f, 75.0f);
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    EquippedWeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedWeaponMesh"));
    EquippedWeaponMeshComponent->SetupAttachment(GetMesh());
    EquippedWeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EquippedWeaponMeshComponent->SetGenerateOverlapEvents(false);
    EquippedWeaponMeshComponent->SetCanEverAffectNavigation(false);
    EquippedWeaponMeshComponent->SetVisibility(false);
    EquippedWeaponMeshComponent->SetRelativeLocation(FVector(28.0f, -24.0f, 112.0f));
    EquippedWeaponMeshComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 82.0f));
    EquippedWeaponMeshComponent->SetRelativeScale3D(FVector(0.055f, 0.055f, 2.15f));

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> AuroraMeshFinder(AuroraMeshPath);
    if (AuroraMeshFinder.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(AuroraMeshFinder.Object);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> AuroraAnimFinder(AuroraAnimBlueprintPath);
    if (AuroraAnimFinder.Succeeded())
    {
        GetMesh()->SetAnimInstanceClass(AuroraAnimFinder.Class);
    }

    AbilityComponent = CreateDefaultSubobject<UAbilityComponent>(TEXT("AbilityComponent"));

    MageMeshAsset = LoadObject<USkeletalMesh>(nullptr, MageMeshPath);
    PrototypeWeaponMeshAsset = LoadObject<UStaticMesh>(nullptr, PrototypeWeaponMeshPath);
    if (PrototypeWeaponMeshAsset)
    {
        EquippedWeaponMeshComponent->SetStaticMesh(PrototypeWeaponMeshAsset);
    }

    MageIdleAnimationAsset = LoadObject<UAnimationAsset>(nullptr, MageIdleAnimationPath);
    MageJogForwardAnimationAsset = LoadObject<UAnimationAsset>(nullptr, MageJogForwardAnimationPath);
    MagePrimaryAttackAnimationAsset = LoadObject<UAnimationAsset>(nullptr, MagePrimaryAttackAnimationPath);
    MageAbilityAnimationAssets.SetNum(UE_ARRAY_COUNT(MageAbilityAnimationPaths));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MageAbilityAnimationPaths); ++Index)
    {
        MageAbilityAnimationAssets[Index] = LoadObject<UAnimationAsset>(nullptr, MageAbilityAnimationPaths[Index]);
    }
}

void AMOBAMMOCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyClassVisualsFromPlayerState();
}

void AMOBAMMOCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ApplyClassVisualsFromPlayerState();
    ApplyEquipmentVisualsFromPlayerState();
    UpdateMageLocomotionAnimation();
}

void AMOBAMMOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AMOBAMMOCharacter::MoveForward);
    PlayerInputComponent->BindAxis("Move Right / Left", this, &AMOBAMMOCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
    PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
    PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMOBAMMOCharacter::TurnAtRate);
    PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AMOBAMMOCharacter::LookUpAtRate);
    PlayerInputComponent->BindAxis("Camera Zoom", this, &AMOBAMMOCharacter::CameraZoom);

    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

    PlayerInputComponent->BindAction("Ability1", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility1);
    PlayerInputComponent->BindAction("Ability2", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility2);
    PlayerInputComponent->BindAction("Ability3", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility3);
    PlayerInputComponent->BindAction("Ability4", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility4);
    PlayerInputComponent->BindAction("Ability5", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility5);
    PlayerInputComponent->BindAction("Ability6", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility6);
    PlayerInputComponent->BindAction("PrimaryAttack", IE_Pressed, this, &AMOBAMMOCharacter::PlayPrimaryAttackAnimation);
}

void AMOBAMMOCharacter::MoveForward(float Value)
{
    if (Controller && Value != 0.0f && CanProcessGameplayInput())
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AMOBAMMOCharacter::MoveRight(float Value)
{
    if (Controller && Value != 0.0f && CanProcessGameplayInput())
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
}

void AMOBAMMOCharacter::TurnAtRate(float Rate)
{
    AddControllerYawInput(Rate * TurnRate * GetWorld()->GetDeltaSeconds());
}

void AMOBAMMOCharacter::LookUpAtRate(float Rate)
{
    AddControllerPitchInput(Rate * LookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMOBAMMOCharacter::CameraZoom(float Value)
{
    AdjustCameraZoom(Value);
}

void AMOBAMMOCharacter::AdjustCameraZoom(float Value)
{
    if (!CameraBoom || FMath::IsNearlyZero(Value))
    {
        return;
    }

    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - (Value * CameraZoomStep),
        MinCameraZoom,
        MaxCameraZoom
    );
}

void AMOBAMMOCharacter::ActivateAbility(int32 Slot)
{
    if (AbilityComponent && CanProcessGameplayInput())
    {
        PlayMageAbilityAnimation(Slot);
        AbilityComponent->TryActivateAbility(Slot);
    }
}

void AMOBAMMOCharacter::ActivateAbilityWithTarget(int32 Slot, AActor* Target)
{
    if (AbilityComponent && CanProcessGameplayInput())
    {
        PlayMageAbilityAnimation(Slot);
        AbilityComponent->TryActivateAbilityWithTarget(Slot, Target);
    }
}

bool AMOBAMMOCharacter::CanProcessGameplayInput() const
{
    const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
    return MOBAPlayerState && !MOBAPlayerState->GetSessionId().TrimStartAndEnd().IsEmpty() && !MOBAPlayerState->GetCharacterId().TrimStartAndEnd().IsEmpty();
}

void AMOBAMMOCharacter::ApplyClassVisualsFromPlayerState()
{
    const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
    if (!MOBAPlayerState)
    {
        return;
    }

    const FString ClassId = MOBAPlayerState->GetClassId().TrimStartAndEnd().ToLower();
    if (ClassId.IsEmpty() || ClassId == AppliedVisualClassId)
    {
        return;
    }

    if (ClassId == TEXT("mage"))
    {
        if (ApplyMageVisuals())
        {
            AppliedVisualClassId = ClassId;
        }
        return;
    }

    ApplyAuroraVisuals();
    AppliedVisualClassId = ClassId;
}

void AMOBAMMOCharacter::ApplyAuroraVisuals()
{
    bUsingMageAnimBlueprint = false;
    bMageOneShotAnimationPlaying = false;
    CurrentMageLoopAnimation = nullptr;

    if (USkeletalMesh* AuroraMesh = LoadObject<USkeletalMesh>(nullptr, AuroraMeshPath))
    {
        GetMesh()->SetSkeletalMesh(AuroraMesh);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        GetMesh()->SetRelativeScale3D(FVector(1.0f));
    }

    if (UClass* AuroraAnimClass = StaticLoadClass(UAnimInstance::StaticClass(), nullptr, AuroraAnimBlueprintClassPath))
    {
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(AuroraAnimClass);
    }
}

bool AMOBAMMOCharacter::ApplyMageVisuals()
{
    USkeletalMesh* MageMesh = MageMeshAsset.Get();
    if (!MageMesh)
    {
        MageMesh = LoadObject<USkeletalMesh>(nullptr, MageMeshPath);
    }

    if (!MageMesh)
    {
        if (!bLoggedMissingMageVisuals)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CharacterVisuals] Wukong mage mesh not found. Expected %s"), MageMeshPath);
            bLoggedMissingMageVisuals = true;
        }
        return false;
    }

    GetMesh()->SetSkeletalMesh(MageMesh);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetRelativeScale3D(FVector(1.0f));

    if (UClass* WukongAnimClass = StaticLoadClass(UAnimInstance::StaticClass(), nullptr, MageAnimBlueprintClassPath))
    {
        bUsingMageAnimBlueprint = true;
        bMageOneShotAnimationPlaying = false;
        CurrentMageLoopAnimation = nullptr;
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(WukongAnimClass);
        return true;
    }

    bUsingMageAnimBlueprint = false;
    if (UAnimationAsset* IdleAnimation = MageIdleAnimationAsset.Get())
    {
        CurrentMageLoopAnimation = nullptr;
        bMageOneShotAnimationPlaying = false;
        UpdateMageLocomotionAnimation(true);
    }
    else
    {
        GetMesh()->SetAnimInstanceClass(nullptr);
    }

    return true;
}

bool AMOBAMMOCharacter::IsMageCharacter() const
{
    const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
    return MOBAPlayerState && MOBAPlayerState->GetClassId().TrimStartAndEnd().ToLower() == TEXT("mage");
}

void AMOBAMMOCharacter::PlayMageAbilityAnimation(int32 Slot)
{
    if (!IsMageCharacter() || !MageAbilityAnimationAssets.IsValidIndex(Slot))
    {
        return;
    }

    if (UAnimationAsset* AbilityAnimation = MageAbilityAnimationAssets[Slot].Get())
    {
        PlayMageOneShotAnimation(AbilityAnimation);

        const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(AbilityAnimation);
        const float RestoreDelay = Sequence ? FMath::Max(Sequence->GetPlayLength(), 0.2f) : 0.8f;
        GetWorldTimerManager().SetTimer(MageIdleTimerHandle, this, &AMOBAMMOCharacter::RestoreMageIdleAnimation, RestoreDelay, false);
    }
}

void AMOBAMMOCharacter::PlayPrimaryAttackAnimation()
{
    if (!IsMageCharacter())
    {
        return;
    }

    if (UAnimationAsset* PrimaryAttack = MagePrimaryAttackAnimationAsset.Get())
    {
        PlayMageOneShotAnimation(PrimaryAttack);

        const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PrimaryAttack);
        const float RestoreDelay = Sequence ? FMath::Max(Sequence->GetPlayLength(), 0.2f) : 0.75f;
        GetWorldTimerManager().SetTimer(MageIdleTimerHandle, this, &AMOBAMMOCharacter::RestoreMageIdleAnimation, RestoreDelay, false);
    }
}

void AMOBAMMOCharacter::RestoreMageIdleAnimation()
{
    if (!IsMageCharacter())
    {
        return;
    }

    bMageOneShotAnimationPlaying = false;
    if (bUsingMageAnimBlueprint)
    {
        if (UClass* WukongAnimClass = StaticLoadClass(UAnimInstance::StaticClass(), nullptr, MageAnimBlueprintClassPath))
        {
            GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            GetMesh()->SetAnimInstanceClass(WukongAnimClass);
            return;
        }
    }

    UpdateMageLocomotionAnimation(true);
}

void AMOBAMMOCharacter::UpdateMageLocomotionAnimation(bool bForce)
{
    if (!IsMageCharacter() || bMageOneShotAnimationPlaying || bUsingMageAnimBlueprint)
    {
        return;
    }

    const float Speed2D = GetVelocity().Size2D();
    UAnimationAsset* DesiredLoop = Speed2D > 12.0f ? MageJogForwardAnimationAsset.Get() : MageIdleAnimationAsset.Get();
    if (!DesiredLoop)
    {
        DesiredLoop = MageIdleAnimationAsset.Get();
    }

    if (!DesiredLoop || (!bForce && CurrentMageLoopAnimation.Get() == DesiredLoop))
    {
        return;
    }

    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetAnimation(DesiredLoop);
    GetMesh()->Play(true);
    CurrentMageLoopAnimation = DesiredLoop;
}

void AMOBAMMOCharacter::PlayMageOneShotAnimation(UAnimationAsset* AnimationAsset)
{
    if (!AnimationAsset)
    {
        return;
    }

    bMageOneShotAnimationPlaying = true;
    CurrentMageLoopAnimation = nullptr;
    GetWorldTimerManager().ClearTimer(MageIdleTimerHandle);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->PlayAnimation(AnimationAsset, false);
}

FString AMOBAMMOCharacter::FindEquippedWeaponItemId() const
{
    const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
    if (!MOBAPlayerState)
    {
        return FString();
    }

    for (const FMOBAMMOInventoryItem& Item : MOBAPlayerState->GetInventoryItems())
    {
        if (Item.SlotIndex == 0 && (Item.ItemId == TEXT("iron_sword") || Item.ItemId == TEXT("crystal_staff")))
        {
            return Item.ItemId;
        }
    }

    return FString();
}

void AMOBAMMOCharacter::ApplyEquipmentVisualsFromPlayerState()
{
    if (!EquippedWeaponMeshComponent)
    {
        return;
    }

    if (IsMageCharacter())
    {
        AppliedWeaponItemId.Reset();
        EquippedWeaponMeshComponent->SetVisibility(false, true);
        return;
    }

    const FString EquippedWeaponItemId = FindEquippedWeaponItemId();
    if (EquippedWeaponItemId == AppliedWeaponItemId)
    {
        return;
    }

    AppliedWeaponItemId = EquippedWeaponItemId;
    const bool bHasWeaponEquipped = !AppliedWeaponItemId.IsEmpty();
    if (bHasWeaponEquipped && !EquippedWeaponMeshComponent->GetStaticMesh())
    {
        if (!PrototypeWeaponMeshAsset)
        {
            PrototypeWeaponMeshAsset = LoadObject<UStaticMesh>(nullptr, PrototypeWeaponMeshPath);
        }
        EquippedWeaponMeshComponent->SetStaticMesh(PrototypeWeaponMeshAsset);
    }

    const bool bIsStaff = AppliedWeaponItemId == TEXT("crystal_staff");
    EquippedWeaponMeshComponent->SetRelativeLocation(bIsStaff ? FVector(34.0f, -28.0f, 118.0f) : FVector(24.0f, -22.0f, 104.0f));
    EquippedWeaponMeshComponent->SetRelativeRotation(bIsStaff ? FRotator(0.0f, 0.0f, 86.0f) : FRotator(0.0f, 0.0f, 72.0f));
    EquippedWeaponMeshComponent->SetRelativeScale3D(bIsStaff ? FVector(0.045f, 0.045f, 2.45f) : FVector(0.075f, 0.075f, 1.45f));
    EquippedWeaponMeshComponent->SetVisibility(bHasWeaponEquipped && EquippedWeaponMeshComponent->GetStaticMesh() != nullptr, true);
}

void AMOBAMMOCharacter::InputAbility1() { ActivateAbility(0); }
void AMOBAMMOCharacter::InputAbility2() { ActivateAbility(1); }
void AMOBAMMOCharacter::InputAbility3() { ActivateAbility(2); }
void AMOBAMMOCharacter::InputAbility4() { ActivateAbility(3); }
void AMOBAMMOCharacter::InputAbility5() { ActivateAbility(4); }
void AMOBAMMOCharacter::InputAbility6() { ActivateAbility(5); }
