#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"

#include "MOBAMMOGameMode.generated.h"

class AMOBAMMOGameState;
class AMOBAMMOPlayerState;

UCLASS()
class MOBAMMO_API AMOBAMMOGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AMOBAMMOGameMode();

    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

private:
    void UpdateConnectedPlayerCount();
    void ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options);
};
