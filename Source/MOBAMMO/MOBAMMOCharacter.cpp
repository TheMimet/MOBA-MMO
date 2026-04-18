#include "MOBAMMOCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MOBAMMOAbilities/Public/AbilityComponent.h"
#include "UObject/ConstructorHelpers.h"

AMOBAMMOCharacter::AMOBAMMOCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bReplicateUsingRegisteredSubObjectList = true;
    SetReplicateMovement(true);

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

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> AuroraMeshFinder(TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Skins/MoonCrystal/Meshes/Aurora_MoonCrystal.Aurora_MoonCrystal"));
    if (AuroraMeshFinder.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(AuroraMeshFinder.Object);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> AuroraAnimFinder(TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Aurora_AnimBlueprint"));
    if (AuroraAnimFinder.Succeeded())
    {
        GetMesh()->SetAnimInstanceClass(AuroraAnimFinder.Class);
    }

    AbilityComponent = CreateDefaultSubobject<UAbilityComponent>(TEXT("AbilityComponent"));
}

void AMOBAMMOCharacter::BeginPlay()
{
    Super::BeginPlay();
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

    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

    PlayerInputComponent->BindAction("Ability1", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility1);
    PlayerInputComponent->BindAction("Ability2", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility2);
    PlayerInputComponent->BindAction("Ability3", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility3);
    PlayerInputComponent->BindAction("Ability4", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility4);
    PlayerInputComponent->BindAction("Ability5", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility5);
    PlayerInputComponent->BindAction("Ability6", IE_Pressed, this, &AMOBAMMOCharacter::InputAbility6);
}

void AMOBAMMOCharacter::MoveForward(float Value)
{
    if (Controller && Value != 0.0f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AMOBAMMOCharacter::MoveRight(float Value)
{
    if (Controller && Value != 0.0f)
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

void AMOBAMMOCharacter::ActivateAbility(int32 Slot)
{
    if (AbilityComponent)
    {
        AbilityComponent->TryActivateAbility(Slot);
    }
}

void AMOBAMMOCharacter::ActivateAbilityWithTarget(int32 Slot, AActor* Target)
{
    if (AbilityComponent)
    {
        AbilityComponent->TryActivateAbilityWithTarget(Slot, Target);
    }
}

void AMOBAMMOCharacter::InputAbility1() { ActivateAbility(0); }
void AMOBAMMOCharacter::InputAbility2() { ActivateAbility(1); }
void AMOBAMMOCharacter::InputAbility3() { ActivateAbility(2); }
void AMOBAMMOCharacter::InputAbility4() { ActivateAbility(3); }
void AMOBAMMOCharacter::InputAbility5() { ActivateAbility(4); }
void AMOBAMMOCharacter::InputAbility6() { ActivateAbility(5); }
