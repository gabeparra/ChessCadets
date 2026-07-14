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
};
