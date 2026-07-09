#include "PieceSkinSwatch.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UPieceSkinSwatch::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(96.f);
		SizeBox->SetHeightOverride(116.f);
		WidgetTree->RootWidget = SizeBox;

		// Frame that highlights around the equipped swatch; RefreshAppearance colors it.
		FrameBorder = WidgetTree->ConstructWidget<UBorder>();
		FrameBorder->SetPadding(FMargin(3.f));
		SizeBox->AddChild(FrameBorder);

		Btn = WidgetTree->ConstructWidget<UButton>();
		FrameBorder->AddChild(Btn);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Btn->AddChild(Box);

		ImageWidget = WidgetTree->ConstructWidget<UImage>();
		ImageWidget->SetDesiredSizeOverride(FVector2D(72.f, 72.f));
		if (UVerticalBoxSlot* IS = Box->AddChildToVerticalBox(ImageWidget))
		{
			IS->SetHorizontalAlignment(HAlign_Center);
			IS->SetPadding(FMargin(6.f, 8.f, 6.f, 2.f));
		}

		LabelWidget = WidgetTree->ConstructWidget<UTextBlock>();
		LabelWidget->SetJustification(ETextJustify::Center);
		LabelWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
		LabelWidget->SetAutoWrapText(true);
		if (UVerticalBoxSlot* LS = Box->AddChildToVerticalBox(LabelWidget))
		{
			LS->SetHorizontalAlignment(HAlign_Center);
			LS->SetPadding(FMargin(4.f, 0.f, 4.f, 6.f));
		}
	}

	RefreshAppearance();
	return Super::RebuildWidget();
}

void UPieceSkinSwatch::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn) Btn->OnClicked.AddUniqueDynamic(this, &UPieceSkinSwatch::HandleClicked);
	RefreshAppearance();
}

void UPieceSkinSwatch::RefreshAppearance()
{
	if (LabelWidget)
	{
		const FText Name = SkinId.IsNone() ? FText::FromString(TEXT("Default")) : DisplayName;
		if (!bUnlocked)
			LabelWidget->SetText(FText::Format(NSLOCTEXT("ChessKids", "LockedSkinLabel", "{0} (Locked)"), Name));
		else if (bEquipped)
			LabelWidget->SetText(FText::Format(NSLOCTEXT("ChessKids", "EquippedSkinLabel", "{0} (Equipped)"), Name));
		else
			LabelWidget->SetText(Name);
	}

	if (FrameBorder)
	{
		// Gold frame on the equipped swatch, a faint neutral outline otherwise.
		FrameBorder->SetBrushColor(bEquipped
			? FLinearColor(1.f, 0.82f, 0.2f, 1.f)
			: FLinearColor(1.f, 1.f, 1.f, 0.12f));
	}

	if (ImageWidget)
	{
		if (Thumbnail)
		{
			ImageWidget->SetBrushFromTexture(Thumbnail, false);
			ImageWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ImageWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		ImageWidget->SetColorAndOpacity(bUnlocked ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f));
	}

	if (Btn)
	{
		Btn->SetIsEnabled(bUnlocked);
		Btn->SetRenderOpacity(bUnlocked ? 1.f : 0.5f);
	}
}

void UPieceSkinSwatch::HandleClicked()
{
	OnSkinSwatchClicked.Broadcast(SkinId);
}
