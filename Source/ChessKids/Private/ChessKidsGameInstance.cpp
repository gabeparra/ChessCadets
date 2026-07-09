#include "ChessKidsGameInstance.h"
#include "ChessMapRegistry.h"
#include "ChessPieceSkinRegistry.h"
#include "ChessKidsSaveGame.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

namespace
{
	const TCHAR* SkinSaveSlotName = TEXT("ChessKidsSave");
	const int32 SkinSaveUserIndex = 0;
}

UChessKidsGameInstance::UChessKidsGameInstance()
{
	// Default the registry to the conventional asset path so OpenMap works without
	// a Blueprint subclass. Resolves silently once DA_MapRegistry is created.
	static ConstructorHelpers::FObjectFinder<UChessMapRegistry> RegistryFinder(
		TEXT("/Game/Data/DA_MapRegistry.DA_MapRegistry"));
	if (RegistryFinder.Succeeded())
	{
		MapRegistry = RegistryFinder.Object;
	}

	// Same convention for the piece skin catalog. Resolves silently once
	// DA_PieceSkinRegistry is created.
	static ConstructorHelpers::FObjectFinder<UChessPieceSkinRegistry> SkinRegistryFinder(
		TEXT("/Game/Data/DA_PieceSkinRegistry.DA_PieceSkinRegistry"));
	if (SkinRegistryFinder.Succeeded())
	{
		PieceSkinRegistry = SkinRegistryFinder.Object;
	}
}

void UChessKidsGameInstance::Init()
{
	Super::Init();

	if (UChessMapRegistry* Registry = LoadRegistry())
	{
		for (const FChessMapEntry& Entry : Registry->Maps)
		{
			if (Entry.bUnlockedByDefault && !Entry.MapId.IsNone())
			{
				UnlockedMaps.Add(Entry.MapId);
			}
		}
	}

	LoadOrInitSkinSave();
}

UChessMapRegistry* UChessKidsGameInstance::LoadRegistry() const
{
	if (MapRegistry.IsNull())
	{
		return nullptr;
	}
	return MapRegistry.LoadSynchronous();
}

bool UChessKidsGameInstance::OpenMap(FName MapId)
{
	UChessMapRegistry* Registry = LoadRegistry();
	if (!Registry)
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenMap: no MapRegistry assigned on GameInstance."));
		return false;
	}

	FChessMapEntry Entry;
	if (!Registry->FindMap(MapId, Entry))
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenMap: unknown map id '%s'."), *MapId.ToString());
		return false;
	}

	if (!IsMapUnlocked(MapId))
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenMap: map '%s' is locked."), *MapId.ToString());
		return false;
	}

	OpenLevelAsset(Entry.Level);
	return true;
}

void UChessKidsGameInstance::OpenLevelAsset(const TSoftObjectPtr<UWorld>& Level)
{
	if (Level.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenLevelAsset: level reference is empty."));
		return;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(Level.ToString());
	if (PackageName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("OpenLevelAsset: could not resolve package name from '%s'."), *Level.ToString());
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*PackageName));
}

bool UChessKidsGameInstance::IsMapUnlocked(FName MapId) const
{
	return UnlockedMaps.Contains(MapId);
}

void UChessKidsGameInstance::UnlockMap(FName MapId)
{
	if (!MapId.IsNone())
	{
		UnlockedMaps.Add(MapId);
	}
}

UChessPieceSkinRegistry* UChessKidsGameInstance::LoadSkinRegistry() const
{
	if (PieceSkinRegistry.IsNull())
	{
		return nullptr;
	}
	return PieceSkinRegistry.LoadSynchronous();
}

void UChessKidsGameInstance::LoadOrInitSkinSave()
{
	bool bLoadedExisting = false;
	if (UGameplayStatics::DoesSaveGameExist(SkinSaveSlotName, SkinSaveUserIndex))
	{
		if (UChessKidsSaveGame* Save = Cast<UChessKidsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SkinSaveSlotName, SkinSaveUserIndex)))
		{
			UnlockedPieceSkins = Save->UnlockedPieceSkins;
			SelectedPieceSkinId = Save->SelectedPieceSkinId;
			bLoadedExisting = true;
		}
	}

	// Union in bUnlockedByDefault entries every load, not just the first — nothing in
	// this system ever revokes a skin, so this is always safe, and it's what lets a
	// later content update add a new default-unlocked skin without stranding it locked
	// for players (or designers testing in-editor) who already have a save on disk.
	const int32 PreviousNum = UnlockedPieceSkins.Num();
	if (UChessPieceSkinRegistry* Registry = LoadSkinRegistry())
	{
		for (const FChessPieceSkinEntry& Entry : Registry->Skins)
		{
			if (Entry.bUnlockedByDefault && !Entry.SkinId.IsNone())
			{
				UnlockedPieceSkins.Add(Entry.SkinId);
			}
		}
	}

	if (!bLoadedExisting || UnlockedPieceSkins.Num() != PreviousNum)
	{
		SaveSkinState();
	}
}

void UChessKidsGameInstance::SaveSkinState() const
{
	if (UChessKidsSaveGame* Save = Cast<UChessKidsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UChessKidsSaveGame::StaticClass())))
	{
		Save->UnlockedPieceSkins = UnlockedPieceSkins;
		Save->SelectedPieceSkinId = SelectedPieceSkinId;
		UGameplayStatics::SaveGameToSlot(Save, SkinSaveSlotName, SkinSaveUserIndex);
	}
}

bool UChessKidsGameInstance::IsPieceSkinUnlocked(FName SkinId) const
{
	return UnlockedPieceSkins.Contains(SkinId);
}

void UChessKidsGameInstance::UnlockPieceSkin(FName SkinId)
{
	if (SkinId.IsNone() || UnlockedPieceSkins.Contains(SkinId))
	{
		return;
	}
	UnlockedPieceSkins.Add(SkinId);
	SaveSkinState();
}

bool UChessKidsGameInstance::SelectPieceSkin(FName SkinId)
{
	// NAME_None always selectable — it means "no skin, use the piece's default look".
	if (!SkinId.IsNone() && !IsPieceSkinUnlocked(SkinId))
	{
		return false;
	}
	SelectedPieceSkinId = SkinId;
	SaveSkinState();
	return true;
}

UMaterialInterface* UChessKidsGameInstance::GetSelectedPieceMaterial() const
{
	if (SelectedPieceSkinId.IsNone())
	{
		return nullptr;
	}
	UChessPieceSkinRegistry* Registry = LoadSkinRegistry();
	if (!Registry)
	{
		return nullptr;
	}
	FChessPieceSkinEntry Entry;
	if (!Registry->FindSkin(SelectedPieceSkinId, Entry))
	{
		return nullptr;
	}
	return Entry.Material.LoadSynchronous();
}
