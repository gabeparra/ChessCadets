#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChessPiece.h"
#include "TutorialManager.generated.h"

class ATutorialBoard;
class AChessPiece;

UENUM(BlueprintType)
enum class ETutorialPhase : uint8
{
	Phase1,
	Phase2,
	Phase3,
	Boss,
	Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, ETutorialPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuideMessage, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTryAgain);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkinUnlocked, FName, SkinId);

UCLASS()
class CHESSKIDS_API ATutorialManager : public AActor
{
	GENERATED_BODY()

public:
	ATutorialManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Events for UI to bind to
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnGuideMessage OnGuideMessage;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnLevelComplete OnLevelComplete;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTryAgain OnTryAgain;

	// Fired right after OnLevelComplete if beating this tutorial's boss granted a new
	// piece skin (see UChessKidsGameInstance::UnlockSkinForTutorial). Not fired on
	// replays once the skin is already owned.
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnSkinUnlocked OnSkinUnlocked;

	// Set in editor — which board this manager controls
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ATutorialBoard* Board = nullptr;

	// The piece type this level teaches
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	EChessPieceType TaughtPiece = EChessPieceType::Pawn;

	// Seconds each line of a multi-line guide explanation stays on screen
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "1.0"))
	float GuideLineSeconds = 3.5f;

	// Lesson slot for MarkTutorialComplete. INDEX_NONE = derive from TaughtPiece
	// (Pawn=0..King=5). Concept lessons that don't teach a new piece (checkmate=6)
	// set this explicitly so they don't collide with a piece lesson.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 LessonIndexOverride = INDEX_NONE;

	// Level to travel to a few seconds after this lesson completes
	// (chains the tutorials: Pawn -> Rook -> ... -> King -> Axiom). None = stay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName NextLevelName = NAME_None;

	// Current state
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	ETutorialPhase CurrentPhase = ETutorialPhase::Phase1;

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 TargetsCollected = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 TargetsTotal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 MoveCount = 0;

	// Player piece position on the grid
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 PlayerFile = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 PlayerRank = 0;

	// Called when the player clicks a square. Virtual so lessons that don't fit
	// the one-piece-moves model (the checkmate level's quiz pads and two-rook
	// ladder) can take over click handling entirely.
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	virtual void OnSquareClicked(int32 File, int32 Rank);

	// Advance to next phase
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void AdvancePhase();

	// Reset current phase
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ResetPhase();

	// Show guide dialogue
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ShowGuideMessage(const FString& Message);

	// Check if a move is legal for the taught piece
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tutorial")
	TArray<FIntPoint> GetLegalMoves() const;

	// Last guide message shown — lets a HUD that constructs after the intro
	// broadcast (BeginPlay order isn't guaranteed) replay it on bind
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tutorial")
	FString GetLastGuideMessage() const { return LastGuideMessage; }

	// Where the post-boss "continue" choice should travel. An explicit
	// NextLevelName wins; otherwise derived from the story-chapter ladder
	// (this map's chapter + 1). NAME_None = no next level.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tutorial")
	FName GetNextLevelName() const;

protected:
	// Override in subclasses for level-specific logic
	virtual void SetupPhase(ETutorialPhase Phase);
	virtual void OnTargetCollected(int32 File, int32 Rank);
	virtual void OnPhaseComplete();
	virtual void AfterPlayerMove(int32 File, int32 Rank);
	virtual bool IsMoveAllowed(int32 FromFile, int32 FromRank, int32 ToFile, int32 ToRank) const;

	// Boss-victory bookkeeping: lesson-complete tracking, story-chapter unlock, and the
	// tutorial's configured skin reward (see OnSkinUnlocked). Every subclass detects
	// "boss defeated" its own way (capturing a runner, tagging a knight, racing to the
	// finish...) and sets CurrentPhase/broadcasts OnLevelComplete itself rather than
	// going through OnPhaseComplete()'s generic Boss branch, since each needs its own
	// victory guide message — call this alongside that instead of duplicating the
	// bookkeeping in every subclass.
	void GrantBossVictoryRewards();

	FString LastGuideMessage;

	// Long explanations play as a sequence of short lines (GuideLineSeconds
	// apart) so nothing overflows and each idea has time to land. A direct
	// ShowGuideMessage or a new sequence replaces whatever is still queued.
	void ShowGuideSequence(TArray<FString> Lines);

	TArray<FString> GuideQueue;
	FTimerHandle GuideTimer;
	void ShowNextGuideLine();

	// Multi-line sequences are remembered as "the instructions" so the HUD's
	// HELP button can replay them; single-line feedback doesn't overwrite them.
	TArray<FString> LastInstructionLines;

public:
	// Replay the current phase's instructions (HUD HELP button)
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ReplayInstructions();

	// Piece mesh config for spawning player piece
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Pieces")
	TMap<EChessPieceType, FPieceMeshConfig> PieceMeshes;

	// Look for generic target/enemy stand-ins spawned by SpawnTargetAt/SpawnEnemyAt.
	// Deliberately separate from PieceMeshes: those markers are always a Pawn-shaped
	// mesh regardless of which piece the level teaches, so keying their material off
	// PieceMeshes[Pawn] made the hero and its own decoys visually identical whenever
	// TaughtPiece was Pawn (the pawn tutorial's capture gauntlet).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial|Pieces")
	FPieceMeshConfig MarkerMeshConfig;

	// Player piece actor
	UPROPERTY() AChessPiece* PlayerPiece = nullptr;

	// Target positions for current phase
	TArray<FIntPoint> TargetPositions;

	// Enemy positions for current phase
	TArray<FIntPoint> EnemyPositions;

	// Tracks whether the player's pawn has moved (for two-square first move)
	bool bPlayerHasMoved = false;

	// Blocks input during the delay between phase completion and the next phase's setup
	bool bPhaseTransitionPending = false;

	void SpawnPlayerPiece(int32 File, int32 Rank);
	void MovePlayerTo(int32 File, int32 Rank);
	void SpawnTargetAt(int32 File, int32 Rank);
	void RemoveTargetAt(int32 File, int32 Rank);
	void SpawnEnemyAt(int32 File, int32 Rank);
	void RemoveEnemyAt(int32 File, int32 Rank);
	void SpawnWallAt(int32 File, int32 Rank);

	// Virtual so subclasses with a named boss rival tracked outside EnemyActors (the
	// AI pawn, Luna, the Runner — each keeps its own File/Rank rather than going
	// through SpawnEnemyAt) can fold that position in too. Without this, sliding/
	// multi-square moves never see the rival as an obstacle: IsMoveAllowed's path
	// checks only consult this function, so a move could land past the rival without
	// ever being blocked by or capturing it.
	virtual bool IsSquareBlocked(int32 File, int32 Rank) const;

	void UpdateHighlights();

	// A boss rival that's meant to read as an opponent piece (the AI pawn, Luna, the
	// Runner) shouldn't inherit whatever skin the hero has equipped via PieceMeshes —
	// when the rival's type matches TaughtPiece (the pawn tutorial's AI pawn, the
	// knight tutorial's Luna) that's the exact same PieceMeshes entry as the hero, and
	// the White/Black tint alone isn't reliably strong enough to still read as black
	// against a bright custom skin. This guarantees a real black piece regardless.
	const FPieceMeshConfig& GetBlackRivalMeshConfig();

protected:
	UPROPERTY() TMap<int32, AChessPiece*> TargetActors;
	UPROPERTY() TMap<int32, AChessPiece*> EnemyActors;

	// Walls: block movement, can't be captured, don't count toward phase completion
	UPROPERTY() TMap<int32, AActor*> WallActors;

	int32 PackPos(int32 File, int32 Rank) const;

	// Lazily loaded/cached by GetBlackRivalMeshConfig().
	UPROPERTY() FPieceMeshConfig BlackRivalMeshConfig;
};
