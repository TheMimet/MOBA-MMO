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

namespace
{
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

    bool ShouldSkipAutomaticBackendBootstrap(const UGameInstanceSubsystem* Subsystem)
    {
        if (!Subsystem)
        {
            return false;
        }

        const UWorld* World = Subsystem->GetWorld();
        if (!World)
        {
            return false;
        }

        // After the client joins the dedicated server, the replicated client world
        // should not restart the local mock login -> create character -> session flow.
        return World->GetNetMode() == NM_Client;
    }
}

FString UMOBAMMOBackendSubsystem::GetBackendBaseUrl() const
{
    return NormalizeBaseUrl(GetDefault<UMOBAMMOBackendConfig>()->BackendBaseUrl);
}

UMOBAMMOBackendSubsystem::UMOBAMMOBackendSubsystem()
    : LoginStatus(TEXT("Idle"))
    , CharacterListStatus(TEXT("Idle"))
    , CharacterStatus(TEXT("Idle"))
    , SessionStatus(TEXT("Idle"))
{
}

void UMOBAMMOBackendSubsystem::NotifyDebugStateChanged()
{
    OnDebugStateChanged.Broadcast();
}

bool UMOBAMMOBackendSubsystem::ShouldUseManualCharacterFlow() const
{
    return !ShouldSkipAutomaticBackendBootstrap(this);
}

bool UMOBAMMOBackendSubsystem::CanRunCharacterFlowAction(const FString& FailureMessage, FBackendRequestFailedSignature& FailureDelegate, FString& InOutStatus)
{
    if (!bManualCharacterFlowPending || bCharacterFlowActionAuthorized)
    {
        return true;
    }

    LastErrorMessage = FailureMessage;
    InOutStatus = TEXT("Blocked");
    NotifyDebugStateChanged();
    FailureDelegate.Broadcast(FailureMessage);
    return false;
}

void UMOBAMMOBackendSubsystem::MockLogin(const FString& Username)
{
    if (ShouldSkipAutomaticBackendBootstrap(this))
    {
        UE_LOG(LogTemp, Log, TEXT("[Backend] MockLogin skipped because world is already a network client."));
        NotifyDebugStateChanged();
        return;
    }

    const FString TrimmedUsername = Username.TrimStartAndEnd();
    if (TrimmedUsername.IsEmpty())
    {
        LastErrorMessage = TEXT("Username gereklidir.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(TEXT("Username gereklidir."));
        return;
    }

    LastUsername = TrimmedUsername;
    LastErrorMessage.Reset();
    LoginStatus = TEXT("LoggingIn");
    NotifyDebugStateChanged();
    const FString LoginUrl = BuildUrl(TEXT("/auth/login"));
    UE_LOG(LogTemp, Log, TEXT("[Backend] MockLogin starting. Username=%s Url=%s"), *TrimmedUsername, *LoginUrl);

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
                    LastErrorMessage = TEXT("Login istegi basarisiz oldu.");
                    LoginStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                    LastErrorMessage = ErrorMessage;
                    LoginStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                LoginStatus = TEXT("Succeeded");
                bManualCharacterFlowPending = ShouldUseManualCharacterFlow();
                bCharacterFlowActionAuthorized = false;
                NotifyDebugStateChanged();
                OnLoginSucceeded.Broadcast(Result);
                if (bManualCharacterFlowPending)
                {
                    ListCharacters(Result.AccountId);
                }
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] MockLogin ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
    if (!bRequestStarted)
    {
        LastErrorMessage = TEXT("Login istegi baslatilamadi.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(TEXT("Login istegi baslatilamadi."));
    }
}

void UMOBAMMOBackendSubsystem::CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId)
{
    if (!CanRunCharacterFlowAction(TEXT("Karakter olusturma artik secim akisi uzerinden yapilmali."), OnCharacterCreateFailed, CharacterStatus))
    {
        return;
    }

    const FString TrimmedAccountId = AccountId.TrimStartAndEnd();
    const FString TrimmedCharacterName = CharacterName.TrimStartAndEnd();
    const FString TrimmedClassId = ClassId.TrimStartAndEnd().IsEmpty() ? TEXT("fighter") : ClassId.TrimStartAndEnd();

    if (TrimmedAccountId.IsEmpty())
    {
        LastErrorMessage = TEXT("AccountId gereklidir.");
        CharacterStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnCharacterCreateFailed.Broadcast(TEXT("AccountId gereklidir."));
        return;
    }

    if (TrimmedCharacterName.IsEmpty())
    {
        LastErrorMessage = TEXT("Karakter adi gereklidir.");
        CharacterStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnCharacterCreateFailed.Broadcast(TEXT("Karakter adi gereklidir."));
        return;
    }

    LastErrorMessage.Reset();
    CharacterStatus = TEXT("Creating");
    NotifyDebugStateChanged();
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
                    LastErrorMessage = TEXT("Karakter olusturma istegi basarisiz oldu.");
                    CharacterStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                    LastErrorMessage = ErrorMessage;
                    CharacterStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                bCharacterFlowActionAuthorized = false;
                SelectedCharacterId = Result.CharacterId;
                LastCharacterId = Result.CharacterId;
                CachedCharacters.Add(Result);
                CharacterStatus = TEXT("Succeeded");
                NotifyDebugStateChanged();
                OnCharacterCreated.Broadcast(Result);
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] CreateCharacter ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
}

void UMOBAMMOBackendSubsystem::CreateCharacterForCurrentAccount(const FString& CharacterName, const FString& ClassId)
{
    bCharacterFlowActionAuthorized = true;
    CreateCharacter(LastAccountId, CharacterName, ClassId);
}

void UMOBAMMOBackendSubsystem::ListCharacters(const FString& AccountId)
{
    const FString TrimmedAccountId = AccountId.TrimStartAndEnd();
    if (TrimmedAccountId.IsEmpty())
    {
        LastErrorMessage = TEXT("AccountId gereklidir.");
        CharacterListStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnCharactersLoadFailed.Broadcast(TEXT("AccountId gereklidir."));
        return;
    }

    LastErrorMessage.Reset();
    CharacterListStatus = TEXT("Loading");
    NotifyDebugStateChanged();
    UE_LOG(LogTemp, Log, TEXT("[Backend] ListCharacters starting. AccountId=%s"), *TrimmedAccountId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(FString::Printf(TEXT("/characters?accountId=%s"), *TrimmedAccountId)));
    Request->SetVerb(TEXT("GET"));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[Backend] ListCharacters request failed. Success=%s ResponseValid=%s Url=%s"),
                    bWasSuccessful ? TEXT("true") : TEXT("false"),
                    Response.IsValid() ? TEXT("true") : TEXT("false"),
                    HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("<invalid request>"));
                RunOnGameThread([this]()
                {
                    LastErrorMessage = TEXT("Karakter listesi alinamadi.");
                    CharacterListStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
                    OnCharactersLoadFailed.Broadcast(TEXT("Karakter listesi alinamadi."));
                });
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                UE_LOG(LogTemp, Error, TEXT("[Backend] ListCharacters response parse/error failed. Code=%d Error=%s Body=%s"),
                    Response->GetResponseCode(),
                    *ErrorMessage,
                    *Response->GetContentAsString());
                RunOnGameThread([this, ErrorMessage]()
                {
                    LastErrorMessage = ErrorMessage;
                    CharacterListStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
                    OnCharactersLoadFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            FBackendCharacterListResult Result;
            const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
            if (JsonObject->TryGetArrayField(TEXT("items"), Items) && Items)
            {
                for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
                {
                    const TSharedPtr<FJsonObject>* ItemObject = nullptr;
                    if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemObject) || !ItemObject || !ItemObject->IsValid())
                    {
                        continue;
                    }

                    FBackendCharacterResult Item;
                    Item.CharacterId = (*ItemObject)->GetStringField(TEXT("id"));
                    Item.Name = (*ItemObject)->GetStringField(TEXT("name"));
                    Item.ClassId = (*ItemObject)->GetStringField(TEXT("classId"));
                    Item.Level = (*ItemObject)->GetIntegerField(TEXT("level"));
                    Result.Items.Add(Item);
                }
            }

            UE_LOG(LogTemp, Log, TEXT("[Backend] ListCharacters succeeded. Count=%d"), Result.Items.Num());

            RunOnGameThread([this, Result]()
            {
                CachedCharacters = Result.Items;
                if (CachedCharacters.Num() > 0)
                {
                    LastCharacterId = CachedCharacters[0].CharacterId;
                    SelectedCharacterId = LastCharacterId;
                }
                else
                {
                    SelectedCharacterId.Reset();
                }
                CharacterListStatus = TEXT("Loaded");
                NotifyDebugStateChanged();
                OnCharactersLoaded.Broadcast(Result);
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] ListCharacters ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
    if (!bRequestStarted)
    {
        LastErrorMessage = TEXT("Karakter listesi istegi baslatilamadi.");
        CharacterListStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnCharactersLoadFailed.Broadcast(TEXT("Karakter listesi istegi baslatilamadi."));
    }
}

void UMOBAMMOBackendSubsystem::StartSession(const FString& CharacterId)
{
    if (!CanRunCharacterFlowAction(TEXT("Session baslatma artik secili karakter uzerinden yapilmali."), OnSessionStartFailed, SessionStatus))
    {
        return;
    }

    const FString TrimmedCharacterId = CharacterId.TrimStartAndEnd();
    if (TrimmedCharacterId.IsEmpty())
    {
        LastErrorMessage = TEXT("CharacterId gereklidir.");
        SessionStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnSessionStartFailed.Broadcast(TEXT("CharacterId gereklidir."));
        return;
    }

    LastErrorMessage.Reset();
    SessionStatus = TEXT("Starting");
    NotifyDebugStateChanged();
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
                    LastErrorMessage = TEXT("Session baslatma istegi basarisiz oldu.");
                    SessionStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                    LastErrorMessage = ErrorMessage;
                    SessionStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                    LastErrorMessage = TEXT("Session yaniti eksik veya gecersiz.");
                    SessionStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
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
                bCharacterFlowActionAuthorized = false;
                bManualCharacterFlowPending = false;
                SessionStatus = TEXT("Ready");
                NotifyDebugStateChanged();
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
        SessionStatus = TEXT("TravelFailed");
        NotifyDebugStateChanged();
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
        SessionStatus = TEXT("TravelFailed");
        NotifyDebugStateChanged();
        return false;
    }

    LastSessionConnectString = FinalConnectString;
    SessionStatus = TEXT("Traveling");
    NotifyDebugStateChanged();
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

void UMOBAMMOBackendSubsystem::SelectCharacter(const FString& CharacterId)
{
    const FString TrimmedCharacterId = CharacterId.TrimStartAndEnd();
    if (TrimmedCharacterId.IsEmpty())
    {
        return;
    }

    for (const FBackendCharacterResult& Item : CachedCharacters)
    {
        if (Item.CharacterId == TrimmedCharacterId)
        {
            SelectedCharacterId = TrimmedCharacterId;
            LastCharacterId = TrimmedCharacterId;
            NotifyDebugStateChanged();
            return;
        }
    }
}

void UMOBAMMOBackendSubsystem::StartSessionForSelectedCharacter()
{
    if (SelectedCharacterId.IsEmpty())
    {
        LastErrorMessage = TEXT("Once bir karakter secilmelidir.");
        SessionStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnSessionStartFailed.Broadcast(TEXT("Once bir karakter secilmelidir."));
        return;
    }

    bCharacterFlowActionAuthorized = true;
    StartSession(SelectedCharacterId);
}
