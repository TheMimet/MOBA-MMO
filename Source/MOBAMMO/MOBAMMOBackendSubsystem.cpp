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
#include "Async/Async.h"
#include "Engine/Engine.h"

namespace
{
    void DebugScreen(const FString& Message, const FColor Color = FColor::Cyan, const float Duration = 5.0f)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Message);
        }
    }

    FString NormalizeBaseUrl(const FString& Url)
    {
        FString Result = Url;
        Result.RemoveFromEnd(TEXT("/"));
        return Result;
    }

    FString NormalizeTravelConnectString(const FString& RequestedConnectString, const FString& FallbackConnectString)
    {
        const FString TrimmedRequested = RequestedConnectString.TrimStartAndEnd();
        const FString TrimmedFallback = FallbackConnectString.TrimStartAndEnd();

        if (TrimmedRequested.IsEmpty())
        {
            return TrimmedFallback;
        }

        if (TrimmedRequested.Contains(TEXT(":")))
        {
            return TrimmedRequested;
        }

        if (!TrimmedFallback.IsEmpty() && TrimmedFallback.Contains(TEXT(":")))
        {
            return TrimmedFallback;
        }

        return TrimmedRequested;
    }

    void RunOnGameThread(TFunction<void()> Callback)
    {
        AsyncTask(ENamedThreads::GameThread, MoveTemp(Callback));
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

    const FString LoginUrl = BuildUrl(TEXT("/auth/login"));
    UE_LOG(LogTemp, Log, TEXT("[Backend] MockLogin starting. Username=%s Url=%s"), *TrimmedUsername, *LoginUrl);
    DebugScreen(FString::Printf(TEXT("Backend URL: %s"), *GetBackendBaseUrl()));

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(LoginUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(FString::Printf(TEXT("{\"username\":\"%s\"}"), *TrimmedUsername.ReplaceCharWithEscapedChar()));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                const FString FailureMessage = FString::Printf(
                    TEXT("Login request failed. Success=%s ResponseValid=%s Url=%s"),
                    bWasSuccessful ? TEXT("true") : TEXT("false"),
                    Response.IsValid() ? TEXT("true") : TEXT("false"),
                    HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("<invalid request>")
                );
                UE_LOG(LogTemp, Error, TEXT("[Backend] %s"), *FailureMessage);
                RunOnGameThread([this]()
                {
                    OnLoginFailed.Broadcast(TEXT("Login istegi basarisiz oldu."));
                });
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                UE_LOG(LogTemp, Error, TEXT("[Backend] Login response parse/error failed. Code=%d Error=%s Body=%s"),
                    Response->GetResponseCode(),
                    *ErrorMessage,
                    *Response->GetContentAsString());
                RunOnGameThread([this, ErrorMessage]()
                {
                    OnLoginFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            FBackendLoginResult Result;
            Result.AccountId = JsonObject->GetStringField(TEXT("accountId"));
            Result.Token = JsonObject->GetStringField(TEXT("token"));
            Result.Username = JsonObject->GetStringField(TEXT("username"));

            LastAccountId = Result.AccountId;
            UE_LOG(LogTemp, Log, TEXT("[Backend] Login succeeded. AccountId=%s Username=%s"), *Result.AccountId, *Result.Username);

            RunOnGameThread([this, Result]()
            {
                OnLoginSucceeded.Broadcast(Result);
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] MockLogin ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
    if (!bRequestStarted)
    {
        OnLoginFailed.Broadcast(TEXT("Login istegi baslatilamadi."));
    }
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

    UE_LOG(LogTemp, Log, TEXT("[Backend] CreateCharacter starting. AccountId=%s CharacterName=%s ClassId=%s"),
        *TrimmedAccountId,
        *TrimmedCharacterName,
        *TrimmedClassId);

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
                UE_LOG(LogTemp, Error, TEXT("[Backend] CreateCharacter request failed. Success=%s ResponseValid=%s Url=%s"),
                    bWasSuccessful ? TEXT("true") : TEXT("false"),
                    Response.IsValid() ? TEXT("true") : TEXT("false"),
                    HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("<invalid request>"));
                RunOnGameThread([this]()
                {
                    OnCharacterCreateFailed.Broadcast(TEXT("Karakter olusturma istegi basarisiz oldu."));
                });
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                UE_LOG(LogTemp, Error, TEXT("[Backend] CreateCharacter response parse/error failed. Code=%d Error=%s Body=%s"),
                    Response->GetResponseCode(),
                    *ErrorMessage,
                    *Response->GetContentAsString());
                RunOnGameThread([this, ErrorMessage]()
                {
                    OnCharacterCreateFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            FBackendCharacterResult Result;
            Result.CharacterId = JsonObject->GetStringField(TEXT("id"));
            Result.Name = JsonObject->GetStringField(TEXT("name"));
            Result.ClassId = JsonObject->GetStringField(TEXT("classId"));
            Result.Level = JsonObject->GetIntegerField(TEXT("level"));

            LastCharacterId = Result.CharacterId;
            UE_LOG(LogTemp, Log, TEXT("[Backend] CreateCharacter succeeded. CharacterId=%s Name=%s"),
                *Result.CharacterId,
                *Result.Name);

            RunOnGameThread([this, Result]()
            {
                OnCharacterCreated.Broadcast(Result);
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] CreateCharacter ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
}

void UMOBAMMOBackendSubsystem::StartSession(const FString& CharacterId)
{
    const FString TrimmedCharacterId = CharacterId.TrimStartAndEnd();
    if (TrimmedCharacterId.IsEmpty())
    {
        OnSessionStartFailed.Broadcast(TEXT("CharacterId gereklidir."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Backend] StartSession starting. CharacterId=%s"), *TrimmedCharacterId);

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
                UE_LOG(LogTemp, Error, TEXT("[Backend] StartSession request failed. Success=%s ResponseValid=%s Url=%s"),
                    bWasSuccessful ? TEXT("true") : TEXT("false"),
                    Response.IsValid() ? TEXT("true") : TEXT("false"),
                    HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("<invalid request>"));
                RunOnGameThread([this]()
                {
                    OnSessionStartFailed.Broadcast(TEXT("Session baslatma istegi basarisiz oldu."));
                });
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                UE_LOG(LogTemp, Error, TEXT("[Backend] StartSession response parse/error failed. Code=%d Error=%s Body=%s"),
                    Response->GetResponseCode(),
                    *ErrorMessage,
                    *Response->GetContentAsString());
                RunOnGameThread([this, ErrorMessage]()
                {
                    OnSessionStartFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            const TSharedPtr<FJsonObject>* CharacterObject = nullptr;
            const TSharedPtr<FJsonObject>* ServerObject = nullptr;
            if (!JsonObject->TryGetObjectField(TEXT("character"), CharacterObject) || !JsonObject->TryGetObjectField(TEXT("server"), ServerObject))
            {
                RunOnGameThread([this]()
                {
                    OnSessionStartFailed.Broadcast(TEXT("Session yaniti eksik veya gecersiz."));
                });
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
            UE_LOG(LogTemp, Log, TEXT("[Backend] StartSession succeeded. CharacterId=%s ConnectString=%s Map=%s"),
                *Result.CharacterId,
                *Result.ConnectString,
                *Result.MapName);

            RunOnGameThread([this, Result]()
            {
                OnSessionStarted.Broadcast(Result);
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] StartSession ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
}

bool UMOBAMMOBackendSubsystem::TravelToSession(APlayerController* PlayerController, const FString& ConnectString)
{
    if (!PlayerController)
    {
        return false;
    }

    const FString RequestedConnectString = ConnectString.TrimStartAndEnd();
    const FString FinalConnectString = NormalizeTravelConnectString(RequestedConnectString, LastSessionConnectString);
    UE_LOG(LogTemp, Log, TEXT("[Backend] TravelToSession requested=%s fallback=%s final=%s"),
        *RequestedConnectString,
        *LastSessionConnectString,
        *FinalConnectString);

    if (FinalConnectString.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[Backend] TravelToSession aborted because connect string is empty."));
        return false;
    }

    LastSessionConnectString = FinalConnectString;
    DebugScreen(FString::Printf(TEXT("Travel: %s"), *FinalConnectString), FColor::Green, 5.0f);
    PlayerController->ClientTravel(FinalConnectString, TRAVEL_Absolute);
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
