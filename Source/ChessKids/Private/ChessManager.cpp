#include "ChessManager.h"
#include "ChessBoard.h"
#include "ChessVoice.h"
#include "FireworkShow.h"
#include "ChessPiece.h"
#include "ChessKidsGameInstance.h"
#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInterface.h"

THIRD_PARTY_INCLUDES_START
#include "position.h"
#include "search.h"
#include "notation.h"
#include "movegenerator.h"
#include "movelist.h"
#include "protocol.h"
#include "model/move.h"
#include "model/piece.h"
#include "model/piecetype.h"
#include "model/square.h"
#include "model/color.h"
#include "model/file.h"
#include "model/rank.h"
#include "model/movetype.h"
THIRD_PARTY_INCLUDES_END

struct AChessManager::FEngineImpl : public pulse::Protocol
{
	pulse::Position Position;
	pulse::Search   Search;

	TFunction<void(int)> BestMoveCallback;

	FEngineImpl()
		: Position(pulse::notation::toPosition(pulse::notation::STANDARDPOSITION))
		, Search(*this)
	{}

	void sendBestMove(int bestMove, int ponderMove) override
	{
		if (BestMoveCallback)
			BestMoveCallback(bestMove);
	}

	void sendStatus(int, int, uint64_t, int, int) override {}
	void sendStatus(bool, int, int, uint64_t, int, int) override {}
	void sendMove(pulse::RootEntry, int, int, uint64_t) override {}
	void sendInfo(const std::string&) override {}
	void sendDebug(const std::string&) override {}
};

int AChessManager::SquareFromString(const FString& Str)
{
	if (Str.Len() < 2) return -1;

	const char FileChar = (char)FChar::ToLower(Str[0]);
	const char RankChar = (char)Str[1];

	if (FileChar < 'a' || FileChar > 'h') return -1;
	if (RankChar < '1' || RankChar > '8') return -1;

	const int File = FileChar - 'a';
	const int Rank = RankChar - '1';
	return pulse::square::valueOf(File, Rank);
}

FString AChessManager::SquareToString(int Square)
{
	if (!pulse::square::isValid(Square)) return TEXT("??");
	const int File = pulse::square::getFile(Square);
	const int Rank = pulse::square::getRank(Square);
	return FString::Printf(TEXT("%c%c"), 'a' + File, '1' + Rank);
}

static const TCHAR* PieceToChar(int Piece)
{
	switch (Piece)
	{
	case pulse::piece::WHITE_PAWN:   return TEXT("P");
	case pulse::piece::WHITE_KNIGHT: return TEXT("N");
	case pulse::piece::WHITE_BISHOP: return TEXT("B");
	case pulse::piece::WHITE_ROOK:   return TEXT("R");
	case pulse::piece::WHITE_QUEEN:  return TEXT("Q");
	case pulse::piece::WHITE_KING:   return TEXT("K");
	case pulse::piece::BLACK_PAWN:   return TEXT("p");
	case pulse::piece::BLACK_KNIGHT: return TEXT("n");
	case pulse::piece::BLACK_BISHOP: return TEXT("b");
	case pulse::piece::BLACK_ROOK:   return TEXT("r");
	case pulse::piece::BLACK_QUEEN:  return TEXT("q");
	case pulse::piece::BLACK_KING:   return TEXT("k");
	default:                          return TEXT("");
	}
}

// Maps a Pulse promotion piecetype to a FEN-style letter (upper = white, lower = black).
static FString PromotionLetter(int PromoType, bool bWhite)
{
	TCHAR C;
	switch (PromoType)
	{
	case pulse::piecetype::ROOK:   C = 'r'; break;
	case pulse::piecetype::BISHOP: C = 'b'; break;
	case pulse::piecetype::KNIGHT: C = 'n'; break;
	case pulse::piecetype::QUEEN:
	default:                       C = 'q'; break;
	}
	if (bWhite) C = FChar::ToUpper(C);
	return FString::Chr(C);
}

// True if the FEN's active-colour field (2nd token) is black-to-move.
static bool FENIsBlackToMove(const FString& FEN)
{
	TArray<FString> Parts;
	FEN.ParseIntoArray(Parts, TEXT(" "), true);
	return Parts.Num() >= 2 && Parts[1] == TEXT("b");
}

AChessManager::AChessManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AChessManager::BeginPlay()
{
	Super::BeginPlay();

	NewGame();

	// Arena lesson position (piece-focused story maps). A malformed FEN is
	// rejected inside SetPositionFromFEN and the standard game remains.
	const FString LessonFEN = StartingFEN.TrimStartAndEnd();
	if (!LessonFEN.IsEmpty())
		SetPositionFromFEN(LessonFEN);
	else if (GetWorld()->GetMapName().Contains(TEXT("Axiom")) && !AxiomAdvantageFEN.TrimStartAndEnd().IsEmpty())
		// Final battle: the kid starts a queen and a rook up (see AxiomAdvantageFEN)
		SetPositionFromFEN(AxiomAdvantageFEN.TrimStartAndEnd());

	// Difficulty + mode are the player's persisted choices (GameInstance survives level travel).
	int32 Level = 1;
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (GI)
	{
		Level = GI->Difficulty;
		bTwoPlayerMode = GI->bTwoPlayerMode;
	}

	// The final battle enforces its own rules regardless of how the map was
	// entered (level select, PIE, console): the kid plays White, solo, vs an
	// Easy AXIOM. The handicap FEN and danger overlay both assume White.
	if (GetWorld()->GetMapName().Contains(TEXT("Axiom")))
	{
		Level = 1;
		bTwoPlayerMode = false;
		PlayerColorMode = EPlayerColorMode::Fixed;
		PlayerColor = EChessColor::White;
	}

	SetDifficulty(Level);

	// Resolve which side the human plays for this arena.
	switch (PlayerColorMode)
	{
	case EPlayerColorMode::Fixed:
		// PlayerColor is already set as configured on this level's ChessManager.
		break;
	case EPlayerColorMode::Random:
		PlayerColor = FMath::RandBool() ? EChessColor::White : EChessColor::Black;
		break;
	case EPlayerColorMode::ChooseAtStart:
	default:
		if (GI) PlayerColor = GI->PlayerColor;
		break;
	}

	if (!Board)
		Board = Cast<AChessBoard>(UGameplayStatics::GetActorOfClass(this, AChessBoard::StaticClass()));
	SpawnAllPieces();   // applies the selected skin internally

	OnMoveMade.AddDynamic(this, &AChessManager::HandleMoveMade);

	// Traffic never crosses the board plane — no exceptions. Placed OrbitCars
	// (L_Field has dozens hand-scattered at various heights) get their orbit
	// pushed below the board at startup, whatever height they were authored at.
	// An OrbitCar's mesh swings on an arm around the actor pivot, so the test is
	// against the orbit's CLOSEST APPROACH to the board (pivot distance minus
	// arm length), not the pivot itself — a far pivot with a wide orbit can
	// still sweep the board's airspace. Distant skyline traffic whose circle
	// can't reach the board keeps its authored altitude.
	if (Board)
	{
		const FVector BoardCentre = Board->GetBoardCenter();
		const float MaxCarZ = BoardCentre.Z - 600.f;
		const float BoardReach = 900.f;   // board footprint + safety margin
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !Actor->GetClass()->GetName().Contains(TEXT("OrbitCar")))
				continue;

			// Orbit arm = farthest any mesh component sits from the pivot in XY.
			// MeshTopZ = how far the visible car reaches ABOVE the pivot (mesh
			// offset + bounds) — clamping the pivot alone left car bodies level
			// with the board even though the pivot obeyed.
			float OrbitArm = 0.f;
			float MeshTopZ = 0.f;
			Actor->ForEachComponent<UStaticMeshComponent>(false,
				[&](const UStaticMeshComponent* Comp)
				{
					OrbitArm = FMath::Max(OrbitArm,
						FVector::Dist2D(Comp->GetComponentLocation(), Actor->GetActorLocation()));
					const FBoxSphereBounds B = Comp->Bounds;
					MeshTopZ = FMath::Max(MeshTopZ,
						(B.Origin.Z + B.BoxExtent.Z) - Actor->GetActorLocation().Z);
				});

			const float ClosestApproach =
				FVector::Dist2D(Actor->GetActorLocation(), BoardCentre) - OrbitArm;
			FVector Loc = Actor->GetActorLocation();
			const float MaxPivotZ = MaxCarZ - FMath::Max(0.f, MeshTopZ);
			if (ClosestApproach < BoardReach && Loc.Z > MaxPivotZ)
			{
				Loc.Z = MaxPivotZ;
				Actor->SetActorLocation(Loc);
			}
		}
	}

	ResolveDefaultSounds();
	if (BackgroundMusic)
	{
		MusicComponent = UGameplayStatics::SpawnSound2D(this, BackgroundMusic, MusicVolume,
			1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
		if (MusicComponent)
			MusicComponent->OnAudioFinished.AddDynamic(this, &AChessManager::RestartMusic);   // loop
	}
}

void AChessManager::ResolveDefaultSounds()
{
	// Runtime soft-path resolution instead of ConstructorHelpers: the sound packs are
	// local-only content, so a machine without them must degrade to silence, not assert.
	auto Resolve = [](USoundBase*& Slot, const TCHAR* Path)
	{
		if (!Slot)
			Slot = Cast<USoundBase>(FSoftObjectPath(Path).TryLoad());
	};
	Resolve(MoveSound,       TEXT("/Game/Audio/SFX/UIClick_Select_001_VZ_GN_Cue.UIClick_Select_001_VZ_GN_Cue"));
	Resolve(CaptureSound,    TEXT("/Game/Audio/SFX/UIAlert_Cancel_001_VZ_GN_Cue.UIAlert_Cancel_001_VZ_GN_Cue"));
	Resolve(CheckSound,      TEXT("/Game/Audio/SFX/UIAlert_Warning_001_VZ_GN_Cue.UIAlert_Warning_001_VZ_GN_Cue"));
	Resolve(WinSound,        TEXT("/Game/Audio/SFX/UIData_Processing_Complete_001_VZ_GN_Cue.UIData_Processing_Complete_001_VZ_GN_Cue"));
	Resolve(LoseSound,       TEXT("/Game/Audio/SFX/UIMvmt_Transition_001_VZ_GN_Cue.UIMvmt_Transition_001_VZ_GN_Cue"));
	Resolve(DrawSound,       TEXT("/Game/Audio/SFX/UIAlert_Notification_001_VZ_GN_Cue.UIAlert_Notification_001_VZ_GN_Cue"));
	// BackgroundMusic gets NO default: arena music is owned by Marco's BP_MusicManager
	// (Content/Music, per-arena tracks). Set this property only to override it.
}

void AChessManager::PlaySfx(USoundBase* Sound) const
{
	if (Sound)
		UGameplayStatics::PlaySound2D(this, Sound, SfxVolume);
}

void AChessManager::RestartMusic()
{
	if (MusicComponent && BackgroundMusic)
		MusicComponent->Play();
}

void AChessManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Level travel flushes audio AFTER EndPlay; without unbinding, OnAudioFinished
	// fires on that Stop() and RestartMusic re-plays into the dying world, stacking
	// an orphaned track per Restart/Quit.
	if (MusicComponent)
	{
		MusicComponent->OnAudioFinished.RemoveDynamic(this, &AChessManager::RestartMusic);
		MusicComponent->Stop();
		MusicComponent = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AChessManager::BeginDestroy()
{
	if (Engine)
	{
		Engine->Search.quit();
		delete Engine;
		Engine = nullptr;
	}
	Super::BeginDestroy();
}

void AChessManager::NewGame()
{
	FChessVoice::Interrupt(this);   // fresh game, fresh narration
	bGameOver = false;
	if (Engine)
	{
		Engine->Search.quit();
		delete Engine;
		Engine = nullptr;
	}

	Engine = new FEngineImpl();
	bExpectingAIMove = false;
	bHintRequest = false;
	FENHistory.Empty();   // a fresh game must not let Undo reach into a previous game's snapshots

	// Capture a weak ptr — IsValid(this) is undefined behaviour once the actor is freed,
	// because it dereferences `this` to read the object flags.
	TWeakObjectPtr<AChessManager> WeakThis(this);
	Engine->BestMoveCallback = [WeakThis](int BestMove)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, BestMove]()
		{
			if (AChessManager* Self = WeakThis.Get())
				Self->OnBestMoveFound(BestMove);
		});
	};
}

void AChessManager::SetPositionFromFEN(const FString& FEN)
{
	if (!Engine) NewGame();

	const std::string FENStd(TCHAR_TO_UTF8(*FEN));
	try
	{
		Engine->Position = pulse::notation::toPosition(FENStd);
	}
	catch (const std::exception&)
	{
		// Malformed FEN (toPosition throws) — keep the current position instead
		// of crashing; callers can detect the no-op by comparing GetFEN().
		UE_LOG(LogTemp, Warning, TEXT("SetPositionFromFEN rejected malformed FEN: %s"), *FEN);
	}
}

bool AChessManager::MakeMove(const FString& MoveStr)
{

	if (!Engine) return false;
	if (bGameOver) return false;   // finished games accept no more moves (undo re-opens)
	if (MoveStr.Len() < 4) return false;

	const FString OriginStr   = MoveStr.Mid(0, 2);
	const FString TargetStr   = MoveStr.Mid(2, 2);
	const FString PromoStr    = MoveStr.Len() >= 5 ? MoveStr.Mid(4, 1) : TEXT("");

	const int OriginSquare = SquareFromString(OriginStr);
	const int TargetSquare = SquareFromString(TargetStr);
	if (OriginSquare < 0 || TargetSquare < 0) return false;

	pulse::MoveGenerator Gen;
	auto& LegalMoves = Gen.getLegalMoves(
		Engine->Position, 1, Engine->Position.isCheck());

	for (int i = 0; i < LegalMoves.size; i++)
	{
		const int M = LegalMoves.entries[i]->move;
		if (pulse::move::getOriginSquare(M) != OriginSquare) continue;
		if (pulse::move::getTargetSquare(M) != TargetSquare) continue;

		if (!PromoStr.IsEmpty())
		{
			const char PromoChar = (char)FChar::ToLower(PromoStr[0]);
			int ExpectedPromo = pulse::piecetype::NOPIECETYPE;
			switch (PromoChar)
			{
			case 'q': ExpectedPromo = pulse::piecetype::QUEEN;  break;
			case 'r': ExpectedPromo = pulse::piecetype::ROOK;   break;
			case 'b': ExpectedPromo = pulse::piecetype::BISHOP; break;
			case 'n': ExpectedPromo = pulse::piecetype::KNIGHT; break;
			}
			if (pulse::move::getPromotion(M) != ExpectedPromo) continue;
		}

		// A real move invalidates any in-flight hint search — cancel only NOW that
		// the move is known legal (an illegal click must not kill a pending hint).
		if (bHintRequest)
			StopSearch();

		// Push BEFORE the move so undo can restore to this state — only now that a real,
		// legal move has matched (a failed/illegal call must not leave a phantom snapshot).
		FENHistory.Push(GetFEN());
		Engine->Position.makeMove(M);

		const FString From = SquareToString(OriginSquare);
		const FString To   = SquareToString(TargetSquare);
		PositionVersion++;
		OnMoveMade.Broadcast(From, To);

		CheckGameOver();
		return true;
	}

	return false;
}

void AChessManager::RequestAIMove()
{
	if (!Engine) return;
	if (bTwoPlayerMode) return;   // hot-seat: no AI ever moves
	bExpectingAIMove = true;   // arm OnBestMoveFound; Undo/NewGame clear this to drop a stale reply
	UE_LOG(LogTemp, Verbose, TEXT("AI thinking at depth %d"), AISearchDepth);
	// At Easy difficulty, 50% chance of playing a random legal move
	if (AISearchDepth == 1)
	{
		pulse::MoveGenerator Gen;
		auto& LegalMoves = Gen.getLegalMoves(
			Engine->Position, 1, Engine->Position.isCheck());

		if (LegalMoves.size > 0)
		{
			// Pick a random move
			int32 RandomIndex = FMath::RandRange(0, LegalMoves.size - 1);
			int RandomMove = LegalMoves.entries[RandomIndex]->move;

			// 50% chance use random move, 50% use engine
			if (FMath::RandBool())
			{
				UE_LOG(LogTemp, Verbose, TEXT("Easy mode: playing random move!"));
				OnBestMoveFound(RandomMove);
				return;
			}
		}
	}

	Engine->Search.newDepthSearch(Engine->Position, AISearchDepth);
	Engine->Search.start();
}

void AChessManager::StopSearch()
{
	// The one true cancel: stop the engine AND disarm the pending-search flags.
	// stop() blocks until the aborted search's callback is queued, so clearing the
	// flags here (game thread) guarantees that callback is dropped, whatever it was.
	if (Engine) Engine->Search.stop();
	bExpectingAIMove = false;
	bHintRequest = false;
}

void AChessManager::RequestHint()
{
	if (!Engine || bGameOver) return;
	if (bExpectingAIMove) return;   // a search is already in flight (AI reply or hint)

	bHintRequest = true;
	bExpectingAIMove = true;
	Engine->Search.newDepthSearch(Engine->Position, 4);   // fast, good enough to teach
	Engine->Search.start();
}

FString AChessManager::GetCapturedPieces(bool bWhitePieces) const
{
	if (!Engine) return TEXT("");

	// Count what's still on the board, subtract from the starting set. Promotions
	// can push a type over its starting count — clamp so the tray never goes negative.
	TMap<TCHAR, int32> OnBoard;
	for (int32 Rank = 0; Rank < 8; ++Rank)
		for (int32 File = 0; File < 8; ++File)
		{
			const FString P = GetPieceOnSquare(FString::Printf(TEXT("%c%c"), 'a' + File, '1' + Rank));
			if (!P.IsEmpty()) OnBoard.FindOrAdd(P[0])++;
		}

	static const TCHAR WhiteSet[] = { 'Q', 'R', 'B', 'N', 'P' };
	static const TCHAR BlackSet[] = { 'q', 'r', 'b', 'n', 'p' };
	static const int32 StartCount[] = { 1, 2, 2, 2, 8 };

	// Promotions add extra non-pawn pieces; each one accounts for a pawn that
	// left the board WITHOUT being captured, so subtract them from the pawn
	// deficit or the tray shows a phantom captured pawn after every promotion.
	int32 PromotionExtras = 0;
	for (int32 i = 0; i < 4; ++i)   // Q R B N only
	{
		const TCHAR C = bWhitePieces ? WhiteSet[i] : BlackSet[i];
		PromotionExtras += FMath::Max(0, OnBoard.FindRef(C) - StartCount[i]);
	}

	FString Result;
	for (int32 i = 0; i < 5; ++i)
	{
		const TCHAR C = bWhitePieces ? WhiteSet[i] : BlackSet[i];
		int32 Missing = FMath::Max(0, StartCount[i] - OnBoard.FindRef(C));
		if (C == 'P' || C == 'p')
			Missing = FMath::Max(0, Missing - PromotionExtras);
		for (int32 k = 0; k < Missing; ++k)
		{
			if (!Result.IsEmpty()) Result += TEXT(" ");
			Result.AppendChar(FChar::ToUpper(C));
		}
	}
	return Result;
}

int32 AChessManager::GetPlayerScore() const
{
	// Robot pieces the human has captured; letters as produced by GetCapturedPieces.
	const FString Taken = GetCapturedPieces(PlayerColor == EChessColor::Black);
	int32 Score = 0;
	for (const TCHAR C : Taken)
	{
		switch (C)
		{
		case 'Q': Score += 900; break;
		case 'R': Score += 500; break;
		case 'B': case 'N': Score += 300; break;
		case 'P': Score += 100; break;
		default: break;   // spaces
		}
	}
	return Score;
}

void AChessManager::DevForceWin()
{
	if (bGameOver) return;
	StopSearch();
	bGameOver = true;
	PlaySfx(WinSound);
	OnGameOver.Broadcast(PlayerColor == EChessColor::Black ? TEXT("black") : TEXT("white"));
}

void AChessManager::UndoLastMove()
{
	// Stop any in-flight AI search, then DISARM so the best move the aborted search still
	// emits is dropped by OnBestMoveFound instead of being applied to the restored position.
	// stop() blocks until that callback is queued, so clearing the flag here (game thread)
	// guarantees the later-running callback observes it as false.
	StopSearch();   // stops the engine and disarms any pending AI/hint reply

	if (FENHistory.Num() == 0) return;

	FChessVoice::Interrupt(this);   // the position is being rewound — drop stale narration

	FString RestoredFEN = FENHistory.Pop();
	if (!bTwoPlayerMode)
	{
		// Vs AI: hand the turn back to the player — restore the most recent WHITE-to-move
		// snapshot, popping any trailing black-to-move (AI) snapshots. Correct whether or
		// not the AI had replied (early undo, a mating move with no AI reply, etc.).
		while (FENIsBlackToMove(RestoredFEN) && FENHistory.Num() > 0)
		{
			RestoredFEN = FENHistory.Pop();
		}
	}
	// Hot-seat: one pop = take back exactly the last move, whichever color made it.

	SetPositionFromFEN(RestoredFEN);
	PositionVersion++;
	SpawnAllPieces();   // re-sync the 3D pieces to match the restored position
	bGameOver = false;  // in case undo happens after checkmate detection
}

void AChessManager::SwapPromotedPiece(const FString& Square, const FString& PieceLetter)
{
	if (!Board) return;

	// Find and destroy the current piece on that square (the queen placeholder)
	AChessPiece* ExistingPiece = FindPieceOnSquare(Square);
	if (ExistingPiece)
	{
		ExistingPiece->Destroy();
		PieceActors.Remove(Square);
	}

	// Determine color and type from the piece letter
	// Lowercase = black, uppercase = white
	TCHAR C = PieceLetter[0];
	EChessColor Color = FChar::IsUpper(C) ? EChessColor::White : EChessColor::Black;
	EChessPieceType Type = CharToPieceType(C);

	// Get file and rank from square string e.g. "e8"
	int32 File = Square[0] - 'a';
	int32 Rank = Square[1] - '1';

	// Get the right mesh config
	const TMap<EChessPieceType, FPieceMeshConfig>& MeshMap =
		(Color == EChessColor::White) ? WhitePieceMeshes : BlackPieceMeshes;
	const FPieceMeshConfig* Override = MeshMap.Find(Type);

	// Spawn the new piece
	FVector SpawnLoc = Board->FileRankToWorldLocation(File, Rank, 0.f);
	FRotator SpawnRot = FRotator::ZeroRotator;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AChessPiece* NewPiece = nullptr;
	if (Override && Override->PieceClass)
	{
		AActor* Spawned = GetWorld()->SpawnActor(Override->PieceClass.Get(), &SpawnLoc, &SpawnRot, Params);
		NewPiece = Cast<AChessPiece>(Spawned);
	}
	else
	{
		NewPiece = GetWorld()->SpawnActor<AChessPiece>(SpawnLoc, SpawnRot, Params);
	}

	if (NewPiece)
	{
		if (Override && Override->PieceClass)
		{
			NewPiece->PieceType = Type;
			NewPiece->PieceColor = Color;
			NewPiece->BoardFile = File;
			NewPiece->BoardRank = Rank;
			NewPiece->SetActorScale3D(Override->Scale);
		}
		else
		{
			NewPiece->Init(Type, Color, File, Rank, Override);
		}
		float ZOff = (Override && Override->PieceClass) ? Override->ZOffset : 0.f;
		Board->SnapActorToSquare(NewPiece, File, Rank, ZOff);
		PieceActors.Add(Square, NewPiece);
	}

	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		ApplyPieceSkin(GI->GetSelectedPieceMaterial());
}

void AChessManager::ApplyPieceSkin(UMaterialInterface* SkinMaterial)
{
	for (const TPair<FString, AChessPiece*>& Pair : PieceActors)
	{
		AChessPiece* Piece = Pair.Value;
		if (!Piece) continue;

		// Only skin the piece color the player actually plays as, and only outside
		// hot-seat mode. In two-player hot-seat there's a single local save/skin
		// selection shared by both people taking turns on one device, so no one side
		// is "the player" — applying it to both made them visually indistinguishable,
		// so neither side gets it instead.
		const bool bShouldSkin = !bTwoPlayerMode && (Piece->PieceColor == PlayerColor);
		Piece->ApplyPieceSkin(bShouldSkin ? SkinMaterial : nullptr);
	}
}

void AChessManager::SetDifficulty(int32 Level)
{
	CurrentDifficulty = FMath::Clamp(Level, 1, 3);
	switch (CurrentDifficulty)
	{
	case 1:  AISearchDepth = 1; break;
	case 2:  AISearchDepth = 3; break;
	default: AISearchDepth = 6; break;
	}

	// Persist so the next arena load (new world, new manager) keeps the choice.
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		GI->Difficulty = CurrentDifficulty;

	UE_LOG(LogTemp, Verbose, TEXT("Difficulty set! AISearchDepth = %d"), AISearchDepth);
}

FString AChessManager::GetFEN() const
{
	if (!Engine) return TEXT("");
	return FString(pulse::notation::fromPosition(Engine->Position).c_str());
}

FString AChessManager::GetActiveColor() const
{
	if (!Engine) return TEXT("white");
	return Engine->Position.activeColor == pulse::color::WHITE
		? TEXT("white") : TEXT("black");
}

TArray<FString> AChessManager::GetLegalMovesFromSquare(const FString& SquareStr) const
{
	TArray<FString> Result;
	if (!Engine) return Result;

	const int OriginSquare = SquareFromString(SquareStr);
	if (OriginSquare < 0) return Result;

	pulse::MoveGenerator Gen;
	auto& LegalMoves = Gen.getLegalMoves(
		Engine->Position, 1, Engine->Position.isCheck());

	for (int i = 0; i < LegalMoves.size; i++)
	{
		const int M = LegalMoves.entries[i]->move;
		if (pulse::move::getOriginSquare(M) == OriginSquare)
			Result.AddUnique(SquareToString(pulse::move::getTargetSquare(M)));
	}

	return Result;
}

bool AChessManager::IsInCheck() const
{
	return Engine ? Engine->Position.isCheck() : false;
}

FString AChessManager::GetPieceOnSquare(const FString& SquareStr) const
{
	if (!Engine) return TEXT("");
	const int Sq = SquareFromString(SquareStr);
	if (Sq < 0) return TEXT("");
	return PieceToChar(Engine->Position.board[Sq]);
}

TArray<FString> AChessManager::GetSquaresAttackedByBlack() const
{
	return ComputeAttackedSquares(true);
}

TArray<FString> AChessManager::GetSquaresAttackedByOpponent() const
{
	return ComputeAttackedSquares(PlayerColor == EChessColor::White);
}

TArray<FString> AChessManager::ComputeAttackedSquares(bool bByBlack) const
{
	TArray<FString> Result;
	if (!Engine) return Result;

	// Snapshot the board as FEN letters (lowercase = black)
	TCHAR Grid[8][8];
	for (int32 F = 0; F < 8; ++F)
	{
		for (int32 R = 0; R < 8; ++R)
		{
			const FString Sq = FString::Printf(TEXT("%c%d"), 'a' + F, R + 1);
			const FString P = GetPieceOnSquare(Sq);
			Grid[F][R] = P.IsEmpty() ? TEXT('\0') : P[0];
		}
	}

	TSet<int32> Attacked;
	auto Add = [&Attacked](int32 F, int32 R)
	{
		if (F >= 0 && F < 8 && R >= 0 && R < 8) Attacked.Add(F * 8 + R);
	};
	auto Ray = [&](int32 F, int32 R, int32 FS, int32 RS)
	{
		F += FS; R += RS;
		while (F >= 0 && F < 8 && R >= 0 && R < 8)
		{
			Attacked.Add(F * 8 + R);
			if (Grid[F][R] != TEXT('\0')) break; // attacks include the blocker, not past it
			F += FS; R += RS;
		}
	};

	// Black pawns capture toward rank 1, white pawns toward rank 8
	const int32 PawnDir = bByBlack ? -1 : 1;

	for (int32 F = 0; F < 8; ++F)
	{
		for (int32 R = 0; R < 8; ++R)
		{
			const TCHAR C = Grid[F][R];
			if (C == TEXT('\0')) continue;
			if (FChar::IsLower(C) != bByBlack) continue; // FEN: lowercase = black

			switch (FChar::ToLower(C))
			{
			case TEXT('p'):
				Add(F - 1, R + PawnDir); Add(F + 1, R + PawnDir);
				break;
			case TEXT('n'):
				Add(F + 2, R + 1); Add(F + 2, R - 1); Add(F - 2, R + 1); Add(F - 2, R - 1);
				Add(F + 1, R + 2); Add(F + 1, R - 2); Add(F - 1, R + 2); Add(F - 1, R - 2);
				break;
			case TEXT('k'):
				for (int32 DF = -1; DF <= 1; ++DF)
					for (int32 DR = -1; DR <= 1; ++DR)
						if (DF != 0 || DR != 0) Add(F + DF, R + DR);
				break;
			case TEXT('r'):
				Ray(F, R, 1, 0); Ray(F, R, -1, 0); Ray(F, R, 0, 1); Ray(F, R, 0, -1);
				break;
			case TEXT('b'):
				Ray(F, R, 1, 1); Ray(F, R, 1, -1); Ray(F, R, -1, 1); Ray(F, R, -1, -1);
				break;
			case TEXT('q'):
				Ray(F, R, 1, 0); Ray(F, R, -1, 0); Ray(F, R, 0, 1); Ray(F, R, 0, -1);
				Ray(F, R, 1, 1); Ray(F, R, 1, -1); Ray(F, R, -1, 1); Ray(F, R, -1, -1);
				break;
			default:
				break;
			}
		}
	}

	for (int32 Packed : Attacked)
		Result.Add(FString::Printf(TEXT("%c%d"), 'a' + Packed / 8, Packed % 8 + 1));
	return Result;
}

void AChessManager::OnBestMoveFound(int BestMove)
{
	if (!Engine) return;

	// The search delivers via AsyncTask, which the engine pumps even while paused.
	// Don't move (audibly) behind the pause menu — re-queue on a world timer, which
	// only advances after unpause. Runs BEFORE the flag is consumed, so an Undo from
	// the pause menu still cancels the deferred move via bExpectingAIMove.
	if (UGameplayStatics::IsGamePaused(this))
	{
		TWeakObjectPtr<AChessManager> WeakThis(this);
		FTimerHandle DeferHandle;
		GetWorldTimerManager().SetTimer(DeferHandle, [WeakThis, BestMove]()
		{
			if (AChessManager* Self = WeakThis.Get())
				Self->OnBestMoveFound(BestMove);
		}, 0.05f, false);
		return;
	}

	// Drop stale callbacks: a search aborted by Undo/NewGame still emits a best move,
	// which must NOT be applied to the (now restored) position. Only act if still armed.
	if (!bExpectingAIMove) return;
	bExpectingAIMove = false;

	if (BestMove == pulse::move::NOMOVE) { bHintRequest = false; return; }

	const FString From = SquareToString(pulse::move::getOriginSquare(BestMove));
	const FString To   = SquareToString(pulse::move::getTargetSquare(BestMove));

	// Hint searches show the move instead of playing it. Presentation is left to
	// the OnHintReady subscribers — the controller owns selection state, and the
	// board highlights must never diverge from it.
	if (bHintRequest)
	{
		bHintRequest = false;
		OnHintReady.Broadcast(From, To);
		return;
	}

	// Capture promotion + mover colour BEFORE makeMove flips the active colour.
	const int Promo = pulse::move::getPromotion(BestMove);
	const bool bMoverIsWhite = (Engine->Position.activeColor == pulse::color::WHITE);

	FENHistory.Push(GetFEN());

	Engine->Position.makeMove(BestMove);
	PositionVersion++;
	OnMoveMade.Broadcast(From, To);   // starts the pawn's glide (HandleMoveMade)

	// The engine now has a promoted piece on the back rank, but HandleMoveMade only
	// slid the pawn there — swap in the real promoted mesh so the board matches.
	// Deferred past the glide so the pawn visibly arrives before morphing; the
	// square re-check drops the swap if an Undo rewound the position meanwhile.
	if (Promo != pulse::piecetype::NOPIECETYPE)
	{
		const FString PromoLetter = PromotionLetter(Promo, bMoverIsWhite);
		TWeakObjectPtr<AChessManager> WeakThis(this);
		FTimerHandle SwapHandle;
		GetWorldTimerManager().SetTimer(SwapHandle, [WeakThis, To, PromoLetter]()
		{
			AChessManager* Self = WeakThis.Get();
			if (Self && Self->GetPieceOnSquare(To) == PromoLetter)
				Self->SwapPromotedPiece(To, PromoLetter);
		}, 0.35f, false);
	}

	OnAIMoveReady.Broadcast(From, To);

	CheckGameOver();
}

void AChessManager::AwardStoryWin()
{
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI) return;

	// Advance the story ladder (no-op outside the tutorial maps).
	GI->NotifyStoryVictory(GetWorld()->GetMapName());

	// AXIOM is King's chapter reward, but it's a real chess match (this class) rather
	// than a scripted tutorial phase, so it hooks the skin-reward system directly and
	// fires the end-of-game firework show.
	if (GetWorld()->GetMapName().Contains(TEXT("Axiom")))
	{
		// Lesson 5 = King (same numbering as MarkTutorialComplete/LessonIndexOverride
		// — see FChessPieceSkinEntry::TutorialUnlockLessonIndex). Reassigning which
		// lesson a skin rewards (e.g. to Checkmate's lesson 6 instead) is a registry
		// edit now, not a code change.
		const FName UnlockedSkin = GI->UnlockSkinForTutorial(5);
		if (!UnlockedSkin.IsNone())
			OnSkinUnlocked.Broadcast(UnlockedSkin);

		if (Board)
		{
			FActorSpawnParameters FxParams;
			FxParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AFireworkShow>(AFireworkShow::StaticClass(),
				Board->GetBoardCenter() + FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, FxParams);
		}
	}
}

void AChessManager::CheckGameOver()
{
	if (!Engine) return;
	if (bGameOver) return;

	pulse::MoveGenerator Gen;
	auto& LegalMoves = Gen.getLegalMoves(
		Engine->Position, 1, Engine->Position.isCheck());

	// The AXIOM finale is designed to be won — a stalemate or technical draw there
	// still awards the kid the victory (fireworks, King skin, thank-you) rather than
	// robbing them of the ending with a tie.
	const bool bAxiomFinale = !bTwoPlayerMode
		&& GetWorld()->GetMapName().Contains(TEXT("Axiom"));

	if (LegalMoves.size == 0)
	{
		if (Engine->Position.isCheck())
		{
			const FString Winner = Engine->Position.activeColor == pulse::color::WHITE
				? TEXT("black") : TEXT("white");
			bGameOver = true;
			PlaySfx(Winner == TEXT("white") ? WinSound : LoseSound);
			if (Winner == TEXT("white") && !bTwoPlayerMode)
				AwardStoryWin();
			OnGameOver.Broadcast(Winner);
		}
		else if (bAxiomFinale)   // stalemate on the finale — still the player's win
		{
			bGameOver = true;
			PlaySfx(WinSound);
			AwardStoryWin();
			OnGameOver.Broadcast(TEXT("white"));
		}
		else
		{
			bGameOver = true;
			PlaySfx(DrawSound);
			OnGameOver.Broadcast(TEXT("draw"));
		}
		return;
	}

	// Draw conditions: true threefold repetition, the 50-move rule (100 half-moves),
	// or insufficient mating material. (isRepetition() is twofold and used by the
	// search; the game RESULT needs real threefold.)
	if (Engine->Position.isThreefoldRepetition()
		|| Engine->Position.halfmoveClock >= 100
		|| Engine->Position.hasInsufficientMaterial())
	{
		bGameOver = true;
		if (bAxiomFinale)   // technical draw on the finale — still the player's win
		{
			PlaySfx(WinSound);
			AwardStoryWin();
			OnGameOver.Broadcast(TEXT("white"));
		}
		else
		{
			PlaySfx(DrawSound);
			OnGameOver.Broadcast(TEXT("draw"));
		}
		return;
	}

	// Game continues — warn if the side to move is now in check.
	if (Engine->Position.isCheck())
		PlaySfx(CheckSound);
}

EChessPieceType AChessManager::CharToPieceType(TCHAR C)
{
	switch (FChar::ToLower(C))
	{
	case 'p': return EChessPieceType::Pawn;
	case 'n': return EChessPieceType::Knight;
	case 'b': return EChessPieceType::Bishop;
	case 'r': return EChessPieceType::Rook;
	case 'q': return EChessPieceType::Queen;
	case 'k': return EChessPieceType::King;
	default:  return EChessPieceType::Pawn;
	}
}

void AChessManager::SpawnAllPieces()
{
	for (auto& Pair : PieceActors)
		if (IsValid(Pair.Value))
			Pair.Value->Destroy();
	PieceActors.Empty();

	// Sweep capture animations too — an undo/rebuild mid-fling must not leave a
	// ghost duplicate rising over the respawned piece.
	for (AChessPiece* Fling : FlingingPieces)
		if (IsValid(Fling))
			Fling->Destroy();
	FlingingPieces.Empty();

	if (!IsValid(Board)) return;

	for (int32 Rank = 0; Rank < 8; ++Rank)
	{
		for (int32 File = 0; File < 8; ++File)
		{
			FString SquareName = FString::Printf(TEXT("%c%c"), 'a' + File, '1' + Rank);
			FString PieceStr = GetPieceOnSquare(SquareName);
			if (PieceStr.IsEmpty()) continue;

			TCHAR PieceChar = PieceStr[0];
			EChessColor Color = FChar::IsUpper(PieceChar) ? EChessColor::White : EChessColor::Black;
			EChessPieceType Type = CharToPieceType(PieceChar);

			FVector SpawnLoc = Board->FileRankToWorldLocation(File, Rank, 0.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const TMap<EChessPieceType, FPieceMeshConfig>& MeshMap =
				(Color == EChessColor::White) ? WhitePieceMeshes : BlackPieceMeshes;
			const FPieceMeshConfig* Override = MeshMap.Find(Type);

			FRotator SpawnRot = FRotator::ZeroRotator;
			AChessPiece* Piece = nullptr;

			if (Override && Override->PieceClass)
			{
				AActor* Spawned = GetWorld()->SpawnActor(Override->PieceClass.Get(), &SpawnLoc, &SpawnRot, Params);
				Piece = Cast<AChessPiece>(Spawned);
			}
			else
			{
				Piece = GetWorld()->SpawnActor<AChessPiece>(SpawnLoc, SpawnRot, Params);
			}
			if (Piece)
			{
				if (Override && Override->PieceClass)
				{
					Piece->PieceType = Type;
					Piece->PieceColor = Color;
					Piece->BoardFile = File;
					Piece->BoardRank = Rank;
					Piece->SetActorScale3D(Override->Scale);
				}
				else
				{
					Piece->Init(Type, Color, File, Rank, Override);
				}
				float ZOff = (Override && Override->PieceClass) ? Override->ZOffset : 0.f;
				Board->SnapActorToSquare(Piece, File, Rank, ZOff);
				PieceActors.Add(SquareName, Piece);
			}
		}
	}

	// Re-skin here, not in the callers: every rebuild path (new game, undo,
	// FEN reload) must keep the player's selected skin — undo used to lose it.
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		ApplyPieceSkin(GI->GetSelectedPieceMaterial());
}

AChessPiece* AChessManager::FindPieceOnSquare(const FString& Square) const
{
	const auto* Found = PieceActors.Find(Square);
	return Found ? *Found : nullptr;
}

void AChessManager::HandleMoveMade(FString From, FString To)
{
	if (!Board) return;

	AChessPiece* Captured = FindPieceOnSquare(To);
	const bool bNormalCapture = (Captured != nullptr);
	bool bEnPassantCapture = false;
	if (Captured)
	{
		Captured->StartCaptureFling();   // pops up + shrinks, destroys itself
		PieceActors.Remove(To);
		FlingingPieces.Add(Captured);
	}

	AChessPiece* MovingPiece = FindPieceOnSquare(From);
	if (MovingPiece)
	{
		PieceActors.Remove(From);

		int32 File = To[0] - 'a';
		int32 Rank = To[1] - '1';
		Board->GlideActorToSquare(MovingPiece, File, Rank, 0.f);
		MovingPiece->BoardFile = File;
		MovingPiece->BoardRank = Rank;

		PieceActors.Add(To, MovingPiece);
	}

	// En passant: a pawn that moved diagonally onto an EMPTY square captured the
	// enemy pawn beside it (on the moved-from rank, target file). The engine has
	// already removed that pawn, so mirror it on the 3D board.
	if (MovingPiece && MovingPiece->PieceType == EChessPieceType::Pawn && !bNormalCapture)
	{
		const int32 FromFile = From[0] - 'a';
		const int32 ToFile   = To[0]   - 'a';
		const int32 FromRank = From[1] - '1';
		if (FromFile != ToFile)
		{
			const FString CapturedSquare = FString::Printf(TEXT("%c%c"), 'a' + ToFile, '1' + FromRank);
			if (AChessPiece* EPPawn = FindPieceOnSquare(CapturedSquare))
			{
				EPPawn->StartCaptureFling();
				PieceActors.Remove(CapturedSquare);
				FlingingPieces.Add(EPPawn);
				bEnPassantCapture = true;
			}
		}
	}

	PlaySfx((bNormalCapture || bEnPassantCapture) ? CaptureSound : MoveSound);

	if (MovingPiece && MovingPiece->PieceType == EChessPieceType::King)
	{
		int32 FromFile = From[0] - 'a';
		int32 ToFile = To[0] - 'a';
		int32 RankNum = To[1] - '1';

		if (FMath::Abs(ToFile - FromFile) == 2)
		{
			if (ToFile > FromFile)
			{
				FString RookFrom = FString::Printf(TEXT("h%c"), '1' + RankNum);
				FString RookTo   = FString::Printf(TEXT("f%c"), '1' + RankNum);
				AChessPiece* Rook = FindPieceOnSquare(RookFrom);
				if (Rook)
				{
					PieceActors.Remove(RookFrom);
					Board->GlideActorToSquare(Rook, 5, RankNum, 0.f);
					Rook->BoardFile = 5;
					Rook->BoardRank = RankNum;
					PieceActors.Add(RookTo, Rook);
				}
			}
			else
			{
				FString RookFrom = FString::Printf(TEXT("a%c"), '1' + RankNum);
				FString RookTo   = FString::Printf(TEXT("d%c"), '1' + RankNum);
				AChessPiece* Rook = FindPieceOnSquare(RookFrom);
				if (Rook)
				{
					PieceActors.Remove(RookFrom);
					Board->GlideActorToSquare(Rook, 3, RankNum, 0.f);
					Rook->BoardFile = 3;
					Rook->BoardRank = RankNum;
					PieceActors.Add(RookTo, Rook);
				}
			}
		}
	}
}
