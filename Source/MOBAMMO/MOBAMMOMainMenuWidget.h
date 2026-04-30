#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOMainMenuWidget.generated.h"

class UMOBAMMOBackendSubsystem;
class UBorder;
class UButton;
class UTextBlock;
class UWebBrowser;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MainMenu")
    FString WebUIBaseUrl = TEXT("http://127.0.0.1:3002");

    UFUNCTION(BlueprintCallable, Category="MainMenu")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="MainMenu")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="MainMenu")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void PushStateToWebUI();
    void UpdateNativeFallback();

    UFUNCTION()
    void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UFUNCTION()
    void HandleNativePlayClicked();

    UFUNCTION()
    void HandleNativeCharactersClicked();

    UFUNCTION()
    void HandleNativeQuitClicked();

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const struct FBackendLoginResult& Result);

    UFUNCTION()
    void HandleCharactersLoaded(const struct FBackendCharacterListResult& Result);

    UFUNCTION()
    void HandleSessionStarted(const struct FBackendSessionResult& Result);

    UFUNCTION()
    void HandleRequestFailed(const FString& ErrorMessage);

    UPROPERTY(Transient)
    UWebBrowser* WebBrowserWidget;

    UPROPERTY(Transient)
    UBorder* NativeFallbackPanel;

    UPROPERTY(Transient)
    UTextBlock* NativeFallbackStatusText;

    UPROPERTY(Transient)
    UButton* NativePlayButton;

    UPROPERTY(Transient)
    UButton* NativeCharactersButton;

    bool bBoundToSubsystem = false;
    bool bPageLoaded = false;
};
