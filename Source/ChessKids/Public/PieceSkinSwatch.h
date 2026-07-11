#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PieceSkinSwatch.generated.h"

class UButton;
class UTexture2D;
class UImage;
class UTextBlock;
class UBorder;
class UHorizontalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkinSwatchClicked, FName, SkinId);

// One clickable row in the piece skins picker's Unlocked/Locked lists
// (UPieceSkinsMenuWidget). A dedicated widget per row because the catalog is
// data-driven/unbounded — UMG's dynamic OnClicked delegate carries no per-instance
// payload, so a single shared handler can't otherwise tell which row fired.
UCLASS()
class CHESSKIDS_API UPieceSkinSwatch : public UUserWidget
{
	GENERATED_BODY()

public:
	// NAME_None represents the always-unlocked "Default" (no skin) entry.
	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	FName SkinId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	UTexture2D* Thumbnail = nullptr;

	// Fallback preview when there's no Thumbnail texture — a flat color swatch (e.g.
	// the "Default" entry uses this to show the piece's actual white/black tint,
	// since there's no static image for the untextured look).
	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	bool bUseSwatchColor = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	FLinearColor SwatchColor = FLinearColor::White;

	// Locked rows show a lock glyph instead of Thumbnail/SwatchColor — the collectible
	// stays a surprise until unlocked — and don't respond to clicks.
	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	bool bUnlocked = true;

	// Whether this is the skin actually applied to the board right now.
	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	bool bEquipped = false;

	// Whether this is what the live preview slot is currently showing (which may
	// differ from bEquipped until the player hits Equip).
	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	bool bPreviewed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	FText DisplayName;

	UPROPERTY(BlueprintAssignable, Category = "Chess|Skin")
	FOnSkinSwatchClicked OnSkinSwatchClicked;

	// Call after setting the properties above to (re)build the row's appearance.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skin")
	void RefreshAppearance();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY()
	UButton* Btn = nullptr;

	// Left accent bar — highlights the equipped and/or previewed row.
	UPROPERTY()
	UBorder* AccentBorder = nullptr;

	UPROPERTY()
	UImage* ImageWidget = nullptr;

	// Shown instead of ImageWidget when bUseSwatchColor is set and there's no Thumbnail.
	UPROPERTY()
	UBorder* ColorSwatchWidget = nullptr;

	// Shown instead of ImageWidget/ColorSwatchWidget when the row is locked.
	UPROPERTY()
	UTextBlock* LockGlyphWidget = nullptr;

	UPROPERTY()
	UTextBlock* LabelWidget = nullptr;

	UFUNCTION()
	void HandleClicked();
};
