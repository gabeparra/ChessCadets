#include "LevelSelectMenuWidget.h"
#include "ChessKidsGameInstance.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void ULevelSelectMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Take the level buttons over completely: clear any Blueprint routing (the BP
	// used to send Level1 to L_Field) and bind the story ladder 1..5.
	auto Take = [](UButton* Btn) { if (Btn) Btn->OnClicked.Clear(); };
	Take(Btn_Level1); Take(Btn_Level2); Take(Btn_Level3);
	Take(Btn_Level4); Take(Btn_Level5); Take(Btn_Level6);

	if (Btn_Level1) Btn_Level1->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel1);
	if (Btn_Level2) Btn_Level2->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel2);
	if (Btn_Level3) Btn_Level3->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel3);
	if (Btn_Level4) Btn_Level4->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel4);
	if (Btn_Level5) Btn_Level5->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel5);
	if (Btn_Level6) Btn_Level6->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel6);

	// Progression gate: chapter buttons enable as the player wins (or the dev
	// unlock-all toggle). SetIsEnabled(true) also overrides any disabled state
	// baked into the Widget Blueprint.
	const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	UButton* Chapters[6] = { Btn_Level1, Btn_Level2, Btn_Level3, Btn_Level4, Btn_Level5, Btn_Level6 };
	for (int32 i = 0; i < 6; ++i)
		if (Chapters[i])
			Chapters[i]->SetIsEnabled(!GI || GI->IsStoryChapterUnlocked(i + 1));
}

void ULevelSelectMenuWidget::OpenArena(FName MapName)
{
	// Story mode is always you vs the robot.
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		GI->bTwoPlayerMode = false;
	UGameplayStatics::OpenLevel(this, MapName);
}

// The class ladder: worker to royalty. Chapters open the v2 tutorial builds
// (duplicated from the arena maps, with the tutorial framework wired in).
void ULevelSelectMenuWidget::OnLevel1() { OpenArena(TEXT("L_Tutorial_Pawn_v2")); }
void ULevelSelectMenuWidget::OnLevel2() { OpenArena(TEXT("L_Tutorial_Rook_v2")); }
void ULevelSelectMenuWidget::OnLevel3() { OpenArena(TEXT("L_Tutorial_Bishop_v2")); }
void ULevelSelectMenuWidget::OnLevel4() { OpenArena(TEXT("L_Tutorial_Knight_v2")); }
void ULevelSelectMenuWidget::OnLevel5() { OpenArena(TEXT("L_Tutorial_Queen_v2")); }
void ULevelSelectMenuWidget::OnLevel6() { OpenArena(TEXT("L_Tutorial_King")); }
