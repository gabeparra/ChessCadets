#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PieceSkinSwatch.generated.h"

class UButton;
class UTexture2D;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkinSwatchClicked, FName, SkinId);

// One clickable entry in the piece skins picker (UPieceSkinsMenuWidget). A dedicated
// widget per swatch because the catalog is data-driven/unbounded — UMG's dynamic
// OnClicked delegate carries no per-instance payload, so a single shared handler can't
// otherwise tell which swatch fired.
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

	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	bool bUnlocked = true;

	UPROPERTY(BlueprintReadWrite, Category = "Chess|Skin")
	FText DisplayName;

	UPROPERTY(BlueprintAssignable, Category = "Chess|Skin")
	FOnSkinSwatchClicked OnSkinSwatchClicked;

	// Call after setting the properties above to (re)build the button's appearance.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skin")
	void RefreshAppearance();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY()
	UButton* Btn = nullptr;

	UPROPERTY()
	UImage* ImageWidget = nullptr;

	UPROPERTY()
	UTextBlock* LabelWidget = nullptr;

	UFUNCTION()
	void HandleClicked();
};
