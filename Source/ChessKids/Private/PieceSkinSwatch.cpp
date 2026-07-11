#include "PieceSkinSwatch.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UPieceSkinSwatch::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		// Fixed-height row that fills whatever width its ScrollBox slot gives it
		// (HAlign_Fill, set by the menu widget that adds this row) — unlike the old
		// vertical card, there's no WidthOverride here.
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetHeightOverride(56.f);
		WidgetTree->RootWidget = SizeBox;

		// Left accent bar — RefreshAppearance colors it to highlight the equipped
		// and/or previewed row. Only the left inset is nonzero so the strip is the one
		// bit of AccentBorder's background that isn't painted over by Btn's own opaque
		// button-style background (Btn fills everything inside the padding).
		AccentBorder = WidgetTree->ConstructWidget<UBorder>();
		AccentBorder->SetPadding(FMargin(7.f, 0.f, 0.f, 0.f));
		SizeBox->AddChild(AccentBorder);

		Btn = WidgetTree->ConstructWidget<UButton>();
		AccentBorder->AddChild(Btn);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Btn->AddChild(Row);

		// A fixed-size SizeBox forces the actual paint geometry the image/lock renders
		// into, same reasoning as the old card layout's image box.
		USizeBox* ImageBox = WidgetTree->ConstructWidget<USizeBox>();
		ImageBox->SetWidthOverride(48.f);
		ImageBox->SetHeightOverride(48.f);
		if (UHorizontalBoxSlot* IS = Row->AddChildToHorizontalBox(ImageBox))
		{
			IS->SetHorizontalAlignment(HAlign_Center);
			IS->SetVerticalAlignment(VAlign_Center);
			IS->SetPadding(FMargin(8.f, 4.f));
		}

		// Overlay hosts the texture preview, the flat-color fallback, and the locked
		// glyph in the same space — RefreshAppearance toggles which one is visible.
		// Overlay slots default to top-left/auto-size, so each child needs an explicit
		// Fill alignment or it shrinks to a tiny icon instead of filling the 48x48 area.
		UOverlay* ImageOverlay = WidgetTree->ConstructWidget<UOverlay>();
		ImageBox->AddChild(ImageOverlay);

		ImageWidget = WidgetTree->ConstructWidget<UImage>();
		if (UOverlaySlot* ImgSlot = ImageOverlay->AddChildToOverlay(ImageWidget))
		{
			ImgSlot->SetHorizontalAlignment(HAlign_Fill);
			ImgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		ColorSwatchWidget = WidgetTree->ConstructWidget<UBorder>();
		if (UOverlaySlot* ColorSlot = ImageOverlay->AddChildToOverlay(ColorSwatchWidget))
		{
			ColorSlot->SetHorizontalAlignment(HAlign_Fill);
			ColorSlot->SetVerticalAlignment(VAlign_Fill);
		}

		LockGlyphWidget = WidgetTree->ConstructWidget<UTextBlock>();
		// Plain ASCII text rather than a lock-character glyph — the project's default
		// font isn't guaranteed to have a padlock glyph (bit us before with a Unicode
		// checkmark rendering as a missing-glyph box), so this avoids that risk entirely.
		LockGlyphWidget->SetText(FText::FromString(TEXT("LOCK")));
		LockGlyphWidget->SetJustification(ETextJustify::Center);
		LockGlyphWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
		LockGlyphWidget->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.5f)));
		if (UOverlaySlot* LockSlot = ImageOverlay->AddChildToOverlay(LockGlyphWidget))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Fill);
			LockSlot->SetVerticalAlignment(VAlign_Fill);
		}

		LabelWidget = WidgetTree->ConstructWidget<UTextBlock>();
		LabelWidget->SetJustification(ETextJustify::Left);
		LabelWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 15));
		LabelWidget->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(LabelWidget))
		{
			LS->SetHorizontalAlignment(HAlign_Fill);
			LS->SetVerticalAlignment(VAlign_Center);
			LS->SetPadding(FMargin(4.f, 4.f, 12.f, 4.f));
			LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
		LabelWidget->SetText(Name);
	}

	if (AccentBorder)
	{
		// Gold background on the equipped row, a softer highlight on the previewed
		// (but not yet equipped) row, dark/transparent otherwise.
		FLinearColor BgColor(0.f, 0.f, 0.f, 0.f);
		if (bEquipped)
			BgColor = FLinearColor(0.55f, 0.42f, 0.05f, 0.9f);
		else if (bPreviewed)
			BgColor = FLinearColor(0.15f, 0.25f, 0.4f, 0.8f);
		AccentBorder->SetBrushColor(BgColor);
	}

	const bool bShowLocked = !bUnlocked;

	if (ImageWidget)
	{
		if (!bShowLocked && Thumbnail)
		{
			ImageWidget->SetBrushFromTexture(Thumbnail, false);
			ImageWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			ImageWidget->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			ImageWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ColorSwatchWidget)
	{
		if (!bShowLocked && !Thumbnail && bUseSwatchColor)
		{
			ColorSwatchWidget->SetBrushColor(SwatchColor);
			ColorSwatchWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ColorSwatchWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (LockGlyphWidget)
	{
		LockGlyphWidget->SetVisibility(bShowLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
