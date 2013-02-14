#include "worldgen.h"
#include <mfx.h>
#include <mempool.h>

#include "mchains.h"
#include "obj_types.h"

extern ObjRabbit* rabbit;

#define page_width 1024.0f
static float fg_page_cursor = 0.0f;
static float bg_page_cursor = 0.0f;

static RndContext rnd = NULL;
static Chain* fg_chain;
static Chain* bg_chain;
static Chain* ground_chain;

static Vector2 gaps[5];
static const int max_gaps = 5;
static int gaps_i = 0;

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
			Vector2 pos = vec2(bg_page_cursor + bg_x + 100.0f, 768.0f);
			objects_create(&obj_deco_desc, pos, (void*)spr);
		}

		bg_x += (float)advance;
	}

	bg_page_cursor += page_width;
}


void worldgen_debug_render(){
	for(int i = 1; i <= gaps_i;i++){
		RectF pos = {
			.left = gaps[i].x, 
			.top = 580,
			.right = gaps[i].y,
			.bottom = 580
		};
		RectF result = objects_world2screen(pos,0);
		
		Vector2 start = vec2(result.left, 0);
		Vector2 end = vec2(result.left, 768);
		
		video_draw_line(10,	&start, &end, COLOR_RGBA(255, 0, 0, 255));
		
		start = vec2(result.right, 0);
		end = vec2(result.right, 768);
		
		video_draw_line(10,	&start, &end, COLOR_RGBA(255, 0, 0, 255));
		
		start = vec2(result.left, 0);
		end = vec2(result.right, 768);
		
		video_draw_line(10,	&start, &end, COLOR_RGBA(255, 0, 0, 255));
		
		start = vec2(result.right, 0);
		end = vec2(result.left, 768);
		
		video_draw_line(10,	&start, &end, COLOR_RGBA(255, 0, 0, 255));
	}
}



static void _gen_fg_page(void) {
	static int prev_advance = 0;
	static int previuos_gaps = 0;
	bool gap_possible = false;
	
	SprHandle spr;	
	uint advance = 0;
	
	if(previuos_gaps > 0)
		gaps_i = 1;
	else 
		gaps_i = 0;

	// Add ground
	static float ground_x = page_width;
	ground_x -= page_width;
	while(ground_x < page_width) {
		gap_possible = false;
		char sym = mchains_next(ground_chain, &rnd);
		mchains_symbol_info(ground_chain, sym, &advance, &spr);
		if(spr) {
			Vector2 pos = vec2(fg_page_cursor + ground_x, 768.0f);
			advance = (uint) sprsheet_get_size_h(spr).x;
			if(sym == 'a' || sym == 'h'){	// no collision for grass_start1 and grass_end2
				objects_create(&obj_fg_deco_desc, pos, (void*)spr);
				if(prev_advance != 0){
					gaps[gaps_i].y += advance;
				} else {
					prev_advance = advance;
					
					gaps[++gaps_i].x = pos.x;
					gaps[gaps_i].y = pos.x + advance;

					if(gaps_i > max_gaps){ 
						gaps_i = 0;
						printf("gaps_i > max_gaps !\n");
					}
				}
				// Token over gap start/end
				Vector2 size = sprsheet_get_size_h(spr);
				float width = size.x;
				float height = size.y;
				objects_create(&obj_token_desc, vec2(pos.x + width/2.0f,479.0f), (void*)sprsheet_get_handle("token"));

			} else {
				if(sym == 'g') {
					gap_possible = true;
				}
				objects_create(&obj_ground_desc, pos, (void*)spr);
				if(sym == 'j' || sym == 'k' || sym == 'l' || sym == 'm' || sym == 'n' || sym == 'o'){
					ObjSpeedTrigger* t = (ObjSpeedTrigger*)objects_create(&obj_speed_trigger_desc, pos, (void*)spr);
					t->drag_coef = 1.9;
				}
				prev_advance = 0;
			}		
		} else {
			if(sym == '_' || sym == '-' || sym == '='){
				Vector2 pos = vec2(fg_page_cursor + ground_x - prev_advance, 768.0f);
				
				if(prev_advance != 0){
					gaps[gaps_i].y += advance;
				} else {
					prev_advance = advance;
					
					gaps[++gaps_i].x = pos.x;
					gaps[gaps_i].y = pos.x + advance;

					if(gaps_i > max_gaps){ 
						gaps_i = 0;
						printf("gaps_i > max_gaps !\n");
					}
				}
				// Tokens over gap
				objects_create(&obj_token_desc, vec2(pos.x + prev_advance + advance/2.0f,400.0f), (void*)sprsheet_get_handle("token"));
				objects_create(&obj_token_desc, vec2(pos.x - advance/2.5f + prev_advance + advance/2.0f,425.0f), (void*)sprsheet_get_handle("token"));
				objects_create(&obj_token_desc, vec2(pos.x + advance/2.5f + prev_advance + advance/2.0f,425.0f), (void*)sprsheet_get_handle("token"));
				// Fall trigger
				objects_create(&obj_fall_trigger_desc, pos, (void*)advance);
			}
		}
		ground_x += (float)advance;
	}
	if(previuos_gaps > 1) previuos_gaps = 1;
	previuos_gaps = gaps_i - previuos_gaps;

	// Add foreground mushrooms
	static float fg_x = page_width;
	static int place_coins = 3;
	fg_x -= page_width;
	while(fg_x < page_width) {
		
		char sym = mchains_next(fg_chain, &rnd);
		mchains_symbol_info(fg_chain, sym, &advance, &spr);
		Vector2 pos = vec2(fg_page_cursor + fg_x, 641.0f);
		uint shroom_width = (uint) sprsheet_get_size_h(spr).x;
		bool place = false;
	
		if(spr){
			place = true;
			if((gap_possible && (pos.x+shroom_width > fg_page_cursor + page_width + (ground_x-page_width)) )) place = false;

			for(int i = 1; i <= gaps_i;i++){
				if(	(pos.x > gaps[i].x && pos.x < gaps[i].y) ||
					(pos.x+shroom_width > gaps[i].x && pos.x+shroom_width < gaps[i].y) ||
					(pos.x < gaps[i].x && pos.x+shroom_width > gaps[i].y )){
					place = false;
				}

				if(sym == 'x'){
					const float dist = 256.0f;
					if(	(pos.x > gaps[i].x - dist				&& pos.x < gaps[i].y + dist) ||
						(pos.x+shroom_width > gaps[i].x - dist && pos.x+shroom_width < gaps[i].y + dist) ||
						(pos.x < gaps[i].x - dist 				&& pos.x+shroom_width > gaps[i].y + dist )
					)place = false;				
				}

			}
		} else {
			if(place_coins <= 0){
				bool c = true;
				for(int i = 1; i <= gaps_i;i++){
					if(	(pos.x > gaps[i].x && pos.x < gaps[i].y) ||
						(pos.x+250.0f > gaps[i].x && pos.x+250.0f < gaps[i].y) ||
						(pos.x < gaps[i].x && pos.x+250.0f > gaps[i].y )){
						c = false;
					}
				}
				if(c){
					place_coins = 3;
					objects_create(&obj_token_desc, vec2(pos.x + 50.0f,	579.0f), (void*)sprsheet_get_handle("token"));
					objects_create(&obj_token_desc, vec2(pos.x + 150.0f,579.0f), (void*)sprsheet_get_handle("token"));
					objects_create(&obj_token_desc, vec2(pos.x + 250.0f,579.0f), (void*)sprsheet_get_handle("token"));					
				}
			}
		}
				
		if(place) {
			GameObject* g = objects_create(&obj_mushroom_desc, pos, (void*)spr);
			ObjMushroom* shroom = (ObjMushroom*)g;
			if(sym == 'x')
				shroom->damage = 1.0f;
			else{
				shroom->damage = 0.0f;	

				// Placing tokens on big shrooms
				Vector2 size = sprsheet_get_size_h(spr);
				float width = size.x;
				float height = size.y;
				if(height > 270.0f)
					objects_create(&obj_token_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f) ), (void*)sprsheet_get_handle("token"));
			}	

		}
	
		fg_x += (float)advance;
	}

	fg_page_cursor += page_width;
	place_coins--;
	// save last gap for next page
	gaps[1].x = gaps[gaps_i].x;
	gaps[1].y = gaps[gaps_i].y; 	
}
void worldgen_reset(uint seed, const LevelDesc* desc) {
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
		mchains_del(ground_chain);
	}

	bg_chain = mchains_new(desc->bg_chain);
	fg_chain = mchains_new(desc->fg_chain);
	ground_chain = mchains_new(desc->ground_chain);

	_gen_bg_page();
	_gen_fg_page();
}

void worldgen_close(void) {
	mchains_del(fg_chain);
	mchains_del(bg_chain);
	mchains_del(ground_chain);
	rand_free_ex(&rnd);
}

void worldgen_update(float fg_camera_extent_max, float bg_camera_extent_max) {
	if(fg_camera_extent_max >= fg_page_cursor)
		_gen_fg_page();
	if(bg_camera_extent_max >= bg_page_cursor)
		_gen_bg_page();
}

