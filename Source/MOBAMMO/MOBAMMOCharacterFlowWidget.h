#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOCharacterFlowWidget.generated.h"

class UMOBAMMOBackendSubsystem;
class UWebBrowser;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOCharacterFlowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Flow")
    FString DefaultCharacterName = TEXT("BPHero");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Flow")
    FString DefaultClassId = TEXT("fighter");

    UFUNCTION(BlueprintCallable, Category="Character Flow")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="Character Flow")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="Character Flow")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void DispatchStateToBrowser();
    FString BuildStateJson() const;
    void HandleBrowserAction(const TSharedPtr<FJsonObject>& ActionObject);

    UFUNCTION()
    void HandleLoginSucceeded(const FBackendLoginResult& Result);

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleCharactersLoaded(const FBackendCharacterListResult& Result);

    UFUNCTION()
    void HandleCharacterCreated(const FBackendCharacterResult& Result);

    UFUNCTION()
    void HandleSessionStarted(const FBackendSessionResult& Result);

    UFUNCTION()
    void HandleRequestFailed(const FString& ErrorMessage);

    UFUNCTION()
    void HandleBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UPROPERTY(Transient)
    TObjectPtr<UWebBrowser> Browser;

    FString LastPushedStateJson;

    bool bBoundToSubsystem = false;
    bool bAutoStartSessionAfterCharacterCreate = false;
};
