#include "worldgen.h"

#include <mempool.h>

#include "mchains.h"
#include "obj_types.h"

#define page_width 1024.0f
static float fg_page_cursor = 0.0f;
static float bg_page_cursor = 0.0f;

static RndContext rnd = NULL;
static Chain* fg_chain;
static Chain* bg_chain;

static void _gen_bg_page(void) {
	SprHandle spr;	
	uint advance;
	
	// Add background mushrooms
	static float bg_x = page_width;
	bg_x -= page_width;	
	while(bg_x < page_width) {
		char sym = mchains_next(bg_chain, &rnd);
		mchains_symbol_info(bg_chain, sym, &advance, &spr);

		if(spr) {
			Vector2 pos = vec2(bg_page_cursor + bg_x + 100.0f, 483.0f);
			objects_create(&obj_deco_desc, pos, (void*)spr);
		}

		bg_x += (float)advance;
	}

	bg_page_cursor += page_width;
}

static void _gen_fg_page(void) {
	// Add ground
	for(uint i = 0; i < 4; ++i) {
		Vector2 pos = vec2(fg_page_cursor + 128.0f + i * 256.0f, 683.0f);
		objects_create(&obj_ground_desc, pos, (void*)i);
	}

	SprHandle spr;	
	uint advance;
	
	// Add foreground mushrooms
	static float fg_x = page_width;
	fg_x -= page_width;
	while(fg_x < page_width) {
		char sym = mchains_next(fg_chain, &rnd);
		mchains_symbol_info(fg_chain, sym, &advance, &spr);

		if(spr) {
			Vector2 pos = vec2(fg_page_cursor + fg_x + 100.0f, 583.0f);
			objects_create(&obj_mushroom_desc, pos, (void*)spr);
		}

		fg_x += (float)advance;
	}

	fg_page_cursor += page_width;
}
void worldgen_reset(uint seed) {
	if(!rnd) {
		// First time
		rand_init_ex(&rnd, seed);
	}
	else {
		// Not first time
		rand_seed_ex(&rnd, seed);
		fg_page_cursor = 0.0f;
		bg_page_cursor = 0.0f;
		mchains_del(fg_chain);
		mchains_del(bg_chain);
	}

	fg_chain = mchains_new("fg");
	bg_chain = mchains_new("bg");

	_gen_bg_page();
	_gen_fg_page();
}

void worldgen_close(void) {
	mchains_del(fg_chain);
	mchains_del(bg_chain);
	rand_free_ex(&rnd);
}

void worldgen_update(float camera_extent_max) {
	if(camera_extent_max >= fg_page_cursor)
		_gen_fg_page();
	if(camera_extent_max >= bg_page_cursor)
		_gen_bg_page();
}

