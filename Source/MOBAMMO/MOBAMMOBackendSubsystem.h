#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpResponse.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOBAMMOInventoryTypes.h"

#include "MOBAMMOBackendSubsystem.generated.h"

class APlayerController;
class FJsonObject;
class IHttpRequest;
class IHttpResponse;

USTRUCT(BlueprintType)
struct FBackendLoginResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString AccountId;

    UPROPERTY(BlueprintReadOnly)
    FString Token;

    UPROPERTY(BlueprintReadOnly)
    FString Username;
};

USTRUCT(BlueprintType)
struct FBackendCharacterResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString CharacterId;

    UPROPERTY(BlueprintReadOnly)
    FString Name;

    UPROPERTY(BlueprintReadOnly)
    FString ClassId;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;
};

USTRUCT(BlueprintType)
struct FBackendCharacterListResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FBackendCharacterResult> Items;
};

USTRUCT(BlueprintType)
struct FBackendSessionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    UPROPERTY(BlueprintReadOnly)
    FString CharacterId;

    UPROPERTY(BlueprintReadOnly)
    FString CharacterName;

    UPROPERTY(BlueprintReadOnly)
    FString CharacterClassId;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterLevel = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterExperience = 0;

    UPROPERTY(BlueprintReadOnly)
    FVector CharacterPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterPresetId = 4;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterColorIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterShade = 58;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterTransparent = 18;

    UPROPERTY(BlueprintReadOnly)
    int32 CharacterTextureDetail = 88;

    UPROPERTY(BlueprintReadOnly)
    float CurrentHealth = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxHealth = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float CurrentMana = 50.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxMana = 50.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 KillCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 DeathCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FMOBAMMOInventoryItem> InventoryItems;

    UPROPERTY(BlueprintReadOnly)
    FString ServerHost;

    UPROPERTY(BlueprintReadOnly)
    int32 ServerPort = 7777;

    UPROPERTY(BlueprintReadOnly)
    FString MapName;

    UPROPERTY(BlueprintReadOnly)
    FString ConnectString;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendLoginSuccessSignature, const FBackendLoginResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendCharacterListLoadedSignature, const FBackendCharacterListResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendCharacterCreatedSignature, const FBackendCharacterResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendSessionStartedSignature, const FBackendSessionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendRequestFailedSignature, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBackendDebugStateChangedSignature);

UCLASS(BlueprintType)
class MOBAMMO_API UMOBAMMOBackendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UMOBAMMOBackendSubsystem();

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendLoginSuccessSignature OnLoginSucceeded;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendCharacterListLoadedSignature OnCharactersLoaded;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendCharacterCreatedSignature OnCharacterCreated;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendSessionStartedSignature OnSessionStarted;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnLoginFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnCharactersLoadFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnCharacterCreateFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnSessionStartFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend|Debug")
    FBackendDebugStateChangedSignature OnDebugStateChanged;

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetBackendBaseUrl() const;

    UFUNCTION(BlueprintCallable, Category="Backend")
    void MockLogin(const FString& Username);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void ListCharacters(const FString& AccountId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId, int32 PresetId = 4, int32 ColorIndex = 0, int32 Shade = 58, int32 Transparent = 18, int32 TextureDetail = 88);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void CreateCharacterForCurrentAccount(const FString& CharacterName, const FString& ClassId, int32 PresetId = 4, int32 ColorIndex = 0, int32 Shade = 58, int32 Transparent = 18, int32 TextureDetail = 88);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void StartSession(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void SelectCharacter(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void StartSessionForSelectedCharacter();

    UFUNCTION(BlueprintCallable, Category="Backend")
    void OpenMainMenuCharacters();

    UFUNCTION(BlueprintCallable, Category="Backend")
    void PlayFromMainMenu();

    UFUNCTION(BlueprintCallable, Category="Backend")
    void ReconnectCurrentSession(APlayerController* PlayerController);

    void SaveCurrentCharacterProgress(APlayerController* PlayerController);
    void HeartbeatCurrentCharacterSession(APlayerController* PlayerController);
    void EndCurrentCharacterSession(APlayerController* PlayerController);

    UFUNCTION(BlueprintCallable, Category="Backend")
    bool TravelToSession(APlayerController* PlayerController, const FString& ConnectString);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void NotifyClientEnteredSessionWorld();

    UFUNCTION(BlueprintPure, Category="Backend")
    FString BuildReplicatedTravelConnectString(const FString& ConnectString) const;

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastAccountId() const { return LastAccountId; }

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastCharacterId() const { return LastCharacterId; }

    UFUNCTION(BlueprintPure, Category="Backend")
    TArray<FBackendCharacterResult> GetCachedCharacters() const { return CachedCharacters; }

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetSelectedCharacterId() const { return SelectedCharacterId; }

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastSessionConnectString() const { return LastSessionConnectString; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLoginStatus() const { return LoginStatus; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetCharacterStatus() const { return CharacterStatus; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetCharacterListStatus() const { return CharacterListStatus; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetSessionStatus() const { return SessionStatus; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetSaveStatus() const { return SaveStatus; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastErrorMessage() const { return LastErrorMessage; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastSaveErrorMessage() const { return LastSaveErrorMessage; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    bool IsSaveConnectionHealthy() const { return bSaveConnectionHealthy; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastUsername() const { return LastUsername; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    bool IsWaitingForCharacterSelection() const { return bManualCharacterFlowPending && bCharacterSelectRequested; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    bool ShouldShowMainMenu() const { return bManualCharacterFlowPending && bMainMenuVisible && !bCharacterSelectRequested; }

private:
    using FHttpResponseHandle = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

    FString LastAccountId;
    FString LastAuthToken;
    FString LastCharacterId;
    FString LastSessionConnectString;
    FString LastErrorMessage;
    FString LastUsername;
    FString SelectedCharacterId;
    FBackendSessionResult LastSessionResult;
    TArray<FBackendCharacterResult> CachedCharacters;
    FString LoginStatus;
    FString CharacterListStatus;
    FString CharacterStatus;
    FString SessionStatus;
    FString SaveStatus;
    FString LastSaveErrorMessage;
    bool bManualCharacterFlowPending = false;
    bool bCharacterFlowActionAuthorized = false;
    bool bMainMenuVisible = false;
    bool bCharacterSelectRequested = false;
    bool bSaveConnectionHealthy = true;
    bool bReconnectTravelPending = false;

    TWeakObjectPtr<APlayerController> PendingReconnectPlayerController;

    FString BuildUrl(const FString& Path) const;
    void ApplyAuthHeader(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request, const FString& TokenFallback = FString()) const;
    bool TryReadJsonResponse(FHttpResponseHandle Response, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const;
    FString BuildErrorMessage(FHttpResponseHandle Response, const FString& FallbackMessage) const;
    FString BuildErrorCode(FHttpResponseHandle Response) const;
    bool IsSessionInvalidationError(FHttpResponseHandle Response) const;
    void MarkSaveFailed(const FString& ErrorMessage, FHttpResponseHandle Response = nullptr, const FString& CharacterId = FString());
    void SetReplicatedPersistenceStatusForCharacter(const FString& CharacterId, const FString& Status, const FString& ErrorMessage) const;
    void NotifyDebugStateChanged();
    bool ShouldUseManualCharacterFlow() const;
    bool CanRunCharacterFlowAction(const FString& FailureMessage, FBackendRequestFailedSignature& FailureDelegate, FString& InOutStatus);
};
