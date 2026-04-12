#include "MOBAMMOGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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
    MOBAPlayerState->InitializeAttributes(100.0f, 50.0f);
}
