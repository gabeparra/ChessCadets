#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelSelectMenuWidget.generated.h"

class UButton;

// C++ base for WBP_LevelSelect. Story mode is the 8-chapter ladder:
// pieces 1-5 (Pawn, Rook, Bishop, Knight, Queen), then King+check (6),
// checkmate (7), and the AXIOM final battle (8).
// The Blueprint's own button bindings are cleared so this base fully owns the routing.
UCLASS()
class CHESSKIDS_API ULevelSelectMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// Names match the widgets inside WBP_LevelSelect. Btn_Back stays BP-owned.
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level1;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level2;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level3;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level4;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level5;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level6;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level7;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level8;

	// Dynamic delegates carry no payload, so each button needs its own thunk;
	// they all funnel into OpenChapter with the chapter number.
	UFUNCTION() void OnLevel1();
	UFUNCTION() void OnLevel2();
	UFUNCTION() void OnLevel3();
	UFUNCTION() void OnLevel4();
	UFUNCTION() void OnLevel5();
	UFUNCTION() void OnLevel6();
	UFUNCTION() void OnLevel7();
	UFUNCTION() void OnLevel8();

private:
	static constexpr int32 NumChapters = 8;

	void OpenChapter(int32 Chapter);
};
