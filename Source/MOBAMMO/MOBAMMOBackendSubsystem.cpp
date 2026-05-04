#include "MOBAMMOBackendSubsystem.h"

#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "JsonObjectWrapper.h"
#include "MOBAMMOBackendConfig.h"
#include "MOBAMMOPlayerState.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

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

    FString SanitizeTravelOptionValue(FString Value)
    {
        Value.ReplaceInline(TEXT("?"), TEXT(""));
        Value.ReplaceInline(TEXT("="), TEXT(""));
        Value.ReplaceInline(TEXT("&"), TEXT(""));
        return Value;
    }

    void RunOnGameThread(TFunction<void()> Callback)
    {
        AsyncTask(ENamedThreads::GameThread, MoveTemp(Callback));
    }

    struct FCharacterSaveSnapshot
    {
        FString AccountId;
        FString CharacterId;
        FString SessionId;
        FVector Position = FVector::ZeroVector;
        bool bHasPosition = false;
        int32 Level = 1;
        int32 Experience = 0;
        float Health = 100.0f;
        float MaxHealth = 100.0f;
        float Mana = 50.0f;
        float MaxMana = 50.0f;
        int32 KillCount = 0;
        int32 DeathCount = 0;
        int32 Gold = 0;
        int64 SaveSequence = 0;
        TArray<FMOBAMMOInventoryItem> InventoryItems;
    };

    bool TryBuildCharacterSaveSnapshot(APlayerController* PlayerController, FCharacterSaveSnapshot& OutSnapshot, FString& OutError, bool bRequirePawn = true)
    {
        if (!PlayerController)
        {
            OutError = TEXT("Karakter kaydi icin PlayerController gereklidir.");
            return false;
        }

        const AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>();
        const APawn* PlayerPawn = PlayerController->GetPawn();
        if (!PlayerState || (bRequirePawn && !PlayerPawn))
        {
            OutError = TEXT("Kayit icin gecerli karakter, session ve pawn bulunamadi.");
            return false;
        }

        OutSnapshot.AccountId = PlayerState->GetAccountId().TrimStartAndEnd();
        OutSnapshot.CharacterId = PlayerState->GetCharacterId().TrimStartAndEnd();
        OutSnapshot.SessionId = PlayerState->GetSessionId().TrimStartAndEnd();
        if (OutSnapshot.CharacterId.IsEmpty() || OutSnapshot.SessionId.IsEmpty())
        {
            OutError = TEXT("Kayit icin gecerli karakter, session ve pawn bulunamadi.");
            return false;
        }

        if (PlayerPawn)
        {
            OutSnapshot.Position = PlayerPawn->GetActorLocation();
            OutSnapshot.bHasPosition = true;
        }

        OutSnapshot.Level = PlayerState->GetCharacterLevel();
        OutSnapshot.Experience = PlayerState->GetCharacterExperience();
        OutSnapshot.Health = PlayerState->GetCurrentHealth();
        OutSnapshot.MaxHealth = PlayerState->GetBaseMaxHealth();
        OutSnapshot.Mana = PlayerState->GetCurrentMana();
        OutSnapshot.MaxMana = PlayerState->GetBaseMaxMana();
        OutSnapshot.KillCount = PlayerState->GetKills();
        OutSnapshot.DeathCount = PlayerState->GetDeaths();
        OutSnapshot.Gold = PlayerState->GetGold();
        OutSnapshot.InventoryItems = PlayerState->GetInventoryItems();

        const FDateTime SnapshotUtc = FDateTime::UtcNow();
        OutSnapshot.SaveSequence = (SnapshotUtc.ToUnixTimestamp() * 1000LL) + SnapshotUtc.GetMillisecond();
        return true;
    }

    FString BuildCharacterSavePayload(const FCharacterSaveSnapshot& Snapshot, bool bIncludeCharacterId)
    {
        const FString CharacterField = bIncludeCharacterId
            ? FString::Printf(TEXT("\"characterId\":\"%s\","), *Snapshot.CharacterId.ReplaceCharWithEscapedChar())
            : FString();
        const FString PositionField = Snapshot.bHasPosition
            ? FString::Printf(TEXT("\"position\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"), Snapshot.Position.X, Snapshot.Position.Y, Snapshot.Position.Z)
            : FString();

        FString InventoryJson = TEXT("\"inventory\":[");
        for (int32 i = 0; i < Snapshot.InventoryItems.Num(); ++i)
        {
            const FMOBAMMOInventoryItem& Item = Snapshot.InventoryItems[i];
            const FString SlotIndexJson = Item.SlotIndex >= 0
                ? FString::FromInt(Item.SlotIndex)
                : TEXT("null");
            InventoryJson += FString::Printf(
                TEXT("{\"itemId\":\"%s\",\"quantity\":%d,\"slotIndex\":%s}%s"),
                *Item.ItemId.ReplaceCharWithEscapedChar(),
                Item.Quantity,
                *SlotIndexJson,
                (i < Snapshot.InventoryItems.Num() - 1) ? TEXT(",") : TEXT("")
            );
        }
        InventoryJson += TEXT("]");

        return FString::Printf(
            TEXT("{%s\"sessionId\":\"%s\",%s\"level\":%d,\"experience\":%d,\"hp\":%.3f,\"maxHp\":%.3f,\"mana\":%.3f,\"maxMana\":%.3f,\"killCount\":%d,\"deathCount\":%d,\"gold\":%d,\"saveSequence\":%lld,%s}"),
            *CharacterField,
            *Snapshot.SessionId.ReplaceCharWithEscapedChar(),
            *PositionField,
            Snapshot.Level,
            Snapshot.Experience,
            Snapshot.Health,
            Snapshot.MaxHealth,
            Snapshot.Mana,
            Snapshot.MaxMana,
            Snapshot.KillCount,
            Snapshot.DeathCount,
            Snapshot.Gold,
            Snapshot.SaveSequence,
            *InventoryJson
        );
    }

    FString BuildSessionHeartbeatPayload(const FCharacterSaveSnapshot& Snapshot)
    {
        return FString::Printf(
            TEXT("{\"characterId\":\"%s\",\"sessionId\":\"%s\"}"),
            *Snapshot.CharacterId.ReplaceCharWithEscapedChar(),
            *Snapshot.SessionId.ReplaceCharWithEscapedChar()
        );
    }

    constexpr int32 MaxTransientBackendRetries = 2;

    bool IsTransientBackendFailure(bool bWasSuccessful, FHttpResponsePtr Response)
    {
        if (!bWasSuccessful || !Response.IsValid())
        {
            return true;
        }

        const int32 ResponseCode = Response->GetResponseCode();
        return ResponseCode == 408 || ResponseCode == 429 || ResponseCode >= 500;
    }

    float GetTransientBackendRetryDelaySeconds(int32 Attempt)
    {
        return Attempt <= 1 ? 0.75f : 1.5f;
    }

    void ScheduleTransientBackendRetry(UObject* Context, const TCHAR* Label, int32 NextAttempt, TFunction<void(int32)> RetryCallback)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Backend] Transient %s failure. Retrying attempt %d/%d in %.2fs."),
            Label,
            NextAttempt + 1,
            MaxTransientBackendRetries + 1,
            GetTransientBackendRetryDelaySeconds(NextAttempt));

        UWorld* World = Context ? Context->GetWorld() : nullptr;
        if (!World)
        {
            RetryCallback(NextAttempt);
            return;
        }

        FTimerDelegate RetryDelegate;
        RetryDelegate.BindLambda([NextAttempt, RetryCallback = MoveTemp(RetryCallback)]() mutable
        {
            RetryCallback(NextAttempt);
        });

        FTimerHandle RetryTimerHandle;
        World->GetTimerManager().SetTimer(RetryTimerHandle, MoveTemp(RetryDelegate), GetTransientBackendRetryDelaySeconds(NextAttempt), false);
    }

    int32 ReadOptionalJsonInt(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue)
    {
        if (!JsonObject.IsValid())
        {
            return DefaultValue;
        }

        double NumericValue = 0.0;
        if (JsonObject->TryGetNumberField(FieldName, NumericValue))
        {
            return static_cast<int32>(NumericValue);
        }

        return DefaultValue;
    }

    float ReadOptionalJsonFloat(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, float DefaultValue)
    {
        if (!JsonObject.IsValid())
        {
            return DefaultValue;
        }

        double NumericValue = 0.0;
        if (JsonObject->TryGetNumberField(FieldName, NumericValue))
        {
            return static_cast<float>(NumericValue);
        }

        return DefaultValue;
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
    , SaveStatus(TEXT("Idle"))
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
    const bool bLegacyMockLoginAllowed =
        FParse::Param(FCommandLine::Get(), TEXT("MOBAMMOAutoSession")) ||
        FParse::Param(FCommandLine::Get(), TEXT("MOBAMMOAllowLegacyMockLogin"));
    if (!bLegacyMockLoginAllowed)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Backend] Legacy MockLogin blocked. Login must come from WebUI or -MOBAMMOAutoSession."));
        return;
    }

    RunLoginRequest(Username);
}

void UMOBAMMOBackendSubsystem::LoginFromWebUI(const FString& Username, const FString& Password, bool bRememberMe)
{
    RunLoginRequest(Username, Password, bRememberMe);
}

void UMOBAMMOBackendSubsystem::RegisterFromWebUI(const FString& Username, const FString& Password, bool bRememberMe)
{
    RunAuthRequest(TEXT("/auth/register"), Username, Password, false, bRememberMe);
}

void UMOBAMMOBackendSubsystem::RefreshAuthSession()
{
    FString TrimmedRefreshToken = LastRefreshToken.TrimStartAndEnd();
    if (TrimmedRefreshToken.IsEmpty())
    {
        FString PersistedUsername;
        FString PersistedAccountId;
        if (LoadPersistedAuthSession(TrimmedRefreshToken, PersistedUsername, PersistedAccountId))
        {
            LastRefreshToken = TrimmedRefreshToken;
            if (!PersistedUsername.IsEmpty())
            {
                LastUsername = PersistedUsername;
            }
            if (!PersistedAccountId.IsEmpty())
            {
                LastAccountId = PersistedAccountId;
            }
            bPersistRefreshToken = true;
        }
    }

    if (TrimmedRefreshToken.IsEmpty())
    {
        LastErrorMessage = TEXT("Refresh token bulunamadi. Lutfen tekrar login olun.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(LastErrorMessage);
        return;
    }

    LoginStatus = TEXT("Refreshing");
    LastErrorMessage.Reset();
    NotifyDebugStateChanged();

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/auth/refresh")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(FString::Printf(TEXT("{\"refreshToken\":\"%s\"}"), *TrimmedRefreshToken.ReplaceCharWithEscapedChar()));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                RunOnGameThread([this]()
                {
                    LastErrorMessage = TEXT("Auth yenileme istegi basarisiz oldu.");
                    LoginStatus = TEXT("Failed");
                    NotifyDebugStateChanged();
                    OnLoginFailed.Broadcast(LastErrorMessage);
                });
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            FString ErrorMessage;
            if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
            {
                RunOnGameThread([this, ErrorMessage]()
                {
                    LastErrorMessage = ErrorMessage;
                    LoginStatus = TEXT("Failed");
                    LastAuthToken.Reset();
                    LastRefreshToken.Reset();
                    ClearPersistedAuthSession();
                    bPersistRefreshToken = false;
                    NotifyDebugStateChanged();
                    OnLoginFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            HandleAuthSessionResponse(JsonObject, bPersistRefreshToken);
        }
    );

    if (!Request->ProcessRequest())
    {
        LastErrorMessage = TEXT("Auth yenileme istegi baslatilamadi.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(LastErrorMessage);
    }
}

void UMOBAMMOBackendSubsystem::LogoutFromWebUI()
{
    RunLogoutRequest();
}

bool UMOBAMMOBackendSubsystem::TryRestoreRememberedAuthSession()
{
    if (LoginStatus == TEXT("LoggingIn") || LoginStatus == TEXT("Refreshing") || LoginStatus == TEXT("Succeeded"))
    {
        return false;
    }

    FString PersistedRefreshToken;
    FString PersistedUsername;
    FString PersistedAccountId;
    if (!LoadPersistedAuthSession(PersistedRefreshToken, PersistedUsername, PersistedAccountId))
    {
        return false;
    }

    LastRefreshToken = PersistedRefreshToken;
    if (!PersistedUsername.IsEmpty())
    {
        LastUsername = PersistedUsername;
    }
    if (!PersistedAccountId.IsEmpty())
    {
        LastAccountId = PersistedAccountId;
    }
    bPersistRefreshToken = true;
    RefreshAuthSession();
    return true;
}

void UMOBAMMOBackendSubsystem::RunLoginRequest(const FString& Username, const FString& Password, bool bRememberMe)
{
    RunAuthRequest(TEXT("/auth/login"), Username, Password, true, bRememberMe);
}

void UMOBAMMOBackendSubsystem::RunAuthRequest(const FString& RoutePath, const FString& Username, const FString& Password, bool bAllowPasswordlessLogin, bool bRememberMe)
{
    if (ShouldSkipAutomaticBackendBootstrap(this))
    {
        UE_LOG(LogTemp, Log, TEXT("[Backend] Auth request skipped because world is already a network client."));
        NotifyDebugStateChanged();
        return;
    }

    const FString TrimmedUsername = Username.TrimStartAndEnd();
    const FString TrimmedPassword = Password.TrimStartAndEnd();
    if (TrimmedUsername.IsEmpty())
    {
        LastErrorMessage = TEXT("Username gereklidir.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(TEXT("Username gereklidir."));
        return;
    }

    if (!bAllowPasswordlessLogin && TrimmedPassword.IsEmpty())
    {
        LastErrorMessage = TEXT("Sifre gereklidir.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(TEXT("Sifre gereklidir."));
        return;
    }

    LastUsername = TrimmedUsername;
    LastErrorMessage.Reset();
    bPersistRefreshToken = bRememberMe;
    if (!bRememberMe)
    {
        ClearPersistedAuthSession();
    }
    bManualCharacterFlowPending = false;
    bMainMenuVisible = false;
    bCharacterSelectRequested = false;
    LoginStatus = TEXT("LoggingIn");
    NotifyDebugStateChanged();
    const FString LoginUrl = BuildUrl(RoutePath);
    UE_LOG(LogTemp, Log, TEXT("[Backend] Auth request starting. Route=%s Username=%s Url=%s"), *RoutePath, *TrimmedUsername, *LoginUrl);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(LoginUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (TrimmedPassword.IsEmpty())
    {
        Request->SetContentAsString(FString::Printf(TEXT("{\"username\":\"%s\"}"), *TrimmedUsername.ReplaceCharWithEscapedChar()));
    }
    else
    {
        Request->SetContentAsString(FString::Printf(
            TEXT("{\"username\":\"%s\",\"password\":\"%s\"}"),
            *TrimmedUsername.ReplaceCharWithEscapedChar(),
            *TrimmedPassword.ReplaceCharWithEscapedChar()));
    }

    Request->OnProcessRequestComplete().BindLambda(
        [this, bRememberMe](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
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

            HandleAuthSessionResponse(JsonObject, bRememberMe);
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] Auth ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
    if (!bRequestStarted)
    {
        LastErrorMessage = TEXT("Login istegi baslatilamadi.");
        LoginStatus = TEXT("Failed");
        NotifyDebugStateChanged();
        OnLoginFailed.Broadcast(TEXT("Login istegi baslatilamadi."));
    }
}

FString UMOBAMMOBackendSubsystem::GetPersistedAuthSessionPath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Auth"), TEXT("session.json"));
}

bool UMOBAMMOBackendSubsystem::LoadPersistedAuthSession(FString& OutRefreshToken, FString& OutUsername, FString& OutAccountId) const
{
    OutRefreshToken.Reset();
    OutUsername.Reset();
    OutAccountId.Reset();

    FString Payload;
    if (!FFileHelper::LoadFileToString(Payload, *GetPersistedAuthSessionPath()))
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    bool bRememberMe = false;
    JsonObject->TryGetBoolField(TEXT("rememberMe"), bRememberMe);
    JsonObject->TryGetStringField(TEXT("refreshToken"), OutRefreshToken);
    JsonObject->TryGetStringField(TEXT("username"), OutUsername);
    JsonObject->TryGetStringField(TEXT("accountId"), OutAccountId);
    OutRefreshToken = OutRefreshToken.TrimStartAndEnd();
    return bRememberMe && !OutRefreshToken.IsEmpty();
}

void UMOBAMMOBackendSubsystem::SavePersistedAuthSession(const FString& RefreshToken, const FString& Username, const FString& AccountId) const
{
    const FString TrimmedRefreshToken = RefreshToken.TrimStartAndEnd();
    if (TrimmedRefreshToken.IsEmpty())
    {
        ClearPersistedAuthSession();
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetBoolField(TEXT("rememberMe"), true);
    JsonObject->SetStringField(TEXT("refreshToken"), TrimmedRefreshToken);
    JsonObject->SetStringField(TEXT("username"), Username);
    JsonObject->SetStringField(TEXT("accountId"), AccountId);
    JsonObject->SetStringField(TEXT("role"), LastAccountRole);
    JsonObject->SetStringField(TEXT("savedAtUtc"), FDateTime::UtcNow().ToIso8601());

    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    if (!FJsonSerializer::Serialize(JsonObject, Writer))
    {
        return;
    }

    const FString PersistedPath = GetPersistedAuthSessionPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(PersistedPath), true);
    FFileHelper::SaveStringToFile(Payload, *PersistedPath);
}

void UMOBAMMOBackendSubsystem::ClearPersistedAuthSession() const
{
    IFileManager::Get().Delete(*GetPersistedAuthSessionPath(), false, true, true);
}

void UMOBAMMOBackendSubsystem::HandleAuthSessionResponse(const TSharedPtr<FJsonObject>& JsonObject, bool bRememberMe)
{
    if (!JsonObject.IsValid())
    {
        RunOnGameThread([this]()
        {
            LastErrorMessage = TEXT("Auth yaniti eksik veya gecersiz.");
            LoginStatus = TEXT("Failed");
            NotifyDebugStateChanged();
            OnLoginFailed.Broadcast(LastErrorMessage);
        });
        return;
    }

    FBackendLoginResult Result;
    JsonObject->TryGetStringField(TEXT("accountId"), Result.AccountId);
    JsonObject->TryGetStringField(TEXT("token"), Result.Token);
    if (Result.Token.IsEmpty())
    {
        JsonObject->TryGetStringField(TEXT("accessToken"), Result.Token);
    }
    JsonObject->TryGetStringField(TEXT("refreshToken"), Result.RefreshToken);
    JsonObject->TryGetStringField(TEXT("username"), Result.Username);
    JsonObject->TryGetStringField(TEXT("role"), Result.Role);
    if (Result.RefreshToken.IsEmpty() && !LastRefreshToken.IsEmpty())
    {
        Result.RefreshToken = LastRefreshToken;
    }
    if (Result.Username.IsEmpty() && !LastUsername.IsEmpty())
    {
        Result.Username = LastUsername;
    }
    if (Result.Role.IsEmpty())
    {
        Result.Role = TEXT("player");
    }

    if (Result.AccountId.IsEmpty() || Result.Token.IsEmpty())
    {
        RunOnGameThread([this]()
        {
            LastErrorMessage = TEXT("Auth yanitinda account veya token eksik.");
            LoginStatus = TEXT("Failed");
            NotifyDebugStateChanged();
            OnLoginFailed.Broadcast(LastErrorMessage);
        });
        return;
    }

    LastAccountId = Result.AccountId;
    LastAuthToken = Result.Token;
    LastRefreshToken = Result.RefreshToken;
    LastUsername = Result.Username;
    LastAccountRole = Result.Role;
    bPersistRefreshToken = bRememberMe;
    if (bRememberMe)
    {
        SavePersistedAuthSession(Result.RefreshToken, Result.Username, Result.AccountId);
    }
    else
    {
        ClearPersistedAuthSession();
    }
    UE_LOG(LogTemp, Log, TEXT("[Backend] Auth succeeded. AccountId=%s Username=%s Refresh=%s"),
        *Result.AccountId,
        *Result.Username,
        Result.RefreshToken.IsEmpty() ? TEXT("no") : TEXT("yes"));

    RunOnGameThread([this, Result]()
    {
        LoginStatus = TEXT("Succeeded");
        bManualCharacterFlowPending = ShouldUseManualCharacterFlow();
        bCharacterFlowActionAuthorized = false;
        bMainMenuVisible = false;
        bCharacterSelectRequested = bManualCharacterFlowPending;
        NotifyDebugStateChanged();
        OnLoginSucceeded.Broadcast(Result);
        if (bManualCharacterFlowPending)
        {
            ListCharacters(Result.AccountId);
        }
    });
}

void UMOBAMMOBackendSubsystem::RunLogoutRequest()
{
    const FString RefreshToken = LastRefreshToken.TrimStartAndEnd();
    if (!RefreshToken.IsEmpty())
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(BuildUrl(TEXT("/auth/logout")));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(FString::Printf(TEXT("{\"refreshToken\":\"%s\"}"), *RefreshToken.ReplaceCharWithEscapedChar()));
        Request->ProcessRequest();
    }

    LastAccountId.Reset();
    LastAuthToken.Reset();
    LastRefreshToken.Reset();
    bPersistRefreshToken = false;
    ClearPersistedAuthSession();
    LastAccountRole.Reset();
    LastCharacterId.Reset();
    SelectedCharacterId.Reset();
    CachedCharacters.Reset();
    LastSessionConnectString.Reset();
    LastSessionResult = FBackendSessionResult();
    LastErrorMessage.Reset();
    LastSaveErrorMessage.Reset();
    LoginStatus = TEXT("Idle");
    CharacterListStatus = TEXT("Idle");
    CharacterStatus = TEXT("Idle");
    SessionStatus = TEXT("Idle");
    SaveStatus = TEXT("Idle");
    bManualCharacterFlowPending = false;
    bCharacterFlowActionAuthorized = false;
    bMainMenuVisible = false;
    bCharacterSelectRequested = false;
    bSaveConnectionHealthy = true;
    bReconnectTravelPending = false;
    PendingReconnectPlayerController.Reset();
    NotifyDebugStateChanged();
}

void UMOBAMMOBackendSubsystem::CreateCharacter(const FString& AccountId, const FString& CharacterName, const FString& ClassId, int32 PresetId, int32 ColorIndex, int32 Shade, int32 Transparent, int32 TextureDetail)
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
    ApplyAuthHeader(Request);
    Request->SetContentAsString(
        FString::Printf(
            TEXT("{\"accountId\":\"%s\",\"name\":\"%s\",\"classId\":\"%s\",\"presetId\":%d,\"colorIndex\":%d,\"shade\":%d,\"transparent\":%d,\"textureDetail\":%d}"),
            *TrimmedAccountId.ReplaceCharWithEscapedChar(),
            *TrimmedCharacterName.ReplaceCharWithEscapedChar(),
            *TrimmedClassId.ReplaceCharWithEscapedChar(),
            PresetId,
            ColorIndex,
            Shade,
            Transparent,
            TextureDetail
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

void UMOBAMMOBackendSubsystem::CreateCharacterForCurrentAccount(const FString& CharacterName, const FString& ClassId, int32 PresetId, int32 ColorIndex, int32 Shade, int32 Transparent, int32 TextureDetail)
{
    bMainMenuVisible = false;
    bCharacterSelectRequested = true;
    bCharacterFlowActionAuthorized = true;
    CreateCharacter(LastAccountId, CharacterName, ClassId, PresetId, ColorIndex, Shade, Transparent, TextureDetail);
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
    ApplyAuthHeader(Request);

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
        bReconnectTravelPending = false;
        PendingReconnectPlayerController.Reset();
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
    ApplyAuthHeader(Request);
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
                    bReconnectTravelPending = false;
                    PendingReconnectPlayerController.Reset();
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
                    bReconnectTravelPending = false;
                    PendingReconnectPlayerController.Reset();
                    NotifyDebugStateChanged();
                    OnSessionStartFailed.Broadcast(ErrorMessage);
                });
                return;
            }

            const TSharedPtr<FJsonObject>* CharacterObject = nullptr;
            const TSharedPtr<FJsonObject>* ServerObject = nullptr;
            const TSharedPtr<FJsonObject>* CharacterSnapshotObject = nullptr;
            if (!JsonObject->TryGetObjectField(TEXT("character"), CharacterObject) || !JsonObject->TryGetObjectField(TEXT("server"), ServerObject))
            {
                RunOnGameThread([this]()
                {
                    LastErrorMessage = TEXT("Session yaniti eksik veya gecersiz.");
                    SessionStatus = TEXT("Failed");
                    bReconnectTravelPending = false;
                    PendingReconnectPlayerController.Reset();
                    NotifyDebugStateChanged();
                    OnSessionStartFailed.Broadcast(TEXT("Session yaniti eksik veya gecersiz."));
                });
                return;
            }

            FBackendSessionResult Result;
            Result.SessionId = JsonObject->GetStringField(TEXT("sessionId"));
            Result.CharacterId = (*CharacterObject)->GetStringField(TEXT("id"));
            Result.CharacterName = (*CharacterObject)->GetStringField(TEXT("name"));
            Result.CharacterClassId = (*CharacterObject)->GetStringField(TEXT("classId"));
            Result.CharacterLevel = (*CharacterObject)->GetIntegerField(TEXT("level"));
            Result.CharacterExperience = ReadOptionalJsonInt(*CharacterObject, TEXT("experience"), 0);
            Result.ServerHost = (*ServerObject)->GetStringField(TEXT("host"));
            Result.ServerPort = (*ServerObject)->GetIntegerField(TEXT("port"));
            Result.MapName = (*ServerObject)->GetStringField(TEXT("map"));
            Result.ConnectString = (*ServerObject)->GetStringField(TEXT("connectString"));

            const TSharedPtr<FJsonObject>* PositionObject = nullptr;
            if ((*CharacterObject)->TryGetObjectField(TEXT("position"), PositionObject) && PositionObject && PositionObject->IsValid())
            {
                Result.CharacterPosition.X = ReadOptionalJsonFloat(*PositionObject, TEXT("x"), 0.0f);
                Result.CharacterPosition.Y = ReadOptionalJsonFloat(*PositionObject, TEXT("y"), 0.0f);
                Result.CharacterPosition.Z = ReadOptionalJsonFloat(*PositionObject, TEXT("z"), 0.0f);
            }

            const TSharedPtr<FJsonObject>* AppearanceObject = nullptr;
            if ((*CharacterObject)->TryGetObjectField(TEXT("appearance"), AppearanceObject) && AppearanceObject && AppearanceObject->IsValid())
            {
                Result.CharacterPresetId = ReadOptionalJsonInt(*AppearanceObject, TEXT("presetId"), Result.CharacterPresetId);
                Result.CharacterColorIndex = ReadOptionalJsonInt(*AppearanceObject, TEXT("colorIndex"), Result.CharacterColorIndex);
                Result.CharacterShade = ReadOptionalJsonInt(*AppearanceObject, TEXT("shade"), Result.CharacterShade);
                Result.CharacterTransparent = ReadOptionalJsonInt(*AppearanceObject, TEXT("transparent"), Result.CharacterTransparent);
                Result.CharacterTextureDetail = ReadOptionalJsonInt(*AppearanceObject, TEXT("textureDetail"), Result.CharacterTextureDetail);
            }

            if (JsonObject->TryGetObjectField(TEXT("characterSnapshot"), CharacterSnapshotObject) && CharacterSnapshotObject && CharacterSnapshotObject->IsValid())
            {
                const TSharedPtr<FJsonObject>* StatsObject = nullptr;
                if ((*CharacterSnapshotObject)->TryGetObjectField(TEXT("stats"), StatsObject) && StatsObject && StatsObject->IsValid())
                {
                    Result.CurrentHealth = ReadOptionalJsonFloat(*StatsObject, TEXT("hp"), Result.CurrentHealth);
                    Result.MaxHealth = ReadOptionalJsonFloat(*StatsObject, TEXT("maxHp"), Result.MaxHealth);
                    Result.CurrentMana = ReadOptionalJsonFloat(*StatsObject, TEXT("mana"), Result.CurrentMana);
                    Result.MaxMana = ReadOptionalJsonFloat(*StatsObject, TEXT("maxMana"), Result.MaxMana);
                    Result.KillCount = ReadOptionalJsonInt(*StatsObject, TEXT("killCount"), Result.KillCount);
                    Result.DeathCount = ReadOptionalJsonInt(*StatsObject, TEXT("deathCount"), Result.DeathCount);
                    Result.Gold = ReadOptionalJsonInt(*StatsObject, TEXT("gold"), Result.Gold);
                }

                const TArray<TSharedPtr<FJsonValue>>* InventoryArray = nullptr;
                if ((*CharacterSnapshotObject)->TryGetArrayField(TEXT("inventory"), InventoryArray) && InventoryArray)
                {
                    for (const TSharedPtr<FJsonValue>& ItemValue : *InventoryArray)
                    {
                        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
                        if (ItemValue.IsValid() && ItemValue->TryGetObject(ItemObject) && ItemObject && ItemObject->IsValid())
                        {
                            FMOBAMMOInventoryItem InventoryItem;
                            InventoryItem.ItemId = (*ItemObject)->GetStringField(TEXT("itemId"));
                            InventoryItem.Quantity = ReadOptionalJsonInt(*ItemObject, TEXT("quantity"), 1);
                            InventoryItem.SlotIndex = ReadOptionalJsonInt(*ItemObject, TEXT("slotIndex"), -1);
                            Result.InventoryItems.Add(InventoryItem);
                        }
                    }
                }
            }

            LastCharacterId = Result.CharacterId;
            LastSessionConnectString = Result.ConnectString;
            LastSessionResult = Result;
            UE_LOG(LogTemp, Log, TEXT("[Backend] StartSession succeeded. CharacterId=%s ConnectString=%s Map=%s"),
                *Result.CharacterId,
                *Result.ConnectString,
                *Result.MapName);

            RunOnGameThread([this, Result]()
            {
                bCharacterFlowActionAuthorized = false;
                bManualCharacterFlowPending = false;
                bMainMenuVisible = false;
                bCharacterSelectRequested = false;
                SessionStatus = TEXT("Ready");
                SaveStatus = TEXT("Ready");
                LastSaveErrorMessage.Reset();
                bSaveConnectionHealthy = true;
                NotifyDebugStateChanged();
                OnSessionStarted.Broadcast(Result);

                if (bReconnectTravelPending)
                {
                    APlayerController* ReconnectController = PendingReconnectPlayerController.Get();
                    bReconnectTravelPending = false;
                    PendingReconnectPlayerController.Reset();

                    if (ReconnectController)
                    {
                        TravelToSession(ReconnectController, Result.ConnectString);
                    }
                }
            });
        }
    );

    const bool bRequestStarted = Request->ProcessRequest();
    UE_LOG(LogTemp, Log, TEXT("[Backend] StartSession ProcessRequest returned %s"), bRequestStarted ? TEXT("true") : TEXT("false"));
    if (!bRequestStarted)
    {
        LastErrorMessage = TEXT("Session baslatma istegi baslatilamadi.");
        SessionStatus = TEXT("Failed");
        bReconnectTravelPending = false;
        PendingReconnectPlayerController.Reset();
        NotifyDebugStateChanged();
        OnSessionStartFailed.Broadcast(TEXT("Session baslatma istegi baslatilamadi."));
    }
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

    if (SessionStatus == TEXT("Traveling") && LastSessionConnectString == FinalConnectString)
    {
        UE_LOG(LogTemp, Log, TEXT("[Backend] TravelToSession skipped because travel is already in progress. ConnectString=%s"), *FinalConnectString);
        return true;
    }

    const UWorld* World = GetWorld();
    const bool bForceDedicatedSessionServer = FParse::Param(FCommandLine::Get(), TEXT("MOBAMMOUseDedicatedSessionServer"));
    const bool bUseLocalSessionTravel = World && World->GetNetMode() == NM_Standalone && !bForceDedicatedSessionServer;
    const FString TravelTarget = bUseLocalSessionTravel ? BuildLocalSessionTravelUrl() : FinalConnectString;

    UE_LOG(LogTemp, Log, TEXT("[Backend] TravelToSession mode=%s target=%s"),
        bUseLocalSessionTravel ? TEXT("LocalStandalone") : TEXT("DedicatedServer"),
        *TravelTarget);

    LastSessionConnectString = FinalConnectString;
    SessionStatus = bUseLocalSessionTravel ? TEXT("Active") : TEXT("Traveling");
    NotifyDebugStateChanged();
    PlayerController->ClientTravel(BuildReplicatedTravelConnectString(TravelTarget.IsEmpty() ? FinalConnectString : TravelTarget), TRAVEL_Absolute);
    return true;
}

void UMOBAMMOBackendSubsystem::NotifyClientEnteredSessionWorld()
{
    const UWorld* World = GetWorld();
    if (!World || (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone))
    {
        return;
    }

    if (SessionStatus == TEXT("Traveling") || SessionStatus == TEXT("Ready"))
    {
        SessionStatus = TEXT("Active");
        LastErrorMessage.Reset();
        NotifyDebugStateChanged();
    }
}

FString UMOBAMMOBackendSubsystem::BuildReplicatedTravelConnectString(const FString& ConnectString) const
{
    const FString BaseConnectString = ConnectString.TrimStartAndEnd();
    if (BaseConnectString.IsEmpty())
    {
        return FString();
    }

    FString FinalConnectString = BaseConnectString;
    auto AppendOption = [&FinalConnectString](const TCHAR* Key, const FString& RawValue)
    {
        const FString Value = SanitizeTravelOptionValue(RawValue);
        if (!Value.IsEmpty())
        {
            FinalConnectString += FString::Printf(TEXT("?%s=%s"), Key, *Value);
        }
    };

    auto AppendIntOption = [&AppendOption](const TCHAR* Key, int32 Value)
    {
        AppendOption(Key, FString::FromInt(Value));
    };

    auto AppendFloatOption = [&AppendOption](const TCHAR* Key, float Value)
    {
        AppendOption(Key, FString::SanitizeFloat(Value));
    };

    AppendOption(TEXT("AccountId"), LastAccountId);
    AppendOption(TEXT("CharacterId"), LastCharacterId);
    AppendOption(TEXT("SessionId"), LastSessionResult.SessionId);

    FString SelectedCharacterName;
    FString SelectedClassId;
    int32 SelectedLevel = 1;
    int32 SelectedExperience = 0;
    for (const FBackendCharacterResult& Item : CachedCharacters)
    {
        if (Item.CharacterId == LastCharacterId)
        {
            SelectedCharacterName = Item.Name;
            SelectedClassId = Item.ClassId;
            SelectedLevel = FMath::Max(1, Item.Level);
            break;
        }
    }

    if (LastSessionResult.CharacterId == LastCharacterId)
    {
        if (!LastSessionResult.CharacterName.IsEmpty())
        {
            SelectedCharacterName = LastSessionResult.CharacterName;
        }

        if (!LastSessionResult.CharacterClassId.IsEmpty())
        {
            SelectedClassId = LastSessionResult.CharacterClassId;
        }

        SelectedLevel = FMath::Max(1, LastSessionResult.CharacterLevel);
        SelectedExperience = FMath::Max(0, LastSessionResult.CharacterExperience);
    }

    AppendOption(TEXT("CharacterName"), SelectedCharacterName.IsEmpty() ? LastUsername : SelectedCharacterName);
    AppendOption(TEXT("ClassId"), SelectedClassId);
    AppendIntOption(TEXT("Level"), SelectedLevel);
    AppendIntOption(TEXT("Experience"), SelectedExperience);

    if (LastSessionResult.CharacterId == LastCharacterId)
    {
        AppendFloatOption(TEXT("PositionX"), LastSessionResult.CharacterPosition.X);
        AppendFloatOption(TEXT("PositionY"), LastSessionResult.CharacterPosition.Y);
        AppendFloatOption(TEXT("PositionZ"), LastSessionResult.CharacterPosition.Z);
        AppendIntOption(TEXT("PresetId"), LastSessionResult.CharacterPresetId);
        AppendIntOption(TEXT("ColorIndex"), LastSessionResult.CharacterColorIndex);
        AppendIntOption(TEXT("Shade"), LastSessionResult.CharacterShade);
        AppendIntOption(TEXT("Transparent"), LastSessionResult.CharacterTransparent);
        AppendIntOption(TEXT("TextureDetail"), LastSessionResult.CharacterTextureDetail);
        AppendFloatOption(TEXT("Health"), LastSessionResult.CurrentHealth);
        AppendFloatOption(TEXT("MaxHealth"), LastSessionResult.MaxHealth);
        AppendFloatOption(TEXT("Mana"), LastSessionResult.CurrentMana);
        AppendFloatOption(TEXT("MaxMana"), LastSessionResult.MaxMana);
        AppendIntOption(TEXT("KillCount"), LastSessionResult.KillCount);
        AppendIntOption(TEXT("DeathCount"), LastSessionResult.DeathCount);
        AppendIntOption(TEXT("Gold"), LastSessionResult.Gold);

        FString InventoryString;
        for (int32 i = 0; i < LastSessionResult.InventoryItems.Num(); ++i)
        {
            const FMOBAMMOInventoryItem& Item = LastSessionResult.InventoryItems[i];
            InventoryString += FString::Printf(TEXT("%s:%d:%d%s"), *Item.ItemId, Item.Quantity, Item.SlotIndex, i < LastSessionResult.InventoryItems.Num() - 1 ? TEXT(",") : TEXT(""));
        }
        
        if (!InventoryString.IsEmpty())
        {
            // Simple replace to avoid breaking the URL
            InventoryString.ReplaceInline(TEXT("="), TEXT(""));
            InventoryString.ReplaceInline(TEXT("?"), TEXT(""));
            InventoryString.ReplaceInline(TEXT("&"), TEXT(""));
            AppendOption(TEXT("Inventory"), InventoryString);
        }
    }

    return FinalConnectString;
}

FString UMOBAMMOBackendSubsystem::BuildUrl(const FString& Path) const
{
    return FString::Printf(TEXT("%s%s"), *GetBackendBaseUrl(), *Path);
}

FString UMOBAMMOBackendSubsystem::BuildLocalSessionTravelUrl() const
{
    FString MapName = LastSessionResult.MapName.TrimStartAndEnd();
    if (MapName.IsEmpty())
    {
        MapName = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");
    }

    if (!MapName.Contains(TEXT("?game=")))
    {
        MapName += TEXT("?game=/Script/MOBAMMO.MOBAMMOGameMode");
    }

    return MapName;
}

void UMOBAMMOBackendSubsystem::ApplyAuthHeader(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request, const FString& TokenFallback) const
{
    FString Token = LastAuthToken.TrimStartAndEnd();
    const FString TrimmedTokenFallback = TokenFallback.TrimStartAndEnd();
    Token = Token.IsEmpty() ? TrimmedTokenFallback : Token;

    if (!Token.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
        return;
    }

    const UWorld* World = GetWorld();
    if (World && World->GetNetMode() == NM_DedicatedServer)
    {
        const FString SessionServerSecret = GetDefault<UMOBAMMOBackendConfig>()->ResolveSessionServerSecret();
        if (!SessionServerSecret.IsEmpty())
        {
            Request->SetHeader(TEXT("X-Session-Server-Secret"), SessionServerSecret);
        }
    }
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

FString UMOBAMMOBackendSubsystem::BuildErrorCode(FHttpResponseHandle Response) const
{
    if (!Response.IsValid())
    {
        return FString();
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString ErrorCode;
        if (JsonObject->TryGetStringField(TEXT("error"), ErrorCode))
        {
            return ErrorCode;
        }
    }

    return FString();
}

bool UMOBAMMOBackendSubsystem::IsSessionInvalidationError(FHttpResponseHandle Response) const
{
    const FString ErrorCode = BuildErrorCode(Response);
    return ErrorCode == TEXT("session_expired") || ErrorCode == TEXT("stale_or_invalid_session");
}

void UMOBAMMOBackendSubsystem::SetReplicatedPersistenceStatusForCharacter(const FString& CharacterId, const FString& Status, const FString& ErrorMessage) const
{
    const FString TrimmedCharacterId = CharacterId.TrimStartAndEnd();
    UWorld* World = GetWorld();
    if (TrimmedCharacterId.IsEmpty() || !World)
    {
        return;
    }

    for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();
        AMOBAMMOPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;
        if (PlayerState && PlayerState->GetCharacterId() == TrimmedCharacterId)
        {
            PlayerState->SetPersistenceStatus(Status, ErrorMessage);
        }
    }
}

void UMOBAMMOBackendSubsystem::MarkSaveFailed(const FString& ErrorMessage, FHttpResponseHandle Response, const FString& CharacterId)
{
    LastErrorMessage = ErrorMessage;
    LastSaveErrorMessage = ErrorMessage;
    CharacterStatus = TEXT("SaveFailed");
    SaveStatus = TEXT("Failed");
    bSaveConnectionHealthy = false;

    if (IsSessionInvalidationError(Response))
    {
        SessionStatus = TEXT("ReconnectRequired");
        SaveStatus = TEXT("SessionInvalid");
    }

    SetReplicatedPersistenceStatusForCharacter(CharacterId, SaveStatus, ErrorMessage);

    const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
    const FString ErrorCode = Response.IsValid() ? BuildErrorCode(Response) : FString();
    UE_LOG(LogTemp, Warning, TEXT("[Backend] Save failed. Status=%s SessionStatus=%s Code=%d ErrorCode=%s Message=%s"),
        *SaveStatus,
        *SessionStatus,
        ResponseCode,
        ErrorCode.IsEmpty() ? TEXT("<none>") : *ErrorCode,
        *ErrorMessage);

    NotifyDebugStateChanged();
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

void UMOBAMMOBackendSubsystem::OpenMainMenuCharacters()
{
    if (!bManualCharacterFlowPending)
    {
        return;
    }

    bMainMenuVisible = false;
    bCharacterSelectRequested = true;
    NotifyDebugStateChanged();
}

void UMOBAMMOBackendSubsystem::PlayFromMainMenu()
{
    if (!bManualCharacterFlowPending)
    {
        return;
    }

    if (CachedCharacters.Num() == 0)
    {
        OpenMainMenuCharacters();
        return;
    }

    if (SelectedCharacterId.IsEmpty())
    {
        SelectedCharacterId = CachedCharacters[0].CharacterId;
        LastCharacterId = SelectedCharacterId;
    }

    bCharacterSelectRequested = false;
    bCharacterFlowActionAuthorized = true;
    NotifyDebugStateChanged();
    StartSessionForSelectedCharacter();
}

void UMOBAMMOBackendSubsystem::ReconnectCurrentSession(APlayerController* PlayerController)
{
    FString CharacterId = SelectedCharacterId;
    if (CharacterId.IsEmpty())
    {
        CharacterId = LastCharacterId;
    }
    if (CharacterId.IsEmpty())
    {
        CharacterId = LastSessionResult.CharacterId;
    }
    if (CharacterId.IsEmpty() && PlayerController)
    {
        if (const AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>())
        {
            CharacterId = PlayerState->GetCharacterId();
        }
    }

    if (!PlayerController || CharacterId.IsEmpty())
    {
        LastErrorMessage = TEXT("Reconnect icin aktif karakter veya PlayerController bulunamadi.");
        LastSaveErrorMessage = LastErrorMessage;
        SessionStatus = TEXT("ReconnectFailed");
        SaveStatus = TEXT("SessionInvalid");
        bSaveConnectionHealthy = false;
        NotifyDebugStateChanged();
        return;
    }

    SelectedCharacterId = CharacterId;
    LastCharacterId = CharacterId;
    bReconnectTravelPending = true;
    PendingReconnectPlayerController = PlayerController;
    bCharacterFlowActionAuthorized = true;
    SessionStatus = TEXT("Reconnecting");
    SaveStatus = TEXT("Reconnecting");
    NotifyDebugStateChanged();

    UE_LOG(LogTemp, Log, TEXT("[Backend] Reconnect starting. CharacterId=%s"), *CharacterId);
    StartSession(CharacterId);
}

void UMOBAMMOBackendSubsystem::SaveCurrentCharacterProgress(APlayerController* PlayerController)
{
    FCharacterSaveSnapshot Snapshot;
    FString SnapshotError;
    // bRequirePawn=false: save works even when pawn is null (dead/respawning); position is omitted from payload
    if (!TryBuildCharacterSaveSnapshot(PlayerController, Snapshot, SnapshotError, false))
    {
        MarkSaveFailed(SnapshotError);
        return;
    }

    LastErrorMessage.Reset();
    CharacterStatus = TEXT("Saving");
    SaveStatus = TEXT("Saving");
    NotifyDebugStateChanged();

    TSharedPtr<TFunction<void(int32)>> SendAttempt = MakeShared<TFunction<void(int32)>>();
    *SendAttempt = [this, Snapshot, SendAttempt](int32 Attempt)
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(BuildUrl(FString::Printf(TEXT("/characters/%s/save"), *Snapshot.CharacterId)));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        ApplyAuthHeader(Request);
        Request->SetContentAsString(BuildCharacterSavePayload(Snapshot, false));

        Request->OnProcessRequestComplete().BindLambda(
            [this, Snapshot, SendAttempt, Attempt](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
            {
                if (IsTransientBackendFailure(bWasSuccessful, Response) && Attempt < MaxTransientBackendRetries)
                {
                    RunOnGameThread([this, SendAttempt, Attempt]()
                    {
                        ScheduleTransientBackendRetry(this, TEXT("save"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                        {
                            (*SendAttempt)(NextAttempt);
                        });
                    });
                    return;
                }

                if (!bWasSuccessful || !Response.IsValid())
                {
                    RunOnGameThread([this, Snapshot]()
                    {
                        MarkSaveFailed(TEXT("Karakter kaydi istegi basarisiz oldu."), nullptr, Snapshot.CharacterId);
                    });
                    return;
                }

                TSharedPtr<FJsonObject> JsonObject;
                FString ErrorMessage;
                if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
                {
                    RunOnGameThread([this, ErrorMessage, Response, Snapshot]()
                    {
                        MarkSaveFailed(ErrorMessage, Response, Snapshot.CharacterId);
                    });
                    return;
                }

                RunOnGameThread([this, Snapshot]()
                {
                    CharacterStatus = TEXT("Saved");
                    SaveStatus = TEXT("Saved");
                    LastSaveErrorMessage.Reset();
                    bSaveConnectionHealthy = true;
                    LastCharacterId = Snapshot.CharacterId;
                    SetReplicatedPersistenceStatusForCharacter(Snapshot.CharacterId, SaveStatus, FString());

                    if (LastSessionResult.CharacterId == Snapshot.CharacterId)
                    {
                        LastSessionResult.CharacterLevel = Snapshot.Level;
                        LastSessionResult.CharacterExperience = Snapshot.Experience;
                        LastSessionResult.CharacterPosition = Snapshot.Position;
                        LastSessionResult.CurrentHealth = Snapshot.Health;
                        LastSessionResult.MaxHealth = Snapshot.MaxHealth;
                        LastSessionResult.CurrentMana = Snapshot.Mana;
                        LastSessionResult.MaxMana = Snapshot.MaxMana;
                        LastSessionResult.KillCount = Snapshot.KillCount;
                        LastSessionResult.DeathCount = Snapshot.DeathCount;
                        LastSessionResult.Gold = Snapshot.Gold;
                        LastSessionResult.InventoryItems = Snapshot.InventoryItems;
                    }

                    NotifyDebugStateChanged();
                });
            }
        );

        if (!Request->ProcessRequest())
        {
            if (Attempt < MaxTransientBackendRetries)
            {
                ScheduleTransientBackendRetry(this, TEXT("save start"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                {
                    (*SendAttempt)(NextAttempt);
                });
                return;
            }

            MarkSaveFailed(TEXT("Karakter kaydi istegi baslatilamadi."), nullptr, Snapshot.CharacterId);
        }
    };

    (*SendAttempt)(0);
}

void UMOBAMMOBackendSubsystem::HeartbeatCurrentCharacterSession(APlayerController* PlayerController)
{
    FCharacterSaveSnapshot Snapshot;
    FString SnapshotError;
    if (!TryBuildCharacterSaveSnapshot(PlayerController, Snapshot, SnapshotError, false))
    {
        MarkSaveFailed(SnapshotError);
        return;
    }

    TSharedPtr<TFunction<void(int32)>> SendAttempt = MakeShared<TFunction<void(int32)>>();
    *SendAttempt = [this, Snapshot, SendAttempt](int32 Attempt)
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(BuildUrl(TEXT("/session/heartbeat")));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        ApplyAuthHeader(Request);
        Request->SetContentAsString(BuildSessionHeartbeatPayload(Snapshot));

        Request->OnProcessRequestComplete().BindLambda(
            [this, Snapshot, SendAttempt, Attempt](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
            {
                if (IsTransientBackendFailure(bWasSuccessful, Response) && Attempt < MaxTransientBackendRetries)
                {
                    RunOnGameThread([this, SendAttempt, Attempt]()
                    {
                        ScheduleTransientBackendRetry(this, TEXT("heartbeat"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                        {
                            (*SendAttempt)(NextAttempt);
                        });
                    });
                    return;
                }

                if (!bWasSuccessful || !Response.IsValid())
                {
                    RunOnGameThread([this, Snapshot]()
                    {
                        MarkSaveFailed(TEXT("Session heartbeat istegi basarisiz oldu."), nullptr, Snapshot.CharacterId);
                    });
                    return;
                }

                TSharedPtr<FJsonObject> JsonObject;
                FString ErrorMessage;
                if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
                {
                    RunOnGameThread([this, ErrorMessage, Response, Snapshot]()
                    {
                        MarkSaveFailed(ErrorMessage, Response, Snapshot.CharacterId);
                    });
                    return;
                }

                RunOnGameThread([this, Snapshot]()
                {
                    LastSaveErrorMessage.Reset();
                    bSaveConnectionHealthy = true;
                    LastCharacterId = Snapshot.CharacterId;

                    if (SessionStatus == TEXT("Ready") || SessionStatus == TEXT("Traveling") || SessionStatus == TEXT("ReconnectRequired"))
                    {
                        SessionStatus = TEXT("Active");
                    }

                    if (SaveStatus == TEXT("Idle") || SaveStatus == TEXT("Failed") || SaveStatus == TEXT("SessionInvalid"))
                    {
                        SaveStatus = TEXT("Ready");
                    }

                    SetReplicatedPersistenceStatusForCharacter(Snapshot.CharacterId, SaveStatus, FString());
                    NotifyDebugStateChanged();
                });
            }
        );

        if (!Request->ProcessRequest())
        {
            if (Attempt < MaxTransientBackendRetries)
            {
                ScheduleTransientBackendRetry(this, TEXT("heartbeat start"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                {
                    (*SendAttempt)(NextAttempt);
                });
                return;
            }

            MarkSaveFailed(TEXT("Session heartbeat istegi baslatilamadi."), nullptr, Snapshot.CharacterId);
        }
    };

    (*SendAttempt)(0);
}

void UMOBAMMOBackendSubsystem::EndCurrentCharacterSession(APlayerController* PlayerController)
{
    FCharacterSaveSnapshot Snapshot;
    FString SnapshotError;
    if (!TryBuildCharacterSaveSnapshot(PlayerController, Snapshot, SnapshotError, false))
    {
        SessionStatus = TEXT("EndSkipped");
        UE_LOG(LogTemp, Warning, TEXT("[Backend] EndSession skipped: %s"), *SnapshotError);
        NotifyDebugStateChanged();
        return;
    }

    LastErrorMessage.Reset();
    CharacterStatus = TEXT("Saving");
    SaveStatus = TEXT("Saving");
    SessionStatus = TEXT("Ending");
    NotifyDebugStateChanged();

    UE_LOG(LogTemp, Log, TEXT("[Backend] EndSession starting. CharacterId=%s SessionId=%s HasPosition=%s"),
        *Snapshot.CharacterId,
        *Snapshot.SessionId,
        Snapshot.bHasPosition ? TEXT("true") : TEXT("false"));

    TSharedPtr<TFunction<void(int32)>> SendAttempt = MakeShared<TFunction<void(int32)>>();
    *SendAttempt = [this, Snapshot, SendAttempt](int32 Attempt)
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(BuildUrl(TEXT("/session/end")));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        ApplyAuthHeader(Request);
        Request->SetContentAsString(BuildCharacterSavePayload(Snapshot, true));

        Request->OnProcessRequestComplete().BindLambda(
            [this, Snapshot, SendAttempt, Attempt](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bWasSuccessful)
            {
                if (IsTransientBackendFailure(bWasSuccessful, Response) && Attempt < MaxTransientBackendRetries)
                {
                    RunOnGameThread([this, SendAttempt, Attempt]()
                    {
                        ScheduleTransientBackendRetry(this, TEXT("end session"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                        {
                            (*SendAttempt)(NextAttempt);
                        });
                    });
                    return;
                }

                if (!bWasSuccessful || !Response.IsValid())
                {
                    RunOnGameThread([this, Snapshot]()
                    {
                        SessionStatus = TEXT("EndFailed");
                        MarkSaveFailed(TEXT("Session kapatma istegi basarisiz oldu."), nullptr, Snapshot.CharacterId);
                        UE_LOG(LogTemp, Warning, TEXT("[Backend] EndSession request failed."));
                    });
                    return;
                }

                TSharedPtr<FJsonObject> JsonObject;
                FString ErrorMessage;
                if (!TryReadJsonResponse(Response, JsonObject, ErrorMessage))
                {
                    RunOnGameThread([this, ErrorMessage, Response, Snapshot]()
                    {
                        SessionStatus = TEXT("EndFailed");
                        MarkSaveFailed(ErrorMessage, Response, Snapshot.CharacterId);
                        UE_LOG(LogTemp, Warning, TEXT("[Backend] EndSession response failed: %s"), *ErrorMessage);
                    });
                    return;
                }

                RunOnGameThread([this, Snapshot]()
                {
                    CharacterStatus = TEXT("Saved");
                    SaveStatus = TEXT("Saved");
                    LastSaveErrorMessage.Reset();
                    bSaveConnectionHealthy = true;
                    SessionStatus = TEXT("Ended");
                    LastCharacterId = Snapshot.CharacterId;
                    SetReplicatedPersistenceStatusForCharacter(Snapshot.CharacterId, SaveStatus, FString());
                    UE_LOG(LogTemp, Log, TEXT("[Backend] EndSession succeeded. CharacterId=%s SessionId=%s"),
                        *Snapshot.CharacterId,
                        *Snapshot.SessionId);

                    if (LastSessionResult.CharacterId == Snapshot.CharacterId)
                    {
                        LastSessionResult.CharacterLevel = Snapshot.Level;
                        LastSessionResult.CharacterExperience = Snapshot.Experience;
                        LastSessionResult.CharacterPosition = Snapshot.Position;
                        LastSessionResult.CurrentHealth = Snapshot.Health;
                        LastSessionResult.MaxHealth = Snapshot.MaxHealth;
                        LastSessionResult.CurrentMana = Snapshot.Mana;
                        LastSessionResult.MaxMana = Snapshot.MaxMana;
                        LastSessionResult.KillCount = Snapshot.KillCount;
                        LastSessionResult.DeathCount = Snapshot.DeathCount;
                        LastSessionResult.Gold = Snapshot.Gold;
                        LastSessionResult.InventoryItems = Snapshot.InventoryItems;
                    }

                    NotifyDebugStateChanged();
                });
            }
        );

        if (!Request->ProcessRequest())
        {
            if (Attempt < MaxTransientBackendRetries)
            {
                ScheduleTransientBackendRetry(this, TEXT("end session start"), Attempt + 1, [SendAttempt](int32 NextAttempt)
                {
                    (*SendAttempt)(NextAttempt);
                });
                return;
            }

            SessionStatus = TEXT("EndFailed");
            MarkSaveFailed(TEXT("Session kapatma istegi baslatilamadi."), nullptr, Snapshot.CharacterId);
            UE_LOG(LogTemp, Warning, TEXT("[Backend] EndSession request could not be started."));
        }
    };

    (*SendAttempt)(0);
}

void UMOBAMMOBackendSubsystem::ReturnToCharacterSelectAfterDisconnect(APlayerController* PlayerController)
{
    FString CurrentCharacterId;
    if (const AMOBAMMOPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AMOBAMMOPlayerState>() : nullptr)
    {
        CurrentCharacterId = PlayerState->GetCharacterId().TrimStartAndEnd();
    }

    if (!CurrentCharacterId.IsEmpty())
    {
        const bool bCharacterStillCached = CachedCharacters.ContainsByPredicate([&CurrentCharacterId](const FBackendCharacterResult& Character)
        {
            return Character.CharacterId == CurrentCharacterId;
        });

        if (bCharacterStillCached)
        {
            SelectedCharacterId = CurrentCharacterId;
            LastCharacterId = CurrentCharacterId;
        }
    }

    LastErrorMessage.Reset();
    LastSaveErrorMessage.Reset();
    LastSessionConnectString.Reset();
    LastSessionResult = FBackendSessionResult();
    bReconnectTravelPending = false;
    PendingReconnectPlayerController.Reset();
    bCharacterFlowActionAuthorized = false;
    bManualCharacterFlowPending = !LastAccountId.TrimStartAndEnd().IsEmpty();
    bMainMenuVisible = false;
    bCharacterSelectRequested = bManualCharacterFlowPending;
    SessionStatus = TEXT("Ended");
    SaveStatus = TEXT("Ready");
    CharacterStatus = TEXT("Idle");

    UE_LOG(LogTemp, Log, TEXT("[Backend] Returned to character select after disconnect. AccountId=%s SelectedCharacterId=%s"),
        *LastAccountId,
        *SelectedCharacterId);

    NotifyDebugStateChanged();
}
