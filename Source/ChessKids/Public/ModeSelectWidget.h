#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChessPiece.h"
#include "ModeSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

// CHESS MODE flow, four pages on one translucent overlay:
//   1) mode:       1 PLAYER VS CPU / 2 PLAYERS
//   2) difficulty (1P only): EASY / MEDIUM / HARD
//   3) side       (1P only): WHITE / BLACK — which color the human plays
//   4) venue:      THE GRID (L_Grid) / THE FIELD (L_Field) — both free play
// 2P skips straight from mode to venue (no difficulty/side choice — both colors
// are human in hot-seat). Choices persist on the GameInstance; the venue click travels.
UCLASS()
class CHESSKIDS_API UModeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Page 1 — mode
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* ModeBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* OnePlayerButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* TwoPlayerButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* BackButton;

	// Page 2 — robot difficulty (1P only)
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* DifficultyBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* EasyButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* MediumButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HardButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* DifficultyBackButton;

	// Page 3 — which side to play (1P only)
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* SideBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* WhiteButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* BlackButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SideBackButton;

	// Page 4 — venue
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* VenueBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* GridButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* FieldButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* VenueBackButton;

	UFUNCTION() void OnOnePlayer();
	UFUNCTION() void OnTwoPlayer();
	UFUNCTION() void OnBack();
	UFUNCTION() void OnEasy();
	UFUNCTION() void OnMedium();
	UFUNCTION() void OnHard();
	UFUNCTION() void OnDifficultyBack();
	UFUNCTION() void OnWhite();
	UFUNCTION() void OnBlack();
	UFUNCTION() void OnSideBack();
	UFUNCTION() void OnGrid();
	UFUNCTION() void OnField();
	UFUNCTION() void OnVenueBack();

private:
	enum class EPage { Mode, Difficulty, Side, Venue };
	void ShowPage(EPage Page);
	void PickDifficulty(int32 Difficulty);
	void PickSide(EChessColor Color);
	void StartGame(FName Venue);

	EPage CurrentPage = EPage::Mode;
	bool bPendingTwoPlayer = false;
	// Difficulty/side picked on pages 2/3, held until the venue is chosen on page 4.
	int32 PendingDifficulty = 1;
	EChessColor PendingColor = EChessColor::White;
};
