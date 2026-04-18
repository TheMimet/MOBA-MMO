#include "MOBAMMOCharacterSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOCharacterEntryWidget.h"

void UMOBAMMOCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();

	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BindToSubsystem(BackendSubsystem);
	}
}

void UMOBAMMOCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::RefreshFromBackend()
{
	UpdateCharacterList();
	SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOCharacterSelectWidget::ShouldBeVisible() const
{
	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	if (NetMode == NM_Client)
	{
		return false;
	}

	if (BackendSubsystem->IsWaitingForCharacterSelection())
	{
		return true;
	}

	return false;
}

UMOBAMMOBackendSubsystem* UMOBAMMOCharacterSelectWidget::GetBackendSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
	}

	return nullptr;
}

void UMOBAMMOCharacterSelectWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelSizeBox->SetWidthOverride(500.0f);
	if (UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(PanelSizeBox))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Right);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
		PanelSlot->SetPadding(FMargin(0.0f, 0.0f, 50.0f, 0.0f));
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.85f));
	RootBorder->SetPadding(FMargin(24.0f));
	PanelSizeBox->SetContent(RootBorder);

	UVerticalBox* VerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RootBorder->SetContent(VerticalBox);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(FText::FromString(TEXT("SELECT CHARACTER")));
	FSlateFontInfo FontInfo = TitleText->GetFont();
	FontInfo.Size = 24;
	TitleText->SetFont(FontInfo);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.9f, 1.0f)));
	if (UVerticalBoxSlot* TitleSlot = VerticalBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	CharacterListContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* ListSlot = VerticalBox->AddChildToVerticalBox(CharacterListContainer))
	{
		ListSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	// Create Character Section
	UHorizontalBox* CreateCharacterBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* CreateBoxSlot = VerticalBox->AddChildToVerticalBox(CreateCharacterBox))
	{
		CreateBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	NewCharacterNameInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	NewCharacterNameInput->SetHintText(FText::FromString(TEXT("New Character Name")));
	if (UHorizontalBoxSlot* InputBoxSlot = CreateCharacterBox->AddChildToHorizontalBox(NewCharacterNameInput))
	{
		InputBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InputBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	}

	CreateCharacterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CreateCharacterButton->SetBackgroundColor(FLinearColor(0.2f, 0.8f, 0.4f, 1.0f));
	UTextBlock* CreateBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CreateBtnText->SetText(FText::FromString(TEXT("CREATE")));
	CreateBtnText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	CreateCharacterButton->AddChild(CreateBtnText);
	CreateCharacterButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCreateButtonClicked);
	CreateCharacterBox->AddChildToHorizontalBox(CreateCharacterButton);

	// Play Button
	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	PlayButton->SetBackgroundColor(FLinearColor(0.9f, 0.6f, 0.1f, 1.0f));
	UTextBlock* PlayText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PlayText->SetText(FText::FromString(TEXT("ENTER GAME")));
	PlayText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	PlayButton->AddChild(PlayText);
	PlayButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandlePlayButtonClicked);

	if (UVerticalBoxSlot* PlaySlot = VerticalBox->AddChildToVerticalBox(PlayButton))
	{
		PlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UMOBAMMOCharacterSelectWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
	if (!BackendSubsystem || bBoundToSubsystem)
	{
		return;
	}

	BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleBackendStateChanged);
	BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleLoginSucceeded);
	BackendSubsystem->OnCharactersLoaded.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCharactersLoaded);
	BackendSubsystem->OnCharacterCreated.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCharacterCreated);
	BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleSessionStarted);
	BackendSubsystem->OnCharactersLoadFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);
	BackendSubsystem->OnCharacterCreateFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);
	BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);
	
	bBoundToSubsystem = true;
}

void UMOBAMMOCharacterSelectWidget::UpdateCharacterList()
{
	if (!CharacterListContainer) return;
	CharacterListContainer->ClearChildren();

	UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem) return;

	TArray<FBackendCharacterResult> Characters = BackendSubsystem->GetCachedCharacters();
	FString SelectedCharacterId = BackendSubsystem->GetSelectedCharacterId();

	for (const FBackendCharacterResult& Char : Characters)
	{
		bool bIsSelected = Char.CharacterId == SelectedCharacterId;
		UMOBAMMOCharacterEntryWidget* CharacterEntry = CreateWidget<UMOBAMMOCharacterEntryWidget>(GetOwningPlayer(), UMOBAMMOCharacterEntryWidget::StaticClass());
		if (!CharacterEntry)
		{
			continue;
		}

		CharacterEntry->ConfigureEntry(
			Char.CharacterId,
			FString::Printf(TEXT("%s\n%s | Lv. %d"), *Char.Name, *Char.ClassId, Char.Level),
			bIsSelected
		);
		CharacterEntry->OnEntrySelected.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCharacterButtonClicked);

		if (SelectedCharacterId.IsEmpty())
		{
			BackendSubsystem->SelectCharacter(Char.CharacterId);
		}

		if (UVerticalBoxSlot* CharSlot = CharacterListContainer->AddChildToVerticalBox(CharacterEntry))
		{
			CharSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}
}

void UMOBAMMOCharacterSelectWidget::HandleBackendStateChanged()
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->ListCharacters(Result.AccountId);
	}
}

void UMOBAMMOCharacterSelectWidget::HandleCharactersLoaded(const FBackendCharacterListResult& Result)
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleCharacterCreated(const FBackendCharacterResult& Result)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->SelectCharacter(Result.CharacterId);
	}

	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleSessionStarted(const FBackendSessionResult& Result)
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

void UMOBAMMOCharacterSelectWidget::HandleRequestFailed(const FString& ErrorMessage)
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleCharacterButtonClicked(const FString& CharacterId)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->SelectCharacter(CharacterId);
		RefreshFromBackend();
	}
}

void UMOBAMMOCharacterSelectWidget::HandleCreateButtonClicked()
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		if (NewCharacterNameInput && !NewCharacterNameInput->GetText().IsEmpty())
		{
			BackendSubsystem->CreateCharacterForCurrentAccount(NewCharacterNameInput->GetText().ToString(), TEXT("fighter"));
		}
	}
}

void UMOBAMMOCharacterSelectWidget::HandlePlayButtonClicked()
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->StartSessionForSelectedCharacter();
	}
}
