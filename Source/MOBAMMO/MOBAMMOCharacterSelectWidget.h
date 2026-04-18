#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMOCharacterSelectWidget.generated.h"

class UMOBAMMOBackendSubsystem;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOCharacterSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

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
    void UpdateCharacterList();

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

    UFUNCTION()
    void HandleCharacterButtonClicked(const FString& CharacterId);

    UFUNCTION()
    void HandleCreateButtonClicked();
    
    UFUNCTION()
    void HandlePlayButtonClicked();

    UPROPERTY(Transient)
    class UBorder* RootBorder;

    UPROPERTY(Transient)
    class UVerticalBox* CharacterListContainer;

    UPROPERTY(Transient)
    class UButton* CreateCharacterButton;

    UPROPERTY(Transient)
    class UButton* PlayButton;

    UPROPERTY(Transient)
    class UEditableTextBox* NewCharacterNameInput;

    bool bBoundToSubsystem = false;
};
