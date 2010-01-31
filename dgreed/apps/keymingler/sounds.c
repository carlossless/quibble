#include "sounds.h"
#include <system.h>
#include "common.h" 

#define BURN_FILE ASSETS_DIR "sudega.wav"
#define BULBUL_FILE ASSETS_DIR "bulbul.wav"
#define SINKED_FILE ASSETS_DIR "issilieja.wav"
#define MISSED_FILE ASSETS_DIR "juokas.wav"

SoundHandle sound_burning;
SoundHandle sound_bulbul;
SoundHandle sound_sinked;
SoundHandle sound_missed;

void sounds_init(void)
{
	sound_init();

	sound_burning = sound_load_sample(BURN_FILE);
	sound_bulbul = sound_load_sample(BULBUL_FILE);
	sound_sinked = sound_load_sample(SINKED_FILE);
	sound_missed = sound_load_sample(MISSED_FILE);
}

void sounds_close(void)
{
	sound_free(sound_burning);
	sound_free(sound_bulbul);
	sound_free(sound_sinked);
	sound_free(sound_missed);

	sound_close();
}

