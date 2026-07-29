#include "PuzzleManager.h"
#include "ChessManager.h"
#include "ChessKidsGameInstance.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

APuzzleManager::APuzzleManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APuzzleManager::BeginPlay()
{
	Super::BeginPlay();

	if (!ChessManager)
	{
		for (TActorIterator<AChessManager> It(GetWorld()); It; ++It)
		{
			ChessManager = *It;
			break;
		}
	}

	if (!ChessManager)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
				TEXT("PuzzleManager: no ChessManager found!"));
		return;
	}

	ChessManager->OnMoveMade.AddDynamic(this, &APuzzleManager::HandleMoveMade);

	FTimerHandle LoadTimer;
	GetWorldTimerManager().SetTimer(LoadTimer, this,
		&APuzzleManager::LoadPuzzlePosition, 0.1f, false);
}

void APuzzleManager::LoadPuzzlePosition()
{
	if (!ChessManager)
		return;

	FName PuzzleID = NAME_None;
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		PuzzleID = GI->PuzzleID;
	}

	// Fallback: no puzzle chosen (e.g. testing in-editor) → use the editor FEN field
	if (PuzzleID.IsNone() || !PuzzleTable)
	{
		if (!PuzzleFEN.IsEmpty())
		{
			ChessManager->SetPositionFromFEN(PuzzleFEN);
			ChessManager->RefreshBoard();
		}
		return;
	}

	const FPuzzleRow* Row = PuzzleTable->FindRow<FPuzzleRow>(PuzzleID, TEXT("PuzzleLookup"));
	if (!Row)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
				FString::Printf(TEXT("Puzzle '%s' not found in table!"), *PuzzleID.ToString()));
		return;
	}

	SolutionMove = Row->SolutionMove;
	HintText = Row->HintText;
	ChessManager->SetPositionFromFEN(Row->FEN);
	ChessManager->RefreshBoard();

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan,
			FString::Printf(TEXT("Loaded '%s'. Hint: %s"), *PuzzleID.ToString(), *HintText));
}

void APuzzleManager::HandleMoveMade(FString FromSquare, FString ToSquare)
{
	const FString PlayedMove = FromSquare + ToSquare;

	if (PlayedMove.Equals(SolutionMove, ESearchCase::IgnoreCase))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
				TEXT("Correct! Puzzle solved!"));

		OnPuzzleSolved();    
	}
	else
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange,
				FString::Printf(TEXT("Not quite. You played %s, try again!"), *PlayedMove));
	}
}