#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "MOBAMMOGameUISubsystem.generated.h"

class UMOBAMMOCharacterSelectWidget;
class UMOBAMMOGameHUDWidget;
class UMOBAMMOLoginScreenWidget;
class UMOBAMMOLoadingScreenWidget;
class UMOBAMMOMainMenuWidget;
class UMOBAMMOPauseMenuWidget;
class UMOBAMMOVendorWidget;

UCLASS()
class MOBAMMO_API UMOBAMMOGameUISubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="HUD")
    void ToggleInventory();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void ToggleSkillPanel();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void CloseSkillPanel();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void OpenChat();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void CloseChat();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsChatOpen() const;

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsInventoryOpen() const;

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsSkillPanelOpen() const;

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsGameplayInputBlocked() const;

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsFrontendFlowVisible() const;

    UFUNCTION(BlueprintCallable, Category="HUD")
    void TogglePauseMenu();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsPauseMenuVisible() const;

    UFUNCTION(BlueprintCallable, Category="HUD")
    void ToggleVendor();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void OpenVendor();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void CloseVendor();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsVendorOpen() const;

private:
    bool Tick(float DeltaTime);
    bool CanCreateWidgets(UWorld* World) const;
    void EnsureWidgets(class APlayerController* PlayerController);
    void ReconcileSessionState(class APlayerController* PlayerController);
    void ReconcileCommandLineAutoSession();
    void UpdateWidgetVisibility(class APlayerController* PlayerController);

    TSubclassOf<UMOBAMMOLoginScreenWidget> ResolveLoginWidgetClass() const;
    TSubclassOf<UMOBAMMOLoadingScreenWidget> ResolveLoadingWidgetClass() const;
    TSubclassOf<UMOBAMMOGameHUDWidget> ResolveHUDWidgetClass() const;
    TSubclassOf<UMOBAMMOCharacterSelectWidget> ResolveCharacterSelectWidgetClass() const;
    TSubclassOf<UMOBAMMOMainMenuWidget> ResolveMainMenuWidgetClass() const;

    FTSTicker::FDelegateHandle TickHandle;
    bool bAutoSessionLoginRequested = false;
    bool bAutoSessionStartRequested = false;
    bool bInputBlockApplied = false;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOLoginScreenWidget> LoginWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOLoadingScreenWidget> LoadingWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOMainMenuWidget> MainMenuWidget;
    
    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOCharacterSelectWidget> CharacterSelectWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOGameHUDWidget> HUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOPauseMenuWidget> PauseMenuWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOVendorWidget> VendorWidget;
};
