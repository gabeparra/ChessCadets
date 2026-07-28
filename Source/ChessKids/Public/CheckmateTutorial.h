#pragma once

#include "CoreMinimal.h"
#include "TutorialManager.h"
#include "CheckmateTutorial.generated.h"

// Level 7: The Firewall — "LOCKDOWN" (checkmate + stalemate).
// Teaching order is feel-first: the kid PLAYS the hunted king before any
// concept is named or tested.
// Phase 1: You're the hunted king — escape a check (that's check), then try
//          to escape a ladder position where every square is guarded (that's
//          checkmate, discovered by failing).
// Phase 2: Ladder mate — two rooks vs a live enemy king; build the mate
//          yourself. He captures unguarded rooks; stalemating him is a
//          lesson + retry.
// Phase 3: Stalemate — first FEEL it as the black king (no check, no moves,
//          draw), then flip to the white queen: one move mates, another
//          stalemates. Choose right.
// Boss: BREACH THE FIREWALL — mate-in-1 puzzles, genuinely evaluated.
// Expects a 6x6 TutorialBoard.
UCLASS()
class CHESSKIDS_API ACheckmateTutorial : public ATutorialManager
{
	GENERATED_BODY()

public:
	ACheckmateTutorial();

	// Tint for squares under white attack while the kid plays the black king
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Colors")
	FLinearColor DangerColor = FLinearColor(0.6f, 0.02f, 0.02f);

	// Tint for the suggested move once hints kick in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Colors")
	FLinearColor HintColor = FLinearColor(0.9f, 0.75f, 0.05f);

	// After this many ladder-mate failures (lost rook / stalemate), the board
	// starts glowing the recommended move each turn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Hints", meta = (ClampMin = "1"))
	int32 FailsBeforeHints = 2;

protected:
	virtual void SetupPhase(ETutorialPhase Phase) override;
	virtual void OnSquareClicked(int32 File, int32 Rank) override;
	virtual void OnPhaseComplete() override;
	virtual bool IsSquareBlocked(int32 File, int32 Rank) const override;

private:
	enum class EPositionVerdict : uint8 { Check, Checkmate, Stalemate };

	// A white piece on the scripted board. bPlayerControlled marks the piece(s)
	// the kid is allowed to move this phase.
	struct FWhitePiece
	{
		EChessPieceType Type;
		int32 File;
		int32 Rank;
		bool bPlayerControlled = false;
		AChessPiece* Actor = nullptr;
	};
	TArray<FWhitePiece> WhitePieces;

	// The black king (AXIOM drone) in every phase
	UPROPERTY() AChessPiece* BlackKing = nullptr;
	int32 BKFile = 0;
	int32 BKRank = 0;

	int32 Step = 0;          // round / boss puzzle index within the phase
	bool bStepsDone = false; // gates OnPhaseComplete (no collectible targets here)

	// Selection state: index into WhitePieces, or the black king in the
	// feel-it-yourself rounds
	int32 SelectedPiece = INDEX_NONE;
	bool bKingSelected = false;

	// Clicks that went nowhere in a no-escape round — after a few, the level
	// names what the kid just experienced
	int32 FailedTries = 0;

	// Ladder-mate failures (lost rook / stalemate). Survives phase resets so
	// hints kick in after FailsBeforeHints; cleared when the phase is beaten.
	int32 LadderFails = 0;

	FTimerHandle StepTimer;

	// --- setup ---
	void SetupHuntedKing();
	void SetupLadderMate();
	void SetupStalemate();
	void SetupBoss();
	void StartHuntedRound();
	void StartTrapRound();
	void StartBossPuzzle();
	void ClearPieces();
	void ResetBoardTint();
	void PaintDanger(); // red overlay of every white-attacked square

	// --- board authoring helpers ---
	void PlaceBlackKing(int32 File, int32 Rank);
	void PlaceWhite(EChessPieceType Type, int32 File, int32 Rank, bool bPlayerControlled = false);

	// --- chess evaluation ---
	// Squares white attacks; the black king is transparent to rays (he can't
	// hide behind himself). IgnoreIndex excludes one white piece (for "can the
	// king capture it safely?" checks).
	TSet<int32> WhiteAttacks(int32 IgnoreIndex = INDEX_NONE) const;
	void AppendPieceAttacks(EChessPieceType Type, int32 File, int32 Rank, TSet<int32>& Out, int32 IgnoreIndex) const;
	bool IsRayBlockedAt(int32 File, int32 Rank, int32 IgnoreIndex) const;

	bool IsBlackKingInCheck() const;
	TArray<FIntPoint> BlackKingMoves() const; // safe squares incl. safe captures
	EPositionVerdict EvaluatePosition() const;

	// --- click flows ---
	void HandleBlackKingClick(int32 File, int32 Rank); // phase 1 + phase 3 step 0
	void HandleLadderClick(int32 File, int32 Rank);    // phase 2
	void HandleMoveClick(int32 File, int32 Rank);      // phase 3 step 1 + boss
	TArray<FIntPoint> LegalMovesFor(const FWhitePiece& P) const;
	void MoveWhitePiece(int32 Index, int32 File, int32 Rank);
	void BlackKingRespond();
	void ShowLadderHint(); // after repeated failures: glow the best move
	void ShowSelection();
	void CompletePhaseSteps(TArray<FString> Lines);
	int32 FindWhiteAt(int32 File, int32 Rank) const;
};
