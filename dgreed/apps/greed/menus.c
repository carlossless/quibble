#include "menus.h"

#include <system.h>
#include <font.h>
#include <gui.h>
#include <gfx_utils.h>

#include "arena.h"
#include "game.h"
#include "ai.h"

MenuState menu_state;
MenuState menu_transition;
float menu_transition_t;

#define MENU_BACKGROUND_LAYER 1
#define MENU_PANEL_LAYER  2
#define MENU_SHADOW_LAYER 3
#define MENU_TEXT_LAYER 12

#define BACKGROUND_IMG "greed_assets/back_chapter_1.png"
#define MENU_ATLAS_IMG "greed_assets/menu_atlas.png"

TexHandle background;
TexHandle menu_atlas;

static RectF background_source = {0.0f, 0.0f, 480.0f, 320.0f};
static RectF panel_source = {0.0f, 0.0f, 360.0f, 225.0f};
static RectF title_source = {362.0f, 0.0f, 362.0f + 70.0f, 256.0f};
static RectF separator_source = {0.0f, 246.0f, 322.0f, 246.0f + 8.0f};

static Color menu_text_color = COLOR_RGBA(214, 214, 215, 255);
static Color menu_sel_text_color = COLOR_RGBA(166, 166, 168, 255);

extern FontHandle huge_font;

// Tweakables
float menu_transition_length = 0.5f;
float menu_animation_depth = 0.5f;

void menus_init(void) {
	background = tex_load(BACKGROUND_IMG);
	menu_atlas = tex_load(MENU_ATLAS_IMG);

	menu_state = menu_transition = MENU_MAIN;
}

void menus_close(void) {
	tex_free(background);
	tex_free(menu_atlas);
}

void menus_update(void) {
}

Vector2 _adjust_scale_pos(Vector2 p, Vector2 center, float scale) {
	Vector2 c_to_p = vec2_sub(p, center);
	c_to_p = vec2_scale(c_to_p, scale);
	return vec2_add(center, c_to_p);
}

float _t_to_scale(float t) {
	float max = 1.0f + menu_animation_depth;
	float min = 1.0f - menu_animation_depth;
	t = smoothstep(0.0f, 1.0f, (t + 1.0f) / 2.0f);
	return 1.0f / lerp(max, min, t);
}

bool _menu_button(const Vector2* center, const char* text,
	const Vector2* ref, float t) {

	uint mx, my;
	mouse_pos(&mx, &my);
	Vector2 vmouse = vec2((float)mx, (float)my);

	Color text_col = color_lerp(menu_text_color, menu_text_color && 0xFFFFFF, 
		fabs(t));
	Color selected_text_col = color_lerp(menu_sel_text_color, 
		menu_sel_text_color && 0xFFFFFF, fabs(t));	

	float scale = _t_to_scale(t);
	Vector2 adj_vdest = _adjust_scale_pos(*center, *ref, scale);

	RectF rect = font_rect_ex(huge_font, text,
		&adj_vdest, scale);

	// Make rect smaller in y direction to prevent multiple
	// items being selected at once
	rect.top += 8.0f;
	rect.bottom -= 8.0f;

	bool mouse_inside = rectf_contains_point(&rect, &vmouse);
	Color curr_text_col = mouse_inside ? selected_text_col : text_col;

	font_draw_ex(huge_font, text, MENU_TEXT_LAYER, 
		&adj_vdest, scale, curr_text_col);

	return mouse_inside && mouse_down(MBTN_LEFT); 	
}

void _menu_text(const Vector2* topleft, const char* text,
	const Vector2* ref, float t) {

	if(!text)
		return;

	Color text_col = color_lerp(menu_text_color, menu_text_color && 0xFFFFFF, 
		fabs(t));

	float scale = _t_to_scale(t);
	Vector2 adj_vdest = _adjust_scale_pos(*topleft, *ref, scale);
	RectF rect = font_rect_ex(huge_font, text,
		&adj_vdest, scale);

	adj_vdest.x += rectf_width(&rect) / 2.0f;
	adj_vdest.y += rectf_height(&rect) / 2.0f;

	font_draw_ex(huge_font, text, MENU_TEXT_LAYER, 
		&adj_vdest, scale, text_col);
}

void _render_main(float t) {
	const char* items[] = {
		"Play",
		"Settings",
		"Info"
	};

	float scale = _t_to_scale(t);

	Color c = color_lerp(COLOR_WHITE, COLOR_TRANSPARENT, fabs(t));

	// Panel
	Vector2 center = vec2(240.0f, 168.0f);
	gfx_draw_textured_rect(menu_atlas, MENU_PANEL_LAYER, &panel_source,
		&center, 0.0f, scale, c);

	// Title
	Vector2 vdest = _adjust_scale_pos(vec2(240.0f, 46.0f), center, scale);
	gfx_draw_textured_rect(menu_atlas, MENU_TEXT_LAYER, &title_source,
		&vdest, -PI/2.0f, scale, c);
	
	// Text
	vdest = vec2(240.0f, 139.0f);
	for(uint i = 0; i < ARRAY_SIZE(items); ++i) {
		if(_menu_button(&vdest, items[i], &center, t) && t==0.0f) {
			if(i == 0)
				menu_transition = MENU_CHAPTER;
			
			menu_transition_t = time_ms() / 1000.0f;
		}

		if(i < ARRAY_SIZE(items)-1) {
			vdest.y += 19.0f;
			Vector2 adj_vdest = _adjust_scale_pos(vdest, center, scale);
			gfx_draw_textured_rect(menu_atlas, MENU_SHADOW_LAYER,
				&separator_source, &adj_vdest, 0.0f, scale, c);	
		}

		vdest.y += 17.0f;	
	}
}

// Sliding menu handling
bool _mouse_acrobatics(float* camera_lookat_x, float* old_camera_lookat_x,
	uint item_count, float x_spacing, Vector2 center, RectF sliding_area,
	float width, float height, float t) {

	if(t != 0.0f)
		return false;

	uint mx, my;
	mouse_pos(&mx, &my);
	Vector2 vmouse = vec2((float)mx, (float)my);

	static Vector2 down_vmouse = {0.0f, 0.0f};
	static bool sliding_action = false;

	if(rectf_contains_point(&sliding_area, &vmouse)) {
		if(mouse_down(MBTN_LEFT)) {
			down_vmouse = vmouse;
			sliding_action = false;
		}

		if(mouse_pressed(MBTN_LEFT)) {
			float dist_sq = vec2_length_sq(vec2_sub(vmouse, down_vmouse));
			if(!sliding_action && dist_sq > 100.0f) {
				sliding_action = true;
				*old_camera_lookat_x = *camera_lookat_x;
			}	
			
			if(sliding_action) {
				float dx = vmouse.x - down_vmouse.x;
				*camera_lookat_x = lerp(*camera_lookat_x,
					*old_camera_lookat_x + dx*2.0f, 0.3f);
			}
		}
	}
	else {
		if(sliding_action)
			goto end_sliding;
	}

	if(mouse_up(MBTN_LEFT)) {
		end_sliding:
		if(sliding_action) {
			float frac, intgr;
			frac = modff(-*camera_lookat_x/x_spacing, &intgr);
			if(frac > 0.5f)
				*old_camera_lookat_x = intgr + 1.0f;
			else
				*old_camera_lookat_x = intgr;

			if(*old_camera_lookat_x < 0.0f)
				*old_camera_lookat_x = 0.0f;
			if(*old_camera_lookat_x > (float)(item_count-1))
				*old_camera_lookat_x = (float)(item_count-1);

			*old_camera_lookat_x *= -x_spacing;	
			
			sliding_action = false;
		}
		else {
			if(vmouse.y > center.y - height/2.0f &&
				vmouse.y < center.y + height/2.0f) {
				
				if(vmouse.x > center.x + width/2.0f) {
					*old_camera_lookat_x /= -x_spacing;
					*old_camera_lookat_x += 1.0f;
					*old_camera_lookat_x = 
						MIN(*old_camera_lookat_x, (float)(item_count-1));
					*old_camera_lookat_x *= -x_spacing;	
				}
				else if(vmouse.x < center.x - width/2.0f) {
					*old_camera_lookat_x /= -x_spacing;
					*old_camera_lookat_x -= 1.0f;
					*old_camera_lookat_x = MAX(*old_camera_lookat_x, 0.0f);
					*old_camera_lookat_x *= -x_spacing;	
				}	
				else
					return true;
			}	
		}
	}

	*camera_lookat_x = lerp(*camera_lookat_x, *old_camera_lookat_x, 0.2f);

	return false;
}	

static int selected_chapter = 0;
static float chp_camera_lookat_x = 0.0f;
static float chp_old_camera_lookat_x = 0.0f;
static float arn_camera_lookat_x = 0.0f;
static float arn_old_camera_lookat_x = 0.0f;
static const float x_spacing = 380.0f;

bool _render_slidemenu(float t, float* camera_lookat_x, 
	float* old_camera_lookat_x, MenuState back, const char** text,
	uint n_items) {

	const uint item_count = n_items;
	const float y_coord = 160.0f;
	const float x_start = 240.0f;
	const Vector2 center = {240.0f, 160.0f};

	float scale = _t_to_scale(t);

	float width = rectf_width(&panel_source) * scale;
	float height = rectf_height(&panel_source) * scale;
	RectF sliding_area = rectf(0.0f, y_coord - height/2.0f, 
		480.0f, y_coord + height/2.0f);

	Color c = color_lerp(COLOR_WHITE, COLOR_TRANSPARENT, fabs(t));

	bool result = false;

	if(_mouse_acrobatics(camera_lookat_x, old_camera_lookat_x, 
		item_count, x_spacing, center, sliding_area, width, height, t)) {

		result = true;
	}

	for(uint i = 0; i < item_count; ++i) {
		Vector2 vdest = _adjust_scale_pos(
			vec2(x_start + *camera_lookat_x + (float)i * x_spacing, y_coord), 
			center, scale);

		if(vdest.x > -width/2.0f && vdest.x < 480.0f + width/2.0f) {
			gfx_draw_textured_rect(menu_atlas, MENU_PANEL_LAYER,
				&panel_source, &vdest, 0.0f, scale, c);
			
			vdest =	
				vec2(x_start + *camera_lookat_x + (float)i * x_spacing - 165.0f,
				y_coord + 70.0f);

			_menu_text(&vdest, text[i], &center, t);
		}	
	}

	Vector2 vdest = vec2(center.x, 295.0f);
	if(_menu_button(&vdest, "Back", &center, t) && t==0.0f) {
		menu_transition = back;
		menu_transition_t = -time_ms() / 1000.0f;
	}

	return result;
}

void _render_chapters(float t) {	
	const char* text[5];
	for(uint i = 0; i < 5; ++i) {
		text[i] = chapters[i].name;
	}

	if(_render_slidemenu(t, &chp_camera_lookat_x, &chp_old_camera_lookat_x,
		MENU_MAIN, text, 5)) {

		selected_chapter = (int)(chp_camera_lookat_x / -x_spacing + 0.5f);
		selected_chapter = MIN(selected_chapter, 4);
		selected_chapter = MAX(selected_chapter, 0);
	
		menu_transition = MENU_ARENA;
		menu_transition_t = time_ms() / 1000.0f;
		arn_camera_lookat_x = arn_old_camera_lookat_x = 0.0f;
	}	
}		

void _render_arenas(float t) {
	if(_render_slidemenu(t, &arn_camera_lookat_x, &arn_old_camera_lookat_x,
		MENU_CHAPTER, chapters[selected_chapter].arena_name, 5)) {

		uint selected_arena = (int)(arn_camera_lookat_x / -x_spacing + 0.5f);
		selected_arena = MIN(selected_arena, 4);
		selected_arena = MAX(selected_arena, 0);

		const char* arena_name =
			chapters[selected_chapter].arena_file[selected_arena];
		if(arena_name != NULL) {
			menu_transition = MENU_GAME;
			menu_transition_t = time_ms() / 1000.0f;

			// TODO: Draw loading text here

			game_reset(arena_name, 2);
			ai_init_agent(1, 0);
		}	
	}
}

void _menus_switch(MenuState state, float t) {
	if(state == MENU_MAIN)
		_render_main(t);
	if(state == MENU_CHAPTER)
		_render_chapters(t);
	if(state == MENU_ARENA)
		_render_arenas(t);
	if(state == MENU_GAME)
		game_render_transition(t);
}

void menus_render(void) {	
	if(menu_transition == MENU_GAME && menu_state == MENU_GAME)
		return;

	// Background
	if(menu_transition != MENU_GAME) {
		RectF dest = rectf(0.0f, 0.0f, 0.0f, 0.0f);
		video_draw_rect(background, MENU_BACKGROUND_LAYER, &background_source,
			&dest, COLOR_WHITE);
	}	

	if(menu_state == menu_transition) {
		_menus_switch(menu_state, 0.0f);
	}	
	else {
		float time = time_ms() / 1000.0f;
		float t = (time - fabs(menu_transition_t)) / menu_transition_length;

		if(menu_transition_t > 0.0f) {
			_menus_switch(menu_state, 0.0f + t);
			_menus_switch(menu_transition, -1.0f + t);
		}
		else {
			_menus_switch(menu_state, 0.0f - t);
			_menus_switch(menu_transition, 1.0f - t);
		}

		if(t >= 1.0f)
			menu_state = menu_transition;
	}
}
