#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialHudWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UImage;
class ATutorialManager;

// Tutorial overlay: the guide's speech panel (bound to OnGuideMessage — replaces
// the debug-text fallback) and a MENU button back to the main menu. C++ fallback
// UI; a styled WBP subclass can rename-match the widgets to restyle.
UCLASS()
class CHESSKIDS_API UTutorialHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(meta = (BindWidgetOptional)) UBorder* GuidePanel;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* GuideText;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* MenuButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HelpButton;

	// Quick-guide popup: brief check/checkmate/stalemate reference + a replay
	// of the current phase's instructions
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* HelpPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HelpReplayButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* HelpCloseButton;

	// Post-boss choice panel: continue to the next lesson or back to the menu.
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* CompletePanel;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* ContinueButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* CompleteMenuButton;

	// "New skin unlocked!" popup — shown before CompletePanel when the boss fight
	// granted a skin (see ATutorialManager::OnSkinUnlocked). Pops in with a manual
	// scale/opacity tween in NativeTick, matching this project's existing eased-Tick
	// animation style (see AChessPiece/AChessBoard) rather than a UMG WidgetAnimation.
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* SkinUnlockPanel;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SkinUnlockNameText;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* SkinUnlockImage;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SkinUnlockOkButton;

	UFUNCTION() void HandleGuideMessage(const FString& Message);
	UFUNCTION() void OnMenuClicked();
	UFUNCTION() void OnHelpClicked();
	UFUNCTION() void OnHelpReplayClicked();
	UFUNCTION() void OnHelpCloseClicked();
	UFUNCTION() void HandleLevelComplete();
	UFUNCTION() void OnContinueClicked();
	UFUNCTION() void HandleSkinUnlocked(FName SkinId);
	UFUNCTION() void OnSkinUnlockOkClicked();

private:
	void ShowCompletePanel();
	void ShowSkinUnlockPopup();

	UPROPERTY() ATutorialManager* Manager = nullptr;
	FTimerHandle HideTimer;

	FName PendingUnlockedSkinId = NAME_None;
	bool bSkinPopupAnimating = false;
	float SkinPopupAnimElapsed = 0.f;
};
