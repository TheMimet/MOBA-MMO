#include "MOBAMMOCharacterFlowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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
    CharacterButtons.Reset();
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

            UButton* Button = MakeButton(WidgetTree, CharacterListBox, Label);
            Button->OnClicked.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharacterSlotClicked);
            CharacterButtons.Add(Button);
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
        const FString Subtitle = BackendSubsystem->IsWaitingForCharacterSelection()
            ? TEXT("Select an existing character or create a new one before entering the session.")
            : TEXT("Preparing your multiplayer session.");
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

void UMOBAMMOCharacterFlowWidget::HandleCharacterSlotClicked()
{
    if (!GetBackendSubsystem())
    {
        return;
    }

    for (int32 Index = 0; Index < CharacterButtons.Num(); ++Index)
    {
        if (CharacterButtons[Index] && CharacterButtons[Index]->HasUserFocus(GetOwningPlayer()) && RenderedCharacters.IsValidIndex(Index))
        {
            GetBackendSubsystem()->SelectCharacter(RenderedCharacters[Index].CharacterId);
            RefreshFromBackend();
            return;
        }
    }

    for (int32 Index = 0; Index < CharacterButtons.Num(); ++Index)
    {
        if (CharacterButtons[Index] && CharacterButtons[Index]->IsPressed() && RenderedCharacters.IsValidIndex(Index))
        {
            GetBackendSubsystem()->SelectCharacter(RenderedCharacters[Index].CharacterId);
            RefreshFromBackend();
            return;
        }
    }

    if (RenderedCharacters.Num() > 0)
    {
        GetBackendSubsystem()->SelectCharacter(RenderedCharacters[0].CharacterId);
        RefreshFromBackend();
    }
}
