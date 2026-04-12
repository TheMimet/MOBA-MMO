#include "MOBAMMODebugLoginWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOBackendSubsystem.h"

namespace
{
    UTextBlock* AddLabel(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text, const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetAutoWrapText(true);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label))
        {
            Slot->SetPadding(FMargin(0.0f, 2.0f));
        }

        return Label;
    }

    UButton* AddButton(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Button->AddChild(Label);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
        {
            Slot->SetPadding(FMargin(0.0f, 4.0f));
        }

        return Button;
    }
}

void UMOBAMMODebugLoginWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    BuildDebugLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMODebugLoginWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshDisplay();
}

FString UMOBAMMODebugLoginWidget::GetBackendUrl() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetBackendBaseUrl();
    }

    return FString();
}

FString UMOBAMMODebugLoginWidget::GetLoginStatus() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLoginStatus();
    }

    return TEXT("Unavailable");
}

FString UMOBAMMODebugLoginWidget::GetCharacterStatus() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetCharacterStatus();
    }

    return TEXT("Unavailable");
}

FString UMOBAMMODebugLoginWidget::GetSessionStatus() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetSessionStatus();
    }

    return TEXT("Unavailable");
}

FString UMOBAMMODebugLoginWidget::GetLastErrorMessage() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLastErrorMessage();
    }

    return FString();
}

FString UMOBAMMODebugLoginWidget::GetLastUsername() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLastUsername();
    }

    return FString();
}

FString UMOBAMMODebugLoginWidget::GetLastAccountId() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLastAccountId();
    }

    return FString();
}

FString UMOBAMMODebugLoginWidget::GetLastCharacterId() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLastCharacterId();
    }

    return FString();
}

FString UMOBAMMODebugLoginWidget::GetLastConnectString() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->GetLastSessionConnectString();
    }

    return FString();
}

void UMOBAMMODebugLoginWidget::TriggerMockLogin()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
        BackendSubsystem->MockLogin(DefaultUsername);
    }
}

void UMOBAMMODebugLoginWidget::TriggerCreateCharacter()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
        BackendSubsystem->CreateCharacter(
            BackendSubsystem->GetLastAccountId(),
            DefaultCharacterName,
            DefaultClassId
        );
    }
}

void UMOBAMMODebugLoginWidget::TriggerStartSession()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
        BackendSubsystem->StartSession(BackendSubsystem->GetLastCharacterId());
    }
}

bool UMOBAMMODebugLoginWidget::TriggerJoinServer()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
        if (APlayerController* PlayerController = GetOwningPlayer())
        {
            return BackendSubsystem->TravelToSession(PlayerController, BackendSubsystem->GetLastSessionConnectString());
        }
    }

    return false;
}

UMOBAMMOBackendSubsystem* UMOBAMMODebugLoginWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMODebugLoginWidget::BuildDebugLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f));
    RootBorder->SetPadding(FMargin(12.0f));
    WidgetTree->RootWidget = RootBorder;

    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Panel);

    AddLabel(WidgetTree, Panel, TEXT("Debug Login Panel"), FLinearColor(0.6f, 1.0f, 0.6f, 1.0f));
    BackendUrlText = AddLabel(WidgetTree, Panel, TEXT("Backend URL: -"));
    LoginStatusText = AddLabel(WidgetTree, Panel, TEXT("Login Status: -"));
    CharacterStatusText = AddLabel(WidgetTree, Panel, TEXT("Character Status: -"));
    SessionStatusText = AddLabel(WidgetTree, Panel, TEXT("Session Status: -"));
    ErrorText = AddLabel(WidgetTree, Panel, TEXT("Last Error: -"), FLinearColor(1.0f, 0.6f, 0.6f, 1.0f));
    UsernameText = AddLabel(WidgetTree, Panel, TEXT("Last Username: -"));
    AccountIdText = AddLabel(WidgetTree, Panel, TEXT("Account Id: -"));
    CharacterIdText = AddLabel(WidgetTree, Panel, TEXT("Character Id: -"));
    ConnectStringText = AddLabel(WidgetTree, Panel, TEXT("Connect String: -"));

    if (UButton* Button = AddButton(WidgetTree, Panel, TEXT("Mock Login")))
    {
        Button->OnClicked.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleMockLoginClicked);
    }

    if (UButton* Button = AddButton(WidgetTree, Panel, TEXT("Create Character")))
    {
        Button->OnClicked.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleCreateCharacterClicked);
    }

    if (UButton* Button = AddButton(WidgetTree, Panel, TEXT("Start Session")))
    {
        Button->OnClicked.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleStartSessionClicked);
    }

    if (UButton* Button = AddButton(WidgetTree, Panel, TEXT("Join Server")))
    {
        Button->OnClicked.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleJoinServerClicked);
    }
}

void UMOBAMMODebugLoginWidget::BroadcastStateChanged()
{
    RefreshDisplay();
    OnDebugStateChanged.Broadcast();
}

void UMOBAMMODebugLoginWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bIsBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleDebugStateChanged);
    BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleLoginSucceeded);
    BackendSubsystem->OnCharacterCreated.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleCharacterCreated);
    BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleSessionStarted);
    BackendSubsystem->OnLoginFailed.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleRequestFailed);
    BackendSubsystem->OnCharacterCreateFailed.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleRequestFailed);
    BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMODebugLoginWidget::HandleRequestFailed);
    bIsBoundToSubsystem = true;
}

void UMOBAMMODebugLoginWidget::RefreshDisplay()
{
    if (BackendUrlText)
    {
        BackendUrlText->SetText(FText::FromString(FString::Printf(TEXT("Backend URL: %s"), *GetBackendUrl())));
    }

    if (LoginStatusText)
    {
        LoginStatusText->SetText(FText::FromString(FString::Printf(TEXT("Login Status: %s"), *GetLoginStatus())));
    }

    if (CharacterStatusText)
    {
        CharacterStatusText->SetText(FText::FromString(FString::Printf(TEXT("Character Status: %s"), *GetCharacterStatus())));
    }

    if (SessionStatusText)
    {
        SessionStatusText->SetText(FText::FromString(FString::Printf(TEXT("Session Status: %s"), *GetSessionStatus())));
    }

    if (ErrorText)
    {
        const FString Value = GetLastErrorMessage().IsEmpty() ? TEXT("-") : GetLastErrorMessage();
        ErrorText->SetText(FText::FromString(FString::Printf(TEXT("Last Error: %s"), *Value)));
    }

    if (UsernameText)
    {
        const FString Value = GetLastUsername().IsEmpty() ? TEXT("-") : GetLastUsername();
        UsernameText->SetText(FText::FromString(FString::Printf(TEXT("Last Username: %s"), *Value)));
    }

    if (AccountIdText)
    {
        const FString Value = GetLastAccountId().IsEmpty() ? TEXT("-") : GetLastAccountId();
        AccountIdText->SetText(FText::FromString(FString::Printf(TEXT("Account Id: %s"), *Value)));
    }

    if (CharacterIdText)
    {
        const FString Value = GetLastCharacterId().IsEmpty() ? TEXT("-") : GetLastCharacterId();
        CharacterIdText->SetText(FText::FromString(FString::Printf(TEXT("Character Id: %s"), *Value)));
    }

    if (ConnectStringText)
    {
        const FString Value = GetLastConnectString().IsEmpty() ? TEXT("-") : GetLastConnectString();
        ConnectStringText->SetText(FText::FromString(FString::Printf(TEXT("Connect String: %s"), *Value)));
    }
}

void UMOBAMMODebugLoginWidget::HandleMockLoginClicked()
{
    TriggerMockLogin();
}

void UMOBAMMODebugLoginWidget::HandleCreateCharacterClicked()
{
    TriggerCreateCharacter();
}

void UMOBAMMODebugLoginWidget::HandleStartSessionClicked()
{
    TriggerStartSession();
}

void UMOBAMMODebugLoginWidget::HandleJoinServerClicked()
{
    TriggerJoinServer();
}

void UMOBAMMODebugLoginWidget::HandleDebugStateChanged()
{
    BroadcastStateChanged();
}

void UMOBAMMODebugLoginWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
    BroadcastStateChanged();
}

void UMOBAMMODebugLoginWidget::HandleCharacterCreated(const FBackendCharacterResult& Result)
{
    BroadcastStateChanged();
}

void UMOBAMMODebugLoginWidget::HandleSessionStarted(const FBackendSessionResult& Result)
{
    BroadcastStateChanged();
}

void UMOBAMMODebugLoginWidget::HandleRequestFailed(const FString& ErrorMessage)
{
    BroadcastStateChanged();
}
