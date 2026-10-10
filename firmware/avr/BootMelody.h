#pragma once

#include "TonePlayer.h"
#include <Protocol.h>

// Queue the project welcome melody without blocking the main loop.
// Calling this function replaces any melody currently queued in the player.
void playBootMelody(TonePlayer &player = buzzer);

// Queue one complete bounded catalog entry. Invalid IDs do not interrupt audio.
bool playAudioCue(BoardCore::Protocol::AudioCue cue, TonePlayer &player = buzzer);
