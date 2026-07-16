#pragma once

#include "CoreMinimal.h"
#include "TutorialManager.h"
#include "KingTutorial.generated.h"

// Level 6: The Grid — "ALARM" (King + the concept of check).
// Phase 1: king basics, then enemy pieces appear and their attacked squares
//          glow red — stepping on red trips the CHECK alarm.
// Phase 2: CPR training — escape check by Capture, Protect (block), or Run,
//          one scripted scenario per letter.
// Phase 3: give check — deliver check on the enemy king with the rook,
//          bishop, knight, queen, and pawn in turn.
// Boss: AXIOM'S SHIELD — check AXIOM's king 5 times; he escapes every one
//       (and punishes a hung queen), proving check alone can't win.
//       Narrative setup for Level 7: checkmate.
UCLASS()
class CHESSKIDS_API AKingTutorial : public ATutorialManager
{
	GENERATED_BODY()

public:
	AKingTutorial();

	// Color of squares under enemy attack
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Colors")
	FLinearColor DangerColor = FLinearColor(0.6f, 0.02f, 0.02f);

	// How many checks the player must give AXIOM in the boss fight
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Boss", meta = (ClampMin = "1"))
	int32 BossChecksToWin = 5;

protected:
	virtual void SetupPhase(ETutorialPhase Phase) override;
	virtual bool IsMoveAllowed(int32 FromFile, int32 FromRank, int32 ToFile, int32 ToRank) const override;
	virtual void OnTargetCollected(int32 File, int32 Rank) override;
	virtual void AfterPlayerMove(int32 File, int32 Rank) override;
	virtual void OnPhaseComplete() override;
	virtual bool IsSquareBlocked(int32 File, int32 Rank) const override;

private:
	// An enemy piece whose attacks paint the danger overlay. Tracked outside
	// EnemyActors so it can't be collected and doesn't gate phase completion.
	struct FThreat
	{
		EChessPieceType Type;
		int32 File;
		int32 Rank;
		AChessPiece* Actor = nullptr;
	};
	TArray<FThreat> Threats;

	// Squares currently under enemy attack (packed), i.e. the red overlay
	TSet<int32> DangerSquares;

	// Step counter within the current phase (CPR letters, give-check pieces)
	int32 Step = 0;

	// Set by the step flow when the phase is genuinely done — OnPhaseComplete
	// ignores the base class's "no targets left" auto-trigger otherwise.
	bool bStepsDone = false;

	// Last safe square the king stood on (bounce-back target for the alarm)
	int32 LastSafeFile = 0;
	int32 LastSafeRank = 0;

	// Static friendly king used in the CPR "Protect" scenario
	UPROPERTY() AChessPiece* FriendlyKing = nullptr;
	int32 FriendlyKingFile = 0;
	int32 FriendlyKingRank = 0;

	// AXIOM's king (phase 3 target and boss rival)
	UPROPERTY() AChessPiece* AxiomKing = nullptr;
	int32 AxiomFile = 0;
	int32 AxiomRank = 0;

	int32 ChecksGiven = 0;

	// When true, ray attacks pass through AXIOM's king (used to compute his
	// legal escape squares — a king can't hide behind himself)
	mutable bool bRaysIgnoreAxiom = false;

	FTimerHandle StepTimer;

	// --- setup ---
	void SetupBasics();
	void SetupCPR();
	void SetupGiveCheck();
	void SetupBoss();
	void ClearExtras();

	void StartCPRStep();
	void StartGiveCheckStep();

	// --- threats / danger overlay ---
	void AddThreat(EChessPieceType Type, int32 File, int32 Rank);
	void SpawnAxiomKing(int32 File, int32 Rank);
	void RefreshDanger();
	void ResetBoardTint();
	void AppendAttacks(EChessPieceType Type, int32 File, int32 Rank, TSet<int32>& Out) const;
	bool IsAttackBlocked(int32 File, int32 Rank) const;
	bool PlayerAttacks(int32 File, int32 Rank) const;

	// --- alarm bounce-back ---
	void BounceBack();

	// --- boss ---
	void AxiomEscape();
	void CompletePhaseSteps(TArray<FString> Lines);
	void SwitchPlayerPiece(EChessPieceType Type, int32 File, int32 Rank);
};
