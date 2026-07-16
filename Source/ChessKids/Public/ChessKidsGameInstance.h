#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ChessPiece.h"
#include "ChessKidsGameInstance.generated.h"

class UChessMapRegistry;
class UChessPieceSkinRegistry;
class UMaterialInterface;

// Game instance that owns the arena list and performs level travel for the
// map picker. Set as GameInstanceClass in DefaultEngine.ini.
UCLASS()
class CHESSKIDS_API UChessKidsGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UChessKidsGameInstance();

	virtual void Init() override;

	// Registry of selectable arenas. Assign DA_MapRegistry in the class defaults
	// of the BP_ChessKidsGameInstance asset (or set on the C++ default).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chess|Maps")
	TSoftObjectPtr<UChessMapRegistry> MapRegistry;

	// Travels to the arena registered under MapId. Returns false if the id is unknown
	// or the map is locked.
	UFUNCTION(BlueprintCallable, Category = "Chess|Maps")
	bool OpenMap(FName MapId);

	// Travels directly to a level asset, bypassing the registry.
	UFUNCTION(BlueprintCallable, Category = "Chess|Maps")
	void OpenLevelAsset(const TSoftObjectPtr<UWorld>& Level);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Maps")
	bool IsMapUnlocked(FName MapId) const;

	UFUNCTION(BlueprintCallable, Category = "Chess|Maps")
	void UnlockMap(FName MapId);

	// AI difficulty chosen by the player (1=Easy, 2=Medium, 3=Hard). Lives here so the
	// choice survives level travel — AChessManager::BeginPlay reads it on every arena load.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game", meta = (ClampMin = "1", ClampMax = "3"))
	int32 Difficulty = 1;

	// Local two-player (hot-seat) mode: both colors are human, no AI. Read by
	// AChessManager::BeginPlay on arena load; set from menus / the pause toggle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game")
	bool bTwoPlayerMode = false;

	// Side chosen on the mode-select screen. Only consulted by arenas whose
	// AChessManager::PlayerColorMode == ChooseAtStart; Random/Fixed arenas ignore this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game")
	EChessColor PlayerColor = EChessColor::White;

	// Board color theme chosen via the settings slider. Applied by
	// AChessBoard::BeginPlay so the choice follows the player between arenas.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game", meta = (ClampMin = "0"))
	int32 BoardThemeIndex = 0;

	// --- Progression (persisted via UChessKidsSaveGame, slot "ChessKidsProgress") ---

	// Dev/testing override: everything unlocked. Session-only, toggled in Settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Dev")
	bool bDevUnlockAll = false;

	// Story chapter numbers are 1 = L_Pawn .. 5 = L_Royalty.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Progress")
	bool IsStoryChapterUnlocked(int32 Chapter) const;

	// Called on a story victory; unlocks the next chapter and saves.
	UFUNCTION(BlueprintCallable, Category = "Chess|Progress")
	void NotifyStoryVictory(const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "Chess|Progress")
	void MarkTutorialComplete(int32 LessonIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Progress")
	bool IsTutorialComplete(int32 LessonIndex) const;

	// Chapter for a story map name (0 if not a story map).
	static int32 StoryChapterForMap(const FString& MapName);

	// The level a story chapter opens (NAME_None if out of range). Single
	// source of truth for the 8-chapter ladder — used by the level select
	// menu and by tutorials to chain into the next lesson.
	static FName MapForChapter(int32 Chapter);

	// Free board recoloring via the settings hue sliders (overrides themes when set).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game")
	bool bCustomBoardColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightSquareHue = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Game", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DarkSquareHue = 0.62f;

	// Shared hue->color mapping so the board and the settings UI always agree.
	// The value bands are hard-separated (light >= 0.85, dark <= 0.28) so NO
	// hue combination can ever produce a low-contrast board.
	static FLinearColor LightColorFromHue(float Hue01)
	{
		return FLinearColor::MakeFromHSV8((uint8)(FMath::Clamp(Hue01, 0.f, 1.f) * 255.f), 85, 252);
	}
	static FLinearColor DarkColorFromHue(float Hue01)
	{
		return FLinearColor::MakeFromHSV8((uint8)(FMath::Clamp(Hue01, 0.f, 1.f) * 255.f), 205, 42);
	}

	// Collectible piece skins. Registry of everything that exists; unlock/selection
	// state below persists to disk across game launches (unlike UnlockedMaps).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chess|Skins")
	TSoftObjectPtr<UChessPieceSkinRegistry> PieceSkinRegistry;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Skins")
	bool IsPieceSkinUnlocked(FName SkinId) const;

	// Generic hook for a future store/achievement system to grant a skin. Nothing in
	// the game calls this yet.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skins")
	void UnlockPieceSkin(FName SkinId);

	// Unlocks whichever skin (if any) has bUnlockedByTutorial set with
	// TutorialUnlockPiece == Piece. Returns that skin's id, or NAME_None if there's no
	// such skin or it was already unlocked before this call — callers use that to
	// decide whether to show a "new skin!" popup.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skins")
	FName UnlockSkinForTutorial(EChessPieceType Piece);

	// Equips SkinId (NAME_None = revert to the piece's default look). Fails (returns
	// false, no state change) if SkinId is set but not unlocked.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skins")
	bool SelectPieceSkin(FName SkinId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Skins")
	FName GetSelectedPieceSkinId() const { return SelectedPieceSkinId; }

	// Resolves the equipped skin through PieceSkinRegistry; null if none equipped/found.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Skins")
	UMaterialInterface* GetSelectedPieceMaterial() const;

private:
	UChessMapRegistry* LoadRegistry() const;
	void LoadProgress();
	void SaveProgress();

	// Maps unlocked this session. Seeded from entries flagged bUnlockedByDefault.
	UPROPERTY()
	TSet<FName> UnlockedMaps;

	UPROPERTY()
	class UChessKidsSaveGame* Progress = nullptr;

	UChessPieceSkinRegistry* LoadSkinRegistry() const;
	void LoadOrInitSkinSave();
	void SaveSkinState() const;

	UPROPERTY()
	TSet<FName> UnlockedPieceSkins;

	UPROPERTY()
	FName SelectedPieceSkinId = NAME_None;
};
