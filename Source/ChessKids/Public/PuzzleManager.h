#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "PuzzleManager.generated.h"

class AChessManager;

USTRUCT(BlueprintType)
struct FPuzzleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString FEN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString SolutionMove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString HintText;
};

UCLASS()
class CHESSKIDS_API APuzzleManager : public AActor
{
	GENERATED_BODY()

public:
	APuzzleManager();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	AChessManager* ChessManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString PuzzleFEN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString SolutionMove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FString HintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	UDataTable* PuzzleTable = nullptr;

	UFUNCTION()
	void HandleMoveMade(FString FromSquare, FString ToSquare);

	void LoadPuzzlePosition();
};