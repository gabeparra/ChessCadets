#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChessHudWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UBorder;
class UImage;
class USlider;
class AChessManager;
class AChessPlayerController;

// In-game HUD: whose turn it is, a check warning, key hints, and a friendly
// game-over overlay with Play Again / Main Menu. Works standalone (the C++
// fallback builds its own tree) or as the base class of a styled WBP whose
// widgets use the BindWidgetOptional names below.
UCLASS()
class CHESSKIDS_API UChessHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TurnText;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ScoreText;
	// Purple chip behind ScoreText (C++ fallback UI only); toggled with it.
	UPROPERTY() class UBorder* ScoreChip = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* CheckText;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* HintResultText;   // "Hint: e2 → e4"
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* WarningText;      // "This is not a valid move"
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* WhiteCapturedText;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* BlackCapturedText;
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* GameOverPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* GameOverText;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* PlayAgainButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* MainMenuButton;

	// "New skin unlocked!" popup — shown before GameOverPanel when winning granted a
	// skin (currently only the AXIOM battle, King's tutorial chapter reward — see
	// AChessManager::OnSkinUnlocked). Mirrors UTutorialHudWidget's SkinUnlockPanel:
	// same manual scale/opacity pop-in via NativeTick rather than a UMG WidgetAnimation.
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* SkinUnlockPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SkinUnlockNameText;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* SkinUnlockImage;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SkinUnlockOkButton;

	// Bottom-right, always available in-game — opens the piece skins locker without
	// needing to pause first.
	UPROPERTY(meta = (BindWidgetOptional)) UButton* CustomizationButton;

	// Bottom-right, above Customization — asks the engine for a suggested move
	// (kids won't discover the H-key shortcut on their own).
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HintButton;

	// Optional styled skins panel; falls back to the C++ UPieceSkinsMenuWidget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|HUD")
	TSubclassOf<UUserWidget> SkinsWidgetClass;

	// Board color sliders — bottom-left, always available in-game.
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* WhiteColorLabel;
	UPROPERTY(meta = (BindWidgetOptional)) USlider* WhiteColorSlider;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* BlackColorLabel;
	UPROPERTY(meta = (BindWidgetOptional)) USlider* BlackColorSlider;

	UFUNCTION() void HandleGameOver(FString Result);
	UFUNCTION() void OnDangerToggleClicked();
	UFUNCTION() void OnHudMenuClicked();
	UFUNCTION() void OnHudHelpClicked();
	UFUNCTION() void OnHudHelpCloseClicked();

	// MENU (opens the pause menu) with a quick-guide HELP popup under it,
	// mirroring the tutorial HUD so the layout feels the same in level 8.
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HudMenuButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HudHelpButton;
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* HudHelpPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HudHelpCloseButton;
	UFUNCTION() void OnIntroBeginClicked();

	// Final-battle intro: explains WHY AXIOM's army is missing pieces before
	// the first move. Only shown on the Axiom map.
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* IntroPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* IntroBeginButton;
	UFUNCTION() void HandleHintReady(FString FromSquare, FString ToSquare);

public:
	// Flash a transient warning (e.g. "This is not a valid move") that fades on its own.
	// Public so the player controller can call it on a rejected move.
	UFUNCTION(BlueprintCallable, Category = "Chess|HUD") void ShowWarning(const FString& Message);
protected:
	UFUNCTION() void OnPlayAgainClicked();
	UFUNCTION() void OnMainMenuClicked();
	UFUNCTION() void OnWhiteColorChanged(float Value);
	UFUNCTION() void OnBlackColorChanged(float Value);
	UFUNCTION() void OnCustomizationClicked();
	UFUNCTION() void OnHintClicked();
	UFUNCTION() void HandleSkinUnlocked(FName SkinId);
	UFUNCTION() void OnSkinUnlockOkClicked();

private:
	UPROPERTY() AChessManager* Manager = nullptr;
	UPROPERTY() UUserWidget* SkinsInstance = nullptr;

	// Last applied turn-indicator state (avoids restyling the text every tick).
	// 0 = P1 (2P), 1 = P2 (2P), 2 = your turn (1P), 3 = robot thinking (1P), -1 = unset.
	int32 LastTurnState = -1;

	void ApplyBoardColors();
	AChessPlayerController* GetChessPC() const;
	void ShowGameOverPanel(FString Result);
	void ShowSkinUnlockPopup();

	FTimerHandle HintHideTimer;
	FTimerHandle WarningHideTimer;
	int32 LastPositionVersion = -1;   // gates the captured-tray board scan to actual moves

	// Safe/danger overlay toggle — training wheels the player opts into.
	// Refreshed on every position change while enabled.
	UPROPERTY(meta = (BindWidgetOptional)) UButton* DangerToggleButton;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DangerToggleLabel;
	bool bShowDanger = false;
	int32 LastDangerVersion = -1;
	void RefreshDangerOverlay();

	FName PendingUnlockedSkinId = NAME_None;
	FString PendingGameOverResult;
	bool bSkinPopupAnimating = false;
	float SkinPopupAnimElapsed = 0.f;
};
