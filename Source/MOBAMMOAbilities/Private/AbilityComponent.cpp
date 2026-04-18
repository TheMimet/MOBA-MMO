#include "AbilityComponent.h"
#include "AbilityBase.h"
#include "AbilityDataAsset.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

void UAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAbilityComponent, ActiveAbilities);
}

UAbilityComponent::UAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    bWantsInitializeComponent = true;
    SetIsReplicatedByDefault(true);
}

void UAbilityComponent::InitializeComponent()
{
    Super::InitializeComponent();

    AbilitySlots.Reset();
    ActiveAbilities.Reset();
    AbilitySlots.AddDefaulted(MaxAbilitySlots);
    ActiveAbilities.AddDefaulted(MaxAbilitySlots);

    for (int32 i = 0; i < MaxAbilitySlots; ++i)
    {
        ActiveAbilities[i].Slot = i;
        ActiveAbilities[i].CooldownRemaining = 0.0f;
    }
}

void UAbilityComponent::UninitializeComponent()
{
    CancelAllAbilities();

    CooldownTimers.Empty();
    ChannelTimers.Empty();

    Super::UninitializeComponent();
}

void UAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ProcessCooldown(DeltaTime);
    ProcessChanneling(DeltaTime);
    ProcessManaRegen(DeltaTime);

    for (int32 i = 0; i < AbilitySlots.Num(); ++i)
    {
        if (UAbilityBase* Ability = AbilitySlots[i])
        {
            if (Ability->GetClass()->GetDefaultObject<UAbilityBase>()->bIsActive)
            {
                Ability->Tick(DeltaTime);
            }
        }
    }
}

void UAbilityComponent::OnRep_ActiveAbilities()
{
}

bool UAbilityComponent::TryActivateAbility(int32 Slot)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    FAbilityActivationInfo Info;
    Info.OwnerActor = Owner;
    Info.StartLocation = Owner->GetActorLocation();
    Info.LookDirection = Owner->GetActorRotation();

    if (Owner->GetClass()->IsChildOf(ACharacter::StaticClass()))
    {
        ACharacter* Character = Cast<ACharacter>(Owner);
        if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
        {
            FVector WorldLocation, WorldDirection;
            if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
            {
                FVector TargetLocation = WorldLocation + WorldDirection * 10000.0f;
                FVector GroundLocation(TargetLocation.X, TargetLocation.Y, Owner->GetActorLocation().Z);
                Info.TargetLocation = GroundLocation;

                FHitResult Hit;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(Owner);
                if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLocation, TargetLocation, ECC_Visibility, Params))
                {
                    Info.TargetActor = Hit.GetActor();
                }
            }
        }
    }

    return TryActivateAbilityAtLocation(Slot, Info.TargetLocation);
}

bool UAbilityComponent::TryActivateAbilityWithTarget(int32 Slot, AActor* TargetActor)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    FAbilityActivationInfo Info;
    Info.OwnerActor = GetOwner();
    Info.TargetActor = TargetActor;
    Info.StartLocation = GetOwner()->GetActorLocation();
    Info.TargetLocation = TargetActor ? TargetActor->GetActorLocation() : FVector::ZeroVector;
    Info.LookDirection = GetOwner()->GetActorRotation();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ServerTryActivateAbility(Slot, Info);
        return true;
    }

    ServerTryActivateAbility(Slot, Info);
    return true;
}

bool UAbilityComponent::TryActivateAbilityAtLocation(int32 Slot, const FVector& Location)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    FAbilityActivationInfo Info;
    Info.OwnerActor = GetOwner();
    Info.StartLocation = GetOwner()->GetActorLocation();
    Info.TargetLocation = Location;
    Info.LookDirection = GetOwner()->GetActorRotation();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ServerTryActivateAbility(Slot, Info);
        return true;
    }

    ServerTryActivateAbility(Slot, Info);
    return true;
}

bool UAbilityComponent::TryActivateAbilityInDirection(int32 Slot, const FRotator& Direction)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    FAbilityActivationInfo Info;
    Info.OwnerActor = GetOwner();
    Info.StartLocation = GetOwner()->GetActorLocation();
    Info.LookDirection = Direction;

    FVector DirectionVec = Direction.Vector();
    Info.TargetLocation = Info.StartLocation + DirectionVec * 1000.0f;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ServerTryActivateAbility(Slot, Info);
        return true;
    }

    ServerTryActivateAbility(Slot, Info);
    return true;
}

void UAbilityComponent::ServerTryActivateAbility_Implementation(int32 Slot, const FAbilityActivationInfo& Info)
{
    if (!ValidateAbilityActivation(Slot, Info))
    {
        return;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (!Ability)
    {
        return;
    }

    ConsumeManaCost(Slot);
    StartCooldown(Slot);

    Ability->Initialize(this);
    Ability->Activate(Info);

    ActiveAbilities[Slot].bIsChanneling = (Ability->AbilityType == EAbilityType::Channel);
    ActiveAbilities[Slot].StartTime = GetWorld()->GetTimeSeconds();
    ActiveAbilities[Slot].TargetActor = Info.TargetActor;
    ActiveAbilities[Slot].TargetLocation = Info.TargetLocation;

    if (Ability->AbilityType == EAbilityType::Channel)
    {
        CurrentlyChannelingSlot = Slot;
        Ability->TicksRemaining = Ability->Channel.MaxTicks;

        FTimerManager& TimerManager = GetWorld()->GetTimerManager();
        FTimerHandle Handle;
        TimerManager.SetTimer(Handle, [this, Slot]()
        {
            if (UAbilityBase* Ab = AbilitySlots[Slot])
            {
                Ab->OnChannelTick();
            }
        }, Ability->Channel.TickInterval, true);

        ChannelTimers.Add(Slot, Handle);
    }

    OnAbilityActivated.Broadcast(Slot, Ability);
}

bool UAbilityComponent::ValidateAbilityActivation(int32 Slot, const FAbilityActivationInfo& Info) const
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    if (!AbilitySlots[Slot])
    {
        return false;
    }

    if (IsOnCooldown(Slot))
    {
        return false;
    }

    if (!HasEnoughMana(Slot))
    {
        return false;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (!Ability->CanActivate(Info))
    {
        return false;
    }

    return true;
}

bool UAbilityComponent::HasEnoughMana(int32 Slot) const
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return false;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (!Ability)
    {
        return false;
    }

    return CurrentMana >= Ability->Cost.ManaCost;
}

void UAbilityComponent::ConsumeManaCost(int32 Slot)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (Ability)
    {
        CurrentMana = FMath::Max(0.0f, CurrentMana - Ability->Cost.ManaCost);
        OnManaChanged.Broadcast(CurrentMana, MaxMana);
    }
}

void UAbilityComponent::StartCooldown(int32 Slot)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (!Ability)
    {
        return;
    }

    ActiveAbilities[Slot].CooldownRemaining = Ability->Cost.Cooldown;
    ActiveAbilities[Slot].EndTime = GetWorld()->GetTimeSeconds() + Ability->Cost.Cooldown;
}

void UAbilityComponent::ProcessCooldown(float DeltaTime)
{
    for (int32 i = 0; i < ActiveAbilities.Num(); ++i)
    {
        if (ActiveAbilities[i].CooldownRemaining > 0.0f)
        {
            ActiveAbilities[i].CooldownRemaining -= DeltaTime;

            if (ActiveAbilities[i].CooldownRemaining <= 0.0f)
            {
                ActiveAbilities[i].CooldownRemaining = 0.0f;

                UAbilityBase* Ability = AbilitySlots.IsValidIndex(i) ? AbilitySlots[i] : nullptr;
                OnCooldownReady.Broadcast(i);

                FTimerHandle* Handle = CooldownTimers.Find(i);
                if (Handle)
                {
                    GetWorld()->GetTimerManager().ClearTimer(*Handle);
                    CooldownTimers.Remove(i);
                }
            }
        }
    }
}

void UAbilityComponent::ProcessChanneling(float DeltaTime)
{
}

void UAbilityComponent::ProcessManaRegen(float DeltaTime)
{
    if (ManaRegenRate <= 0.0f)
    {
        return;
    }

    ManaRegenAccumulator += ManaRegenRate * DeltaTime;

    if (ManaRegenAccumulator >= 1.0f)
    {
        int32 RegenAmount = FMath::FloorToInt(ManaRegenAccumulator);
        ManaRegenAccumulator -= RegenAmount;

        float OldMana = CurrentMana;
        CurrentMana = FMath::Min(MaxMana, CurrentMana + RegenAmount);

        if (CurrentMana != OldMana)
        {
            OnManaChanged.Broadcast(CurrentMana, MaxMana);
        }
    }
}

void UAbilityComponent::CancelAbility(int32 Slot)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return;
    }

    ServerCancelAbility(Slot);
}

void UAbilityComponent::ServerCancelAbility_Implementation(int32 Slot)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return;
    }

    UAbilityBase* Ability = AbilitySlots[Slot];
    if (Ability)
    {
        Ability->CancelAbility();
    }

    FTimerHandle* ChannelHandle = ChannelTimers.Find(Slot);
    if (ChannelHandle)
    {
        GetWorld()->GetTimerManager().ClearTimer(*ChannelHandle);
        ChannelTimers.Remove(Slot);
    }

    if (CurrentlyChannelingSlot == Slot)
    {
        CurrentlyChannelingSlot = -1;
    }

    OnAbilityEnded.Broadcast(Slot, false);
}

void UAbilityComponent::CancelAllAbilities()
{
    for (int32 i = 0; i < AbilitySlots.Num(); ++i)
    {
        CancelAbility(i);
    }
}

void UAbilityComponent::InterruptChanneling()
{
    if (CurrentlyChannelingSlot >= 0 && CurrentlyChannelingSlot < AbilitySlots.Num())
    {
        CancelAbility(CurrentlyChannelingSlot);
    }
}

UAbilityBase* UAbilityComponent::GetAbilityInSlot(int32 Slot) const
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return nullptr;
    }
    return AbilitySlots[Slot];
}

float UAbilityComponent::GetCooldownRemaining(int32 Slot) const
{
    if (Slot < 0 || Slot >= ActiveAbilities.Num())
    {
        return 0.0f;
    }
    return ActiveAbilities[Slot].CooldownRemaining;
}

float UAbilityComponent::GetMaxCooldown(int32 Slot) const
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return 0.0f;
    }
    UAbilityBase* Ability = AbilitySlots[Slot];
    return Ability ? Ability->Cost.Cooldown : 0.0f;
}

bool UAbilityComponent::IsOnCooldown(int32 Slot) const
{
    return GetCooldownRemaining(Slot) > 0.0f;
}

bool UAbilityComponent::IsCasting(int32 Slot) const
{
    if (Slot < 0 || Slot >= ActiveAbilities.Num())
    {
        return false;
    }
    return ActiveAbilities[Slot].bIsChanneling;
}

void UAbilityComponent::SetAbilityInSlot(int32 Slot, UAbilityBase* Ability)
{
    if (Slot < 0 || Slot >= AbilitySlots.Num())
    {
        return;
    }

    if (IsCasting(Slot) || IsOnCooldown(Slot))
    {
        return;
    }

    AbilitySlots[Slot] = Ability;

    if (Ability)
    {
        Ability->Initialize(this);
    }
}

void UAbilityComponent::ClearAbilityInSlot(int32 Slot)
{
    SetAbilityInSlot(Slot, nullptr);
}

void UAbilityComponent::LoadAbilitiesFromDataAsset(UAbilityDataAsset* DataAsset)
{
    if (!DataAsset)
    {
        return;
    }

    CurrentMana = DataAsset->StartingMana;
    MaxMana = DataAsset->MaxMana;
    ManaRegenRate = DataAsset->ManaRegenRate;

    for (int32 i = 0; i < AbilitySlots.Num(); ++i)
    {
        AbilitySlots[i] = nullptr;
    }

    for (int32 i = 0; i < DataAsset->Abilities.Num() && i < AbilitySlots.Num(); ++i)
    {
        if (TSubclassOf<UAbilityBase> AbilityClass = DataAsset->Abilities[i])
        {
            UAbilityBase* Ability = NewObject<UAbilityBase>(GetOwner(), AbilityClass);
            SetAbilityInSlot(i, Ability);
        }
    }
}
