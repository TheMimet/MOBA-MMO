#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOCharacterEntryWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBAMMOCharacterEntrySelectedSignature, const FString&, CharacterId);

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOCharacterEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    UPROPERTY(BlueprintAssignable, Category="Character Entry")
    FMOBAMMOCharacterEntrySelectedSignature OnEntrySelected;

    UFUNCTION(BlueprintCallable, Category="Character Entry")
    void ConfigureEntry(const FString& InCharacterId, const FString& InLabel, bool bInSelected);

private:
    void BuildLayout();
    void RefreshVisuals();

    UFUNCTION()
    void HandleButtonClicked();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UButton> SelectButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LabelText;

    FString CharacterId;
    FString Label;
    bool bIsSelected = false;
};
