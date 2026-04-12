#include "MOBAMMOCharacterFlowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOCharacterEntryWidget.h"

namespace
{
    UTextBlock* MakeText(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text, int32 FontSize = 20, const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetAutoWrapText(true);

        FSlateFontInfo FontInfo = Label->GetFont();
        FontInfo.Size = FontSize;
        Label->SetFont(FontInfo);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label))
        {
            Slot->SetPadding(FMargin(0.0f, 4.0f));
        }

        return Label;
    }

    UButton* MakeButton(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Button->AddChild(Label);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
        {
            Slot->SetPadding(FMargin(0.0f, 6.0f));
        }

        return Button;
    }

    FString BuildSessionPhaseText(UMOBAMMOBackendSubsystem* BackendSubsystem)
    {
        if (!BackendSubsystem)
        {
            return TEXT("Preparing flow.");
        }

        if (BackendSubsystem->GetSessionStatus() == TEXT("Starting"))
        {
            return TEXT("Starting session and reserving a server slot...");
        }

        if (BackendSubsystem->GetSessionStatus() == TEXT("Traveling"))
        {
            return TEXT("Joining dedicated server...");
        }

        if (BackendSubsystem->IsWaitingForCharacterSelection())
        {
            return TEXT("Select an existing character or create a default one before entering the session.");
        }

        return TEXT("Preparing your multiplayer session.");
    }
}

void UMOBAMMOCharacterFlowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMOCharacterFlowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::RefreshFromBackend()
{
    UpdateHeaderText();
    RebuildCharacterButtons();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOCharacterFlowWidget::ShouldBeVisible() const
{
    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        return BackendSubsystem->IsWaitingForCharacterSelection()
            || BackendSubsystem->GetSessionStatus() == TEXT("Starting")
            || BackendSubsystem->GetSessionStatus() == TEXT("Traveling");
    }

    return false;
}

UMOBAMMOBackendSubsystem* UMOBAMMOCharacterFlowWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMOCharacterFlowWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.92f));
    RootBorder->SetPadding(FMargin(28.0f));
    WidgetTree->RootWidget = RootBorder;

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Layout);

    TitleText = MakeText(WidgetTree, Layout, TEXT("Character Select"), 30, FLinearColor(0.85f, 0.95f, 1.0f, 1.0f));
    SubtitleText = MakeText(WidgetTree, Layout, TEXT("Choose a character or create a new one."), 16, FLinearColor(0.75f, 0.82f, 0.9f, 1.0f));
    StatusText = MakeText(WidgetTree, Layout, TEXT("Status: Waiting"), 16, FLinearColor(0.9f, 0.9f, 0.7f, 1.0f));

    CharacterListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    if (UVerticalBoxSlot* ListSlot = Layout->AddChildToVerticalBox(CharacterListBox))
    {
        ListSlot->SetPadding(FMargin(0.0f, 12.0f));
    }

    RefreshCharactersButton = MakeButton(WidgetTree, Layout, TEXT("Refresh Characters"));
    RefreshCharactersButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRefreshCharactersClicked);

    CreateCharacterButton = MakeButton(WidgetTree, Layout, TEXT("Create Default Character"));
    CreateCharacterButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCreateCharacterClicked);

    StartSessionButton = MakeButton(WidgetTree, Layout, TEXT("Continue With Selected Character"));
    StartSessionButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleStartSessionClicked);
}

void UMOBAMMOCharacterFlowWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleBackendStateChanged);
    BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleLoginSucceeded);
    BackendSubsystem->OnCharactersLoaded.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharactersLoaded);
    BackendSubsystem->OnCharacterCreated.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharacterCreated);
    BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleSessionStarted);
    BackendSubsystem->OnLoginFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
    BackendSubsystem->OnCharactersLoadFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
    BackendSubsystem->OnCharacterCreateFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
    BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
    bBoundToSubsystem = true;
}

void UMOBAMMOCharacterFlowWidget::RebuildCharacterButtons()
{
    if (!WidgetTree || !CharacterListBox)
    {
        return;
    }

    CharacterListBox->ClearChildren();
    CharacterEntryWidgets.Reset();
    RenderedCharacters = GetBackendSubsystem() ? GetBackendSubsystem()->GetCachedCharacters() : TArray<FBackendCharacterResult>{};

    if (RenderedCharacters.Num() == 0)
    {
        MakeText(WidgetTree, CharacterListBox, TEXT("No characters found yet."), 16, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f));
    }
    else
    {
        const FString SelectedCharacterId = GetBackendSubsystem() ? GetBackendSubsystem()->GetSelectedCharacterId() : FString();
        for (int32 Index = 0; Index < RenderedCharacters.Num(); ++Index)
        {
            const FBackendCharacterResult& Character = RenderedCharacters[Index];
            const bool bSelected = Character.CharacterId == SelectedCharacterId;
            const FString Label = FString::Printf(TEXT("%s  |  %s  |  Level %d%s"),
                *Character.Name,
                *Character.ClassId,
                Character.Level,
                bSelected ? TEXT("  [Selected]") : TEXT(""));

            UMOBAMMOCharacterEntryWidget* EntryWidget = WidgetTree->ConstructWidget<UMOBAMMOCharacterEntryWidget>(UMOBAMMOCharacterEntryWidget::StaticClass());
            EntryWidget->ConfigureEntry(Character.CharacterId, Label, bSelected);
            EntryWidget->OnEntrySelected.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharacterEntrySelected);
            if (UVerticalBoxSlot* EntrySlot = CharacterListBox->AddChildToVerticalBox(EntryWidget))
            {
                EntrySlot->SetPadding(FMargin(0.0f, 6.0f));
            }
            CharacterEntryWidgets.Add(EntryWidget);
        }
    }

    if (StartSessionButton)
    {
        StartSessionButton->SetIsEnabled(GetBackendSubsystem() && !GetBackendSubsystem()->GetSelectedCharacterId().IsEmpty());
    }
}

void UMOBAMMOCharacterFlowWidget::UpdateHeaderText()
{
    UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    if (!BackendSubsystem)
    {
        return;
    }

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("Character Select")));
    }

    if (SubtitleText)
    {
        const FString Subtitle = BuildSessionPhaseText(BackendSubsystem);
        SubtitleText->SetText(FText::FromString(Subtitle));
    }

    if (StatusText)
    {
        const FString StatusLine = FString::Printf(TEXT("Login: %s | Characters: %s | Session: %s"),
            *BackendSubsystem->GetLoginStatus(),
            *BackendSubsystem->GetCharacterListStatus(),
            *BackendSubsystem->GetSessionStatus());
        StatusText->SetText(FText::FromString(StatusLine));
    }
}

void UMOBAMMOCharacterFlowWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleCharactersLoaded(const FBackendCharacterListResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleCharacterCreated(const FBackendCharacterResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleSessionStarted(const FBackendSessionResult& Result)
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        if (APlayerController* PlayerController = GetOwningPlayer())
        {
            BackendSubsystem->TravelToSession(PlayerController, Result.ConnectString);
        }
    }

    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleRequestFailed(const FString& ErrorMessage)
{
    RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleCreateCharacterClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->CreateCharacterForCurrentAccount(DefaultCharacterName, DefaultClassId);
    }
}

void UMOBAMMOCharacterFlowWidget::HandleStartSessionClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->StartSessionForSelectedCharacter();
    }
}

void UMOBAMMOCharacterFlowWidget::HandleRefreshCharactersClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->ListCharacters(BackendSubsystem->GetLastAccountId());
    }
}

void UMOBAMMOCharacterFlowWidget::HandleCharacterEntrySelected(const FString& CharacterId)
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->SelectCharacter(CharacterId);
        RefreshFromBackend();
    }
}
