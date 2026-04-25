#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOLoginScreenWidget.generated.h"

class UMOBAMMOBackendSubsystem;
class UWebBrowser;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOLoginScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Login")
    FString DefaultUsername = TEXT("mimet_bp_test");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Login")
    FString WebUIBaseUrl = TEXT("http://127.0.0.1:3002");

    UFUNCTION(BlueprintCallable, Category="Login")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="Login")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="Login")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void PushStateToWebUI();
    UFUNCTION()
    void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const struct FBackendLoginResult& Result);

    UFUNCTION()
    void HandleLoginFailed(const FString& ErrorMessage);

    UPROPERTY(Transient)
    UWebBrowser* WebBrowserWidget;

    bool bBoundToSubsystem = false;
    bool bPageLoaded = false;
};
