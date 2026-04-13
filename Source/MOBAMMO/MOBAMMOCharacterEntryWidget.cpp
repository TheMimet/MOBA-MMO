#include "MOBAMMOCharacterEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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
    RootBorder->SetPadding(FMargin(8.0f));
    WidgetTree->RootWidget = RootBorder;

    USizeBox* TileSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    TileSizeBox->SetWidthOverride(118.0f);
    TileSizeBox->SetHeightOverride(118.0f);
    RootBorder->SetContent(TileSizeBox);

    SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    SelectButton->SetBackgroundColor(FLinearColor(0.07f, 0.08f, 0.10f, 1.0f));
    TileSizeBox->SetContent(SelectButton);

    UBorder* InnerFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    InnerFrame->SetPadding(FMargin(6.0f));
    InnerFrame->SetBrushColor(FLinearColor(0.09f, 0.09f, 0.10f, 1.0f));
    SelectButton->AddChild(InnerFrame);

    UVerticalBox* TileLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    InnerFrame->SetContent(TileLayout);

    UBorder* PortraitPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    PortraitPlate->SetBrushColor(FLinearColor(0.19f, 0.18f, 0.17f, 1.0f));
    PortraitPlate->SetPadding(FMargin(0.0f));
    if (UVerticalBoxSlot* PortraitSlot = TileLayout->AddChildToVerticalBox(PortraitPlate))
    {
        PortraitSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
        PortraitSlot->SetSize(ESlateSizeRule::Fill);
    }

    UImage* PortraitGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    PortraitGlow->SetColorAndOpacity(FLinearColor(0.34f, 0.27f, 0.18f, 0.85f));
    PortraitPlate->SetContent(PortraitGlow);

    LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    LabelText->SetAutoWrapText(true);
    LabelText->SetJustification(ETextJustify::Center);
    FSlateFontInfo FontInfo = LabelText->GetFont();
    FontInfo.Size = 12;
    LabelText->SetFont(FontInfo);
    if (UVerticalBoxSlot* LabelSlot = TileLayout->AddChildToVerticalBox(LabelText))
    {
        LabelSlot->SetPadding(FMargin(2.0f, 0.0f));
    }

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
                ? FLinearColor(0.87f, 0.71f, 0.33f, 0.98f)
                : FLinearColor(0.22f, 0.22f, 0.22f, 0.92f)
        );
    }

    if (SelectButton)
    {
        SelectButton->SetBackgroundColor(
            bIsSelected
                ? FLinearColor(0.24f, 0.18f, 0.09f, 1.0f)
                : FLinearColor(0.06f, 0.07f, 0.09f, 1.0f)
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
