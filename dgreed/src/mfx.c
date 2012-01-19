#include "mfx.h"

#include "system.h"
#include "particles.h"
#include "darray.h"
#include "datastruct.h"
#include "memory.h"
#include "mml.h"

static const bool preload_all = true; 

typedef enum {
	SUB_SOUND,
	SUB_PARTICLES
} SubEffectType;

typedef struct {
	SubEffectType type;
	const char* name;
	float delay;
	float dir_offset;
	Vector2 pos_offset;
} SubEffect;

typedef size_t SubEffectIdx;

typedef struct {
	bool remove;
	Vector2 pos;
	float dir;
	SubEffectIdx sub;
} LiveSubEffect;

typedef size_t LiveSubEffectIdx;

typedef struct {
	uint sub_start, sub_count;
} MetaEffect;

typedef size_t MetaEffectIdx;

typedef enum {
	SND_AMBIENT,
	SND_EVENT
} SndType;

typedef struct {
	SndType type;
	SoundHandle handle;
	bool loaded;
	float volume;
	float decay, attack, trigger;
} SndDef;

typedef size_t SndDefIdx;

typedef struct {
	SourceHandle handle;
	float volume;
	float trigger_t;
	SndDefIdx def;
} LiveSnd;

MMLObject mfx_mml;
float mfx_volume;
const char* mfx_prefix;

Dict meta_effect_dict;
DArray meta_effects;
DArray sub_effects;
DArray live_sub_effects;
Heap live_sub_effects_pq; 

Dict snd_dict;
DArray snd_defs;
DArray snd_live;

static SubEffect* _get_sub_effect(SubEffectIdx idx) {
	assert(idx < sub_effects.size);
	SubEffect* subs = DARRAY_DATA_PTR(sub_effects, SubEffect);
	return &subs[idx];
}

static LiveSubEffect* _get_live_sub_effect(LiveSubEffectIdx idx) {
	assert(idx < live_sub_effects.size);
	LiveSubEffect* subs = DARRAY_DATA_PTR(live_sub_effects, LiveSubEffect);
	return &subs[idx];
}

static SndDef* _get_snd_def(SndDefIdx idx) {
	assert(idx < snd_defs.size);
	SndDef* defs = DARRAY_DATA_PTR(snd_defs, SndDef);
	return &defs[idx];
}

static MetaEffect* _get_meta_effect(MetaEffectIdx idx) {
	assert(idx < meta_effects.size);
	MetaEffect* metas = DARRAY_DATA_PTR(meta_effects, MetaEffect);
	return &metas[idx];
}

static void _perform_sub_effect(LiveSubEffect* sf) {
	assert(sf);
	assert(!sf->remove);

	// Delay is already taken into account
	
	SubEffect* sub = _get_sub_effect(sf->sub);

	if(sub->type == SUB_SOUND) {
		mfx_snd_play(sub->name);
	}
	else {
		Vector2 p = vec2_add(sf->pos, sub->pos_offset);
		float dir = sf->dir + sub->dir_offset;
		particles_spawn(sub->name, &p, dir);
	}
}

static void _snd_load(SndDef* def, const char* name) {
	assert(def);
	assert(!def->loaded);

	char path[128];
	assert(strlen(mfx_prefix) + strlen(name) < 128);
	strcpy(path, mfx_prefix);
	strcat(path, name);

	// Load
	def->handle = sound_load_sample(path);	
	def->loaded = true;

	// Set volume for events
	if(def->type == SND_EVENT) {
		sound_set_volume(def->handle, def->volume * mfx_volume);
	}
}

static void _load_sounds(NodeIdx node) {
	assert(strcmp("sounds", mml_get_name(&mfx_mml, node)) == 0);

	// Iterate over all sounds
	NodeIdx sound_node = mml_get_first_child(&mfx_mml, node);
	for(; sound_node != 0; sound_node = mml_get_next(&mfx_mml, sound_node)) {
		assert(strcmp("s", mml_get_name(&mfx_mml, sound_node)) == 0);
		const char* snd_name = mml_getval_str(&mfx_mml, sound_node); 

		SndDef new = {
			.type = SND_EVENT,
			.handle = 0,
			.loaded = false,
			.volume = 1.0f,
			.decay = 0.0f,
			.attack = 0.0f,
			.trigger = 0.0f
		};

		// Parse params
		NodeIdx param = mml_get_first_child(&mfx_mml, sound_node);
		for(; param != 0; param = mml_get_next(&mfx_mml, param)) {
			const char* param_name = mml_get_name(&mfx_mml, param);

			if(strcmp("type", param_name) == 0) {
				const char* type = mml_getval_str(&mfx_mml, param);
				if(strcmp("ambient", type) == 0)
					new.type = SND_AMBIENT;
				else if(strcmp("event", type) == 0)
					new.type = SND_EVENT;
				else
					LOG_ERROR("Unknow sound type");
			}

			if(strcmp("volume", param_name) == 0) {
				new.volume = mml_getval_float(&mfx_mml, param);
				assert(0.0f <= new.volume && new.volume <= 1.0f);
			}

			if(strcmp("decay", param_name) == 0) {
				new.decay = mml_getval_float(&mfx_mml, param);
				assert(new.decay >= 0.0f);
			}

			if(strcmp("attack", param_name) == 0) {
				new.attack = mml_getval_float(&mfx_mml, param);
				assert(new.attack >= 0.0f);
			}

			if(strcmp("trigger", param_name) == 0) {
				new.trigger = mml_getval_float(&mfx_mml, param);
				assert(new.trigger > 0.0f);
			}
		}

		if(preload_all)
			_snd_load(&new, snd_name);

		darray_append(&snd_defs, &new);
		SndDefIdx idx = snd_defs.size-1;

#ifdef _DEBUG
		assert(dict_insert(&snd_dict, snd_name, (void*)idx));
#else
		dict_insert(&snd_dict, snd_name, (void*)idx);
#endif
	}
}

static void _load_effects(NodeIdx node) {
	assert(strcmp("effects", mml_get_name(&mfx_mml, node)) == 0);

	// Iterate over meta effects
	NodeIdx mfx_node = mml_get_first_child(&mfx_mml, node);
	for(; mfx_node != 0; mfx_node = mml_get_next(&mfx_mml, mfx_node)) {
		assert(strcmp("e", mml_get_name(&mfx_mml, mfx_node)) == 0);

		const char* mfx_name = mml_getval_str(&mfx_mml, mfx_node);

		MetaEffect new = {
			.sub_start = sub_effects.size,
			.sub_count = 0
		};

		// Sub effects
		NodeIdx sub_node = mml_get_first_child(&mfx_mml, mfx_node);
		for(; sub_node != 0; sub_node = mml_get_next(&mfx_mml, sub_node)) {

			const char* name = mml_get_name(&mfx_mml, sub_node);
			SubEffectType type;

			if(strcmp("sound", name) == 0)
				type = SUB_SOUND;
			else if(strcmp("particles", name) == 0)
				type = SUB_PARTICLES;
			else
				LOG_ERROR("Uknown sub effect type");

			SubEffect sub = {
				.type = type,
				.name = mml_getval_str(&mfx_mml, sub_node),
				.delay = 0.0f,
				.dir_offset = 0.0f,
				.pos_offset = {0.0f, 0.0f}
			};

			// Params 
			NodeIdx param = mml_get_first_child(&mfx_mml, sub_node);
			for(; param != 0; param = mml_get_next(&mfx_mml, param)) {
				const char* param_name = mml_get_name(&mfx_mml, param);

				if(strcmp("delay", param_name) == 0) {
					sub.delay = mml_getval_float(&mfx_mml, param);
					assert(sub.delay >= 0.0f);
				}

				if(strcmp("dir_offset", param_name) == 0) {
					sub.dir_offset = mml_getval_float(&mfx_mml, param) * DEG_TO_RAD;
				}

				if(strcmp("pos_offset", param_name) == 0) {
					sub.pos_offset = mml_getval_vec2(&mfx_mml, param);
				}
			}

			darray_append(&sub_effects, &sub);
			new.sub_count++;
		}

		darray_append(&meta_effects, &new);
		MetaEffectIdx idx = meta_effects.size-1;

#ifdef _DEBUG
		assert(dict_insert(&meta_effect_dict, mfx_name, (void*)idx));
#else
		dict_insert(&meta_effect_dict, mfx_name, (void*)idx);
#endif
	}
}

static void _load_desc(const char* filename) {
	assert(filename);

	char* mml_text = txtfile_read(filename);
	if(!mml_deserialize(&mfx_mml, mml_text))
		LOG_ERROR("Unable to parse mfx desc %s", filename);
	MEM_FREE(mml_text);

	NodeIdx root = mml_root(&mfx_mml);
	if(strcmp("mfx", mml_get_name(&mfx_mml, root)) != 0)
		LOG_ERROR("Invalid mfx desc %s", filename);

	NodeIdx child = mml_get_first_child(&mfx_mml, root);
	for(; child != 0; child = mml_get_next(&mfx_mml, child)) {
		const char* name = mml_get_name(&mfx_mml, child);

		if(strcmp("sounds", name) == 0)
			_load_sounds(child);

		if(strcmp("effects", name) == 0)
			_load_effects(child);

		if(strcmp("prefix", name) == 0) 
			mfx_prefix = mml_getval_str(&mfx_mml, child);
	}
}

void mfx_init(const char* desc) {
	assert(desc);

	dict_init(&meta_effect_dict);
	dict_init(&snd_dict);
	heap_init(&live_sub_effects_pq);

	meta_effects = darray_create(sizeof(MetaEffect), 0);
	sub_effects = darray_create(sizeof(SubEffect), 0);
	live_sub_effects = darray_create(sizeof(LiveSubEffect), 0);
	snd_defs = darray_create(sizeof(SndDef), 0);
	snd_live = darray_create(sizeof(LiveSnd), 0);

	mfx_volume = 1.0f;

	_load_desc(desc);
}

void mfx_close(void) {
	mml_free(&mfx_mml);

	// Check for live sounds, stop them
	LiveSnd* snds = DARRAY_DATA_PTR(snd_live, LiveSnd);
	for(uint i = 0; i < snd_live.size; ++i)
		sound_stop_ex(snds[i].handle);

	// Free loaded sounds
	SndDef* defs = DARRAY_DATA_PTR(snd_defs, SndDef);
	for(uint i = 0; i < snd_defs.size; ++i) {
		if(defs[i].loaded)
			sound_free(defs[i].handle);
	}

	darray_free(&snd_live);
	darray_free(&snd_defs);
	darray_free(&live_sub_effects);
	darray_free(&sub_effects);
	darray_free(&meta_effects);

	heap_free(&live_sub_effects_pq);
	dict_free(&snd_dict);
	dict_free(&meta_effect_dict);
}

static void _snd_update(void);

void mfx_update(void) {
	float t = time_s();
	uint ms = lrintf(t * 1000.0f);

	// Check live sub effects priority queue
	while(	heap_size(&live_sub_effects_pq) > 0 &&
			heap_peek(&live_sub_effects_pq, NULL) <= ms) {
		void* top;
		heap_pop(&live_sub_effects_pq, &top);

		LiveSubEffect* sub = _get_live_sub_effect((LiveSubEffectIdx)top);
		assert(!sub->remove);

		_perform_sub_effect(sub);

		sub->remove = true;
	}

	// Update ambient sounds
	_snd_update();
}

void mfx_trigger(const char* name) {
	mfx_trigger_ex(name, vec2(0.0f, 0.0f), 0.0f);
}

void mfx_trigger_ex(const char* name, Vector2 pos, float dir) {
	assert(name);	

	MetaEffectIdx idx = (MetaEffectIdx)dict_get(&meta_effect_dict, name);
	MetaEffect* mfx = _get_meta_effect(idx);

	// Iterate over sub effects, perform them or push to delayed queue
	for(uint i = 0; i < mfx->sub_count; ++i) {
		SubEffect* sub = _get_sub_effect(mfx->sub_start + i);

		LiveSubEffect live = {
			.remove = false,
			.pos = pos,
			.dir = dir,
			.sub = mfx->sub_start + i
		};

		if(sub->delay == 0.0f) {
			_perform_sub_effect(&live);
		}
		else {
			// Find a removed live effect, or add new one
			LiveSubEffectIdx dest = ~0;
			LiveSubEffect* lives = DARRAY_DATA_PTR(live_sub_effects, LiveSubEffect);
			for(uint j = 0; j < live_sub_effects.size; ++j) {
				if(lives[j].remove) {
					dest = j;
					break;
				}
			}
			if(dest != ~0) {
				lives[dest] = live;
			}
			else {
				darray_append(&live_sub_effects, &live);
				dest = live_sub_effects.size-1;
			}

			// Add it to the priority queue
			uint t = lrintf((time_s() + sub->delay) * 1000.0f);
			heap_push(&live_sub_effects_pq, t, (void*)dest);
		}
	}
}

// ---

float mfx_snd_volume(void) {
	return mfx_volume;
}

void mfx_snd_set_volume(float volume) {
	assert(volume >= 0.0f && volume <= 1.0f);

	mfx_volume = volume;

	// Set event volume
	SndDef* snds = DARRAY_DATA_PTR(snd_defs, SndDef);
	for(uint i = 0; i < snd_defs.size; ++i) {
		if(snds[i].type == SND_EVENT && snds[i].loaded) {
			sound_set_volume(snds[i].handle, snds[i].volume = mfx_volume);
		}
	}
}

static void _snd_update(void) {
	LiveSnd* snds = DARRAY_DATA_PTR(snd_live, LiveSnd);
	for(uint i = 0; i < snd_live.size; ++i) {
		LiveSnd* snd = &snds[i];
		float dt = time_delta() / 1000.0f;
		float t = time_s();

		SndDef* def = _get_snd_def(snd->def);

		if(t - snd->trigger_t < def->trigger)
			snd->volume += def->attack * dt;
		else
			snd->volume -= def->decay * dt;
		
		snd->volume = clamp(0.0f, 1.0f, snd->volume);
		if(snd->volume == 0.0f) {
			// Remove from live sounds
			sound_stop_ex(snd->handle);
			snds[i] = snds[--snd_live.size];
			i--;
		}
		else {
			// Set volume
			sound_set_volume_ex(snd->handle, def->volume * snd->volume * mfx_volume);
		}
	}
}

static LiveSnd* _get_playing(SndDefIdx idx) {
	LiveSnd* snds = DARRAY_DATA_PTR(snd_live, LiveSnd);
	for(uint i = 0; i < snd_live.size; ++i) {
		if(snds[i].def == idx)
			return &snds[i];
	}
	return NULL;
}

void mfx_snd_play(const char* name) {
	SndDefIdx idx = (SndDefIdx)dict_get(&snd_dict, name);
	SndDef* def = _get_snd_def(idx);

	if(!def->loaded)
		_snd_load(def, name);

	if(def->type == SND_AMBIENT) {
		LiveSnd* live = _get_playing(idx);
		if(live) {
			// Already playing, just trigger volume attack
			live->trigger_t = time_s();
		}
		else {
			// Play
			LiveSnd new = {
				.handle = sound_play_ex(def->handle, true),
				.volume = 0.0f,
				.trigger_t = time_s(),
				.def = idx
			};
			sound_set_volume_ex(new.handle, 0.0f);
			darray_append(&snd_live, &new);
		}
	}
	else {
		// Sound volume is already set, so just play
		sound_play(def->handle);
	}
}

void mfx_snd_set_ambient(const char* name, float volume) {
	SndDefIdx idx = (SndDefIdx)dict_get(&snd_dict, name);
	SndDef* def = _get_snd_def(idx);
	LiveSnd* live = NULL;
	
	if(!def->loaded)
		_snd_load(def, name);
	else
		live = _get_playing(idx);

	if(live) {
		live->volume = volume;
	}
	else {
		// Play
		LiveSnd new = {
			.handle = sound_play_ex(def->handle, true),
			.volume = volume,
			.trigger_t = -100.0f,
			.def = idx
		};
		sound_set_volume_ex(new.handle, volume);
		darray_append(&snd_live, &new);
	}
}
