#include "PieceSkinSwatch.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
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

		Btn = WidgetTree->ConstructWidget<UButton>();
		SizeBox->AddChild(Btn);
		DefaultButtonStyle = Btn->GetStyle();

		// The thumbnail (or flat-color fallback) now fills the entire row as a
		// background instead of sitting in a small icon box, so everything stacks in
		// one Overlay rather than the old icon-then-label HorizontalBox.
		UOverlay* RowOverlay = WidgetTree->ConstructWidget<UOverlay>();
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Btn->AddChild(RowOverlay)))
		{
			// UButtonSlot defaults to Center/Center (auto-sized to content) — without
			// this, Btn's own background only ever paints a box the size of the label
			// text, centered in the row, instead of filling it.
			BtnSlot->SetHorizontalAlignment(HAlign_Fill);
			BtnSlot->SetVerticalAlignment(VAlign_Fill);
		}

		ImageWidget = WidgetTree->ConstructWidget<UImage>();
		if (UOverlaySlot* ImgSlot = RowOverlay->AddChildToOverlay(ImageWidget))
		{
			ImgSlot->SetHorizontalAlignment(HAlign_Fill);
			ImgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		ColorSwatchWidget = WidgetTree->ConstructWidget<UBorder>();
		if (UOverlaySlot* ColorSlot = RowOverlay->AddChildToOverlay(ColorSwatchWidget))
		{
			ColorSlot->SetHorizontalAlignment(HAlign_Fill);
			ColorSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Darkens the bottom of the row so the label stays readable over whatever the
		// thumbnail/color happens to look like, without hiding the art entirely.
		ScrimBorder = WidgetTree->ConstructWidget<UBorder>();
		ScrimBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		if (UOverlaySlot* ScrimSlot = RowOverlay->AddChildToOverlay(ScrimBorder))
		{
			ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrimSlot->SetVerticalAlignment(VAlign_Fill);
		}

		LockGlyphWidget = WidgetTree->ConstructWidget<UTextBlock>();
		// Plain ASCII text rather than a lock-character glyph — the project's default
		// font isn't guaranteed to have a padlock glyph (bit us before with a Unicode
		// checkmark rendering as a missing-glyph box), so this avoids that risk entirely.
		LockGlyphWidget->SetText(FText::FromString(TEXT("LOCK")));
		LockGlyphWidget->SetJustification(ETextJustify::Center);
		LockGlyphWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
		LockGlyphWidget->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.5f)));
		if (UOverlaySlot* LockSlot = RowOverlay->AddChildToOverlay(LockGlyphWidget))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Right);
			LockSlot->SetVerticalAlignment(VAlign_Fill);
			LockSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}

		LabelWidget = WidgetTree->ConstructWidget<UTextBlock>();
		LabelWidget->SetJustification(ETextJustify::Center);
		LabelWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
		LabelWidget->SetAutoWrapText(true);
		LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor(0.97f, 0.97f, 1.f)));
		// A drop shadow keeps the text legible over a busy or light-colored thumbnail,
		// on top of ScrimBorder's darkening.
		LabelWidget->SetShadowOffset(FVector2D(1.f, 1.f));
		LabelWidget->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		if (UOverlaySlot* LS = RowOverlay->AddChildToOverlay(LabelWidget))
		{
			LS->SetHorizontalAlignment(HAlign_Fill);
			LS->SetVerticalAlignment(VAlign_Center);
			LS->SetPadding(FMargin(40.f, 4.f));
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
		FString NameStr = SkinId.IsNone() ? TEXT("Default") : DisplayName.ToString();
		if (bEquipped)
		{
			NameStr += TEXT("  (Equipped)");
		}
		LabelWidget->SetText(FText::FromString(NameStr));
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

		if (bPreviewed)
		{
			// The equipped row no longer gets its own background color (it clashed
			// with real skins' own material tones) — the "(Equipped)" text appended
			// to its label above is the only equipped indicator now. Previewed-but-
			// not-yet-equipped still gets a highlight so browsing feels responsive.
			// Flat color brush (not just a tint) so it's genuinely one solid color
			// with no bevel/border showing through.
			const FSlateColorBrush FlatBrush(FLinearColor(0.15f, 0.25f, 0.4f, 0.9f));
			FButtonStyle Style = DefaultButtonStyle;
			Style.Normal = FlatBrush;
			Style.Hovered = FlatBrush;
			Style.Pressed = FlatBrush;
			Style.Disabled = FlatBrush;
			Btn->SetStyle(Style);
		}
		else
		{
			// Same look as the menu's Back button — the engine's plain default button
			// style, no per-row background of its own.
			Btn->SetStyle(DefaultButtonStyle);
		}
	}
}

void UPieceSkinSwatch::HandleClicked()
{
	OnSkinSwatchClicked.Broadcast(SkinId);
}
