#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOGameHUDWidget.generated.h"

class UBorder;
class UTextBlock;
class UMOBAMMOBackendSubsystem;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category="HUD")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="HUD")
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
    TObjectPtr<UTextBlock> StatusText;

    bool bBoundToSubsystem = false;
};
