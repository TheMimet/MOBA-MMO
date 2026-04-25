#include "MOBAMMOPlayerState.h"

#include "Net/UnrealNetwork.h"

AMOBAMMOPlayerState::AMOBAMMOPlayerState()
{
    bReplicates = true;
}

void AMOBAMMOPlayerState::ApplySessionIdentity(const FString& InAccountId, const FString& InCharacterId, const FString& InSessionId, const FString& InCharacterName, const FString& InClassId, int32 InLevel)
{
    if (!HasAuthority())
    {
        return;
    }

    AccountId = InAccountId;
    CharacterId = InCharacterId;
    SessionId = InSessionId;
    CharacterName = InCharacterName;
    ClassId = InClassId;
    CharacterLevel = FMath::Max(1, InLevel);
    PersistenceStatus = TEXT("Ready");
    PersistenceErrorMessage.Reset();
    SetPlayerName(CharacterName.IsEmpty() ? TEXT("Player") : CharacterName);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetPersistenceStatus(const FString& InStatus, const FString& InErrorMessage)
{
    if (!HasAuthority())
    {
        return;
    }

    PersistenceStatus = InStatus.IsEmpty() ? TEXT("Ready") : InStatus;
    PersistenceErrorMessage = InErrorMessage;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ApplyPersistentCharacterState(int32 InExperience, const FVector& InSavedWorldPosition, float InCurrentHealth, float InMaxHealth, float InCurrentMana, float InMaxMana, int32 InKillCount, int32 InDeathCount)
{
    if (!HasAuthority())
    {
        return;
    }

    CharacterExperience = FMath::Max(0, InExperience);
    SavedWorldPosition = InSavedWorldPosition;
    MaxHealth = FMath::Max(1.0f, InMaxHealth);
    CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
    MaxMana = FMath::Max(0.0f, InMaxMana);
    CurrentMana = FMath::Clamp(InCurrentMana, 0.0f, MaxMana);
    KillCount = FMath::Max(0, InKillCount);
    DeathCount = FMath::Max(0, InDeathCount);
    bHasPersistentCharacterSnapshot = true;
    bPersistentSpawnLocationConsumed = false;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ApplyAppearanceSelection(int32 InPresetId, int32 InColorIndex, int32 InShade, int32 InTransparent, int32 InTextureDetail)
{
    if (!HasAuthority())
    {
        return;
    }

    PresetId = InPresetId;
    ColorIndex = InColorIndex;
    Shade = InShade;
    Transparent = InTransparent;
    TextureDetail = InTextureDetail;
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
    DamageCooldownEndServerTime = 0.0f;
    HealCooldownEndServerTime = 0.0f;
    DrainCooldownEndServerTime = 0.0f;
    ManaSurgeCooldownEndServerTime = 0.0f;
    ArcChargeEndServerTime = 0.0f;
    RespawnAvailableServerTime = 0.0f;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::SetSelectedTargetIdentity(const FString& InCharacterId, const FString& InCharacterName)
{
    if (!HasAuthority())
    {
        return;
    }

    SelectedTargetCharacterId = InCharacterId;
    SelectedTargetName = InCharacterName;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::ClearSelectedTarget()
{
    if (!HasAuthority())
    {
        return;
    }

    SelectedTargetCharacterId.Reset();
    SelectedTargetName.Reset();
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

void AMOBAMMOPlayerState::RecordKill()
{
    if (!HasAuthority())
    {
        return;
    }

    ++KillCount;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::RecordDeath()
{
    if (!HasAuthority())
    {
        return;
    }

    ++DeathCount;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartRespawnCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    RespawnAvailableServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartDamageCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    DamageCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartHealCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    HealCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartDrainCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    DrainCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartManaSurgeCooldown(float CooldownDuration, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    ManaSurgeCooldownEndServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds + FMath::Max(0.0f, CooldownDuration));
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::StartArcCharge(float DurationSeconds, float ServerWorldTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    ArcChargeEndServerTime = DurationSeconds > 0.0f
        ? FMath::Max(0.0f, ServerWorldTimeSeconds + DurationSeconds)
        : 0.0f;
    ForceNetUpdate();
    BroadcastStateUpdated();
}

float AMOBAMMOPlayerState::GetDamageCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, DamageCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetHealCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, HealCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetDrainCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, DrainCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetManaSurgeCooldownRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, ManaSurgeCooldownEndServerTime - ServerWorldTimeSeconds);
}

float AMOBAMMOPlayerState::GetArcChargeRemaining(float ServerWorldTimeSeconds) const
{
    return FMath::Max(0.0f, ArcChargeEndServerTime - ServerWorldTimeSeconds);
}

bool AMOBAMMOPlayerState::IsDamageSpellReady(float ServerWorldTimeSeconds) const
{
    return GetDamageCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsHealSpellReady(float ServerWorldTimeSeconds) const
{
    return GetHealCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsDrainSpellReady(float ServerWorldTimeSeconds) const
{
    return GetDrainCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::IsManaSurgeReady(float ServerWorldTimeSeconds) const
{
    return GetManaSurgeCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

bool AMOBAMMOPlayerState::HasArcCharge(float ServerWorldTimeSeconds) const
{
    return GetArcChargeRemaining(ServerWorldTimeSeconds) > KINDA_SMALL_NUMBER;
}

float AMOBAMMOPlayerState::GetRespawnCooldownRemaining(float ServerWorldTimeSeconds) const
{
    if (IsAlive())
    {
        return 0.0f;
    }

    return FMath::Max(0.0f, RespawnAvailableServerTime - ServerWorldTimeSeconds);
}

bool AMOBAMMOPlayerState::CanRespawn(float ServerWorldTimeSeconds) const
{
    return !IsAlive() && GetRespawnCooldownRemaining(ServerWorldTimeSeconds) <= KINDA_SMALL_NUMBER;
}

void AMOBAMMOPlayerState::PushIncomingCombatFeedback(const FString& InFeedbackText, float DurationSeconds, bool bIsHealingFeedback)
{
    if (!HasAuthority())
    {
        return;
    }

    IncomingCombatFeedbackText = InFeedbackText;
    bIncomingCombatFeedbackHealing = bIsHealingFeedback;
    const float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    IncomingCombatFeedbackEndServerTime = ServerWorldTimeSeconds + FMath::Max(0.1f, DurationSeconds);
    ForceNetUpdate();
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOPlayerState, AccountId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterId);
    DOREPLIFETIME(AMOBAMMOPlayerState, SessionId);
    DOREPLIFETIME(AMOBAMMOPlayerState, PersistenceStatus);
    DOREPLIFETIME(AMOBAMMOPlayerState, PersistenceErrorMessage);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterName);
    DOREPLIFETIME(AMOBAMMOPlayerState, ClassId);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterLevel);
    DOREPLIFETIME(AMOBAMMOPlayerState, CharacterExperience);
    DOREPLIFETIME(AMOBAMMOPlayerState, SavedWorldPosition);
    DOREPLIFETIME(AMOBAMMOPlayerState, PresetId);
    DOREPLIFETIME(AMOBAMMOPlayerState, ColorIndex);
    DOREPLIFETIME(AMOBAMMOPlayerState, Shade);
    DOREPLIFETIME(AMOBAMMOPlayerState, Transparent);
    DOREPLIFETIME(AMOBAMMOPlayerState, TextureDetail);
    DOREPLIFETIME(AMOBAMMOPlayerState, bHasPersistentCharacterSnapshot);
    DOREPLIFETIME(AMOBAMMOPlayerState, CurrentHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, MaxHealth);
    DOREPLIFETIME(AMOBAMMOPlayerState, CurrentMana);
    DOREPLIFETIME(AMOBAMMOPlayerState, MaxMana);
    DOREPLIFETIME(AMOBAMMOPlayerState, DamageCooldownEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, HealCooldownEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, DrainCooldownEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, ManaSurgeCooldownEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, ArcChargeEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, KillCount);
    DOREPLIFETIME(AMOBAMMOPlayerState, DeathCount);
    DOREPLIFETIME(AMOBAMMOPlayerState, RespawnAvailableServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, SelectedTargetCharacterId);
    DOREPLIFETIME(AMOBAMMOPlayerState, SelectedTargetName);
    DOREPLIFETIME(AMOBAMMOPlayerState, IncomingCombatFeedbackText);
    DOREPLIFETIME(AMOBAMMOPlayerState, IncomingCombatFeedbackEndServerTime);
    DOREPLIFETIME(AMOBAMMOPlayerState, bIncomingCombatFeedbackHealing);
}

void AMOBAMMOPlayerState::OnRep_PlayerIdentity()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_PersistenceStatus()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_Attributes()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_TargetSelection()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::OnRep_CombatFeedback()
{
    BroadcastStateUpdated();
}

void AMOBAMMOPlayerState::BroadcastStateUpdated()
{
    OnReplicatedStateUpdated.Broadcast();
}
