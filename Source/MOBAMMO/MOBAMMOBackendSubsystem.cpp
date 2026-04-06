#include "MOBAMMOBackendSubsystem.h"

#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "JsonObjectWrapper.h"
#include "MOBAMMOBackendConfig.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString NormalizeBaseUrl(const FString& Url)
    {
        FString Result = Url;
        Result.RemoveFromEnd(TEXT("/"));
        return Result;
    }
}

FString UMOBAMMOBackendSubsystem::GetBackendBaseUrl() const
{
    return NormalizeBaseUrl(GetDefault<UMOBAMMOBackendConfig>()->BackendBaseUrl);
}

void UMOBAMMOBackendSubsystem::MockLogin(const FString& Username)
{
    const FString TrimmedUsername = Username.TrimStartAndEnd();
    if (TrimmedUsername.IsEmpty())
    {
        OnLoginFailed.Broadcast(TEXT("Username gereklidir."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/auth/login")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(FString::Printf(TEXT("{\"username\":\"%s\"}"), *TrimmedUsername.ReplaceCharWithEscapedChar()));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                OnLoginFailed.Broadcast(TEXT("Login istegi basarisiz oldu."));
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                OnLoginFailed.Broadcast(ErrorMessage);
                return;
            }

            FBackendLoginResult Result;
            Result.AccountId = JsonObject->GetStringField(TEXT("accountId"));
            Result.Token = JsonObject->GetStringField(TEXT("token"));
            Result.Username = JsonObject->GetStringField(TEXT("username"));

            LastAccountId = Result.AccountId;

            OnLoginSucceeded.Broadcast(Result);
        }
    );

    Request->ProcessRequest();
}

void UMOBAMMOBackendSubsystem::CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId)
{
    const FString TrimmedAccountId = AccountId.TrimStartAndEnd();
    const FString TrimmedCharacterName = CharacterName.TrimStartAndEnd();
    const FString TrimmedClassId = ClassId.TrimStartAndEnd().IsEmpty() ? TEXT("fighter") : ClassId.TrimStartAndEnd();

    if (TrimmedAccountId.IsEmpty())
    {
        OnCharacterCreateFailed.Broadcast(TEXT("AccountId gereklidir."));
        return;
    }

    if (TrimmedCharacterName.IsEmpty())
    {
        OnCharacterCreateFailed.Broadcast(TEXT("Karakter adi gereklidir."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/characters")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(
        FString::Printf(
            TEXT("{\"accountId\":\"%s\",\"name\":\"%s\",\"classId\":\"%s\"}"),
            *TrimmedAccountId.ReplaceCharWithEscapedChar(),
            *TrimmedCharacterName.ReplaceCharWithEscapedChar(),
            *TrimmedClassId.ReplaceCharWithEscapedChar()
        )
    );

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                OnCharacterCreateFailed.Broadcast(TEXT("Karakter olusturma istegi basarisiz oldu."));
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                OnCharacterCreateFailed.Broadcast(ErrorMessage);
                return;
            }

            FBackendCharacterResult Result;
            Result.CharacterId = JsonObject->GetStringField(TEXT("id"));
            Result.Name = JsonObject->GetStringField(TEXT("name"));
            Result.ClassId = JsonObject->GetStringField(TEXT("classId"));
            Result.Level = JsonObject->GetIntegerField(TEXT("level"));

            LastCharacterId = Result.CharacterId;

            OnCharacterCreated.Broadcast(Result);
        }
    );

    Request->ProcessRequest();
}

void UMOBAMMOBackendSubsystem::StartSession(const FString& CharacterId)
{
    const FString TrimmedCharacterId = CharacterId.TrimStartAndEnd();
    if (TrimmedCharacterId.IsEmpty())
    {
        OnSessionStartFailed.Broadcast(TEXT("CharacterId gereklidir."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/session/start")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(FString::Printf(TEXT("{\"characterId\":\"%s\"}"), *TrimmedCharacterId.ReplaceCharWithEscapedChar()));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                OnSessionStartFailed.Broadcast(TEXT("Session baslatma istegi basarisiz oldu."));
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                OnSessionStartFailed.Broadcast(ErrorMessage);
                return;
            }

            const TSharedPtr<FJsonObject>* CharacterObject = nullptr;
            const TSharedPtr<FJsonObject>* ServerObject = nullptr;
            if (!JsonObject->TryGetObjectField(TEXT("character"), CharacterObject) || !JsonObject->TryGetObjectField(TEXT("server"), ServerObject))
            {
                OnSessionStartFailed.Broadcast(TEXT("Session yaniti eksik veya gecersiz."));
                return;
            }

            FBackendSessionResult Result;
            Result.SessionId = JsonObject->GetStringField(TEXT("sessionId"));
            Result.CharacterId = (*CharacterObject)->GetStringField(TEXT("id"));
            Result.CharacterName = (*CharacterObject)->GetStringField(TEXT("name"));
            Result.CharacterLevel = (*CharacterObject)->GetIntegerField(TEXT("level"));
            Result.ServerHost = (*ServerObject)->GetStringField(TEXT("host"));
            Result.ServerPort = (*ServerObject)->GetIntegerField(TEXT("port"));
            Result.MapName = (*ServerObject)->GetStringField(TEXT("map"));
            Result.ConnectString = (*ServerObject)->GetStringField(TEXT("connectString"));

            LastCharacterId = Result.CharacterId;
            LastSessionConnectString = Result.ConnectString;

            OnSessionStarted.Broadcast(Result);
        }
    );

    Request->ProcessRequest();
}

bool UMOBAMMOBackendSubsystem::TravelToSession(APlayerController* PlayerController, const FString& ConnectString)
{
    if (!PlayerController)
    {
        return false;
    }

    const FString TrimmedConnectString = ConnectString.TrimStartAndEnd();
    if (TrimmedConnectString.IsEmpty())
    {
        return false;
    }

    LastSessionConnectString = TrimmedConnectString;
    PlayerController->ClientTravel(TrimmedConnectString, TRAVEL_Absolute);
    return true;
}

FString UMOBAMMOBackendSubsystem::BuildUrl(const FString& Path) const
{
    return FString::Printf(TEXT("%s%s"), *GetBackendBaseUrl(), *Path);
}

bool UMOBAMMOBackendSubsystem::TryReadJsonResponse(FHttpResponseHandle Response, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const
{
    const int32 ResponseCode = Response->GetResponseCode();
    if (!EHttpResponseCodes::IsOk(ResponseCode))
    {
        OutError = BuildErrorMessage(Response, TEXT("Backend istegi basarisiz oldu."));
        return false;
    }

    const FString Body = Response->GetContentAsString();
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
    {
        OutError = TEXT("Backend yaniti JSON olarak okunamadi.");
        return false;
    }

    return true;
}

FString UMOBAMMOBackendSubsystem::BuildErrorMessage(FHttpResponseHandle Response, const FString& FallbackMessage) const
{
    if (!Response.IsValid())
    {
        return FallbackMessage;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString Message;
        if (JsonObject->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
        {
            return Message;
        }
    }

    return FallbackMessage;
}
