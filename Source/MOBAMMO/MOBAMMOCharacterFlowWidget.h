#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOCharacterFlowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UMOBAMMOCharacterEntryWidget;
class UMOBAMMOBackendSubsystem;

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
    void RebuildCharacterButtons();
    void UpdateHeaderText();

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
    void HandleCreateCharacterClicked();

    UFUNCTION()
    void HandleStartSessionClicked();

    UFUNCTION()
    void HandleRefreshCharactersClicked();

    UFUNCTION()
    void HandleCharacterEntrySelected(const FString& CharacterId);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SubtitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> CharacterListBox;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CreateCharacterButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> RefreshCharactersButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> StartSessionButton;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMOBAMMOCharacterEntryWidget>> CharacterEntryWidgets;

    UPROPERTY(Transient)
    TArray<FBackendCharacterResult> RenderedCharacters;

    bool bBoundToSubsystem = false;
};
