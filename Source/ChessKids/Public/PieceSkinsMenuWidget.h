#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PieceSkinsMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWrapBox;

// Dedicated "locker" screen for the piece skin collectible system: lists every skin in
// UChessKidsGameInstance::PieceSkinRegistry as a swatch (locked ones disabled),
// equipping the clicked one and live-refreshing any pieces already on the board.
// Pure C++ UMG (RebuildWidget) so it works without a hand-authored WBP, same as
// USettingsMenuWidget/UPauseMenuWidget.
UCLASS()
class CHESSKIDS_API UPieceSkinsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) UWrapBox* SkinSwatchBox;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* BackButton;

	UFUNCTION() void OnBack();

private:
	void PopulateSkins();

	UFUNCTION()
	void OnSkinClicked(FName SkinId);
};
