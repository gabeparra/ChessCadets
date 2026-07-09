#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChessPiece.h"
#include "ModeSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

// ARCADE flow, four pages on one translucent overlay, in the order the player
// picks things:
//   1) level:      THE GRID / THE FIELD / DEV piece arenas (Pawn..Royalty)
//   2) players:    1 PLAYER VS CPU / 2 PLAYERS
//   3) difficulty (1P only): EASY / MEDIUM / HARD
//   4) side:       WHITE / BLACK — which color the human plays; the click starts the game
// 2P skips difficulty. The chosen level is held until the final click, which travels.
UCLASS()
class CHESSKIDS_API UModeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Page 1 — level
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* LevelBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelGridButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelFieldButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelPawnButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelKnightButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelBishopButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelRookButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelRoyaltyButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* LevelBackButton;

	// Page 2 — players
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* PlayersBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* OnePlayerButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* TwoPlayerButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* PlayersBackButton;

	// Page 3 — robot difficulty (1P only)
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* DifficultyBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* EasyButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* MediumButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HardButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* DifficultyBackButton;

	// Page 4 — which side to play
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* SideBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* WhiteButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* BlackButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SideBackButton;

	UFUNCTION() void OnLevelGrid();
	UFUNCTION() void OnLevelField();
	UFUNCTION() void OnLevelPawn();
	UFUNCTION() void OnLevelKnight();
	UFUNCTION() void OnLevelBishop();
	UFUNCTION() void OnLevelRook();
	UFUNCTION() void OnLevelRoyalty();
	UFUNCTION() void OnLevelBack();
	UFUNCTION() void OnOnePlayer();
	UFUNCTION() void OnTwoPlayer();
	UFUNCTION() void OnPlayersBack();
	UFUNCTION() void OnEasy();
	UFUNCTION() void OnMedium();
	UFUNCTION() void OnHard();
	UFUNCTION() void OnDifficultyBack();
	UFUNCTION() void OnWhite();
	UFUNCTION() void OnBlack();
	UFUNCTION() void OnSideBack();

private:
	enum class EPage { Level, Players, Difficulty, Side };
	void ShowPage(EPage Page);
	void PickLevel(FName Level);
	void PickDifficulty(int32 Difficulty);
	void PickSide(EChessColor Color);
	void StartGame();

	EPage CurrentPage = EPage::Level;
	FName PendingLevel = NAME_None;
	bool bPendingTwoPlayer = false;
	int32 PendingDifficulty = 1;
	EChessColor PendingColor = EChessColor::White;
};
