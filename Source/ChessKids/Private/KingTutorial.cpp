#include "KingTutorial.h"
#include "TutorialBoard.h"
#include "ChessPiece.h"
#include "Engine/World.h"
#include "TimerManager.h"

AKingTutorial::AKingTutorial()
{
	TaughtPiece = EChessPieceType::King;
}

// ---------------------------------------------------------------------------
// Phase routing
// ---------------------------------------------------------------------------

void AKingTutorial::SetupPhase(ETutorialPhase Phase)
{
	Super::SetupPhase(Phase);

	GetWorldTimerManager().ClearTimer(StepTimer);
	ClearExtras();
	DangerSquares.Empty();
	Step = 0;
	bStepsDone = false;
	TaughtPiece = EChessPieceType::King; // steps change it — always restore
	ResetBoardTint();

	switch (Phase)
	{
	case ETutorialPhase::Phase1: SetupBasics();    break;
	case ETutorialPhase::Phase2: SetupCPR();       break;
	case ETutorialPhase::Phase3: SetupGiveCheck(); break;
	case ETutorialPhase::Boss:   SetupBoss();      break;
	case ETutorialPhase::Complete: break;
	}

	UpdateHighlights();
}

void AKingTutorial::ClearExtras()
{
	for (FThreat& T : Threats)
		if (IsValid(T.Actor)) T.Actor->Destroy();
	Threats.Empty();

	if (IsValid(FriendlyKing)) FriendlyKing->Destroy();
	FriendlyKing = nullptr;

	if (IsValid(AxiomKing)) AxiomKing->Destroy();
	AxiomKing = nullptr;
}

// The maze/recap King level was replaced by the ALARM check tutorial; the old
// SetupMaze/SetupRecap flow (and main's tweaks to it) went with it.
void AKingTutorial::ResetBoardTint()
{
	if (!Board) return;
	for (int32 F = 0; F < Board->NumFiles; ++F)
		for (int32 R = 0; R < Board->NumRanks; ++R)
			Board->TintSquare(F, R, ((F + R) % 2 != 0) ? Board->LightSquareColor : Board->DarkSquareColor);
}

// ---------------------------------------------------------------------------
// Phase 1: king basics, then the danger overlay ("red = guarded")
// ---------------------------------------------------------------------------

void AKingTutorial::SetupBasics()
{
	SpawnPlayerPiece(0, 0);
	LastSafeFile = 0; LastSafeRank = 0;

	// Part A: free movement — three tokens anywhere (king walks one square at a time).
	// All three stay safe once the part-B threats appear, so the king is never
	// standing on a square that turns red under him.
	SpawnTargetAt(1, 1);
	SpawnTargetAt(3, 2);
	SpawnTargetAt(2, 4);

	ShowGuideSequence({
		TEXT("Your king moves one square in any direction."),
		TEXT("Grab the data tokens to power up the grid!"),
	});
}

void AKingTutorial::OnTargetCollected(int32 File, int32 Rank)
{
	if (CurrentPhase != ETutorialPhase::Phase1) return;

	// Part B: after the free-roam tokens, enemies appear and paint the danger
	// overlay. Spawned from OnTargetCollected so a new target exists before the
	// base class's "no targets left" phase-complete check runs.
	if (TargetsCollected == 3)
	{
		AddThreat(EChessPieceType::Rook, 5, 0);
		AddThreat(EChessPieceType::Bishop, 0, 3);
		RefreshDanger();
		SpawnTargetAt(4, 5);
		ShowGuideSequence({
			TEXT("Enemies! RED shows every square they can attack."),
			TEXT("If your king stands on red, they could capture him next turn. That's called CHECK."),
			TEXT("Get the last token without stepping on red!"),
		});
	}
	else if (TargetsCollected > 3)
	{
		ShowGuideMessage(TEXT("You made it through without a single alarm. That's king discipline!"));
	}
}

// ---------------------------------------------------------------------------
// Phase 2: CPR — Capture, Protect, Run
// ---------------------------------------------------------------------------

void AKingTutorial::SetupCPR()
{
	Step = 0;
	StartCPRStep();
}

void AKingTutorial::StartCPRStep()
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	ClearExtras();
	ResetBoardTint();
	bPhaseTransitionPending = false;

	switch (Step)
	{
	case 0: // C — capture the attacker
		SwitchPlayerPiece(EChessPieceType::King, 2, 2);
		AddThreat(EChessPieceType::Bishop, 3, 3);
		RefreshDanger();
		ShowGuideSequence({
			TEXT("CHECK! That bishop is one move from taking your king!"),
			TEXT("In chess you must escape check RIGHT NOW. There are 3 ways: C-P-R."),
			TEXT("C = CAPTURE. The bishop is close enough, take it!"),
		});
		break;

	case 1: // P — protect (block) with another piece
	{
		SwitchPlayerPiece(EChessPieceType::Rook, 1, 0);

		// A friendly king pinned on the back file, checked along its rank
		if (Board)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FriendlyKing = GetWorld()->SpawnActor<AChessPiece>(AChessPiece::StaticClass(),
				Board->GridToWorldLocation(0, 2, 5.f), FRotator::ZeroRotator, Params);
			if (FriendlyKing)
			{
				const FPieceMeshConfig* Config = PieceMeshes.Find(EChessPieceType::King);
				if (!Config) Config = &GetWhitePieceMeshConfig();
				FriendlyKing->Init(EChessPieceType::King, EChessColor::White, 0, 2, Config);
			}
		}
		FriendlyKingFile = 0; FriendlyKingRank = 2;

		AddThreat(EChessPieceType::Rook, 5, 2);
		RefreshDanger();
		ShowGuideSequence({
			TEXT("CHECK! This rook attacks from across the board. Too far to capture!"),
			TEXT("P = PROTECT: block the path with another piece."),
			TEXT("Slide your rook between them, like a shield!"),
		});
		break;
	}

	case 2: // R — run to a safe square
		SwitchPlayerPiece(EChessPieceType::King, 2, 2);
		AddThreat(EChessPieceType::Queen, 2, 5);
		RefreshDanger();
		ShowGuideSequence({
			TEXT("CHECK! Too far to capture, and nothing can block!"),
			TEXT("R = RUN: move your king to any square that isn't red."),
		});
		break;
	}

	UpdateHighlights();
}

// ---------------------------------------------------------------------------
// Phase 3: give check with every piece
// ---------------------------------------------------------------------------

namespace
{
	struct FGiveCheckStep
	{
		EChessPieceType Piece;
		int32 StartFile;
		int32 StartRank;
		const TCHAR* Intro;
	};

	// Enemy king sits on (3,4); every start position has at least one clean
	// one-move check available on a 6x6 board.
	const FGiveCheckStep GGiveCheckSteps[] = {
		{ EChessPieceType::Rook,   0, 0, TEXT("Now flip it: YOU give the check! Move your rook so its path hits AXIOM's king.") },
		{ EChessPieceType::Bishop, 1, 0, TEXT("Now the bishop! Check the king on a diagonal.") },
		{ EChessPieceType::Knight, 0, 1, TEXT("Luna's turn! One L-hop to check the king.") },
		{ EChessPieceType::Queen,  0, 0, TEXT("The queen can check ANY way. Pick your path!") },
		{ EChessPieceType::Pawn,   2, 1, TEXT("Even the pawn can check! Pawns hit diagonally. March up!") },
	};
	constexpr int32 NumGiveCheckSteps = 5;
}

void AKingTutorial::SetupGiveCheck()
{
	Step = 0;
	SpawnAxiomKing(3, 4);
	StartGiveCheckStep();
}

void AKingTutorial::StartGiveCheckStep()
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	bPhaseTransitionPending = false;

	const FGiveCheckStep& S = GGiveCheckSteps[Step];
	SwitchPlayerPiece(S.Piece, S.StartFile, S.StartRank);
	ShowGuideMessage(S.Intro);
	UpdateHighlights();
}

void AKingTutorial::SpawnAxiomKing(int32 File, int32 Rank)
{
	if (!Board) return;
	if (IsValid(AxiomKing)) AxiomKing->Destroy();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AxiomKing = GetWorld()->SpawnActor<AChessPiece>(AChessPiece::StaticClass(),
		Board->GridToWorldLocation(File, Rank, 5.f), FRotator::ZeroRotator, Params);
	if (AxiomKing)
		AxiomKing->Init(EChessPieceType::King, EChessColor::Black, File, Rank, &GetBlackRivalMeshConfig());
	AxiomFile = File;
	AxiomRank = Rank;
}

// ---------------------------------------------------------------------------
// Boss: AXIOM'S SHIELD — five checks, five escapes
// ---------------------------------------------------------------------------

void AKingTutorial::SetupBoss()
{
	ChecksGiven = 0;
	SwitchPlayerPiece(EChessPieceType::Queen, 0, 1);
	SpawnAxiomKing(4, 4);
	RefreshDanger(); // red = squares AXIOM's king guards — park your queen there and lose her
	ShowGuideSequence({
		FString::Printf(TEXT("AXIOM: \"Alarms can't stop me!\" Check his king %d times to win!"), BossChecksToWin),
		TEXT("You can't capture a king. No one can! Threaten him, over and over."),
		TEXT("The red squares are HIS reach. Leave your queen there and he'll capture her!"),
	});
}

void AKingTutorial::AxiomEscape()
{
	if (!Board || !IsValid(AxiomKing)) return;

	// What the player's queen attacks right now — AXIOM must end up off it.
	// His own body must not block the ray for this computation, or the square
	// directly behind him on the checking line would look safe.
	bRaysIgnoreAxiom = true;
	TSet<int32> QueenAttacks;
	AppendAttacks(TaughtPiece, PlayerFile, PlayerRank, QueenAttacks);
	bRaysIgnoreAxiom = false;

	// Try P (block) on odd escapes: drop a firewall fragment on the checking
	// line. Only possible when there's an empty square between queen and king.
	const bool bTryBlock = (ChecksGiven % 2 == 0);
	if (bTryBlock)
	{
		const int32 FD = AxiomFile - PlayerFile;
		const int32 RD = AxiomRank - PlayerRank;
		const int32 Dist = FMath::Max(FMath::Abs(FD), FMath::Abs(RD));
		if (Dist > 1)
		{
			const int32 FS = FMath::Sign(FD), RS = FMath::Sign(RD);
			const int32 BlockF = AxiomFile - FS, BlockR = AxiomRank - RS;
			SpawnWallAt(BlockF, BlockR);
			RefreshDanger();
			ShowGuideSequence({
				FString::Printf(TEXT("CHECK %d of %d! %d more to win!"), ChecksGiven, BossChecksToWin, BossChecksToWin - ChecksGiven),
				TEXT("AXIOM blocks it. That's P, Protect!"),
				TEXT("He knows C-P-R too..."),
			});
			return;
		}
	}

	// R (run): step to the adjacent square that's off the queen's lines,
	// preferring distance from her.
	int32 BestF = AxiomFile, BestR = AxiomRank, BestScore = -1;
	for (int32 DF = -1; DF <= 1; ++DF)
	{
		for (int32 DR = -1; DR <= 1; ++DR)
		{
			if (DF == 0 && DR == 0) continue;
			const int32 NF = AxiomFile + DF, NR = AxiomRank + DR;
			if (!Board->IsValidSquare(NF, NR)) continue;
			if (QueenAttacks.Contains(PackPos(NF, NR))) continue;
			if (WallActors.Contains(PackPos(NF, NR))) continue;
			if (NF == PlayerFile && NR == PlayerRank) continue;
			const int32 Score = FMath::Abs(NF - PlayerFile) + FMath::Abs(NR - PlayerRank);
			if (Score > BestScore) { BestScore = Score; BestF = NF; BestR = NR; }
		}
	}

	if (BestScore >= 0)
	{
		AxiomFile = BestF; AxiomRank = BestR;
		Board->GlideActorToSquare(AxiomKing, BestF, BestR, 5.f);
		RefreshDanger();
		ShowGuideSequence({
			FString::Printf(TEXT("CHECK %d of %d! %d more to win!"), ChecksGiven, BossChecksToWin, BossChecksToWin - ChecksGiven),
			TEXT("AXIOM runs away. That's R, Run!"),
			TEXT("Keep chasing him!"),
		});
	}
	else
	{
		// Nowhere safe to run and no block possible — narratively this is the
		// moment the player has already won the argument.
		RefreshDanger();
		ShowGuideMessage(TEXT("AXIOM: \"...no safe squares?!\" Interesting. Remember this feeling, cadet."));
	}
}

// ---------------------------------------------------------------------------
// Threats & the danger overlay
// ---------------------------------------------------------------------------

void AKingTutorial::AddThreat(EChessPieceType Type, int32 File, int32 Rank)
{
	if (!Board) return;

	FThreat T;
	T.Type = Type; T.File = File; T.Rank = Rank;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T.Actor = GetWorld()->SpawnActor<AChessPiece>(AChessPiece::StaticClass(),
		Board->GridToWorldLocation(File, Rank, 5.f), FRotator::ZeroRotator, Params);
	if (T.Actor)
	{
		// Always the guaranteed-black look, not PieceMeshes.Find(Type) — TaughtPiece
		// isn't stable here (SwitchPlayerPiece reassigns it as the hero swaps pieces
		// through the level), so a "same type as the hero" check only ever matched by
		// accident. PieceMeshes is realistically only ever configured for whatever
		// piece the hero currently holds anyway, never a fixed set of threat types.
		T.Actor->Init(Type, EChessColor::Black, File, Rank, &GetBlackRivalMeshConfig());
	}

	Threats.Add(T);
}

void AKingTutorial::RefreshDanger()
{
	DangerSquares.Empty();
	if (!Board) return;

	for (const FThreat& T : Threats)
		AppendAttacks(T.Type, T.File, T.Rank, DangerSquares);

	// AXIOM's king guards his neighborhood (phase 3 uses him as a target only;
	// the boss shows his reach so the queen learns to keep her distance)
	if (IsValid(AxiomKing) && CurrentPhase == ETutorialPhase::Boss)
		AppendAttacks(EChessPieceType::King, AxiomFile, AxiomRank, DangerSquares);

	ResetBoardTint();
	for (int32 Packed : DangerSquares)
	{
		// PackPos is File * 100 + Rank (see TutorialManager)
		Board->TintSquare(Packed / 100, Packed % 100, DangerColor);
	}
}

bool AKingTutorial::IsAttackBlocked(int32 File, int32 Rank) const
{
	const int32 Key = PackPos(File, Rank);
	if (WallActors.Contains(Key)) return true;
	if (IsValid(FriendlyKing) && File == FriendlyKingFile && Rank == FriendlyKingRank) return true;
	// AXIOM's own king doesn't block rays when we're computing where he could
	// escape to — in chess, a king backing up along the checking line is still
	// in check, so the ray must extend through him.
	if (!bRaysIgnoreAxiom && IsValid(AxiomKing) && File == AxiomFile && Rank == AxiomRank) return true;
	// Same rule for the player's king: it can't shield the squares behind it
	// from a checking ray (running backward along the line is still check).
	// A non-king player piece (the CPR rook) IS a legitimate blocker.
	if (TaughtPiece != EChessPieceType::King &&
		PlayerPiece && File == PlayerFile && Rank == PlayerRank) return true;
	for (const FThreat& T : Threats)
		if (T.File == File && T.Rank == Rank) return true;
	return false;
}

void AKingTutorial::AppendAttacks(EChessPieceType Type, int32 File, int32 Rank, TSet<int32>& Out) const
{
	if (!Board) return;

	auto AddIfValid = [&](int32 F, int32 R)
	{
		if (Board->IsValidSquare(F, R)) Out.Add(PackPos(F, R));
	};

	auto Ray = [&](int32 FS, int32 RS)
	{
		int32 F = File + FS, R = Rank + RS;
		while (Board->IsValidSquare(F, R))
		{
			Out.Add(PackPos(F, R));
			if (IsAttackBlocked(F, R)) break; // attacks include the blocker's square, not past it
			F += FS; R += RS;
		}
	};

	switch (Type)
	{
	case EChessPieceType::Pawn:
		// Threat pawns are black: they attack diagonally DOWN the board.
		// (The player's white pawn is handled in PlayerAttacks.)
		AddIfValid(File - 1, Rank - 1);
		AddIfValid(File + 1, Rank - 1);
		break;
	case EChessPieceType::Knight:
		AddIfValid(File + 2, Rank + 1); AddIfValid(File + 2, Rank - 1);
		AddIfValid(File - 2, Rank + 1); AddIfValid(File - 2, Rank - 1);
		AddIfValid(File + 1, Rank + 2); AddIfValid(File + 1, Rank - 2);
		AddIfValid(File - 1, Rank + 2); AddIfValid(File - 1, Rank - 2);
		break;
	case EChessPieceType::King:
		for (int32 DF = -1; DF <= 1; ++DF)
			for (int32 DR = -1; DR <= 1; ++DR)
				if (DF != 0 || DR != 0) AddIfValid(File + DF, Rank + DR);
		break;
	case EChessPieceType::Rook:
		Ray(1, 0); Ray(-1, 0); Ray(0, 1); Ray(0, -1);
		break;
	case EChessPieceType::Bishop:
		Ray(1, 1); Ray(1, -1); Ray(-1, 1); Ray(-1, -1);
		break;
	case EChessPieceType::Queen:
		Ray(1, 0); Ray(-1, 0); Ray(0, 1); Ray(0, -1);
		Ray(1, 1); Ray(1, -1); Ray(-1, 1); Ray(-1, -1);
		break;
	}
}

bool AKingTutorial::PlayerAttacks(int32 File, int32 Rank) const
{
	if (TaughtPiece == EChessPieceType::Pawn)
	{
		// White pawn: attacks diagonally UP the board
		return FMath::Abs(File - PlayerFile) == 1 && Rank - PlayerRank == 1;
	}
	TSet<int32> Attacks;
	AppendAttacks(TaughtPiece, PlayerFile, PlayerRank, Attacks);
	return Attacks.Contains(PackPos(File, Rank));
}

// ---------------------------------------------------------------------------
// Movement rules & move reactions
// ---------------------------------------------------------------------------

bool AKingTutorial::IsSquareBlocked(int32 File, int32 Rank) const
{
	if (IsValid(FriendlyKing) && File == FriendlyKingFile && Rank == FriendlyKingRank) return true;
	if (IsValid(AxiomKing) && File == AxiomFile && Rank == AxiomRank) return true;
	for (const FThreat& T : Threats)
		if (T.File == File && T.Rank == Rank) return true;
	return Super::IsSquareBlocked(File, Rank);
}

bool AKingTutorial::IsMoveAllowed(int32 FromFile, int32 FromRank, int32 ToFile, int32 ToRank) const
{
	// Landing on AXIOM's king or the friendly king is never allowed
	if (IsValid(AxiomKing) && ToFile == AxiomFile && ToRank == AxiomRank) return false;
	if (IsValid(FriendlyKing) && ToFile == FriendlyKingFile && ToRank == FriendlyKingRank) return false;

	// Landing on a threat piece is only allowed in the CPR Capture step
	const bool bOnThreat = [&]
	{
		for (const FThreat& T : Threats)
			if (T.File == ToFile && T.Rank == ToRank) return true;
		return false;
	}();
	const bool bCaptureStep = (CurrentPhase == ETutorialPhase::Phase2 && Step == 0);
	if (bOnThreat && !bCaptureStep) return false;

	if (CurrentPhase == ETutorialPhase::Phase2)
	{
		switch (Step)
		{
		case 0: // C: the only lesson move is taking the bishop
			if (!(bOnThreat)) return false;
			break;
		case 1: // P: the only lesson move is stepping into the attack line
			if (!(ToFile == 1 && ToRank == 2)) return false;
			break;
		case 2: // R: never let the king run INTO another attacked square
			if (DangerSquares.Contains(PackPos(ToFile, ToRank))) return false;
			break;
		}
	}

	// Boss: the queen may enter AXIOM's guarded squares — and pay for it
	// (that's the C lesson from the other side). Phase 1 also allows stepping
	// on red: the alarm bounce-back IS the lesson.
	return Super::IsMoveAllowed(FromFile, FromRank, ToFile, ToRank);
}

void AKingTutorial::AfterPlayerMove(int32 File, int32 Rank)
{
	switch (CurrentPhase)
	{
	case ETutorialPhase::Phase1:
	{
		if (DangerSquares.Contains(PackPos(File, Rank)))
		{
			// The alarm moment the whole level is named after
			ShowGuideMessage(TEXT("ALARM! CHECK! Back to safety!"));
			bPhaseTransitionPending = true;
			GetWorldTimerManager().SetTimer(StepTimer,
				FTimerDelegate::CreateUObject(this, &AKingTutorial::BounceBack), 1.5f, false);
		}
		else
		{
			LastSafeFile = File; LastSafeRank = Rank;
		}
		break;
	}

	case ETutorialPhase::Phase2:
	{
		switch (Step)
		{
		case 0: // captured the bishop?
			for (int32 i = Threats.Num() - 1; i >= 0; --i)
			{
				if (Threats[i].File == File && Threats[i].Rank == Rank)
				{
					if (IsValid(Threats[i].Actor)) Threats[i].Actor->StartCaptureFling();
					Threats.RemoveAt(i);
					Step = 1;
					RefreshDanger();
					ShowGuideSequence({
						TEXT("CAPTURED! The attacker is gone, so the check is gone."),
					});
					bPhaseTransitionPending = true;
					GetWorldTimerManager().SetTimer(StepTimer,
						FTimerDelegate::CreateUObject(this, &AKingTutorial::StartCPRStep), GuideLineSeconds * 2.f, false);
				}
			}
			break;

		case 1: // rook reached the block square (the only legal move)
			Step = 2;
			RefreshDanger(); // the ray now stops at the rook — watch the red drain away
			ShowGuideSequence({
				TEXT("BLOCKED! Your rook stands in the attack's path."),
				TEXT("See the red disappear behind it? Your king is safe."),
			});
			bPhaseTransitionPending = true;
			GetWorldTimerManager().SetTimer(StepTimer,
				FTimerDelegate::CreateUObject(this, &AKingTutorial::StartCPRStep), GuideLineSeconds * 3.f, false);
			break;

		case 2: // king ran somewhere safe (unsafe squares were disallowed)
			CompletePhaseSteps({
				TEXT("Safe! That's all three escapes: Capture, Protect, Run."),
				TEXT("Remember: in chess you can NEVER ignore check. Fix it first, always."),
			});
			break;
		}
		break;
	}

	case ETutorialPhase::Phase3:
	{
		if (PlayerAttacks(AxiomFile, AxiomRank))
		{
			Step++;
			if (Step >= NumGiveCheckSteps)
			{
				CompletePhaseSteps({
					TEXT("CHECK, five ways!"),
					TEXT("Every piece can do it."),
					TEXT("AXIOM has noticed you..."),
				});
			}
			else
			{
				ShowGuideMessage(TEXT("CHECK! The alarm rings. Next piece!"));
				bPhaseTransitionPending = true;
				GetWorldTimerManager().SetTimer(StepTimer,
					FTimerDelegate::CreateUObject(this, &AKingTutorial::StartGiveCheckStep), 2.0f, false);
			}
		}
		else
		{
			// Miss: reset the piece so every attempt starts from the same puzzle
			ShowGuideSequence({
				TEXT("No check yet..."),
				TEXT("Pick a square that can HIT the king!"),
			});
			const FGiveCheckStep& S = GGiveCheckSteps[Step];
			SpawnPlayerPiece(S.StartFile, S.StartRank);
		}
		break;
	}

	case ETutorialPhase::Boss:
	{
		const bool bCheck = PlayerAttacks(AxiomFile, AxiomRank);
		if (bCheck)
		{
			ChecksGiven++;
			if (ChecksGiven >= BossChecksToWin)
			{
				// The point of the whole level: check is not victory.
				CurrentPhase = ETutorialPhase::Complete;
				ShowGuideSequence({
					TEXT("Five checks, and AXIOM escaped every single one."),
					TEXT("That's the secret: check can always be answered with C-P-R."),
					TEXT("CHECKMATE is a check with NO escape. That's how you win. LEVEL 7: THE FIREWALL!"),
				});
				OnLevelComplete.Broadcast();
				TaughtPiece = EChessPieceType::King; // reward bookkeeping is keyed on the level's piece, not the boss queen
				GrantBossVictoryRewards();
				return;
			}
			AxiomEscape(); // its guide sequence opens with the check count
			return;
		}

		// Hung the queen inside AXIOM's reach? He takes it — C works for him too.
		if (DangerSquares.Contains(PackPos(File, Rank)))
		{
			ShowGuideSequence({
				TEXT("He took your queen! That's C, Capture!"),
				TEXT("Don't stop on red squares."),
				TEXT("Here's a new queen. Try again!"),
			});
			if (PlayerPiece) PlayerPiece->StartCaptureFling();
			PlayerPiece = nullptr;
			bPhaseTransitionPending = true;
			GetWorldTimerManager().SetTimer(StepTimer,
				FTimerDelegate::CreateUObject(this, &AKingTutorial::BounceBack), 1.5f, false);
		}
		break;
	}

	default:
		break;
	}
}

void AKingTutorial::BounceBack()
{
	if (CurrentPhase == ETutorialPhase::Boss)
	{
		// Fresh queen at the start square
		SpawnPlayerPiece(0, 0);
	}
	else
	{
		MovePlayerTo(LastSafeFile, LastSafeRank);
	}
	bPhaseTransitionPending = false;
	UpdateHighlights();
}

// ---------------------------------------------------------------------------
// Phase completion plumbing
// ---------------------------------------------------------------------------

void AKingTutorial::CompletePhaseSteps(TArray<FString> Lines)
{
	// Let the wrap-up lines finish before the base class's phase-advance
	// message replaces them.
	bStepsDone = true;
	const float Delay = GuideLineSeconds * Lines.Num();
	ShowGuideSequence(MoveTemp(Lines));
	bPhaseTransitionPending = true;
	GetWorldTimerManager().SetTimer(StepTimer,
		FTimerDelegate::CreateUObject(this, &AKingTutorial::OnPhaseComplete), Delay, false);
}

void AKingTutorial::OnPhaseComplete()
{
	// The boss victory is handled inline in AfterPlayerMove (custom message +
	// reward bookkeeping) — the base Boss branch must not run on top of it.
	if (CurrentPhase == ETutorialPhase::Boss)
		return;

	// Phases 2 and 3 have no collectible targets, so the base class's
	// "no targets left" check fires on every move — only advance when the
	// step flow says the phase is actually done.
	if ((CurrentPhase == ETutorialPhase::Phase2 || CurrentPhase == ETutorialPhase::Phase3) && !bStepsDone)
		return;

	Super::OnPhaseComplete();
}

void AKingTutorial::SwitchPlayerPiece(EChessPieceType Type, int32 File, int32 Rank)
{
	TaughtPiece = Type;
	SpawnPlayerPiece(File, Rank);
	LastSafeFile = File;
	LastSafeRank = Rank;
}
