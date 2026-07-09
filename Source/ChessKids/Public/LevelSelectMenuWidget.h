#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelSelectMenuWidget.generated.h"

class UButton;

// C++ base for WBP_LevelSelect. Story mode is the class ladder: buttons 1-6 walk
// the tutorial levels Pawn -> Knight -> Bishop -> Rook -> Queen -> King.
// The Blueprint's own button bindings are cleared so this base fully owns the routing.
UCLASS()
class CHESSKIDS_API ULevelSelectMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// Names match the widgets already inside WBP_LevelSelect. Btn_Back stays BP-owned.
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level1;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level2;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level3;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level4;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level5;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Btn_Level6;

	UFUNCTION() void OnLevel1();
	UFUNCTION() void OnLevel2();
	UFUNCTION() void OnLevel3();
	UFUNCTION() void OnLevel4();
	UFUNCTION() void OnLevel5();
	UFUNCTION() void OnLevel6();

private:
	void OpenArena(FName MapName);
};
