#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOPauseMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UOverlay;
class USizeBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MOBAMMO_API UMOBAMMOPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="PauseMenu")
    void ToggleVisibility();

    UFUNCTION(BlueprintCallable, Category="PauseMenu")
    void ShowMenu();

    UFUNCTION(BlueprintCallable, Category="PauseMenu")
    void HideMenu();

    UFUNCTION(BlueprintPure, Category="PauseMenu")
    bool IsMenuVisible() const { return bIsVisible; }

    UFUNCTION(BlueprintCallable, Category="PauseMenu")
    void RefreshContent();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildLayout();
    void BuildHeader(UVerticalBox* Parent);
    void BuildActionButtons(UVerticalBox* Parent);
    void BuildLeaderboard(UVerticalBox* Parent);
    void BuildDangerButtons(UVerticalBox* Parent);
    void BuildFooter(UVerticalBox* Parent);

    UBorder* MakeGlassPanel(const FLinearColor& Color, float Alpha) const;
    UBorder* MakeDivider() const;
    UButton* MakeMenuButton(const FString& Label, const FLinearColor& LabelColor,
                             const FLinearColor& BgColor, int32 FontSize = 14) const;

    UFUNCTION() void HandleResumeClicked();
    UFUNCTION() void HandleInventoryClicked();
    UFUNCTION() void HandleDisconnectClicked();
    UFUNCTION() void HandleBackdropClicked();

    // Root
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel>  RootCanvas;
    UPROPERTY(Transient) TObjectPtr<UButton>       BackdropButton;

    // Card
    UPROPERTY(Transient) TObjectPtr<UBorder>       CardGlow;
    UPROPERTY(Transient) TObjectPtr<UBorder>       CardBg;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox>  CardContent;

    // Header
    UPROPERTY(Transient) TObjectPtr<UTextBlock>    HeaderNameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock>    HeaderClassText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock>    HeaderKDText;

    // Leaderboard
    UPROPERTY(Transient) TObjectPtr<UTextBlock>    LeaderboardText;

    // State
    bool bIsVisible = false;

    // Fade
    float FadeAlpha    = 0.0f;
    float FadeTarget   = 0.0f;
    static constexpr float FadeSpeed = 10.0f;
};
