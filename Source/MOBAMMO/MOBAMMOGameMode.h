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

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool ApplyDamageToPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool HealPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool ConsumeManaForPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool RestoreManaForPlayer(AController* TargetController, float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Gameplay")
    bool RespawnPlayer(AController* TargetController);

private:
    void UpdateConnectedPlayerCount();
    void ApplyPlayerSessionData(APlayerController* NewPlayerController, const FString& Options);
    AMOBAMMOPlayerState* ResolveMOBAPlayerState(AController* Controller) const;
    void InitializeDefaultAttributes(AMOBAMMOPlayerState* PlayerState) const;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DefaultMaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category="MOBAMMO|Gameplay")
    float DefaultMaxMana = 50.0f;
};
