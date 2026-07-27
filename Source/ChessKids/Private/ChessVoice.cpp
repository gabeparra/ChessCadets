#include "ChessVoice.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
	struct FVoiceLineEntry
	{
		const TCHAR* Match;      // in-game message text, or nullptr if only playable by id
		const TCHAR* Character;  // VO subfolder / filename part ("Guide", "AXIOM")
		const TCHAR* Id;         // manifest line id
	};

#include "VoiceLineMap.gen.h"

	constexpr float VoiceVolume   = 1.6f;   // voice sits above the mix
	constexpr float DuckLevel     = 0.15f;  // music/sfx volume scale while a voice line plays
	constexpr int32 MaxQueuedLines = 4;

	TWeakObjectPtr<UWorld>          GVoiceWorld;
	TWeakObjectPtr<UAudioComponent> GActiveVoice;
	TArray<const FVoiceLineEntry*>  GQueue;      // entries point into the static GVoiceLines table
	struct FDuckedComponent
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		float OriginalVolume = 1.f;
	};
	TArray<FDuckedComponent> GDucked;
	FTimerHandle                    GVoiceTimer;

	void StartEntry(UWorld* World, const FVoiceLineEntry& Entry);

	void RestoreDucked()
	{
		for (FDuckedComponent& Ducked : GDucked)
		{
			if (Ducked.Comp.IsValid())
			{
				Ducked.Comp->AdjustVolume(0.5f, Ducked.OriginalVolume);
			}
		}
		GDucked.Empty();
	}

	void DuckOthers(UWorld* World)
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			UAudioComponent* Comp = *It;
			if (Comp && Comp->GetWorld() == World && Comp->IsPlaying()
				&& Comp != GActiveVoice.Get()
				&& !GDucked.ContainsByPredicate([Comp](const FDuckedComponent& Ducked)
				{
					return Ducked.Comp == Comp;
				}))
			{
				const float OriginalVolume = Comp->VolumeMultiplier;
				Comp->AdjustVolume(0.3f, DuckLevel);
				GDucked.Add({Comp, OriginalVolume});
			}
		}
	}

	void OnVoiceFinished(UWorld* World)
	{
		if (World && GQueue.Num() > 0)
		{
			const FVoiceLineEntry* Next = GQueue[0];
			GQueue.RemoveAt(0);
			StartEntry(World, *Next);
			return;
		}
		RestoreDucked();
	}

	void StartEntry(UWorld* World, const FVoiceLineEntry& Entry)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/Audio/VO/%s/VO_%s_%s.VO_%s_%s"),
			Entry.Character, Entry.Character, Entry.Id, Entry.Character, Entry.Id);
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
		if (!Sound)
		{
			UE_LOG(LogTemp, Warning, TEXT("ChessVoice: missing clip %s"), *Path);
			OnVoiceFinished(World);   // keep the queue draining
			return;
		}

		GActiveVoice = UGameplayStatics::SpawnSound2D(World, Sound, VoiceVolume);
		DuckOthers(World);

		World->GetTimerManager().SetTimer(GVoiceTimer,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World)]()
			{
				OnVoiceFinished(WeakWorld.Get());
			}),
			Sound->GetDuration() + 0.2f, false);
	}

	void PlayEntry(UObject* WorldContext, const FVoiceLineEntry& Entry)
	{
		UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		if (!World) return;

		if (GVoiceWorld.Get() != World)   // level changed: old queue/duck state is stale
		{
			GQueue.Empty();
			GDucked.Empty();
			GActiveVoice = nullptr;
			GVoiceWorld = World;
		}

		if (GActiveVoice.IsValid() && GActiveVoice->IsPlaying())
		{
			if (GQueue.Num() < MaxQueuedLines)
			{
				GQueue.Add(&Entry);
			}
			return;
		}
		StartEntry(World, Entry);
	}
}

void FChessVoice::PlayLine(UObject* WorldContext, const FString& ClipId)
{
	for (const FVoiceLineEntry& Entry : GVoiceLines)
	{
		if (ClipId == Entry.Id)
		{
			PlayEntry(WorldContext, Entry);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("ChessVoice: unknown clip id '%s'"), *ClipId);
}

bool FChessVoice::PlayForMessage(UObject* WorldContext, const FString& Message)
{
	for (const FVoiceLineEntry& Entry : GVoiceLines)
	{
		if (Entry.Match && Message == Entry.Match)
		{
			PlayEntry(WorldContext, Entry);
			return true;
		}
	}
	return false;
}
