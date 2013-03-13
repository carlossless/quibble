#include "common.h"
#include "obj_types.h"
#include "levels.h"
#include "minimap.h"
#include "game.h"
#include "hud.h"
#include "tutorials.h"
#include "ai.h"

#include <mfx.h>
#include <memory.h>
#include <system.h>
#include <gfx_utils.h>
#include <async.h>

#define TIME_S (malka_state_time("game") + malka_state_time("game_over"))

static const float rabbit_hitbox_width = 70.0f;
static const float rabbit_hitbox_height = 62.0f;
static const float ground_y = 579.0f;

extern bool draw_ai_debug;
extern bool camera_follow;
extern ObjRabbit* rabbit;

static Vector2 _rabbit_calculate_forces(GameObject* self,bool gravity_only){
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = self->physics;

	Vector2 result = {0.0f,0.0f};

	// Constantly move right
	result = vec2_add(result, vec2(d->speed,0.0f) );

	// Boost
	if(d->combo_counter >= 3)
		result = vec2_add(result, vec2(200.0f * d->combo_counter, 0.0f) );

	// Rubber band
	if(d->rubber_band){

		Vector2 rb = vec2(0.0f,0.0f);

		for(int i = 0; i < minimap_get_count();i++){
			ObjRabbit* other = minimap_get_rabbit(i);
			ObjRabbitData* od = other->data;
			if(other == rabbit)
				continue;

			float delta = other->header.physics->cd_obj->pos.x - p->cd_obj->pos.x;

			if(other->data->player_control) {
				delta = delta*2.0f + od->speed;
				rb = vec2(delta, 0.0f);
			} else {
				float min_dist = 800.0f;
				float force = od->speed * od->speed;
				force *= 10.0f;

				if(delta > 0.0f && delta < min_dist)
					rb.x = -force/delta;
			}
		}

		if(rb.x < 0.0f) 
			rb.x = d->speed;

		result = vec2_add(result, rb);

	}

	// Gravity
	result = vec2_add(result, vec2(0.0f, 6000.0f) );	

	if(!gravity_only){
		// Jumping
		if(d->jumped)
			result = vec2_add(result, vec2(d->xjump*d->xjump, -d->yjump*d->yjump) );

		if(d->dived){

			if(d->virtual_key_down){
				result = vec2_add(result, vec2(0.0f, 20000.0f) );
			}
			else {
				result = vec2_add(result, vec2(0.0f, 25000.0f) );
			}
		}
	}

	return result;
}

static Vector2 _rabbit_damping(Vector2 vel) {
	float t = clamp(0.0f, 1.0f, (vel.x - 220.0f) / 1000.0f);
	float damp = smoothstep(1.0f, 0.98f, t);
	return vec2(vel.x * damp, vel.y * 0.995f);
}

void obj_rabbit_player_control(GameObject* self){
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;

	if(!d->input_disabled){
		d->virtual_key_up = key_up(KEY_A);
		d->virtual_key_down = key_down(KEY_A);
		d->virtual_key_pressed = key_pressed(KEY_A);
	}

	if(tutorials_are_enabled()){

		ObjRabbit* rabbit = (ObjRabbit*)self;
		ObjRabbitData* d = rabbit->data;
		PhysicsComponent* p = self->physics;
		Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);

		Vector2 start;

		if(d->touching_ground){
			tutorials_hint_press(false);
			// coldet for shroom in front
			start = vec2(pos.x + 50.0f + p->vel.x * 0.6f,pos.y - 90.0f);
			
			RectF rec = {
				.left = start.x - 35.0f,
				.top = 500.0f,
				.right = start.x + rabbit_hitbox_width,
				.bottom = 500.0f + rabbit_hitbox_height
			};				

			if(draw_ai_debug){
				RectF r = objects_world2screen(rec,0);
				gfx_draw_rect(10, &r, COLOR_WHITE);
			}

			GameObject* obj = NULL;
			objects_aabb_query(&rec,&obj,1);

			// tutorial event on mushroom in front
			if(obj){
				if(obj->type == OBJ_MUSHROOM_TYPE){
					tutorial_event(MUSHROOM_IN_FRONT);
					tutorials_hint_press(true);
				}	
			}

		} else {

			// coldet below rabbit for shrooms
			float x = pos.x + (p->vel.x * 0.3f) *(ground_y-pos.y)/ground_y;
			float y = pos.y+rabbit_hitbox_height + 30.0f;
			start = vec2(x,y);

			RectF rec = {
				.left = start.x - 20.0f,
				.top = 500.0f,
				.right = start.x + rabbit_hitbox_width,
				.bottom = 500.0f + rabbit_hitbox_height
			};

			if(draw_ai_debug){
				RectF r = objects_world2screen(rec,0);
				gfx_draw_rect(10, &r, COLOR_WHITE);
			}

			GameObject* obj = NULL;
			objects_aabb_query(&rec,&obj,1);

			// tutorial event when mushroom below rabbit
			if(obj){
				if(obj->type == OBJ_MUSHROOM_TYPE){
					tutorial_event(MUSHROOM_BELOW);
					tutorials_hint_press(true);
				} else {
					tutorials_hint_press(false);
				}
			}

		} 

	}
}

static ObjFloaterParams trampoline_floater_params = {
	.spr = "token",
	.text = "-10",
	.duration = 0.5f
};

static Vector2 _predict_landing(ObjRabbit* rabbit, Vector2 force){
	GameObject* self = (GameObject*) rabbit;
	PhysicsComponent* p = rabbit->header.physics;	

	bool jumped = false;
	Vector2 acc = p->acc;
	Vector2 vel = p->vel;
	Vector2 landing = p->cd_obj->pos;	
	acc = vec2_add(acc,force);

	// predict landing
	while(landing.y <= 579.0f || !jumped){
		if(landing.y < 579.0f) jumped = true;

			acc = vec2_add(acc, _rabbit_calculate_forces(self,true) );

			// physics tick
			Vector2 a = vec2_scale(acc, p->inv_mass * PHYSICS_DT);
			vel = vec2_add(vel, a);
			acc = vec2(0.0f, 0.0f);
			landing = vec2_add(landing, vec2_scale(vel, PHYSICS_DT));

			// damping
			vel = _rabbit_damping(vel);
	}
	return landing;
}

static void obj_rabbit_update(GameObject* self, float ts, float dt) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;

	if(!d->is_dead){

		PhysicsComponent* p = self->physics;

		d->jumped = false;
		d->dived = false;

		// rubber band
		if(d->rubber_band){

			p->cd_obj->pos.y = ground_y;
			p->vel.y = 0.0f;
			if(p->vel.x < 0.0f) 
				p->vel.x = d->speed;
			if(minimap_player_x() - p->cd_obj->pos.x < 0.0) 
				d->rubber_band = false;

		} else if(camera_follow)
			rabbit->control(self);

		if(d->virtual_key_down)
			d->last_keypress_t = ts;
		if(d->virtual_key_up)
			d->last_keyrelease_t = ts;

		// Position for follower particles
		Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);
		pos.y += rabbit_hitbox_height - 20;
	
		RectF rec = {
			.left = pos.x, 
			.top = pos.y,
			.right = 0,
			.bottom = 0
		};
		RectF result = objects_world2screen(rec,0);
		// Position for standard particles
		Vector2 screen_pos = vec2(result.left,result.top);

		RenderComponent* r = self->render;
		r->anim_frame = anim_frame_ex(rabbit->anim, TIME_S);
		
		if(d->touching_ground) {	
			d->force_dive = false;	
			d->is_diving = false;
			d->has_trampoline = false;			
			d->combo_counter = 0;
			d->boost = 0;

			// Jump
			if(d->virtual_key_down || d->force_jump){
				d->force_jump = false;
				d->touching_ground = false;
				d->jump_off_mushroom = false;
				d->jump_time = ts;
				d->jumped = true;
				Vector2 force = vec2(d->xjump*d->xjump, -d->yjump*d->yjump);
				anim_play_ex(rabbit->anim, "jump", TIME_S);
				
				d->land = _predict_landing(rabbit,force).x;

				d->combo_counter = 0;
			
				ObjParticleAnchor* anchor = (ObjParticleAnchor*)objects_create(&obj_particle_anchor_desc, pos, NULL);
				if(r->was_visible)
					mfx_trigger_follow("jump",&anchor->screen_pos,NULL);
			}

			// Trigger water/land particle effects on ground
			if(d->last_frame != r->anim_frame && r->was_visible) {
				const char* effect = NULL;
				if(d->on_water){
					if(r->anim_frame == 1)
						effect = "water";
					if(r->anim_frame == 11)
						effect = "water_front";
				}
				else { 
					if(r->anim_frame == 1)
						effect = "run1";
					if(r->anim_frame == 11)
						effect = "run1_front";
				}

				if(effect)
					mfx_trigger_ex(effect, screen_pos, 0.0f);
			}
			d->last_frame = r->anim_frame;

		}
		else {

			if(d->combo_counter >= 3){
				if(d->boost == 0){
					if(r->was_visible)
						mfx_trigger_ex("boost",vec2_add(screen_pos,vec2(20.0f,0.0f)),0.0f);
					d->boost = 5;
				} else d->boost--;
			}			
	
			if(p->vel.y > 0.0f && !d->falling_down){
				anim_play_ex(rabbit->anim, "down", TIME_S);
				d->falling_down = true;
			} else if (p->vel.y <= 0.0f) {
				d->falling_down = false;		
			}

			if(ts - d->mushroom_hit_time < 0.1f) {

				if(fabsf(d->mushroom_hit_time - d->last_keypress_t) < 0.1f)
					d->jump_off_mushroom = true;

				if(fabsf(d->mushroom_hit_time - d->last_keyrelease_t) < 0.1f)
					d->jump_off_mushroom = true;
					
			}
			else {
				if(d->virtual_key_pressed && (ts - d->jump_time) < 0.2f) {
				//	objects_apply_force(self, vec2(0.0f, -8000.0f));
				}
				else if( (!d->is_diving && d->virtual_key_down) || d->force_dive ) {
					// Dive	
					d->is_diving = true;
					d->dived = true;
					anim_play_ex(rabbit->anim, "dive", TIME_S);
				}
				else if( (d->is_diving && d->virtual_key_pressed) || d->force_dive ) {
					d->dived = true;
				}
				else if(d->is_diving && !d->virtual_key_pressed) {
					d->is_diving = false;
					anim_play_ex(rabbit->anim, "glide", TIME_S);
				}
			}
		}	

		p->vel = _rabbit_damping(p->vel);
	
		d->on_water = false;

		// Above screen
		if(p->cd_obj->pos.y < rabbit_hitbox_height){
			p->cd_obj->pos.y = rabbit_hitbox_height;
			p->vel.y = 0.0f;
			d->touching_ground = false;
		}
		// Below screen
		if(p->cd_obj->pos.y > HEIGHT){
			rabbit->data->is_dead = true;
			p->vel.x = 0.0f;
			p->vel.y = 0.0f;
			if(!rabbit->data->game_over) rabbit->data->rabbit_time = -1.0f;
		}

		if(p->cd_obj->pos.y < ground_y) d->touching_ground = false;

		if(!d->game_over) d->rabbit_time += time_delta() / 1000.0f;

		objects_apply_force(self,_rabbit_calculate_forces(self,false));	

	}

}

static void obj_rabbit_update_pos(GameObject* self) {
	// Update render data
	RenderComponent* r = self->render;
	PhysicsComponent* p = self->physics;
	Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);
	pos = vec2_add(pos, vec2(rabbit_hitbox_width / 2.0f, rabbit_hitbox_height / 2.0f));
	float half_w = rectf_width(&r->world_dest) / 2.0f;
	float half_h = rectf_height(&r->world_dest) / 2.0f;
	r->world_dest = rectf(
			pos.x - half_w, pos.y - half_h, pos.x + half_w, pos.y + half_h
	);
}

static void obj_rabbit_became_visible(GameObject* self) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = self->physics;
	if(!d->player_control && d->rubber_band){
		Vector2 pos = p->cd_obj->pos;

		p->vel.y = 0.0f;
		d->touching_ground = false;

		// is it safe to appear on screen?
		bool safe_to_appear = false;

		RectF rec = {
			.left = pos.x + rabbit_hitbox_width + 70.0f,
			.top = HEIGHT - rabbit_hitbox_height,
			.right = pos.x + rabbit_hitbox_width + 70.0f,
			.bottom = HEIGHT
		};

		GameObject* obj = objects_raycast(vec2(rec.left,rec.top),vec2(rec.right,rec.bottom));

		if(obj && obj->type == OBJ_GROUND_TYPE) {
			safe_to_appear = true;
		}	

		if(safe_to_appear){
			p->vel.x = rabbit->header.physics->vel.x;
			p->cd_obj->pos.y = ground_y;
			d->rubber_band = false;			
		} else {
			bool safe_to_jump = true;

			float landing = _predict_landing(rabbit,vec2(d->xjump*d->xjump, -d->yjump*d->yjump)).x;

			RectF rec = {
				.left = landing,
				.top = HEIGHT - 50.0f,
				.right = landing + rabbit_hitbox_width,
				.bottom = HEIGHT-10.0f
			};

			GameObject* obj = NULL;
			objects_aabb_query(&rec,&obj,1);

			if(obj && (obj->type == OBJ_FALL_TRIGGER_TYPE || obj->type == OBJ_TRAMPOLINE_TYPE )) {
					safe_to_jump = false;
			}	

			if(safe_to_jump){
				p->cd_obj->pos.y = rand_float_range(550.0f,ground_y);
				Vector2 force = vec2(d->xjump*d->xjump, -d->yjump*d->yjump);
				objects_apply_force(self, force);
				d->touching_ground = false;
				d->jump_off_mushroom = false;
				d->rubber_band = false;
			} else {
				// if it is unsafe to appear and jump, keep rubber band active
			}			
		}
	}
}
static void obj_rabbit_became_invisible(GameObject* self) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = self->physics;
	float delta = minimap_player_x() - p->cd_obj->pos.x;
	if(!d->is_dead && !d->player_control && delta > 0.0f){
		p->cd_obj->pos.y = ground_y;
		p->vel.y = 0.0f;
		p->acc.y = 0.0f;
		d->touching_ground = true;
		d->rubber_band = true;
	} 
}

static void _rabbit_delayed_bounce(void* r) {
	ObjRabbit* rabbit = r;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = rabbit->header.physics;
	if(p->acc.y >= 0.0f && p->vel.y > 0.0f && !d->touching_ground && (d->jump_off_mushroom || d->is_diving)) {
		d->land = _predict_landing(rabbit,d->bounce_force).x;

		if(d->player_control) tutorial_event(BOUNCE_PERFORMED);
		d->force_dive = false;
		d->is_diving = false;
		d->has_trampoline = false;
		GameObject* self = r;
		objects_apply_force(self, d->bounce_force); 
		d->jump_off_mushroom = false;

		ObjParticleAnchor* anchor = (ObjParticleAnchor*)objects_create(&obj_particle_anchor_desc, p->cd_obj->pos, NULL);
		mfx_trigger_follow("jump",&anchor->screen_pos,NULL);

		if(d->combo_counter+1 == 3){
			if(d->player_control) tutorial_event(COMBO_X3);	

			Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);
		
			RectF rec = {
				.left = pos.x, 
				.top = pos.y,
				.right = 0,
				.bottom = 0
			};
			RectF result = objects_world2screen(rec,0);
			Vector2 screen_pos = vec2(result.left,result.top);

			RenderComponent* render = rabbit->header.render;
			if(render->was_visible)
				mfx_trigger_ex("boost_explosion",screen_pos,0.0f);
		} 

		d->combo_counter++;
		 
		if(d->player_control) hud_trigger_combo(d->combo_counter);
	}
	else
		d->combo_counter = 0;

	d->bounce_force = vec2(0.0f, 0.0f);
}

static void obj_rabbit_collide(GameObject* self, GameObject* other) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;

	// Collision with ground
	if(other->type == OBJ_GROUND_TYPE) {
		CDObj* cd_rabbit = self->physics->cd_obj;
		CDObj* cd_ground = other->physics->cd_obj;
		float rabbit_bottom = cd_rabbit->pos.y + cd_rabbit->size.size.y;
		float ground_top = cd_ground->pos.y;
		float penetration = (rabbit_bottom + cd_rabbit->offset.y) - ground_top;
		if(penetration > 0.0f && cd_rabbit->pos.y < cd_ground->pos.y) {
			self->physics->vel.y = 0.0f;
			if(!d->touching_ground) {
				if(d->player_control) hud_trigger_combo(0);
				anim_play_ex(rabbit->anim, "land", TIME_S);
			}
			d->touching_ground = true;
			cd_rabbit->offset = vec2_add(
				cd_rabbit->offset, 
				vec2(0.0f, -penetration)
			);
		}
	}

	// Collision with fall trigger
	if(other->type == OBJ_FALL_TRIGGER_TYPE) {
		if(!d->rubber_band) d->touching_ground = false;
		PhysicsComponent* p = self->physics;	

		// Trampoline
		if(p->cd_obj->pos.y > ground_y && d->tokens >=10 && !d->has_trampoline && !rabbit->data->game_over){
			d->has_trampoline = true;
			d->tokens -= 10;

			// Trampoline sprite
			SprHandle sprt = sprsheet_get_handle("trampoline");
			Vector2 size = sprsheet_get_size_h(sprt);
			float width = size.x;
			float height = size.y;

			PhysicsComponent* gap = other->physics;
			Vector2 gap_pos = vec2(gap->cd_obj->pos.x + (gap->cd_obj->size.size.x - width) / 2.0f,HEIGHT + 15.0f);
			Vector2 txt_pos = vec2(gap->cd_obj->pos.x + (gap->cd_obj->size.size.x) / 2.0f,HEIGHT + 15.0f - height);

			// Create Trampoline
			ObjTrampoline* trampoline = (ObjTrampoline*) objects_create(&obj_trampoline_desc, gap_pos, (void*)sprt);
			trampoline->owner = self;

			if(d->player_control){
				sprt = sprsheet_get_handle("token_tag");

				// Create floating text
				objects_create(&obj_floater_desc, txt_pos, (void*)&trampoline_floater_params);
			}
		}
	}

	// Collision with speed trigger
	if(other->type == OBJ_SPEED_TRIGGER_TYPE) {
		ObjSpeedTrigger* t = (ObjSpeedTrigger*)other;
		objects_apply_force(self, 
			vec2(-self->physics->vel.x * t->drag_coef, 0.0f)
		);
		d->on_water = true;
	}

	// Collision with mushroom
	Vector2 vel = self->physics->vel;
	if(other->type == OBJ_MUSHROOM_TYPE && !d->touching_ground &&
		vel.y > 500.0f && d->bounce_force.y == 0.0f) {

		d->mushroom_hit_time = time_s();
		anim_play_ex(rabbit->anim, "bounce", TIME_S);
		vel.y = -vel.y;

		Vector2 f = {
			.x = MIN(vel.x*d->xjump, 110000.0f),
			.y = MAX(vel.y*d->yjump,-250000.0f)
		};

		d->bounce_force = f;

		// Slow down vertical movevment
		self->physics->vel.y *= 0.2f;

		// Delay actual bouncing 0.1s
		async_schedule(_rabbit_delayed_bounce, 100, self);
	}

	// Collision with trampoline
	if(other->type == OBJ_TRAMPOLINE_TYPE) {
		ObjTrampoline* trampoline = (ObjTrampoline*)other;

		if(trampoline->owner == self && 
			!d->touching_ground && 
			self->physics->acc.y >= 0.0f) {

			anim_play_ex(rabbit->anim, "bounce", TIME_S);
			self->physics->vel.y = 0.0f;
			objects_apply_force(self, 
				vec2(d->xjump*d->xjump, -d->yjump*d->yjump)
			);
		}
	}	
}

static void obj_rabbit_construct(GameObject* self, Vector2 pos, void* user_data) {
	int id = (int)user_data;

	ObjRabbit* rabbit = (ObjRabbit*)self;
	rabbit->anim = anim_new_ex("rabbit", TIME_S);
	
	// Init physics
	PhysicsComponent* physics = self->physics;
	RectF rect = rectf_centered(pos, rabbit_hitbox_width, rabbit_hitbox_height);
	physics->cd_obj = coldet_new_aabb(objects_cdworld, &rect, 1, NULL);
	float mass = 3.0f;
	physics->inv_mass = 1.0f / mass;
	physics->vel = vec2(0.0f, 0.0f);
	physics->hit_callback = obj_rabbit_collide;

	physics->cd_obj->pos.y = ground_y;

	// Init render
	Vector2 size = sprsheet_get_size("rabbit");
	RenderComponent* render = self->render;
	render->world_dest = rectf_centered(pos, size.x, size.y);
	render->angle = 0.0f;
	render->layer = rabbit_layer;
	render->anim_frame = 0;
	render->update_pos = obj_rabbit_update_pos;
	render->became_visible = obj_rabbit_became_visible;
	render->became_invisible = obj_rabbit_became_invisible;
	
	// Init update
	UpdateComponent* update = self->update;
	update->update = obj_rabbit_update;

	// additional rabbit data
	rabbit->data = MEM_ALLOC(sizeof(ObjRabbitData));
	ObjRabbitData* d = rabbit->data;
	memset(d, 0, sizeof(ObjRabbitData));
	// Everything is initialized to zero, except these:
	if(id < 0){
		// Player rabbit
		render->spr = sprsheet_get_handle("rabbit");
		rabbit->control = obj_rabbit_player_control;
		//rabbit->control = ai_control;
		d->minimap_color = COLOR_RGBA(150, 150, 150, 255);
		d->rabbit_name = "You";
		d->player_control = true;
		d->speed = 500.0f;
		d->xjump = 100.0f;
		d->yjump = 400.0f;
	} else {
		// AI rabbit
		LevelDesc* lvl_desc = levels_current_desc();

		render->spr = lvl_desc->ai_rabbit_spr[id];
		d->minimap_color = lvl_desc->ai_rabbit_colors[id];
		rabbit->control = ai_control;
		d->rabbit_name = lvl_desc->ai_rabbit_names[id];
		d->speed = lvl_desc->ai_rabbit_speeds[id];
		d->xjump = lvl_desc->ai_rabbit_xjumps[id];
		d->yjump = lvl_desc->ai_rabbit_yjumps[id];
		d->ai_max_combo = lvl_desc->ai_max_combo[id];
		render->layer = ai_rabbit_layer;
	}
}

static void obj_rabbit_destruct(GameObject* self) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	MEM_FREE(rabbit->data);
	anim_del(rabbit->anim);
}

GameObjectDesc obj_rabbit_desc = {
	.type = OBJ_RABBIT_TYPE,
	.size = sizeof(ObjRabbit),
	.has_physics = true,
	.has_render = true,
	.has_update = true,
	.construct = obj_rabbit_construct,
	.destruct = obj_rabbit_destruct
};

