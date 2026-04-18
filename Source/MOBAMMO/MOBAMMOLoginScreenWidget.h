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

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const struct FBackendLoginResult& Result);

    UFUNCTION()
    void HandleLoginFailed(const FString& ErrorMessage);

    UFUNCTION()
    void HandleLoginButtonClicked();

    UPROPERTY(Transient)
    class UBorder* RootBorder;

    UPROPERTY(Transient)
    class UEditableTextBox* UsernameInput;

    UPROPERTY(Transient)
    class UButton* LoginButton;

    UPROPERTY(Transient)
    class UTextBlock* ErrorText;

    bool bBoundToSubsystem = false;
};
