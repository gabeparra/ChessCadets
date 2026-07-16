#include "LevelSelectMenuWidget.h"
#include "ChessKidsGameInstance.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void ULevelSelectMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UButton* Chapters[NumChapters] = {
		Btn_Level1, Btn_Level2, Btn_Level3, Btn_Level4,
		Btn_Level5, Btn_Level6, Btn_Level7, Btn_Level8,
	};

	// Take the level buttons over completely: clear any Blueprint routing (the BP
	// used to send Level1 to L_Field) and bind the story ladder 1..8.
	// AddUniqueDynamic is a macro that stringifies the literal function name,
	// so the handlers must be spelled out — they can't be looped over.
	for (UButton* Btn : Chapters)
		if (Btn) Btn->OnClicked.Clear();
	if (Btn_Level1) Btn_Level1->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel1);
	if (Btn_Level2) Btn_Level2->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel2);
	if (Btn_Level3) Btn_Level3->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel3);
	if (Btn_Level4) Btn_Level4->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel4);
	if (Btn_Level5) Btn_Level5->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel5);
	if (Btn_Level6) Btn_Level6->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel6);
	if (Btn_Level7) Btn_Level7->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel7);
	if (Btn_Level8) Btn_Level8->OnClicked.AddUniqueDynamic(this, &ULevelSelectMenuWidget::OnLevel8);

	// Progression gate: chapter buttons enable as the player wins (or the dev
	// unlock-all toggle). SetIsEnabled(true) also overrides any disabled state
	// baked into the Widget Blueprint.
	const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	for (int32 i = 0; i < NumChapters; ++i)
		if (Chapters[i])
			Chapters[i]->SetIsEnabled(!GI || GI->IsStoryChapterUnlocked(i + 1));
}

void ULevelSelectMenuWidget::OpenChapter(int32 Chapter)
{
	const FName Map = UChessKidsGameInstance::MapForChapter(Chapter);
	if (Map == NAME_None) return;
	// Story mode is always you vs the robot.
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		GI->bTwoPlayerMode = false;
	UGameplayStatics::OpenLevel(this, Map);
}

void ULevelSelectMenuWidget::OnLevel1() { OpenChapter(1); }
void ULevelSelectMenuWidget::OnLevel2() { OpenChapter(2); }
void ULevelSelectMenuWidget::OnLevel3() { OpenChapter(3); }
void ULevelSelectMenuWidget::OnLevel4() { OpenChapter(4); }
void ULevelSelectMenuWidget::OnLevel5() { OpenChapter(5); }
void ULevelSelectMenuWidget::OnLevel6() { OpenChapter(6); }
void ULevelSelectMenuWidget::OnLevel7() { OpenChapter(7); }
void ULevelSelectMenuWidget::OnLevel8() { OpenChapter(8); }
