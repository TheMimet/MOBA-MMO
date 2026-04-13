#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOLoadingScreenWidget.generated.h"

class UBorder;
class UTextBlock;
class UMOBAMMOBackendSubsystem;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOLoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category="Loading")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="Loading")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="Loading")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void UpdateTexts();

    UFUNCTION()
    void HandleBackendStateChanged();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HintText;

    bool bBoundToSubsystem = false;
};
