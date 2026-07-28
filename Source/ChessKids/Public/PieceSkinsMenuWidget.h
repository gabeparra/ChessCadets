#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PieceSkinsMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UScrollBox;
class UImage;
class APieceSkinPreviewActor;

// Dedicated "locker" screen for the piece skin collectible system: a live 3D preview of
// a pawn wearing the currently browsed skin, plus Unlocked/Locked lists of every skin in
// UChessKidsGameInstance::PieceSkinRegistry. Clicking an unlocked entry only updates the
// preview; Equip commits it and live-refreshes any pieces already on the board.
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

	// Shows APieceSkinPreviewActor's render target — a live 3D render of a pawn wearing
	// whichever skin is currently being previewed.
	UPROPERTY(meta = (BindWidgetOptional)) UImage* PreviewImage;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* PreviewNameText;

	// Vertically stacked, fixed-height scrollers so the panel's footprint stays constant
	// no matter how many skins are in the catalog.
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* UnlockedListBox;
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* LockedListBox;

	UPROPERTY(meta = (BindWidgetOptional)) UButton* EquipButton;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* BackButton;

	UFUNCTION() void OnBack();

private:
	void PopulateSkins();
	void RefreshPreview();

	UFUNCTION()
	void OnSkinRowClicked(FName SkinId);

	UFUNCTION()
	void OnEquipClicked();

	// Binds PreviewImage's Slate brush to PreviewActor's render target. Only needs to
	// happen once per PreviewActor (not once per skin/color change) — the capture
	// component runs with bCaptureEveryFrame = true, so the render target continuously
	// reflects the piece's current look on its own; the brush just needs to point at it.
	void BindPreviewImage();

	// Cached showroom rig — found-or-spawned once per widget instance and reused across
	// Back/reopen so the preview doesn't respawn (and re-capture) every time the screen opens.
	UPROPERTY()
	APieceSkinPreviewActor* PreviewActor = nullptr;

	// What the live preview slot is showing right now — may differ from the skin
	// actually equipped on the board until the player hits Equip. NAME_None = Default.
	FName PreviewedSkinId = NAME_None;
};
