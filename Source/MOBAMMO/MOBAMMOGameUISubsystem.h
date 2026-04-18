#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "MOBAMMOGameUISubsystem.generated.h"

class UMOBAMMOCharacterSelectWidget;
class UMOBAMMOGameHUDWidget;
class UMOBAMMOLoginScreenWidget;
class UMOBAMMOLoadingScreenWidget;

UCLASS()
class MOBAMMO_API UMOBAMMOGameUISubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool Tick(float DeltaTime);
    bool CanCreateWidgets(UWorld* World) const;
    void EnsureWidgets(class APlayerController* PlayerController);
    void ReconcileSessionState(class APlayerController* PlayerController);
    void UpdateWidgetVisibility(class APlayerController* PlayerController);

    TSubclassOf<UMOBAMMOLoginScreenWidget> ResolveLoginWidgetClass() const;
    TSubclassOf<UMOBAMMOLoadingScreenWidget> ResolveLoadingWidgetClass() const;
    TSubclassOf<UMOBAMMOGameHUDWidget> ResolveHUDWidgetClass() const;
    TSubclassOf<UMOBAMMOCharacterSelectWidget> ResolveCharacterSelectWidgetClass() const;

    FTSTicker::FDelegateHandle TickHandle;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOLoginScreenWidget> LoginWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOLoadingScreenWidget> LoadingWidget;
    
    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOCharacterSelectWidget> CharacterSelectWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOGameHUDWidget> HUDWidget;
};
