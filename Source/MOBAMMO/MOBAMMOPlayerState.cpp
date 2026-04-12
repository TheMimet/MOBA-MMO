#include "MOBAMMOPlayerState.h"

#include "Net/UnrealNetwork.h"

AMOBAMMOPlayerState::AMOBAMMOPlayerState()
{
    bReplicates = true;
}

void AMOBAMMOPlayerState::ApplySessionIdentity(const FString& InAccountId, const FString& InCharacterId, const FString& InCharacterName, const FString& InClassId, int32 InLevel)
{
    if (!HasAuthority())
    {
        return;
    }

    AccountId = InAccountId;
    CharacterId = InCharacterId;
    CharacterName = InCharacterName;
    ClassId = InClassId;
    CharacterLevel = FMath::Max(1, InLevel);
    SetPlayerName(CharacterName.IsEmpty() ? TEXT("Player") : CharacterName);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::InitializeAttributes(float InMaxHealth, float InMaxMana)
{
    if (!HasAuthority())
    {
        return;
    }

    MaxHealth = FMath::Max(1.0f, InMaxHealth);
    CurrentHealth = MaxHealth;
    MaxMana = FMath::Max(0.0f, InMaxMana);
    CurrentMana = MaxMana;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetCurrentHealth(float NewHealth)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetCurrentMana(float NewMana)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentMana = FMath::Clamp(NewMana, 0.0f, MaxMana);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

float AMOBAMMOPlayerState::ApplyDamage(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);

    if (!FMath::IsNearlyEqual(CurrentHealth, PreviousHealth))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return PreviousHealth - CurrentHealth;
}

float AMOBAMMOPlayerState::ApplyHealing(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    if (!FMath::IsNearlyEqual(CurrentHealth, PreviousHealth))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return CurrentHealth - PreviousHealth;
}

bool AMOBAMMOPlayerState::ConsumeMana(float Amount)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (Amount <= 0.0f)
    {
        return true;
    }

    if (CurrentMana < Amount)
    {
        return false;
    }

    CurrentMana = FMath::Clamp(CurrentMana - Amount, 0.0f, MaxMana);
    ForceNetUpdate();
    BroadcastStateUpdated();
    return true;
}

float AMOBAMMOPlayerState::RestoreMana(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousMana = CurrentMana;
    CurrentMana = FMath::Clamp(CurrentMana + Amount, 0.0f, MaxMana);

    if (!FMath::IsNearlyEqual(CurrentMana, PreviousMana))
    {
        ForceNetUpdate();
        BroadcastStateUpdated();
    }

    return CurrentMana - PreviousMana;
}

void AMOBAMMOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOPlayerState, AccountId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterName);
    DOREPLIFETIME(AMOBAMMOPlayerState, ClassId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterLevel);
    DOREPLIFETIME(AMOBAMMOPlayerState, CurrentHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, MaxHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, CurrentMana);
    DOREPLIFETIME(AMOBAMMOPlayerState, MaxMana);
}

void AMOBAMMOPlayerState::OnRep_PlayerIdentity()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_Attributes()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::BroadcastStateUpdated()
{
    OnReplicatedStateUpdated.Broadcast();
}
