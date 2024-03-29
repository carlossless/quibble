#ifndef MFX_H
#define MFX_H

#include "utils.h"

/*
Metaeffect system

Example desc:
(mfx _
	(prefix "nulis2_assets/")
	(sounds _
		(s wind.wav 
			(type ambient) # can be 'ambient' or 'event', default is event 
			(volume 0.8)   # max volume, default is 1
			(decay 0.5)	   # how quickly volume decays to 0
			(attack 1)	   # how quickly volume reaches 1 after trigger
			(trigger 0.3)  # length of attack phase after trigger
		)
		(s hit.wav
			(volume 0.3)
		)

		(s step1.wav)
		(s step2.wav)
		(s step3.wav)
	)

	(effects _
		(e collide
			(sound hit.wav)
			(sound hit_echo.wav
				(delay 0.4)
			)
			(particles hit_a)
			(particles hit_a
				(dir_offset 180) 
			)
			(particles hit_b
				(delay 0.3)
				(pos_offset 0,-10)
			)
		)

		(e wind
			(sound wind.wav)
		)

		(e step3
			(sound step3.wav)
		)

		(e steps
			(random ->
				(weight 2)
				(e sound_a
					(sound step1.wav)
				)
			)

			(random ->
				(delay 0.1)
				(e sound_b
					(sound step2.wav)
				)
			)

			(random step3)
		)
	)
)

*/

void mfx_init(const char* desc);
void mfx_close(void);
void mfx_update(void);

void mfx_trigger(const char* name);
void mfx_trigger_ex(const char* name, Vector2 pos, float dir);
void mfx_trigger_follow(const char* name, const Vector2* pos, const float* dir);

float mfx_snd_volume(void);
void mfx_snd_set_volume(float volume);
void mfx_snd_play(const char* name);
void mfx_snd_set_ambient(const char* name, float volume);

#endif
