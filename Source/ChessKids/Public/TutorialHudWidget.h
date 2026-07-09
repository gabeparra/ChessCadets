#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialHudWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
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
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(meta = (BindWidgetOptional)) UBorder* GuidePanel;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* GuideText;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* MenuButton;

	// Post-boss choice panel: continue to the next lesson or back to the menu.
	UPROPERTY(meta = (BindWidgetOptional)) UBorder* CompletePanel;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* ContinueButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* CompleteMenuButton;

	UFUNCTION() void HandleGuideMessage(const FString& Message);
	UFUNCTION() void OnMenuClicked();
	UFUNCTION() void HandleLevelComplete();
	UFUNCTION() void OnContinueClicked();

private:
	UPROPERTY() ATutorialManager* Manager = nullptr;
	FTimerHandle HideTimer;
};
