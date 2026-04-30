#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOCharacterSelectWidget.generated.h"

class UMOBAMMOBackendSubsystem;
class UBorder;
class UButton;
class UTextBlock;
class UWebBrowser;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOCharacterSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CharacterSelect")
    FString WebUIBaseUrl = TEXT("http://127.0.0.1:3002");

    UFUNCTION(BlueprintCallable, Category="CharacterSelect")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="CharacterSelect")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="CharacterSelect")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void LoadWebUIIfNeeded();
    void PushStateToWebUI();
    void UpdateNativeFallback();

    UFUNCTION()
    void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UFUNCTION()
    void HandleNativeStartClicked();

    UFUNCTION()
    void HandleNativeCreateClicked();

    UFUNCTION()
    void HandleNativeRefreshClicked();

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const struct FBackendLoginResult& Result);

    UFUNCTION()
    void HandleCharactersLoaded(const struct FBackendCharacterListResult& Result);

    UFUNCTION()
    void HandleCharacterCreated(const struct FBackendCharacterResult& Result);

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
    UButton* NativeStartButton;

    UPROPERTY(Transient)
    UButton* NativeCreateButton;

    bool bBoundToSubsystem = false;
    bool bPageLoaded = false;
    bool bWebUILoadRequested = false;
    bool bStartSessionAfterCharacterCreate = false;
};
