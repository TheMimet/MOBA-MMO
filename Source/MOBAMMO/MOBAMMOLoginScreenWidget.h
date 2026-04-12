#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOLoginScreenWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UMOBAMMOBackendSubsystem;

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
    void UpdateTexts();

    UFUNCTION()
    void HandleLoginClicked();

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const struct FBackendLoginResult& Result);

    UFUNCTION()
    void HandleLoginFailed(const FString& ErrorMessage);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SubtitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ErrorText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> LoginButton;

    bool bBoundToSubsystem = false;
};
