#include "ModeSelectWidget.h"
#include "ChessKidsGameInstance.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Text,
	                     int32 Size, const TCHAR* Style, const FMargin& Pad)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Text));
		T->SetJustification(ETextJustify::Center);
		T->SetFont(FCoreStyle::GetDefaultFontStyle(Style, Size));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(T))
		{
			S->SetPadding(Pad);
			S->SetHorizontalAlignment(HAlign_Center);
		}
		return T;
	}

	void MakeButton(UWidgetTree* Tree, UVerticalBox* Box, UButton*& OutBtn,
	                const TCHAR* Label, int32 FontSize)
	{
		OutBtn = Tree->ConstructWidget<UButton>();
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Label));
		T->SetJustification(ETextJustify::Center);
		T->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		OutBtn->AddChild(T);
		if (UVerticalBoxSlot* BS = Box->AddChildToVerticalBox(OutBtn))
		{
			BS->SetPadding(FMargin(48.f, 5.f));
			BS->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

TSharedRef<SWidget> UModeSelectWidget::RebuildWidget()
{
	// C++ fallback UI — a WBP subclass with the named widgets can restyle this freely.
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>();
		Scrim->SetBrushColor(FLinearColor(0.f, 0.f, 0.05f, 0.55f));
		Scrim->SetHorizontalAlignment(HAlign_Center);
		Scrim->SetVerticalAlignment(VAlign_Center);
		if (UCanvasPanelSlot* SS = Root->AddChildToCanvas(Scrim))
		{
			SS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			SS->SetOffsets(FMargin(0.f));
		}

		UCanvasPanel* Pages = WidgetTree->ConstructWidget<UCanvasPanel>();
		Scrim->AddChild(Pages);

		// --- Page 1: level (arcade opens here — pick where to play first) ---
		LevelBox = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UCanvasPanelSlot* PS = Pages->AddChildToCanvas(LevelBox)) { PS->SetAutoSize(true); }
		MakeText(WidgetTree, LevelBox, TEXT("ARCADE"), 42, TEXT("Bold"), FMargin(24.f, 8.f));
		MakeText(WidgetTree, LevelBox, TEXT("Pick a level"), 20, TEXT("Regular"), FMargin(24.f, 0.f, 24.f, 14.f));
		MakeButton(WidgetTree, LevelBox, LevelGridButton,    TEXT("THE GRID"), 24);
		MakeButton(WidgetTree, LevelBox, LevelFieldButton,   TEXT("THE FIELD"), 24);
		MakeButton(WidgetTree, LevelBox, LevelPawnButton,    TEXT("DEV — PAWN"), 22);
		MakeButton(WidgetTree, LevelBox, LevelKnightButton,  TEXT("DEV — KNIGHT"), 22);
		MakeButton(WidgetTree, LevelBox, LevelBishopButton,  TEXT("DEV — BISHOP"), 22);
		MakeButton(WidgetTree, LevelBox, LevelRookButton,    TEXT("DEV — ROOK"), 22);
		MakeButton(WidgetTree, LevelBox, LevelRoyaltyButton, TEXT("DEV — ROYALTY"), 22);
		MakeButton(WidgetTree, LevelBox, LevelBackButton,    TEXT("BACK"), 18);

		// --- Page 2: players ---
		PlayersBox = WidgetTree->ConstructWidget<UVerticalBox>();
		PlayersBox->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PS = Pages->AddChildToCanvas(PlayersBox)) { PS->SetAutoSize(true); }
		MakeText(WidgetTree, PlayersBox, TEXT("PLAYERS"), 42, TEXT("Bold"), FMargin(24.f, 8.f));
		MakeText(WidgetTree, PlayersBox, TEXT("How many players?"), 20, TEXT("Regular"), FMargin(24.f, 0.f, 24.f, 18.f));
		MakeButton(WidgetTree, PlayersBox, OnePlayerButton,   TEXT("1 PLAYER  VS  CPU"), 26);
		MakeButton(WidgetTree, PlayersBox, TwoPlayerButton,   TEXT("2 PLAYERS"), 26);
		MakeButton(WidgetTree, PlayersBox, PlayersBackButton, TEXT("BACK"), 18);

		// --- Page 3: robot difficulty (1P only) ---
		DifficultyBox = WidgetTree->ConstructWidget<UVerticalBox>();
		DifficultyBox->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PS = Pages->AddChildToCanvas(DifficultyBox)) { PS->SetAutoSize(true); }
		MakeText(WidgetTree, DifficultyBox, TEXT("DIFFICULTY"), 42, TEXT("Bold"), FMargin(24.f, 8.f));
		MakeText(WidgetTree, DifficultyBox, TEXT("How strong should the robot be?"), 20, TEXT("Regular"), FMargin(24.f, 0.f, 24.f, 18.f));
		MakeButton(WidgetTree, DifficultyBox, EasyButton,           TEXT("EASY"), 26);
		MakeButton(WidgetTree, DifficultyBox, MediumButton,         TEXT("MEDIUM"), 26);
		MakeButton(WidgetTree, DifficultyBox, HardButton,           TEXT("HARD"), 26);
		MakeButton(WidgetTree, DifficultyBox, DifficultyBackButton, TEXT("BACK"), 18);

		// --- Page 4: which side to play (the click starts the game) ---
		SideBox = WidgetTree->ConstructWidget<UVerticalBox>();
		SideBox->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PS = Pages->AddChildToCanvas(SideBox)) { PS->SetAutoSize(true); }
		MakeText(WidgetTree, SideBox, TEXT("YOUR SIDE"), 42, TEXT("Bold"), FMargin(24.f, 8.f));
		MakeText(WidgetTree, SideBox, TEXT("Which side do you play?"), 20, TEXT("Regular"), FMargin(24.f, 0.f, 24.f, 18.f));
		MakeButton(WidgetTree, SideBox, WhiteButton,    TEXT("WHITE"), 26);
		MakeButton(WidgetTree, SideBox, BlackButton,    TEXT("BLACK"), 26);
		MakeButton(WidgetTree, SideBox, SideBackButton, TEXT("BACK"), 18);
	}

	return Super::RebuildWidget();
}

void UModeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LevelGridButton)      LevelGridButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelGrid);
	if (LevelFieldButton)     LevelFieldButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelField);
	if (LevelPawnButton)      LevelPawnButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelPawn);
	if (LevelKnightButton)    LevelKnightButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelKnight);
	if (LevelBishopButton)    LevelBishopButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelBishop);
	if (LevelRookButton)      LevelRookButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelRook);
	if (LevelRoyaltyButton)   LevelRoyaltyButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelRoyalty);
	if (LevelBackButton)      LevelBackButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnLevelBack);
	if (OnePlayerButton)      OnePlayerButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnOnePlayer);
	if (TwoPlayerButton)      TwoPlayerButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnTwoPlayer);
	if (PlayersBackButton)    PlayersBackButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnPlayersBack);
	if (EasyButton)           EasyButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnEasy);
	if (MediumButton)         MediumButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnMedium);
	if (HardButton)           HardButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnHard);
	if (DifficultyBackButton) DifficultyBackButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnDifficultyBack);
	if (WhiteButton)          WhiteButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnWhite);
	if (BlackButton)          BlackButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnBlack);
	if (SideBackButton)       SideBackButton->OnClicked.AddUniqueDynamic(this, &UModeSelectWidget::OnSideBack);

	ShowPage(EPage::Level);
	SetIsFocusable(true);
	SetKeyboardFocus();
}

FReply UModeSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Escape steps back one page; on the first (level) page it closes the overlay.
		switch (CurrentPage)
		{
		case EPage::Side:       ShowPage(bPendingTwoPlayer ? EPage::Players : EPage::Difficulty); break;
		case EPage::Difficulty: ShowPage(EPage::Players); break;
		case EPage::Players:    ShowPage(EPage::Level); break;
		default:                OnLevelBack(); break;
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UModeSelectWidget::ShowPage(EPage Page)
{
	CurrentPage = Page;
	if (LevelBox)      LevelBox->SetVisibility(Page == EPage::Level ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (PlayersBox)    PlayersBox->SetVisibility(Page == EPage::Players ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (DifficultyBox) DifficultyBox->SetVisibility(Page == EPage::Difficulty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (SideBox)       SideBox->SetVisibility(Page == EPage::Side ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UModeSelectWidget::PickLevel(FName Level)
{
	PendingLevel = Level;
	ShowPage(EPage::Players);
}

void UModeSelectWidget::PickDifficulty(int32 Difficulty)
{
	PendingDifficulty = FMath::Clamp(Difficulty, 1, 3);
	ShowPage(EPage::Side);
}

void UModeSelectWidget::PickSide(EChessColor Color)
{
	PendingColor = Color;
	StartGame();
}

void UModeSelectWidget::StartGame()
{
	if (PendingLevel.IsNone()) return;
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		GI->bTwoPlayerMode = bPendingTwoPlayer;
		if (!bPendingTwoPlayer)
		{
			GI->Difficulty = PendingDifficulty;
			GI->PlayerColor = PendingColor;
		}
	}
	UGameplayStatics::OpenLevel(this, PendingLevel);
}

void UModeSelectWidget::OnLevelGrid()     { PickLevel(TEXT("L_Grid")); }
void UModeSelectWidget::OnLevelField()    { PickLevel(TEXT("L_Field")); }
void UModeSelectWidget::OnLevelPawn()     { PickLevel(TEXT("L_Pawn")); }
void UModeSelectWidget::OnLevelKnight()   { PickLevel(TEXT("L_Knight")); }
void UModeSelectWidget::OnLevelBishop()   { PickLevel(TEXT("L_Bishop")); }
void UModeSelectWidget::OnLevelRook()     { PickLevel(TEXT("L_Rook")); }
void UModeSelectWidget::OnLevelRoyalty()  { PickLevel(TEXT("L_Royalty")); }
void UModeSelectWidget::OnLevelBack()     { RemoveFromParent(); }

void UModeSelectWidget::OnOnePlayer()      { bPendingTwoPlayer = false; ShowPage(EPage::Difficulty); }
void UModeSelectWidget::OnTwoPlayer()      { bPendingTwoPlayer = true;  ShowPage(EPage::Side); }
void UModeSelectWidget::OnPlayersBack()    { ShowPage(EPage::Level); }
void UModeSelectWidget::OnEasy()           { PickDifficulty(1); }
void UModeSelectWidget::OnMedium()         { PickDifficulty(2); }
void UModeSelectWidget::OnHard()           { PickDifficulty(3); }
void UModeSelectWidget::OnDifficultyBack() { ShowPage(EPage::Players); }
void UModeSelectWidget::OnWhite()          { PickSide(EChessColor::White); }
void UModeSelectWidget::OnBlack()          { PickSide(EChessColor::Black); }
void UModeSelectWidget::OnSideBack()       { ShowPage(bPendingTwoPlayer ? EPage::Players : EPage::Difficulty); }
