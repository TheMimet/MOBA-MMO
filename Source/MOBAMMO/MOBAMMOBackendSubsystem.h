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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendCharacterCreatedSignature, const FBackendCharacterResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendSessionStartedSignature, const FBackendSessionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBackendRequestFailedSignature, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class MOBAMMO_API UMOBAMMOBackendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendLoginSuccessSignature OnLoginSucceeded;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendCharacterCreatedSignature OnCharacterCreated;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendSessionStartedSignature OnSessionStarted;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnLoginFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnCharacterCreateFailed;

    UPROPERTY(BlueprintAssignable, Category="Backend")
    FBackendRequestFailedSignature OnSessionStartFailed;

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetBackendBaseUrl() const;

    UFUNCTION(BlueprintCallable, Category="Backend")
    void MockLogin(const FString& Username);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    void StartSession(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="Backend")
    bool TravelToSession(APlayerController* PlayerController, const FString& ConnectString);

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastAccountId() const { return LastAccountId; }

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastCharacterId() const { return LastCharacterId; }

    UFUNCTION(BlueprintPure, Category="Backend")
    FString GetLastSessionConnectString() const { return LastSessionConnectString; }

private:
    using FHttpResponseHandle = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

    FString LastAccountId;
    FString LastCharacterId;
    FString LastSessionConnectString;

    FString BuildUrl(const FString& Path) const;
    TSharedPtr<FJsonObject> BuildJsonObject(const TFunctionRef<void(TSharedRef<FJsonObject>)>& Builder) const;
    bool TryReadJsonResponse(FHttpResponseHandle Response, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const;
    FString BuildErrorMessage(FHttpResponseHandle Response, const FString& FallbackMessage) const;
};
