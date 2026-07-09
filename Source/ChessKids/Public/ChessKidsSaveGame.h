#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ChessKidsSaveGame.generated.h"

// Persists the player's piece-skin inventory across game launches. Written/read by
// UChessKidsGameInstance; the slot name/user index it uses live there.
UCLASS()
class CHESSKIDS_API UChessKidsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSet<FName> UnlockedPieceSkins;

	UPROPERTY()
	FName SelectedPieceSkinId = NAME_None;
};
