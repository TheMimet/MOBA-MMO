#include "MOBAMMOCharacterEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMOBAMMOCharacterEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
    RefreshVisuals();
}

void UMOBAMMOCharacterEntryWidget::ConfigureEntry(const FString& InCharacterId, const FString& InLabel, bool bInSelected)
{
    CharacterId = InCharacterId;
    Label = InLabel;
    bIsSelected = bInSelected;
    RefreshVisuals();
}

void UMOBAMMOCharacterEntryWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetPadding(FMargin(10.0f));
    WidgetTree->RootWidget = RootBorder;

    SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    RootBorder->SetContent(SelectButton);

    LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    LabelText->SetAutoWrapText(true);
    SelectButton->AddChild(LabelText);
    SelectButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterEntryWidget::HandleButtonClicked);
}

void UMOBAMMOCharacterEntryWidget::RefreshVisuals()
{
    if (LabelText)
    {
        LabelText->SetText(FText::FromString(Label));
        LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    }

    if (RootBorder)
    {
        RootBorder->SetBrushColor(
            bIsSelected
                ? FLinearColor(0.10f, 0.28f, 0.18f, 0.95f)
                : FLinearColor(0.06f, 0.08f, 0.12f, 0.90f)
        );
    }
}

void UMOBAMMOCharacterEntryWidget::HandleButtonClicked()
{
    if (!CharacterId.IsEmpty())
    {
        OnEntrySelected.Broadcast(CharacterId);
    }
}
