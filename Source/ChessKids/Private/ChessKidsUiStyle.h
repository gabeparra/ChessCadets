#pragma once

// One button look for the whole game, copied from Marco's WBP_InGameHud UNDO
// button (rounded blue box, grey outline, white Cyberpunks-Italic label). Any
// C++-built button goes through here so HUD, menus, and dev buttons all match.

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace ChessKidsUi
{
	inline FSlateBrush MakeButtonBrush(const FLinearColor& Fill, const FLinearColor& Outline)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Fill;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(4.f, 4.f, 4.f, 4.f), Outline, 1.f);
		return Brush;
	}

	inline void StyleButton(UButton* Button)
	{
		if (!Button) return;

		FButtonStyle Style;
		Style.Normal  = MakeButtonBrush(FLinearColor(0.106915f, 0.035592f, 0.846956f), FLinearColor(0.695111f, 0.695111f, 0.695111f));
		Style.Hovered = MakeButtonBrush(FLinearColor(0.065655f, 0.040576f, 0.865625f), FLinearColor(0.724268f, 0.724268f, 0.724268f));
		Style.Pressed = MakeButtonBrush(FLinearColor(0.384266f, 0.384266f, 0.384266f), FLinearColor(0.724268f, 0.724268f, 0.724268f));
		Style.Disabled = MakeButtonBrush(FLinearColor(0.15f, 0.15f, 0.2f), FLinearColor(0.4f, 0.4f, 0.4f));
		Style.NormalPadding  = FMargin(12.f, 1.5f);
		Style.PressedPadding = FMargin(12.f, 1.5f);
		Button->SetStyle(Style);
	}

	// White UPPERCASE label in the HUD's Cyberpunks Italic font (falls back to the
	// engine bold face if the font asset is missing).
	inline void StyleButtonLabel(UTextBlock* Label, const FString& Text, int32 FontSize)
	{
		if (!Label) return;

		Label->SetText(FText::FromString(Text.ToUpper()));
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		static TWeakObjectPtr<UObject> CachedFont;
		if (!CachedFont.IsValid())
			CachedFont = LoadObject<UObject>(nullptr, TEXT("/Game/UI/Fonts/Cyberpunks_Italic_Font.Cyberpunks_Italic_Font"));

		if (CachedFont.IsValid())
			Label->SetFont(FSlateFontInfo(CachedFont.Get(), FontSize, TEXT("Default")));
		else
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
	}
}
