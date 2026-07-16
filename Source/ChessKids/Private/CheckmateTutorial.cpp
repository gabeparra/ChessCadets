#include "CheckmateTutorial.h"
#include "TutorialBoard.h"
#include "ChessPiece.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACheckmateTutorial::ACheckmateTutorial()
{
	// No new piece this level — the concept is the lesson. King keeps the level
	// themed on him; the lesson slot is the dedicated checkmate index.
	TaughtPiece = EChessPieceType::King;
	LessonIndexOverride = 6;
}

// ---------------------------------------------------------------------------
// Scripted positions
// ---------------------------------------------------------------------------

namespace
{
	struct FScriptedWhite { EChessPieceType Type; int32 F, R; };

	// Phase 1 rounds: the kid plays the BLACK king.
	// Round 0: rook check with escapes — feel CHECK.
	// Round 1: ladder position, zero escapes — feel CHECKMATE by failing.
	struct FHuntedRound
	{
		int32 KingF, KingR;
		FScriptedWhite White[2];
		int32 NumWhite;
	};
	const FHuntedRound GHunted[] = {
		{ 3, 4, { { EChessPieceType::Rook, 0, 4 } }, 1 },
		{ 5, 5, { { EChessPieceType::Rook, 0, 5 }, { EChessPieceType::Rook, 0, 4 } }, 2 },
	};

	// Phase 3 step 0: stalemate, felt as the black king (no check, no moves)
	constexpr int32 StaleKingF = 5, StaleKingR = 5;
	const FScriptedWhite GStaleWhite = { EChessPieceType::Queen, 3, 4 };

	struct FBossPuzzle
	{
		int32 KingF, KingR;
		FScriptedWhite Support[2]; // static white pieces (King entry = white king)
		int32 NumSupport;
		EChessPieceType Mover;     // the piece the player moves
		int32 MoverF, MoverR;
		int32 Walls[3][2];         // AXIOM's own firewall blocks; -1 = unused
	};
	const FBossPuzzle GBoss[] = {
		{ 5, 5, { { EChessPieceType::King, 4, 3 } }, 1, EChessPieceType::Queen, 1, 1, { {-1,-1}, {-1,-1}, {-1,-1} } },
		{ 2, 5, { { EChessPieceType::Rook, 5, 4 } }, 1, EChessPieceType::Rook,  0, 0, { {-1,-1}, {-1,-1}, {-1,-1} } },
		{ 5, 3, { { EChessPieceType::King, 3, 3 } }, 1, EChessPieceType::Queen, 4, 0, { {-1,-1}, {-1,-1}, {-1,-1} } },
		{ 3, 5, { }, 0, EChessPieceType::Queen, 0, 0, { {2,4}, {3,4}, {4,4} } },
		{ 0, 5, { { EChessPieceType::King, 2, 4 } }, 1, EChessPieceType::Queen, 1, 0, { {-1,-1}, {-1,-1}, {-1,-1} } },
	};
	constexpr int32 NumBoss = 5;
}

// ---------------------------------------------------------------------------
// Phase routing
// ---------------------------------------------------------------------------

void ACheckmateTutorial::SetupPhase(ETutorialPhase Phase)
{
	Super::SetupPhase(Phase);

	GetWorldTimerManager().ClearTimer(StepTimer);
	ClearPieces();
	Step = 0;
	bStepsDone = false;
	SelectedPiece = INDEX_NONE;
	bKingSelected = false;
	FailedTries = 0;
	ResetBoardTint();
	if (Board) Board->ClearHighlights();

	switch (Phase)
	{
	case ETutorialPhase::Phase1: SetupHuntedKing(); break;
	case ETutorialPhase::Phase2: SetupLadderMate(); break;
	case ETutorialPhase::Phase3: SetupStalemate();  break;
	case ETutorialPhase::Boss:   SetupBoss();       break;
	case ETutorialPhase::Complete: break;
	}
}

void ACheckmateTutorial::ClearPieces()
{
	for (FWhitePiece& P : WhitePieces)
		if (IsValid(P.Actor)) P.Actor->Destroy();
	WhitePieces.Empty();

	if (IsValid(BlackKing)) BlackKing->Destroy();
	BlackKing = nullptr;

	for (auto& Pair : WallActors)
		if (IsValid(Pair.Value)) Pair.Value->Destroy();
	WallActors.Empty();
}

void ACheckmateTutorial::ResetBoardTint()
{
	if (!Board) return;
	for (int32 F = 0; F < Board->NumFiles; ++F)
		for (int32 R = 0; R < Board->NumRanks; ++R)
			Board->TintSquare(F, R, ((F + R) % 2 != 0) ? Board->LightSquareColor : Board->DarkSquareColor);
}

void ACheckmateTutorial::PaintDanger()
{
	ResetBoardTint();
	if (!Board) return;
	for (int32 Packed : WhiteAttacks())
		Board->TintSquare(Packed / 100, Packed % 100, DangerColor);
}

// ---------------------------------------------------------------------------
// Board authoring
// ---------------------------------------------------------------------------

void ACheckmateTutorial::PlaceBlackKing(int32 File, int32 Rank)
{
	if (!Board) return;
	if (IsValid(BlackKing)) BlackKing->Destroy();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BlackKing = GetWorld()->SpawnActor<AChessPiece>(AChessPiece::StaticClass(),
		Board->GridToWorldLocation(File, Rank, 5.f), FRotator::ZeroRotator, Params);
	if (BlackKing)
		BlackKing->Init(EChessPieceType::King, EChessColor::Black, File, Rank, &GetBlackRivalMeshConfig());
	BKFile = File;
	BKRank = Rank;
}

void ACheckmateTutorial::PlaceWhite(EChessPieceType Type, int32 File, int32 Rank, bool bPlayerControlled)
{
	if (!Board) return;

	FWhitePiece P;
	P.Type = Type; P.File = File; P.Rank = Rank; P.bPlayerControlled = bPlayerControlled;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.Actor = GetWorld()->SpawnActor<AChessPiece>(AChessPiece::StaticClass(),
		Board->GridToWorldLocation(File, Rank, 5.f), FRotator::ZeroRotator, Params);
	if (P.Actor)
	{
		// PieceMeshes is realistically only ever configured for TaughtPiece (King
		// here), but PlaceWhite spawns Rooks/Queens/etc. — never a King — so
		// PieceMeshes.Find(Type) is null for essentially every call. Fall back to a
		// guaranteed real white piece instead of an unstyled default mesh.
		const FPieceMeshConfig* Config = PieceMeshes.Find(Type);
		if (!Config) Config = &GetWhitePieceMeshConfig();
		P.Actor->Init(Type, EChessColor::White, File, Rank, Config);
	}

	WhitePieces.Add(P);
}

int32 ACheckmateTutorial::FindWhiteAt(int32 File, int32 Rank) const
{
	for (int32 i = 0; i < WhitePieces.Num(); ++i)
		if (WhitePieces[i].File == File && WhitePieces[i].Rank == Rank)
			return i;
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------
// Chess evaluation
// ---------------------------------------------------------------------------

bool ACheckmateTutorial::IsRayBlockedAt(int32 File, int32 Rank, int32 IgnoreIndex) const
{
	if (WallActors.Contains(PackPos(File, Rank))) return true;
	for (int32 i = 0; i < WhitePieces.Num(); ++i)
		if (i != IgnoreIndex && WhitePieces[i].File == File && WhitePieces[i].Rank == Rank)
			return true;
	// The black king is deliberately transparent: rays extend through him so
	// the square behind him on a checking line still reads as attacked.
	return false;
}

void ACheckmateTutorial::AppendPieceAttacks(EChessPieceType Type, int32 File, int32 Rank, TSet<int32>& Out, int32 IgnoreIndex) const
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
			if (IsRayBlockedAt(F, R, IgnoreIndex)) break;
			F += FS; R += RS;
		}
	};

	switch (Type)
	{
	case EChessPieceType::Pawn:
		AddIfValid(File - 1, Rank + 1); AddIfValid(File + 1, Rank + 1);
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

TSet<int32> ACheckmateTutorial::WhiteAttacks(int32 IgnoreIndex) const
{
	TSet<int32> Out;
	for (int32 i = 0; i < WhitePieces.Num(); ++i)
		if (i != IgnoreIndex)
			AppendPieceAttacks(WhitePieces[i].Type, WhitePieces[i].File, WhitePieces[i].Rank, Out, IgnoreIndex);
	return Out;
}

bool ACheckmateTutorial::IsBlackKingInCheck() const
{
	return WhiteAttacks().Contains(PackPos(BKFile, BKRank));
}

TArray<FIntPoint> ACheckmateTutorial::BlackKingMoves() const
{
	TArray<FIntPoint> Moves;
	if (!Board) return Moves;

	const TSet<int32> Attacked = WhiteAttacks();
	for (int32 DF = -1; DF <= 1; ++DF)
	{
		for (int32 DR = -1; DR <= 1; ++DR)
		{
			if (DF == 0 && DR == 0) continue;
			const int32 F = BKFile + DF, R = BKRank + DR;
			if (!Board->IsValidSquare(F, R)) continue;
			if (WallActors.Contains(PackPos(F, R))) continue;

			const int32 WhiteIdx = FindWhiteAt(F, R);
			if (WhiteIdx != INDEX_NONE)
			{
				// Capture is legal only if the piece isn't the white king and
				// isn't defended by anyone else.
				if (WhitePieces[WhiteIdx].Type == EChessPieceType::King) continue;
				if (WhiteAttacks(WhiteIdx).Contains(PackPos(F, R))) continue;
				Moves.Add(FIntPoint(F, R));
			}
			else if (!Attacked.Contains(PackPos(F, R)))
			{
				Moves.Add(FIntPoint(F, R));
			}
		}
	}
	return Moves;
}

ACheckmateTutorial::EPositionVerdict ACheckmateTutorial::EvaluatePosition() const
{
	const bool bCheck = IsBlackKingInCheck();
	const bool bHasMoves = BlackKingMoves().Num() > 0;
	if (bCheck) return bHasMoves ? EPositionVerdict::Check : EPositionVerdict::Checkmate;
	return bHasMoves ? EPositionVerdict::Check /*ongoing game — unused*/ : EPositionVerdict::Stalemate;
}

// ---------------------------------------------------------------------------
// Phase 1: You're the hunted king — feel check, then feel checkmate
// ---------------------------------------------------------------------------

void ACheckmateTutorial::SetupHuntedKing()
{
	Step = 0;
	LadderFails = 0; // fresh level, fresh coaching state
	StartHuntedRound();
	ShowGuideSequence({
		TEXT("This time, YOU are the hunted king!"),
		TEXT("The rook has you in CHECK! The red squares show its reach."),
		TEXT("Click your king, then click a safe square. RUN!"),
	});
}

void ACheckmateTutorial::StartHuntedRound()
{
	ClearPieces();
	bKingSelected = false;
	FailedTries = 0;
	bPhaseTransitionPending = false;
	if (Board) Board->ClearHighlights();

	const FHuntedRound& R = GHunted[Step];
	PlaceBlackKing(R.KingF, R.KingR);
	for (int32 i = 0; i < R.NumWhite; ++i)
		PlaceWhite(R.White[i].Type, R.White[i].F, R.White[i].R);
	PaintDanger();
}

void ACheckmateTutorial::HandleBlackKingClick(int32 File, int32 Rank)
{
	// Select the king: glow his legal (safe) squares — in a mate/stalemate
	// position nothing glows, and that absence IS the lesson
	if (File == BKFile && Rank == BKRank)
	{
		bKingSelected = true;
		if (Board)
		{
			Board->ClearHighlights();
			Board->SelectSquare(BKFile, BKRank);
			Board->ShowLegalMoves(BlackKingMoves());
		}
		if (BlackKingMoves().Num() == 0)
			ShowGuideMessage(TEXT("...no squares glow. Try clicking around. Is ANY square safe?"));
		return;
	}

	if (!bKingSelected)
	{
		ShowGuideMessage(TEXT("Click your king first!"));
		return;
	}

	const bool bStalemateRound = (CurrentPhase == ETutorialPhase::Phase3);

	if (BlackKingMoves().Contains(FIntPoint(File, Rank)))
	{
		// Only reachable in the escapable round
		BKFile = File; BKRank = Rank;
		if (Board && IsValid(BlackKing)) Board->GlideActorToSquare(BlackKing, File, Rank, 5.f);
		if (Board) Board->ClearHighlights();

		if (!IsBlackKingInCheck())
		{
			Step++;
			ShowGuideSequence({
				TEXT("You escaped! When the king CAN get away, it's only CHECK."),
				TEXT("The game goes on. But what if there were NOWHERE to run..."),
			});
			bPhaseTransitionPending = true;
			GetWorldTimerManager().SetTimer(StepTimer,
				FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::StartHuntedRound),
				GuideLineSeconds * 2.f, false);
		}
		return;
	}

	// Clicked an unreachable / guarded square
	FailedTries++;

	if (FailedTries < 3)
	{
		ShowGuideMessage(bStalemateRound
			? TEXT("Can't go there. Keep trying, find ANY legal move...")
			: TEXT("That square is guarded! Try another..."));
		return;
	}

	// Third failed try: name what the kid just experienced
	if (bStalemateRound)
	{
		// Phase 3 step 0 → hand over to the queen trap round
		Step = 1;
		ShowGuideSequence({
			TEXT("Nothing works! But look: you're NOT in check."),
			TEXT("No check + no moves = STALEMATE. Nobody wins. It's a DRAW."),
			TEXT("When YOU'RE winning, stalemate is a trap. Never leave him move-less without check!"),
		});
		bPhaseTransitionPending = true;
		GetWorldTimerManager().SetTimer(StepTimer,
			FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::StartTrapRound),
			GuideLineSeconds * 3.f, false);
	}
	else if (Step == 1)
	{
		// Phase 1 round 2 — checkmate discovered
		CompletePhaseSteps({
			TEXT("Every square is guarded. There IS no escape."),
			TEXT("Check + no way out = CHECKMATE. The game ends right there."),
			TEXT("That's how chess is won. Now YOU learn to build one."),
		});
	}
}

// ---------------------------------------------------------------------------
// Phase 2: Ladder mate
// ---------------------------------------------------------------------------

void ACheckmateTutorial::SetupLadderMate()
{
	PlaceBlackKing(3, 3);
	PlaceWhite(EChessPieceType::Rook, 0, 0, true);
	PlaceWhite(EChessPieceType::Rook, 5, 0, true);

	ShowGuideSequence({
		TEXT("The LADDER MATE! It's the first checkmate every cadet learns."),
		TEXT("One rook guards a row like a wall. The other pushes the king up."),
		TEXT("Walk him to the top edge! Click a rook, then click where it goes."),
	});

	// Struggling? After enough failures the board coaches move by move.
	if (LadderFails >= FailsBeforeHints)
		ShowLadderHint();
}

// Search every rook move, simulate it, and glow the best one. Preference
// order: instant mate > a safe check (pushes the ladder) > building the wall
// on the row under the king > rescuing a rook the king is about to eat.
// Stalemates and moves that hang a rook are never suggested.
void ACheckmateTutorial::ShowLadderHint()
{
	int32 BestPiece = INDEX_NONE;
	FIntPoint BestDest(0, 0);
	int32 BestScore = 0;

	for (int32 i = 0; i < WhitePieces.Num(); ++i)
	{
		if (!WhitePieces[i].bPlayerControlled) continue;
		const int32 OrigF = WhitePieces[i].File, OrigR = WhitePieces[i].Rank;

		for (const FIntPoint& M : LegalMovesFor(WhitePieces[i]))
		{
			WhitePieces[i].File = M.X; WhitePieces[i].Rank = M.Y;

			int32 Score = 0;
			const EPositionVerdict V = EvaluatePosition();
			const TArray<FIntPoint> Replies = BlackKingMoves();

			bool bHangsARook = false;
			bool bForcedUp = true; // every reply pushes him toward the top edge
			for (const FIntPoint& K : Replies)
			{
				if (FindWhiteAt(K.X, K.Y) != INDEX_NONE) bHangsARook = true;
				if (K.Y <= BKRank) bForcedUp = false;
			}

			if (V == EPositionVerdict::Checkmate)
				Score = 1000;
			else if (V == EPositionVerdict::Stalemate || bHangsARook)
				Score = 0;
			else if (IsBlackKingInCheck())
				// A check only helps when the wall already exists so every
				// escape goes UP. Checking without the wall just shoos him
				// sideways or down — endless back-and-forth.
				Score = bForcedUp ? (500 + BKRank) : 5;
			else if (M.Y == BKRank - 1)
				Score = 300 + FMath::Abs(M.X - BKFile); // build the wall below, far = safer
			else
				Score = 10;

			if (Score > BestScore)
			{
				BestScore = Score;
				BestPiece = i;
				BestDest = M;
			}

			WhitePieces[i].File = OrigF; WhitePieces[i].Rank = OrigR;
		}
	}

	if (BestPiece == INDEX_NONE || !Board) return;

	Board->TintSquare(WhitePieces[BestPiece].File, WhitePieces[BestPiece].Rank, HintColor);
	Board->TintSquare(BestDest.X, BestDest.Y, HintColor);
	ShowGuideMessage(TEXT("Hint: try the GOLD move! Click the gold rook, then the gold square."));
}

void ACheckmateTutorial::ShowSelection()
{
	if (!Board) return;
	Board->ClearHighlights();
	if (SelectedPiece == INDEX_NONE) return;

	const FWhitePiece& P = WhitePieces[SelectedPiece];
	Board->SelectSquare(P.File, P.Rank);
	Board->ShowLegalMoves(LegalMovesFor(P));
}

TArray<FIntPoint> ACheckmateTutorial::LegalMovesFor(const FWhitePiece& P) const
{
	TArray<FIntPoint> Moves;
	if (!Board) return Moves;

	auto Ray = [&](int32 FS, int32 RS)
	{
		int32 F = P.File + FS, R = P.Rank + RS;
		while (Board->IsValidSquare(F, R))
		{
			// Own pieces, walls, and the enemy king all stop the ray; none of
			// them can be landed on (kings are never captured).
			if (WallActors.Contains(PackPos(F, R))) break;
			if (FindWhiteAt(F, R) != INDEX_NONE) break;
			if (F == BKFile && R == BKRank) break;
			Moves.Add(FIntPoint(F, R));
			F += FS; R += RS;
		}
	};

	if (P.Type == EChessPieceType::Rook || P.Type == EChessPieceType::Queen)
	{
		Ray(1, 0); Ray(-1, 0); Ray(0, 1); Ray(0, -1);
	}
	if (P.Type == EChessPieceType::Bishop || P.Type == EChessPieceType::Queen)
	{
		Ray(1, 1); Ray(1, -1); Ray(-1, 1); Ray(-1, -1);
	}
	return Moves;
}

void ACheckmateTutorial::MoveWhitePiece(int32 Index, int32 File, int32 Rank)
{
	FWhitePiece& P = WhitePieces[Index];
	P.File = File; P.Rank = Rank;
	if (Board && IsValid(P.Actor))
		Board->GlideActorToSquare(P.Actor, File, Rank, 5.f);
	MoveCount++;
}

void ACheckmateTutorial::HandleLadderClick(int32 File, int32 Rank)
{
	const int32 ClickedWhite = FindWhiteAt(File, Rank);
	if (ClickedWhite != INDEX_NONE && WhitePieces[ClickedWhite].bPlayerControlled)
	{
		SelectedPiece = ClickedWhite;
		ShowSelection();
		return;
	}

	if (SelectedPiece == INDEX_NONE)
	{
		ShowGuideMessage(TEXT("Click one of YOUR rooks first, then where it should go."));
		return;
	}

	if (!LegalMovesFor(WhitePieces[SelectedPiece]).Contains(FIntPoint(File, Rank)))
	{
		ShowGuideMessage(TEXT("A rook only slides in straight lines. Pick a glowing square!"));
		return;
	}

	MoveWhitePiece(SelectedPiece, File, Rank);
	SelectedPiece = INDEX_NONE;
	if (Board) Board->ClearHighlights();
	ResetBoardTint(); // clear any hint glow from last turn

	const EPositionVerdict V = EvaluatePosition();
	if (V == EPositionVerdict::Checkmate)
	{
		LadderFails = 0;
		CompletePhaseSteps({
			TEXT("CHECKMATE! Attacked, and nowhere to go. LOCKDOWN!"),
			TEXT("The ladder mate: wall, push, wall, push. You built it yourself."),
		});
		return;
	}
	if (V == EPositionVerdict::Stalemate)
	{
		LadderFails++;
		ShowGuideSequence({
			TEXT("Whoa, STALEMATE! He's NOT in check, but he has no moves."),
			TEXT("That's a DRAW, not a win. Always leave him a path... right into your trap."),
			TEXT("Let's try again."),
		});
		bPhaseTransitionPending = true;
		GetWorldTimerManager().SetTimer(StepTimer,
			FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::ResetPhase),
			GuideLineSeconds * 3.f, false);
		return;
	}

	BlackKingRespond();
}

void ACheckmateTutorial::BlackKingRespond()
{
	TArray<FIntPoint> Moves = BlackKingMoves();
	if (Moves.Num() == 0) return; // handled by the caller's verdict check

	// Grab an undefended rook if he can — that's the lesson about keeping the
	// ladder rooks far away from the king.
	for (const FIntPoint& M : Moves)
	{
		const int32 WhiteIdx = FindWhiteAt(M.X, M.Y);
		if (WhiteIdx != INDEX_NONE)
		{
			LadderFails++;
			if (IsValid(WhitePieces[WhiteIdx].Actor)) WhitePieces[WhiteIdx].Actor->StartCaptureFling();
			WhitePieces.RemoveAt(WhiteIdx);
			BKFile = M.X; BKRank = M.Y;
			if (Board && IsValid(BlackKing)) Board->GlideActorToSquare(BlackKing, M.X, M.Y, 5.f);

			ShowGuideSequence({
				TEXT("He grabbed your rook! It stood too close, with no one guarding it."),
				TEXT("Keep the rooks far along their rows. The wall works from ANY distance."),
			});
			bPhaseTransitionPending = true;
			GetWorldTimerManager().SetTimer(StepTimer,
				FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::ResetPhase),
				GuideLineSeconds * 2.f, false);
			return;
		}
	}

	// Otherwise: flee downward, away from the edge he's being walked toward
	FIntPoint Best = Moves[0];
	for (const FIntPoint& M : Moves)
		if (M.Y < Best.Y || (M.Y == Best.Y && FMath::Abs(M.X - 2) < FMath::Abs(Best.X - 2)))
			Best = M;

	BKFile = Best.X; BKRank = Best.Y;
	if (Board && IsValid(BlackKing)) Board->GlideActorToSquare(BlackKing, Best.X, Best.Y, 5.f);

	// Coaching mode: after enough failed attempts, glow the next best move
	if (LadderFails >= FailsBeforeHints)
		ShowLadderHint();
}

// ---------------------------------------------------------------------------
// Phase 3: Stalemate — feel it, then dodge it
// ---------------------------------------------------------------------------

void ACheckmateTutorial::SetupStalemate()
{
	Step = 0;
	bKingSelected = false;
	FailedTries = 0;

	PlaceBlackKing(StaleKingF, StaleKingR);
	PlaceWhite(GStaleWhite.Type, GStaleWhite.F, GStaleWhite.R);
	PaintDanger();

	ShowGuideSequence({
		TEXT("One more secret. You're the black king again."),
		TEXT("Here's the strange part: you are NOT in check right now."),
		TEXT("It's your turn. Click your king and try to move ANYWHERE..."),
	});
}

void ACheckmateTutorial::StartTrapRound()
{
	ClearPieces();
	ResetBoardTint();
	if (Board) Board->ClearHighlights();
	SelectedPiece = INDEX_NONE;
	bKingSelected = false;
	bPhaseTransitionPending = false;

	PlaceBlackKing(5, 5);
	PlaceWhite(EChessPieceType::King, 4, 3);
	PlaceWhite(EChessPieceType::Queen, 0, 4, true);

	ShowGuideSequence({
		TEXT("Now YOU have the queen. Don't fall for the trap you just felt!"),
		TEXT("One queen move is CHECKMATE. Another is that STALEMATE. Choose!"),
	});
}

void ACheckmateTutorial::HandleMoveClick(int32 File, int32 Rank)
{
	// Single scripted mover: click it to select, click a glowing square to move
	int32 MoverIdx = INDEX_NONE;
	for (int32 i = 0; i < WhitePieces.Num(); ++i)
		if (WhitePieces[i].bPlayerControlled) MoverIdx = i;
	if (MoverIdx == INDEX_NONE) return;

	if (FindWhiteAt(File, Rank) == MoverIdx)
	{
		SelectedPiece = MoverIdx;
		ShowSelection();
		return;
	}

	if (SelectedPiece != MoverIdx)
	{
		ShowGuideMessage(TEXT("Click your piece first, then where it should go."));
		return;
	}

	if (!LegalMovesFor(WhitePieces[MoverIdx]).Contains(FIntPoint(File, Rank)))
	{
		ShowGuideMessage(TEXT("That square isn't reachable. Pick a glowing one!"));
		return;
	}

	MoveWhitePiece(MoverIdx, File, Rank);
	SelectedPiece = INDEX_NONE;
	if (Board) Board->ClearHighlights();

	const EPositionVerdict V = EvaluatePosition();
	const bool bBossPhase = (CurrentPhase == ETutorialPhase::Boss);

	if (V == EPositionVerdict::Checkmate)
	{
		if (!bBossPhase)
		{
			CompletePhaseSteps({
				TEXT("CHECKMATE! Attacked AND trapped. That's the winning move!"),
				TEXT("You dodged the stalemate trap. AXIOM's firewall is next."),
			});
			return;
		}

		Step++;
		if (Step >= NumBoss)
		{
			CurrentPhase = ETutorialPhase::Complete;
			ShowGuideSequence({
				TEXT("The firewall is DOWN!"),
				TEXT("You can spot mate, build mate, and dodge the stalemate trap."),
				TEXT("AXIOM is waiting in THE CORE. Finish this."),
			});
			OnLevelComplete.Broadcast();
			GrantBossVictoryRewards();
			return;
		}
		ShowGuideMessage(FString::Printf(TEXT("LOCKDOWN %d of %d! Next wall..."), Step, NumBoss));
		bPhaseTransitionPending = true;
		GetWorldTimerManager().SetTimer(StepTimer,
			FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::StartBossPuzzle),
			2.f, false);
		return;
	}

	if (V == EPositionVerdict::Stalemate)
	{
		ShowGuideSequence({
			TEXT("STALEMATE! No check and no moves is only a DRAW."),
			TEXT("So close! The winning move traps him IN check. Again!"),
		});
	}
	else
	{
		ShowGuideSequence({
			TEXT("He can still escape. That wasn't a lockdown."),
			TEXT("Look for the move that leaves NO safe squares. Again!"),
		});
	}

	bPhaseTransitionPending = true;
	GetWorldTimerManager().SetTimer(StepTimer,
		FTimerDelegate::CreateUObject(this,
			bBossPhase ? &ACheckmateTutorial::StartBossPuzzle : &ACheckmateTutorial::StartTrapRound),
		GuideLineSeconds * 2.f, false);
}

// ---------------------------------------------------------------------------
// Boss: BREACH THE FIREWALL
// ---------------------------------------------------------------------------

void ACheckmateTutorial::SetupBoss()
{
	Step = 0;
	StartBossPuzzle();
	ShowGuideSequence({
		TEXT("AXIOM: \"My firewall has no weaknesses.\""),
		FString::Printf(TEXT("BREACH THE FIREWALL: %d mate-in-one puzzles!"), NumBoss),
		TEXT("Each board has ONE move that locks his king down. Find it!"),
	});
}

void ACheckmateTutorial::StartBossPuzzle()
{
	ClearPieces();
	ResetBoardTint();
	if (Board) Board->ClearHighlights();
	SelectedPiece = INDEX_NONE;
	bPhaseTransitionPending = false;

	const FBossPuzzle& P = GBoss[Step];
	PlaceBlackKing(P.KingF, P.KingR);
	for (int32 i = 0; i < P.NumSupport; ++i)
		PlaceWhite(P.Support[i].Type, P.Support[i].F, P.Support[i].R);
	for (int32 i = 0; i < 3; ++i)
		if (P.Walls[i][0] >= 0)
			SpawnWallAt(P.Walls[i][0], P.Walls[i][1]);
	PlaceWhite(P.Mover, P.MoverF, P.MoverR, true);

	if (Step > 0)
		ShowGuideMessage(FString::Printf(TEXT("Firewall %d of %d. Find the checkmate!"), Step + 1, NumBoss));
}

// ---------------------------------------------------------------------------
// Click routing & plumbing
// ---------------------------------------------------------------------------

void ACheckmateTutorial::OnSquareClicked(int32 File, int32 Rank)
{
	if (CurrentPhase == ETutorialPhase::Complete) return;
	if (bPhaseTransitionPending) return;
	if (!Board || !Board->IsValidSquare(File, Rank)) return;

	switch (CurrentPhase)
	{
	case ETutorialPhase::Phase1:
		HandleBlackKingClick(File, Rank);
		return;

	case ETutorialPhase::Phase2:
		HandleLadderClick(File, Rank);
		return;

	case ETutorialPhase::Phase3:
		if (Step == 0) HandleBlackKingClick(File, Rank);
		else           HandleMoveClick(File, Rank);
		return;

	case ETutorialPhase::Boss:
		HandleMoveClick(File, Rank);
		return;

	default:
		return;
	}
}

void ACheckmateTutorial::CompletePhaseSteps(TArray<FString> Lines)
{
	bStepsDone = true;
	const float Delay = GuideLineSeconds * Lines.Num();
	ShowGuideSequence(MoveTemp(Lines));
	bPhaseTransitionPending = true;
	GetWorldTimerManager().SetTimer(StepTimer,
		FTimerDelegate::CreateUObject(this, &ACheckmateTutorial::OnPhaseComplete), Delay, false);
}

void ACheckmateTutorial::OnPhaseComplete()
{
	// Boss victory is handled inline (custom sequence + rewards); phases only
	// advance when their step flow says so.
	if (CurrentPhase == ETutorialPhase::Boss)
		return;
	if (!bStepsDone)
		return;

	Super::OnPhaseComplete();
}

bool ACheckmateTutorial::IsSquareBlocked(int32 File, int32 Rank) const
{
	if (File == BKFile && Rank == BKRank && IsValid(BlackKing)) return true;
	if (FindWhiteAt(File, Rank) != INDEX_NONE) return true;
	return Super::IsSquareBlocked(File, Rank);
}
