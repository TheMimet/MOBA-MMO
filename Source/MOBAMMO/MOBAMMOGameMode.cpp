#include "MOBAMMOGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"

namespace
{
    FString ReadOption(const FString& Options, const TCHAR* Key)
    {
        return UGameplayStatics::ParseOption(Options, Key);
    }
}

AMOBAMMOGameMode::AMOBAMMOGameMode()
{
    GameStateClass = AMOBAMMOGameState::StaticClass();
    PlayerStateClass = AMOBAMMOPlayerState::StaticClass();

    static ConstructorHelpers::FClassFinder<ACharacter> CharacterClassFinder(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (CharacterClassFinder.Succeeded())
    {
        DefaultPawnClass = CharacterClassFinder.Class;
    }

    static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerFinder(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
    if (PlayerControllerFinder.Succeeded())
    {
        PlayerControllerClass = PlayerControllerFinder.Class;
    }
}

FString AMOBAMMOGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    ApplyPlayerSessionData(NewPlayerController, Options);
    return ErrorMessage;
}

void AMOBAMMOGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UpdateConnectedPlayerCount();
}

void AMOBAMMOGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    UpdateConnectedPlayerCount();
}

bool AMOBAMMOGameMode::ApplyDamageToPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    return PlayerState->ApplyDamage(Amount) > 0.0f;
}

bool AMOBAMMOGameMode::HealPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    return PlayerState->ApplyHealing(Amount) > 0.0f;
}

bool AMOBAMMOGameMode::ConsumeManaForPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount < 0.0f)
    {
        return false;
    }

    return PlayerState->ConsumeMana(Amount);
}

bool AMOBAMMOGameMode::RestoreManaForPlayer(AController* TargetController, float Amount)
{
    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState || Amount <= 0.0f)
    {
        return false;
    }

    return PlayerState->RestoreMana(Amount) > 0.0f;
}

bool AMOBAMMOGameMode::RespawnPlayer(AController* TargetController)
{
    if (!HasAuthority() || !IsValid(TargetController))
    {
        return false;
    }

    AMOBAMMOPlayerState* PlayerState = ResolveMOBAPlayerState(TargetController);
    if (!PlayerState)
    {
        return false;
    }

    if (APawn* ExistingPawn = TargetController->GetPawn())
    {
        TargetController->UnPossess();
        ExistingPawn->Destroy();
    }

    RestartPlayer(TargetController);
    InitializeDefaultAttributes(PlayerState);
    return true;
}

void AMOBAMMOGameMode::UpdateConnectedPlayerCount()
{
    if (AMOBAMMOGameState* MOBAGameState = GetGameState<AMOBAMMOGameState>())
    {
        int32 ValidPlayers = 0;
        for (APlayerState* PlayerState : GameState->PlayerArray)
        {
            if (IsValid(PlayerState))
            {
                ++ValidPlayers;
            }
        }

        MOBAGameState->SetConnectedPlayers(ValidPlayers);
    }
}

void AMOBAMMOGameMode::ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options)
{
    if (!NewPlayerController)
    {
        return;
    }

    AMOBAMMOPlayerState* MOBAPlayerState = NewPlayerController->GetPlayerState<AMOBAMMOPlayerState>();
    if (!MOBAPlayerState)
    {
        return;
    }

    const FString AccountId = ReadOption(Options, TEXT("AccountId"));
    const FString CharacterId = ReadOption(Options, TEXT("CharacterId"));
    const FString CharacterName = ReadOption(Options, TEXT("CharacterName"));
    const FString ClassId = ReadOption(Options, TEXT("ClassId"));
    const FString LevelString = ReadOption(Options, TEXT("Level"));
    const int32 CharacterLevel = LevelString.IsEmpty() ? 1 : FMath::Max(1, FCString::Atoi(*LevelString));

    MOBAPlayerState->ApplySessionIdentity(AccountId, CharacterId, CharacterName, ClassId, CharacterLevel);
    InitializeDefaultAttributes(MOBAPlayerState);
}

AMOBAMMOPlayerState* AMOBAMMOGameMode::ResolveMOBAPlayerState(AController* Controller) const
{
    if (!HasAuthority() || !IsValid(Controller))
    {
        return nullptr;
    }

    return Controller->GetPlayerState<AMOBAMMOPlayerState>();
}

void AMOBAMMOGameMode::InitializeDefaultAttributes(AMOBAMMOPlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return;
    }

    PlayerState->InitializeAttributes(DefaultMaxHealth, DefaultMaxMana);
}
