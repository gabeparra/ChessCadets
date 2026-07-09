#include "PieceSkinsMenuWidget.h"
#include "PieceSkinSwatch.h"
#include "ChessPieceSkinRegistry.h"
#include "ChessKidsGameInstance.h"
#include "ChessManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UPieceSkinsMenuWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		// Dark strip pinned to the bottom-center of the screen (not a full-center
		// overlay) so the board stays visible above while a skin is picked — the
		// board itself is the live preview.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.04f, 0.85f));
		Panel->SetPadding(FMargin(28.f, 12.f));
		if (UCanvasPanelSlot* PS = Root->AddChildToCanvas(Panel))
		{
			PS->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			PS->SetAlignment(FVector2D(0.5f, 1.f));
			PS->SetPosition(FVector2D(0.f, -24.f));
			PS->SetAutoSize(true);
		}

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->AddChild(Box);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>();
		TitleText->SetText(FText::FromString(TEXT("PIECE SKINS")));
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
		if (UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(TitleText))
		{
			TS->SetPadding(FMargin(8.f, 2.f, 8.f, 6.f));
			TS->SetHorizontalAlignment(HAlign_Center);
		}

		SkinSwatchBox = WidgetTree->ConstructWidget<UWrapBox>();
		SkinSwatchBox->SetInnerSlotPadding(FVector2D(8.f, 8.f));
		if (UVerticalBoxSlot* WS = Box->AddChildToVerticalBox(SkinSwatchBox))
		{
			WS->SetPadding(FMargin(8.f, 0.f));
			WS->SetHorizontalAlignment(HAlign_Center);
		}

		BackButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>();
		BackLabel->SetText(FText::FromString(TEXT("Back")));
		BackLabel->SetJustification(ETextJustify::Center);
		BackLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		BackLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		BackButton->AddChild(BackLabel);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(BackButton))
		{
			S->SetPadding(FMargin(48.f, 6.f, 48.f, 2.f));
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	return Super::RebuildWidget();
}

void UPieceSkinsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// AddUniqueDynamic: this widget is cached and re-added by the pause/main menu each
	// time it opens, so NativeConstruct re-fires; plain AddDynamic would stack handlers.
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this, &UPieceSkinsMenuWidget::OnBack);

	PopulateSkins();

	SetIsFocusable(true);
	SetKeyboardFocus();
}

FReply UPieceSkinsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPieceSkinsMenuWidget::OnBack()
{
	RemoveFromParent();
}

void UPieceSkinsMenuWidget::PopulateSkins()
{
	if (!SkinSwatchBox) return;
	SkinSwatchBox->ClearChildren();

	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI) return;

	const FName EquippedSkinId = GI->GetSelectedPieceSkinId();

	auto AddSwatch = [&](FName SkinId, const FText& DisplayName, UTexture2D* Thumbnail, bool bUnlocked)
	{
		UPieceSkinSwatch* Swatch = CreateWidget<UPieceSkinSwatch>(this, UPieceSkinSwatch::StaticClass());
		if (!Swatch) return;
		Swatch->SkinId = SkinId;
		Swatch->DisplayName = DisplayName;
		Swatch->Thumbnail = Thumbnail;
		Swatch->bUnlocked = bUnlocked;
		Swatch->bEquipped = (SkinId == EquippedSkinId);
		Swatch->OnSkinSwatchClicked.AddUniqueDynamic(this, &UPieceSkinsMenuWidget::OnSkinClicked);
		SkinSwatchBox->AddChildToWrapBox(Swatch);
		Swatch->RefreshAppearance();
	};

	// Always-unlocked entry that reverts pieces to their own default look.
	AddSwatch(NAME_None, FText::FromString(TEXT("Default")), nullptr, true);

	if (UChessPieceSkinRegistry* Registry = GI->PieceSkinRegistry.LoadSynchronous())
	{
		for (const FChessPieceSkinEntry& Entry : Registry->Skins)
		{
			if (Entry.SkinId.IsNone()) continue;
			AddSwatch(Entry.SkinId, Entry.DisplayName, Entry.Thumbnail, GI->IsPieceSkinUnlocked(Entry.SkinId));
		}
	}
}

void UPieceSkinsMenuWidget::OnSkinClicked(FName SkinId)
{
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI || !GI->SelectPieceSkin(SkinId))
		return;

	if (AChessManager* Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass())))
	{
		Manager->ApplyPieceSkin(GI->GetSelectedPieceMaterial());
	}

	// Move the equipped highlight to the newly selected swatch.
	PopulateSkins();
}
