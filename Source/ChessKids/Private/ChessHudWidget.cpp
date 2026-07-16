#include "ChessHudWidget.h"
#include "ChessManager.h"
#include "ChessPlayerController.h"
#include "ChessBoard.h"
#include "ChessKidsGameInstance.h"
#include "ChessPieceSkinRegistry.h"
#include "PieceSkinsMenuWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

TSharedRef<SWidget> UChessHudWidget::RebuildWidget()
{
	// C++ fallback UI — used when no Widget Blueprint provides a tree.
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		// Turn indicator — top center.
		TurnText = WidgetTree->ConstructWidget<UTextBlock>();
		TurnText->SetText(FText::FromString(TEXT("Your turn!")));
		TurnText->SetJustification(ETextJustify::Center);
		TurnText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
		TurnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.95f, 1.f)));
		if (UCanvasPanelSlot* TS = Root->AddChildToCanvas(TurnText))
		{
			TS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			TS->SetAlignment(FVector2D(0.5f, 0.f));
			TS->SetPosition(FVector2D(0.f, 24.f));
			TS->SetAutoSize(true);
		}

		// Check warning — under the turn indicator, red, hidden by default.
		CheckText = WidgetTree->ConstructWidget<UTextBlock>();
		CheckText->SetText(FText::FromString(TEXT("Check!")));
		CheckText->SetJustification(ETextJustify::Center);
		CheckText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
		CheckText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.25f, 0.2f)));
		CheckText->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* CS = Root->AddChildToCanvas(CheckText))
		{
			CS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			CS->SetAlignment(FVector2D(0.5f, 0.f));
			CS->SetPosition(FVector2D(0.f, 62.f));
			CS->SetAutoSize(true);
		}

		// Hint result — under the check line, cyan, hidden until requested.
		HintResultText = WidgetTree->ConstructWidget<UTextBlock>();
		HintResultText->SetJustification(ETextJustify::Center);
		HintResultText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
		HintResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.9f, 1.f)));
		HintResultText->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* HRS = Root->AddChildToCanvas(HintResultText))
		{
			HRS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			HRS->SetAlignment(FVector2D(0.5f, 0.f));
			HRS->SetPosition(FVector2D(0.f, 96.f));
			HRS->SetAutoSize(true);
		}

		// Captured-piece trays — top corners.
		auto MakeTray = [&](UTextBlock*& OutText, float AnchorX, float AlignX, float PosX)
		{
			OutText = WidgetTree->ConstructWidget<UTextBlock>();
			OutText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
			OutText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.75f)));
			if (UCanvasPanelSlot* TS2 = Root->AddChildToCanvas(OutText))
			{
				TS2->SetAnchors(FAnchors(AnchorX, 0.f, AnchorX, 0.f));
				TS2->SetAlignment(FVector2D(AlignX, 0.f));
				TS2->SetPosition(FVector2D(PosX, 24.f));
				TS2->SetAutoSize(true);
			}
		};
		MakeTray(WhiteCapturedText, 0.f, 0.f, 24.f);    // left: pieces YOU/P1 captured
		MakeTray(BlackCapturedText, 1.f, 1.f, -24.f);   // right: pieces the robot/P2 captured

		// Key hints — bottom left.
		HintText = WidgetTree->ConstructWidget<UTextBlock>();
		HintText->SetText(FText::FromString(TEXT("P — Pause    Z — Undo    H — Hint")));
		HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
		HintText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.55f)));
		if (UCanvasPanelSlot* HS = Root->AddChildToCanvas(HintText))
		{
			HS->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
			HS->SetAlignment(FVector2D(0.f, 1.f));
			HS->SetPosition(FVector2D(24.f, -18.f));
			HS->SetAutoSize(true);
		}

		// Board color sliders — bottom left, stacked above the key hints.
		UVerticalBox* ColorBox = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UCanvasPanelSlot* CBS = Root->AddChildToCanvas(ColorBox))
		{
			CBS->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
			CBS->SetAlignment(FVector2D(0.f, 1.f));
			CBS->SetPosition(FVector2D(24.f, -44.f));
			CBS->SetAutoSize(false);
			CBS->SetSize(FVector2D(190.f, 116.f));
		}

		auto MakeHueSlider = [&](UTextBlock*& OutLabel, USlider*& OutSlider, const TCHAR* Label)
		{
			OutLabel = WidgetTree->ConstructWidget<UTextBlock>();
			OutLabel->SetText(FText::FromString(Label));
			OutLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
			OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.6f)));
			if (UVerticalBoxSlot* S = ColorBox->AddChildToVerticalBox(OutLabel))
				S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));

			OutSlider = WidgetTree->ConstructWidget<USlider>();
			OutSlider->SetMinValue(0.f);
			OutSlider->SetMaxValue(1.f);
			if (UVerticalBoxSlot* S = ColorBox->AddChildToVerticalBox(OutSlider))
			{
				S->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));
				S->SetHorizontalAlignment(HAlign_Fill);
			}
		};
		MakeHueSlider(WhiteColorLabel, WhiteColorSlider, TEXT("White squares"));
		MakeHueSlider(BlackColorLabel, BlackColorSlider, TEXT("Black squares"));

		// Customization — bottom right, mirroring the board-color cluster on the left.
		CustomizationButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* CustomizationLabel = WidgetTree->ConstructWidget<UTextBlock>();
		CustomizationLabel->SetText(FText::FromString(TEXT("Customization")));
		CustomizationLabel->SetJustification(ETextJustify::Center);
		CustomizationLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		CustomizationLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		CustomizationButton->AddChild(CustomizationLabel);
		if (UCanvasPanelSlot* CustS = Root->AddChildToCanvas(CustomizationButton))
		{
			CustS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
			CustS->SetAlignment(FVector2D(1.f, 1.f));
			CustS->SetPosition(FVector2D(-24.f, -18.f));
			CustS->SetAutoSize(true);
		}

		// Safe/danger overlay toggle — stacked above Customization. Training
		// wheels the player can switch on: every square black can attack
		// glows red, same language as the tutorial levels.
		DangerToggleButton = WidgetTree->ConstructWidget<UButton>();
		DangerToggleLabel = WidgetTree->ConstructWidget<UTextBlock>();
		DangerToggleLabel->SetText(FText::FromString(TEXT("Danger view: OFF")));
		DangerToggleLabel->SetJustification(ETextJustify::Center);
		DangerToggleLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		DangerToggleLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		DangerToggleButton->AddChild(DangerToggleLabel);
		if (UCanvasPanelSlot* DTS = Root->AddChildToCanvas(DangerToggleButton))
		{
			DTS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
			DTS->SetAlignment(FVector2D(1.f, 1.f));
			DTS->SetPosition(FVector2D(-24.f, -58.f));
			DTS->SetAutoSize(true);
		}

		// MENU + HELP — top right, same placement as the tutorial HUD so the
		// controls feel identical across the whole game.
		auto MakeTopRightButton = [&](const TCHAR* Text, float PosY) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>();
			BtnLabel->SetText(FText::FromString(Text));
			BtnLabel->SetJustification(ETextJustify::Center);
			BtnLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
			BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
			Btn->AddChild(BtnLabel);
			if (UCanvasPanelSlot* BtnSlot = Root->AddChildToCanvas(Btn))
			{
				BtnSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
				BtnSlot->SetAlignment(FVector2D(1.f, 0.f));
				BtnSlot->SetPosition(FVector2D(-24.f, PosY));
				BtnSlot->SetAutoSize(true);
			}
			return Btn;
		};
		HudMenuButton = MakeTopRightButton(TEXT("MENU"), 60.f);
		HudHelpButton = MakeTopRightButton(TEXT("? HELP"), 114.f);

		// Quick-guide popup: the same cheat sheet as the tutorial levels
		HudHelpPanel = WidgetTree->ConstructWidget<UBorder>();
		HudHelpPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.05f, 0.92f));
		HudHelpPanel->SetPadding(FMargin(40.f, 28.f));
		HudHelpPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* HHS = Root->AddChildToCanvas(HudHelpPanel))
		{
			HHS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			HHS->SetAlignment(FVector2D(0.5f, 0.5f));
			HHS->SetPosition(FVector2D(0.f, 0.f));
			HHS->SetAutoSize(true);
		}
		{
			UVerticalBox* HelpColumn = WidgetTree->ConstructWidget<UVerticalBox>();
			HudHelpPanel->AddChild(HelpColumn);

			auto AddHelpLine = [&](const TCHAR* Text, const FLinearColor& Color, int32 Size)
			{
				UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
				Line->SetText(FText::FromString(Text));
				Line->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
				Line->SetColorAndOpacity(FSlateColor(Color));
				Line->SetWrapTextAt(560.f);
				if (UVerticalBoxSlot* LS = HelpColumn->AddChildToVerticalBox(Line))
					LS->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			};
			AddHelpLine(TEXT("QUICK GUIDE"), FLinearColor(0.4f, 1.f, 0.8f), 26);
			AddHelpLine(TEXT("CHECK: the king is attacked. He must escape NOW."), FLinearColor(0.5f, 0.75f, 1.f), 17);
			AddHelpLine(TEXT("Escapes: C-P-R. Capture the attacker, Protect (block the path), or Run."), FLinearColor(0.9f, 0.95f, 1.f), 17);
			AddHelpLine(TEXT("CHECKMATE: check with NO escape. Game over. Attacker wins!"), FLinearColor(1.f, 0.45f, 0.45f), 17);
			AddHelpLine(TEXT("STALEMATE: NOT in check, but no moves at all. Nobody wins, it's a draw."), FLinearColor(1.f, 0.85f, 0.4f), 17);
			AddHelpLine(TEXT("Keys: P pauses, Z undoes a move, H shows a hint."), FLinearColor(0.6f, 0.65f, 0.75f), 14);

			HudHelpCloseButton = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
			CloseLabel->SetText(FText::FromString(TEXT("GOT IT")));
			CloseLabel->SetJustification(ETextJustify::Center);
			CloseLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
			CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
			HudHelpCloseButton->AddChild(CloseLabel);
			if (UVerticalBoxSlot* CBS2 = HelpColumn->AddChildToVerticalBox(HudHelpCloseButton))
				CBS2->SetHorizontalAlignment(HAlign_Center);
		}

		// Final-battle intro — centered, only made visible on the Axiom map.
		// Explains the handicap so the missing pieces read as story, not a bug.
		// Wrapped in a dark border so it reads as a panel, not text soup over
		// the pieces.
		IntroPanel = WidgetTree->ConstructWidget<UBorder>();
		IntroPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.05f, 0.92f));
		IntroPanel->SetPadding(FMargin(48.f, 36.f));
		IntroPanel->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBox* IntroColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		IntroPanel->AddChild(IntroColumn);
		if (UCanvasPanelSlot* IPS = Root->AddChildToCanvas(IntroPanel))
		{
			IPS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			IPS->SetAlignment(FVector2D(0.5f, 0.5f));
			IPS->SetPosition(FVector2D(0.f, 0.f));
			IPS->SetAutoSize(true);
		}
		{
			auto AddIntroLine = [this](UVerticalBox* Box, const TCHAR* Text, int32 Size, FLinearColor Color)
			{
				UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
				Line->SetText(FText::FromString(Text));
				Line->SetJustification(ETextJustify::Center);
				Line->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
				Line->SetColorAndOpacity(FSlateColor(Color));
				Line->SetWrapTextAt(700.f);
				if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Line))
				{
					S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
					S->SetHorizontalAlignment(HAlign_Center);
				}
			};
			AddIntroLine(IntroColumn, TEXT("THE FINAL BATTLE"), 32, FLinearColor(0.4f, 1.f, 0.8f));
			AddIntroLine(IntroColumn, TEXT("AXIOM: \"So the little cadet made it to my core.\""), 18, FLinearColor(1.f, 0.5f, 0.5f));
			AddIntroLine(IntroColumn, TEXT("Your friends already hacked his army. His QUEEN and a ROOK are gone!"), 18, FLinearColor(0.9f, 0.95f, 1.f));
			AddIntroLine(IntroColumn, TEXT("Now finish it the only way that counts: CHECKMATE."), 18, FLinearColor(0.9f, 0.95f, 1.f));
			AddIntroLine(IntroColumn, TEXT("(Stuck? Turn on Danger view to see his reach.)"), 14, FLinearColor(0.6f, 0.65f, 0.75f));

			IntroBeginButton = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* BeginLabel = WidgetTree->ConstructWidget<UTextBlock>();
			BeginLabel->SetText(FText::FromString(TEXT("BEGIN")));
			BeginLabel->SetJustification(ETextJustify::Center);
			BeginLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
			BeginLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
			IntroBeginButton->AddChild(BeginLabel);
			if (UVerticalBoxSlot* BS = IntroColumn->AddChildToVerticalBox(IntroBeginButton))
				BS->SetHorizontalAlignment(HAlign_Center);
		}

		// Game-over overlay — centered, hidden until OnGameOver.
		GameOverPanel = WidgetTree->ConstructWidget<UVerticalBox>();
		GameOverPanel->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* GS = Root->AddChildToCanvas(GameOverPanel))
		{
			GS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			GS->SetAlignment(FVector2D(0.5f, 0.5f));
			GS->SetAutoSize(true);
		}

		GameOverText = WidgetTree->ConstructWidget<UTextBlock>();
		GameOverText->SetJustification(ETextJustify::Center);
		GameOverText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 32));
		GameOverText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.9f, 0.3f)));
		if (UVerticalBoxSlot* GTS = GameOverPanel->AddChildToVerticalBox(GameOverText))
		{
			GTS->SetPadding(FMargin(24.f, 16.f));
			GTS->SetHorizontalAlignment(HAlign_Center);
		}

		auto MakeButton = [&](UButton*& OutBtn, const TCHAR* Label)
		{
			OutBtn = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
			T->SetText(FText::FromString(Label));
			T->SetJustification(ETextJustify::Center);
			OutBtn->AddChild(T);
			if (UVerticalBoxSlot* BS = GameOverPanel->AddChildToVerticalBox(OutBtn))
			{
				BS->SetPadding(FMargin(32.f, 8.f));
				BS->SetHorizontalAlignment(HAlign_Fill);
			}
		};
		MakeButton(PlayAgainButton, TEXT("Play Again"));
		MakeButton(MainMenuButton, TEXT("Main Menu"));

		// "New skin unlocked!" popup — shown ahead of GameOverPanel when winning
		// granted a skin. Gold accent to read as a reward, matching
		// UTutorialHudWidget's SkinUnlockPanel.
		SkinUnlockPanel = WidgetTree->ConstructWidget<UBorder>();
		SkinUnlockPanel->SetBrushColor(FLinearColor(0.05f, 0.03f, 0.f, 0.95f));
		SkinUnlockPanel->SetPadding(FMargin(48.f, 32.f));
		SkinUnlockPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* SUS = Root->AddChildToCanvas(SkinUnlockPanel))
		{
			SUS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			SUS->SetAlignment(FVector2D(0.5f, 0.5f));
			SUS->SetPosition(FVector2D(0.f, 0.f));
			SUS->SetAutoSize(true);
		}

		UVerticalBox* SkinColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		SkinUnlockPanel->AddChild(SkinColumn);

		UTextBlock* SkinHeadline = WidgetTree->ConstructWidget<UTextBlock>();
		SkinHeadline->SetText(FText::FromString(TEXT("NEW SKIN UNLOCKED!")));
		SkinHeadline->SetJustification(ETextJustify::Center);
		SkinHeadline->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
		SkinHeadline->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.2f)));
		if (UVerticalBoxSlot* SHS = SkinColumn->AddChildToVerticalBox(SkinHeadline))
			SHS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		SkinUnlockImage = WidgetTree->ConstructWidget<UImage>();
		if (UVerticalBoxSlot* SIS = SkinColumn->AddChildToVerticalBox(SkinUnlockImage))
		{
			SIS->SetHorizontalAlignment(HAlign_Center);
			SIS->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		SkinUnlockNameText = WidgetTree->ConstructWidget<UTextBlock>();
		SkinUnlockNameText->SetJustification(ETextJustify::Center);
		SkinUnlockNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
		SkinUnlockNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.f)));
		if (UVerticalBoxSlot* SNS = SkinColumn->AddChildToVerticalBox(SkinUnlockNameText))
			SNS->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

		SkinUnlockOkButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* SkinOkLabel = WidgetTree->ConstructWidget<UTextBlock>();
		SkinOkLabel->SetText(FText::FromString(TEXT("NICE!")));
		SkinOkLabel->SetJustification(ETextJustify::Center);
		SkinUnlockOkButton->AddChild(SkinOkLabel);
		if (UVerticalBoxSlot* SOS = SkinColumn->AddChildToVerticalBox(SkinUnlockOkButton))
			SOS->SetHorizontalAlignment(HAlign_Center);
	}

	return Super::RebuildWidget();
}

void UChessHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// AddUniqueDynamic: NativeConstruct can re-fire if the HUD is ever re-added.
	if (PlayAgainButton) PlayAgainButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnPlayAgainClicked);
	if (MainMenuButton)  MainMenuButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnMainMenuClicked);
	if (CustomizationButton) CustomizationButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnCustomizationClicked);
	if (DangerToggleButton) DangerToggleButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnDangerToggleClicked);
	if (IntroBeginButton) IntroBeginButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnIntroBeginClicked);
	if (HudMenuButton) HudMenuButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnHudMenuClicked);
	if (HudHelpButton) HudHelpButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnHudHelpClicked);
	if (HudHelpCloseButton) HudHelpCloseButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnHudHelpCloseClicked);

	// The final battle opens on the story explanation for the missing pieces
	if (IntroPanel && GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Axiom")))
		IntroPanel->SetVisibility(ESlateVisibility::Visible);
	if (SkinUnlockOkButton) SkinUnlockOkButton->OnClicked.AddUniqueDynamic(this, &UChessHudWidget::OnSkinUnlockOkClicked);

	if (AChessPlayerController* PC = GetChessPC())
		Manager = PC->Manager;
	if (!Manager)
		Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass()));

	if (Manager)
	{
		Manager->OnGameOver.AddUniqueDynamic(this, &UChessHudWidget::HandleGameOver);
		Manager->OnHintReady.AddUniqueDynamic(this, &UChessHudWidget::HandleHintReady);
		Manager->OnSkinUnlocked.AddUniqueDynamic(this, &UChessHudWidget::HandleSkinUnlocked);
	}

	// Two-player hot-seat has one local save/skin selection shared by both people
	// taking turns — ApplyPieceSkin() already refuses to skin either side in that mode
	// (see AChessManager.cpp), so the button would just open a screen whose Equip has
	// no visible effect. Hide the entry point in that mode and explicitly restore it
	// otherwise — NativeConstruct can re-fire if the HUD is ever re-added, so this has
	// to actively set the right state each time rather than only ever collapsing it.
	// Reads GI->bTwoPlayerMode directly rather than Manager->IsTwoPlayerMode(): the
	// latter only becomes accurate once AChessManager::BeginPlay() has copied it from
	// the GameInstance, and that isn't guaranteed to happen before this widget's own
	// NativeConstruct — GI's copy is set from the mode-select screen, well before any
	// level/actor BeginPlay runs, so it's always already correct here.
	if (CustomizationButton)
	{
		const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
		const bool bTwoPlayer = GI && GI->bTwoPlayerMode;
		CustomizationButton->SetVisibility(bTwoPlayer ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (WhiteColorSlider) WhiteColorSlider->OnValueChanged.AddUniqueDynamic(this, &UChessHudWidget::OnWhiteColorChanged);
	if (BlackColorSlider) BlackColorSlider->OnValueChanged.AddUniqueDynamic(this, &UChessHudWidget::OnBlackColorChanged);

	// Sliders start where the player left them (SetValue doesn't fire OnValueChanged).
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		if (WhiteColorSlider) WhiteColorSlider->SetValue(GI->LightSquareHue);
		if (BlackColorSlider) BlackColorSlider->SetValue(GI->DarkSquareHue);
	}
}

void UChessHudWidget::OnHudMenuClicked()
{
	if (AChessPlayerController* PC = Cast<AChessPlayerController>(GetOwningPlayer()))
		PC->TogglePause();
}

void UChessHudWidget::OnHudHelpClicked()
{
	if (HudHelpPanel)
		HudHelpPanel->SetVisibility(HudHelpPanel->GetVisibility() == ESlateVisibility::Collapsed
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UChessHudWidget::OnHudHelpCloseClicked()
{
	if (HudHelpPanel)
		HudHelpPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UChessHudWidget::OnIntroBeginClicked()
{
	if (IntroPanel)
		IntroPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UChessHudWidget::OnDangerToggleClicked()
{
	bShowDanger = !bShowDanger;
	if (DangerToggleLabel)
		DangerToggleLabel->SetText(FText::FromString(
			bShowDanger ? TEXT("Danger view: ON") : TEXT("Danger view: OFF")));

	LastDangerVersion = -1; // force a refresh on the next tick when enabling
	if (!bShowDanger)
		RefreshDangerOverlay(); // clears immediately
}

void UChessHudWidget::RefreshDangerOverlay()
{
	AChessPlayerController* PC = Cast<AChessPlayerController>(GetOwningPlayer());
	AChessBoard* Board = PC ? PC->Board : nullptr;
	if (!Board) return;

	if (bShowDanger && Manager)
		Board->SetDangerOverlay(Manager->GetSquaresAttackedByOpponent());
	else
		Board->ClearDangerOverlay();
}

void UChessHudWidget::ApplyBoardColors()
{
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI) return;

	GI->bCustomBoardColors = true;
	if (WhiteColorSlider) GI->LightSquareHue = WhiteColorSlider->GetValue();
	if (BlackColorSlider) GI->DarkSquareHue  = BlackColorSlider->GetValue();

	AChessPlayerController* PC = GetChessPC();
	AChessBoard* Board = PC ? PC->Board : nullptr;
	if (Board)
		Board->SetSquareColors(
			UChessKidsGameInstance::LightColorFromHue(GI->LightSquareHue),
			UChessKidsGameInstance::DarkColorFromHue(GI->DarkSquareHue));
}

void UChessHudWidget::OnWhiteColorChanged(float Value) { ApplyBoardColors(); }
void UChessHudWidget::OnBlackColorChanged(float Value) { ApplyBoardColors(); }

void UChessHudWidget::OnCustomizationClicked()
{
	UClass* Cls = SkinsWidgetClass ? SkinsWidgetClass.Get() : UPieceSkinsMenuWidget::StaticClass();
	if (!Cls) return;
	if (!SkinsInstance)
		SkinsInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), Cls);
	if (SkinsInstance && !SkinsInstance->IsInViewport())
		SkinsInstance->AddToViewport(100);
}

void UChessHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Runs even once the game is over (below this point is frozen then) — the skin
	// popup's pop-in animation plays exactly when that's true. Eased scale/opacity
	// tween, same manual-Tick idiom as UTutorialHudWidget's SkinUnlockPanel.
	if (bSkinPopupAnimating && SkinUnlockPanel)
	{
		SkinPopupAnimElapsed += InDeltaTime;
		constexpr float PopDuration = 0.45f;
		const float Alpha = FMath::Clamp(SkinPopupAnimElapsed / PopDuration, 0.f, 1.f);

		const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);
		const float Overshoot = 1.f + FMath::Sin(Alpha * PI) * 0.12f * (1.f - Alpha);
		const float Scale = FMath::Lerp(0.6f, 1.f, Eased) * Overshoot;

		SkinUnlockPanel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SkinUnlockPanel->SetRenderScale(FVector2D(Scale, Scale));
		SkinUnlockPanel->SetRenderOpacity(FMath::Clamp(SkinPopupAnimElapsed / 0.2f, 0.f, 1.f));

		if (Alpha >= 1.f)
			bSkinPopupAnimating = false;
	}

	if (!Manager) return;
	if (Manager->IsGameOver()) return;   // frozen once the overlay is up

	// Undo after checkmate resumes the game (bGameOver resets) — take the overlay
	// back down and bring the turn indicator back.
	if (GameOverPanel && GameOverPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		GameOverPanel->SetVisibility(ESlateVisibility::Hidden);
		if (TurnText) TurnText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (SkinUnlockPanel && SkinUnlockPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		SkinUnlockPanel->SetVisibility(ESlateVisibility::Collapsed);
		bSkinPopupAnimating = false;
	}

	AChessPlayerController* PC = GetChessPC();
	const bool bTwoPlayer = Manager->IsTwoPlayerMode();
	const bool bWhiteToMove = Manager->GetActiveColor() == TEXT("white");
	const bool bPlayerIsWhite = Manager->PlayerColor == EChessColor::White;
	const bool bThinking = !bTwoPlayer && ((PC && PC->IsAIThinking()) || (bWhiteToMove != bPlayerIsWhite));

	if (TurnText)
	{
		const int32 TurnState = bTwoPlayer ? (bWhiteToMove ? 0 : 1) : (bThinking ? 3 : 2);
		if (TurnState != LastTurnState)
		{
			LastTurnState = TurnState;
			if (bTwoPlayer)
			{
				// P1 = White team, P2 = Black team: text in the team's color,
				// outlined with the ENEMY team's color so it pops either way.
				const FLinearColor WhiteTeam(0.95f, 0.95f, 0.90f);
				const FLinearColor BlackTeam(0.06f, 0.06f, 0.10f);
				const bool bP1 = bWhiteToMove;

				TurnText->SetText(FText::FromString(bP1 ? TEXT("P1's Turn") : TEXT("P2's Turn")));
				FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 28);
				Font.OutlineSettings = FFontOutlineSettings(3, bP1 ? BlackTeam : WhiteTeam);
				TurnText->SetFont(Font);
				TurnText->SetColorAndOpacity(FSlateColor(bP1 ? WhiteTeam : BlackTeam));
			}
			else
			{
				TurnText->SetText(FText::FromString(bThinking ? TEXT("Robot is thinking…") : TEXT("Your turn!")));
				TurnText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
				TurnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.95f, 1.f)));
			}
		}
	}

	// Captured trays: the 64-square scan runs only when a move/undo actually
	// happened (version bump), not every frame.
	// Danger overlay follows the position while enabled
	if (bShowDanger && Manager->GetPositionVersion() != LastDangerVersion)
	{
		LastDangerVersion = Manager->GetPositionVersion();
		RefreshDangerOverlay();
	}

	const int32 Version = Manager->GetPositionVersion();
	if (Version != LastPositionVersion)
	{
		LastPositionVersion = Version;
		const FString BlackLost = Manager->GetCapturedPieces(false);   // black pieces lost = captured by White/P1
		const FString WhiteLost = Manager->GetCapturedPieces(true);    // white pieces lost = captured by Black/P2
		if (WhiteCapturedText)
		{
			const FString Who = bTwoPlayer ? TEXT("P1 took") : TEXT("You took");
			WhiteCapturedText->SetText(FText::FromString(BlackLost.IsEmpty() ? TEXT("") : FString::Printf(TEXT("%s:  %s"), *Who, *BlackLost)));
		}
		if (BlackCapturedText)
		{
			const FString Who = bTwoPlayer ? TEXT("P2 took") : TEXT("Robot took");
			BlackCapturedText->SetText(FText::FromString(WhiteLost.IsEmpty() ? TEXT("") : FString::Printf(TEXT("%s:  %s"), *Who, *WhiteLost)));
		}
	}

	if (CheckText)
	{
		const bool bInCheck = Manager->IsInCheck();
		CheckText->SetVisibility(bInCheck ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (bInCheck)
		{
			FString Warn;
			if (bTwoPlayer)
				Warn = bWhiteToMove ? TEXT("P1 — your King is in check!") : TEXT("P2 — your King is in check!");
			else
				Warn = bWhiteToMove ? TEXT("Careful — your King is in check!") : TEXT("Nice one — enemy King is in check!");
			CheckText->SetText(FText::FromString(Warn));
		}
	}
}

void UChessHudWidget::HandleGameOver(FString Result)
{
	// OnSkinUnlocked/OnGameOver both broadcast synchronously from the same checkmate
	// handling in AChessManager, so PendingUnlockedSkinId (if any) is already set by
	// the time this runs — show that popup first, GameOverPanel once it's dismissed.
	if (!PendingUnlockedSkinId.IsNone())
	{
		PendingGameOverResult = Result;
		ShowSkinUnlockPopup();
	}
	else
	{
		ShowGameOverPanel(Result);
	}
}

void UChessHudWidget::ShowGameOverPanel(FString Result)
{
	if (GameOverText)
	{
		const bool bTwoPlayer = Manager && Manager->IsTwoPlayerMode();
		const bool bAxiom = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("Axiom"));
		FString Msg;
		if (Result == TEXT("white") && !bTwoPlayer && bAxiom)
			// The story's final beat: beating AXIOM earns the title.
			Msg = TEXT("CHECKMATE! AXIOM IS DEFEATED!\nNeo City is free. You are a CHESS CADET!");
		else if (Result == TEXT("white")) Msg = bTwoPlayer ? TEXT("P1 wins! Great game!") : TEXT("You win! Amazing game!");
		else if (Result == TEXT("black")) Msg = bTwoPlayer ? TEXT("P2 wins! Great game!") : TEXT("Good try! You'll get the robot next time!");
		else                              Msg = TEXT("It's a draw — what a battle!");
		GameOverText->SetText(FText::FromString(Msg));
	}
	if (TurnText)  TurnText->SetVisibility(ESlateVisibility::Hidden);
	if (CheckText) CheckText->SetVisibility(ESlateVisibility::Hidden);
	if (HintResultText) HintResultText->SetVisibility(ESlateVisibility::Hidden);
	if (GameOverPanel) GameOverPanel->SetVisibility(ESlateVisibility::Visible);

	// The arena level blueprints still spawn the legacy WBP_GameOver menu off the
	// same OnGameOver broadcast — sweep it next tick (works regardless of which
	// binding fired first) so only this overlay is on screen.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UChessHudWidget> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			UChessHudWidget* Self = WeakThis.Get();
			if (!Self) return;
			TArray<UUserWidget*> TopLevel;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Self, TopLevel, UUserWidget::StaticClass(), true);
			for (UUserWidget* W : TopLevel)
				if (W != Self && W->GetClass()->GetName().StartsWith(TEXT("WBP_GameOver")))
					W->RemoveFromParent();
		});
	}
}

void UChessHudWidget::HandleSkinUnlocked(FName SkinId)
{
	PendingUnlockedSkinId = SkinId;
}

void UChessHudWidget::ShowSkinUnlockPopup()
{
	if (!SkinUnlockPanel)
	{
		ShowGameOverPanel(PendingGameOverResult);
		return;
	}

	FChessPieceSkinEntry Entry;
	bool bFound = false;
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		if (UChessPieceSkinRegistry* Registry = GI->PieceSkinRegistry.LoadSynchronous())
			bFound = Registry->FindSkin(PendingUnlockedSkinId, Entry);
	}

	if (SkinUnlockNameText)
		SkinUnlockNameText->SetText(bFound ? Entry.DisplayName : FText::FromName(PendingUnlockedSkinId));

	if (SkinUnlockImage)
	{
		if (bFound && Entry.Thumbnail)
		{
			SkinUnlockImage->SetBrushFromTexture(Entry.Thumbnail, false);
			SkinUnlockImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			SkinUnlockImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (TurnText)  TurnText->SetVisibility(ESlateVisibility::Hidden);
	if (CheckText) CheckText->SetVisibility(ESlateVisibility::Hidden);
	if (HintResultText) HintResultText->SetVisibility(ESlateVisibility::Hidden);

	SkinPopupAnimElapsed = 0.f;
	bSkinPopupAnimating = true;
	SkinUnlockPanel->SetRenderOpacity(0.f);
	SkinUnlockPanel->SetVisibility(ESlateVisibility::Visible);
}

void UChessHudWidget::OnSkinUnlockOkClicked()
{
	if (SkinUnlockPanel)
		SkinUnlockPanel->SetVisibility(ESlateVisibility::Collapsed);
	bSkinPopupAnimating = false;
	PendingUnlockedSkinId = NAME_None;
	ShowGameOverPanel(PendingGameOverResult);
}

void UChessHudWidget::HandleHintReady(FString FromSquare, FString ToSquare)
{
	if (!HintResultText) return;
	HintResultText->SetText(FText::FromString(
		FString::Printf(TEXT("Hint: %s → %s"), *FromSquare.ToUpper(), *ToSquare.ToUpper())));
	HintResultText->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Fade the tip out after a few seconds.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UChessHudWidget> WeakThis(this);
		World->GetTimerManager().SetTimer(HintHideTimer, [WeakThis]()
		{
			if (UChessHudWidget* Self = WeakThis.Get())
				if (Self->HintResultText)
					Self->HintResultText->SetVisibility(ESlateVisibility::Hidden);
		}, 5.f, false);
	}
}

void UChessHudWidget::OnPlayAgainClicked()
{
	if (AChessPlayerController* PC = GetChessPC())
		PC->RestartArena();
}

void UChessHudWidget::OnMainMenuClicked()
{
	if (AChessPlayerController* PC = GetChessPC())
		PC->QuitToMainMenu();
}

AChessPlayerController* UChessHudWidget::GetChessPC() const
{
	return Cast<AChessPlayerController>(GetOwningPlayer());
}
