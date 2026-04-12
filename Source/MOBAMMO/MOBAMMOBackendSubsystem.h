#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpResponse.h"
#include "Subsystems/GameInstanceSubsystem.h"

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
    int32 CharacterLevel = 1;

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
    void CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void CreateCharacterForCurrentAccount(const FString& CharacterName, const FString& ClassId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void StartSession(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void SelectCharacter(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void StartSessionForSelectedCharacter();

    UFUNCTION(BlueprintCallable, Category="Backend")
    bool TravelToSession(APlayerController* PlayerController, const FString& ConnectString);

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
    FString GetLastErrorMessage() const { return LastErrorMessage; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    FString GetLastUsername() const { return LastUsername; }

    UFUNCTION(BlueprintPure, Category="Backend|Debug")
    bool IsWaitingForCharacterSelection() const { return bManualCharacterFlowPending; }

private:
    using FHttpResponseHandle = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

    FString LastAccountId;
    FString LastCharacterId;
    FString LastSessionConnectString;
    FString LastErrorMessage;
    FString LastUsername;
    FString SelectedCharacterId;
    TArray<FBackendCharacterResult> CachedCharacters;
    FString LoginStatus;
    FString CharacterListStatus;
    FString CharacterStatus;
    FString SessionStatus;
    bool bManualCharacterFlowPending = false;
    bool bCharacterFlowActionAuthorized = false;

    FString BuildUrl(const FString& Path) const;
    bool TryReadJsonResponse(FHttpResponseHandle Response, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const;
    FString BuildErrorMessage(FHttpResponseHandle Response, const FString& FallbackMessage) const;
    void NotifyDebugStateChanged();
    bool ShouldUseManualCharacterFlow() const;
    bool CanRunCharacterFlowAction(const FString& FailureMessage, FBackendRequestFailedSignature& FailureDelegate, FString& InOutStatus);
};
