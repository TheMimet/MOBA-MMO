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
