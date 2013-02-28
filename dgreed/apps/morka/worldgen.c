#include "worldgen.h"
#include <mfx.h>
#include <mempool.h>

#include "mchains.h"
#include "obj_types.h"

extern ObjRabbit* rabbit;
extern bool tutorial_level;

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

static int coins = 3;
static int coins_cd = 2;

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

static bool place_powerup(GameObjectDesc* desc, Vector2 pos,PowerupParams *params,PowerupType type){
	uint num = params->count;
	if(num){
		float min = (float)(levels_current_desc()->distance + 2.0f) * (1024.0f/3.0f) * levels_current_desc()->powerup_pos[type].x;
		float max = (float)(levels_current_desc()->distance + 2.0f) * (1024.0f/3.0f) * levels_current_desc()->powerup_pos[type].y;
		float d = (float)num / levels_current_desc()->powerup_num[type];
		float place = max - ((max-min) * d);

		if(pos.x > place && pos.x < max){
			objects_create(desc, pos,(void*)params);
			params->count--;
			return true;
		}
	}
	return false;
}

static void _gen_fg_page(void) {
	static int prev_advance = 0;
	static int previuos_gaps = 0;
	bool gap_possible = false;
	bool gap_warning = false;
	
	SprHandle spr;	
	uint advance = 0;
	
	if(previuos_gaps > 0)
		gaps_i = 1;
	else 
		gaps_i = 0;


	float water_start = 0.0f;
	float water_end = 0.0f;

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

					assert(gaps_i < max_gaps);
				}
				// Coin over gap start/end
				if(!tutorial_level){
					Vector2 size = sprsheet_get_size_h(spr);
					objects_create(&obj_powerup_desc, vec2(pos.x + size.x/2.0f,479.0f), (void*)&coin_powerup);
				}
			} else {
				if(sym == 'g') {
					gap_possible = true;
				}
				if(sym == 'q') {
					gap_warning = true;
				}
				objects_create(&obj_ground_desc, pos, (void*)spr);
				if(sym == 'j' || sym == 'k' || sym == 'l' || sym == 'm' || sym == 'n' || sym == 'o'){
					if(water_start == 0) water_start = pos.x;
					water_end = pos.x + advance;
					ObjSpeedTrigger* t = (ObjSpeedTrigger*)objects_create(&obj_speed_trigger_desc, pos, (void*)spr);
					t->drag_coef = 1.9;
				}
				if(sym == 'i'){
					water_start = pos.x;
					water_end = pos.x + advance;
				}
				if(sym == 'p') water_end = pos.x + advance;
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
				// Fall trigger
				objects_create(&obj_fall_trigger_desc, pos, (void*)advance);

				if(!tutorial_level){
					// Coins over gap
					objects_create(&obj_powerup_desc, vec2(pos.x + prev_advance + advance/2.0f,400.0f),(void*)&coin_powerup);
					objects_create(&obj_powerup_desc, vec2(pos.x - advance/2.5f + prev_advance + advance/2.0f,425.0f),(void*)&coin_powerup);
					objects_create(&obj_powerup_desc, vec2(pos.x + advance/2.5f + prev_advance + advance/2.0f,425.0f),(void*)&coin_powerup);
				}
			}
		}
		ground_x += (float)advance;
	}
	if(previuos_gaps > 1) previuos_gaps = 1;
	previuos_gaps = gaps_i - previuos_gaps;

	// Add foreground mushrooms
	static float fg_x = page_width;
	fg_x -= page_width;
	while(fg_x < page_width) {
		
		char sym = mchains_next(fg_chain, &rnd);
		mchains_symbol_info(fg_chain, sym, &advance, &spr);
		Vector2 pos = vec2(fg_page_cursor + fg_x, 641.0f);
		uint shroom_width = (uint) sprsheet_get_size_h(spr).x;
		bool place = false;
	
		if(spr){
			const float dist = 500.0f;
			place = true;

			if( gap_possible && (pos.x+shroom_width > fg_page_cursor + ground_x ) ) place = false;

			for(int i = 1; i <= gaps_i;i++){
				if(	(pos.x > gaps[i].x && pos.x < gaps[i].y) ||
					(pos.x+shroom_width > gaps[i].x && pos.x+shroom_width < gaps[i].y) ||
					(pos.x < gaps[i].x && pos.x+shroom_width > gaps[i].y )){
					place = false;
				}

				if(sym == 'x'){

					// no cactus within 500 pixels of a gap
					if(	(pos.x > gaps[i].x - dist && pos.x < gaps[i].y + dist) ||
						(pos.x + shroom_width > gaps[i].x - dist && pos.x + shroom_width < gaps[i].y + dist) ||
						(pos.x > gaps[i].x - dist && pos.x + shroom_width < gaps[i].y + dist )
					)place = false;		
				}

			}

			if(place && sym == 'x' && ( gap_warning && (pos.x + shroom_width + dist > fg_page_cursor + ground_x) )) place = false;

			// no cactus on water tiles
			if(place && sym == 'x'){					
				if(	water_end > water_start && 
					(
						(pos.x > water_start && pos.x < water_end) ||
						(pos.x + shroom_width > water_start && pos.x + shroom_width < water_end) ||
						(pos.x < water_start && pos.x + shroom_width > water_end ) 
					)
				)place = false;
			}	

		} else if (!tutorial_level && coins > 0){
				coins_cd = 2;
				bool c = true;
				for(int i = 1; i <= gaps_i;i++){
					if(	(pos.x > gaps[i].x && pos.x < gaps[i].y) ||
						(pos.x + 250.0f > gaps[i].x && pos.x + 250.0f < gaps[i].y) ||
						(pos.x < gaps[i].x && pos.x + 250.0f > gaps[i].y )){
						c = false;
					}
				}
				if(c){
					objects_create(&obj_powerup_desc, vec2(pos.x + advance / 2.0f, 579.0f), (void*)&coin_powerup);
					coins--;				
				}
		}
				
		if(place) {
			if(sym == 'x'){
				objects_create(&obj_cactus_desc, pos, (void*)spr);

				// placing bomb powerup after cactuses
				place_powerup(&obj_powerup_desc, vec2(pos.x + advance / 2.0f + 100.0f, 579.0f), &bomb_powerup, BOMB);
				
			}
			else {
				objects_create(&obj_mushroom_desc, pos, (void*)spr);
				
				if(!tutorial_level){
					// Placing coins on big shrooms
					Vector2 size = sprsheet_get_size_h(spr);
					float width = size.x;
					float height = size.y;

					if(sym == 'j'){

						// Place rocket or a coin on top of mushroom
						if(!place_powerup(&obj_powerup_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f)), &rocket_powerup, ROCKET))
							objects_create(&obj_powerup_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f) ), (void*)&coin_powerup);

					} else if(sym == 'h'){

						// Place shield or a coin on top of mushroom
						if(!place_powerup(&obj_powerup_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f)), &shield_powerup, SHIELD))
							objects_create(&obj_powerup_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f) ), (void*)&coin_powerup);

					} else if(height > 270.0f)
						objects_create(&obj_powerup_desc, vec2_add( pos, vec2(width/2.0f,-height - 50.0f) ), (void*)&coin_powerup);
				}
			}	

		}
	
		fg_x += (float)advance;

	}
	
	if(coins_cd > 0) coins_cd--;
	if(coins_cd == 0) coins = 3;

	fg_page_cursor += page_width;
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
	coins = 3;
	coins_cd = 2;	
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

