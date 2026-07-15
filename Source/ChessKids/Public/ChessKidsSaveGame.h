#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ChessKidsSaveGame.generated.h"

// Backs two independent save slots on UChessKidsGameInstance, each populating only the
// fields it cares about:
//  - "ChessKidsSave" (skin inventory): UnlockedPieceSkins / SelectedPieceSkinId.
//  - "ChessKidsProgress" (story progress): UnlockedStoryChapters / CompletedTutorials.
// The slot names/user indices live with the GameInstance's load/save calls.
UCLASS()
class CHESSKIDS_API UChessKidsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	TSet<FName> UnlockedPieceSkins;

	UPROPERTY(BlueprintReadOnly)
	FName SelectedPieceSkinId = NAME_None;

	// Highest story chapter available (1 = L_Pawn ... 5 = L_Royalty).
	UPROPERTY(BlueprintReadOnly)
	int32 UnlockedStoryChapters = 1;

	// Lesson indices (0 = Pawn ... 5 = King) the player has finished.
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> CompletedTutorials;

	// Master game volume, 0..1. Applied to the audio device on startup and
	// whenever the pause-menu slider changes it.
	UPROPERTY()
	float MasterVolume = 1.f;

	// Board camera zoom, 0..1 (0 = level default distance, 1 = closest). Read by
	// AChessCamera to shorten its spring-arm; set by the settings slider.
	UPROPERTY()
	float BoardZoom = 0.f;
};
