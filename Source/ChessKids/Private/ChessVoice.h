#pragma once

#include "CoreMinimal.h"

// Plays the generated ElevenLabs voice-over clips under Content/Audio/VO.
// The line table (VoiceLineMap.gen.h) is generated from Tools/vo_manifest.json
// by `python Tools/generate_vo.py --emit-cpp` — regenerate it whenever a
// manifest line or a matching in-game string changes.
struct FChessVoice
{
	// Play a clip by manifest id (e.g. "axiom_core_intro"). Any voice line
	// still playing is cut off first — one narrator at a time.
	static void PlayLine(UObject* WorldContext, const FString& ClipId);

	// Play the clip whose source text equals this guide message, if one
	// exists. Returns true if a clip started.
	static bool PlayForMessage(UObject* WorldContext, const FString& Message);
};
