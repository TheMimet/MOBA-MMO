#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOBackendSubsystem.h"

#include "MOBAMMODebugLoginWidget.generated.h"

class APlayerController;
class UMOBAMMOBackendSubsystem;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBAMMODebugStateChangedSignature);

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMODebugLoginWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintAssignable, Category="Backend|Debug")
    FMOBAMMODebugStateChangedSignature OnDebugStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backend|Debug")
    FString DefaultUsername = TEXT("mimet_bp_test");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backend|Debug")
    FString DefaultCharacterName = TEXT("BPHero");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backend|Debug")
    FString DefaultClassId = TEXT("fighter");

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetBackendUrl() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLoginStatus() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetCharacterStatus() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetCharacterListStatus() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetSessionStatus() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastErrorMessage() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastUsername() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastAccountId() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastCharacterId() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastConnectString() const;

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    TArray<FBackendCharacterResult> GetCachedCharacters() const;

    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    void TriggerMockLogin();

    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    void TriggerListCharacters();

    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    void TriggerCreateCharacter();

    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    void TriggerStartSession();

    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    bool TriggerJoinServer();

protected:
    UFUNCTION(BlueprintCallable, Category="Backend|Debug")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildDebugLayout();
    void BroadcastStateChanged();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void RefreshDisplay();

    UFUNCTION()
    void HandleMockLoginClicked();

    UFUNCTION()
    void HandleCreateCharacterClicked();

    UFUNCTION()
    void HandleRefreshCharactersClicked();

    UFUNCTION()
    void HandleStartSessionClicked();

    UFUNCTION()
    void HandleJoinServerClicked();

    UFUNCTION()
    void HandleDebugStateChanged();

    UFUNCTION()
    void HandleLoginSucceeded(const FBackendLoginResult& Result);

    UFUNCTION()
    void HandleCharacterCreated(const FBackendCharacterResult& Result);

    UFUNCTION()
    void HandleSessionStarted(const FBackendSessionResult& Result);

    UFUNCTION()
    void HandleRequestFailed(const FString& ErrorMessage);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BackendUrlText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LoginStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterListStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SessionStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ErrorText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UsernameText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> AccountIdText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterIdText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ConnectStringText;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> CharacterEntryTexts;

    bool bIsBoundToSubsystem = false;
};
