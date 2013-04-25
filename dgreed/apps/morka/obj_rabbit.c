#include "obj_types.h"
#include "characters.h"
#include "common.h"
#include "levels.h"
#include "minimap.h"
#include "game.h"
#include "hud.h"
#include "tutorials.h"
#include "ai.h"
#include "shop.h"

#include <mfx.h>
#include <memory.h>
#include <system.h>
#include <gfx_utils.h>
#include <async.h>

#define TIME_S (malka_state_time("game") + malka_state_time("game_over"))

static const float rabbit_hitbox_width = 70.0f;
static const float rabbit_hitbox_height = 62.0f;
static float ground_y = 0;

static SprHandle shield_spr;
static float shield_width = 0.0f;
static float shield_height = 0.0f;

extern bool draw_ai_debug;


static Vector2 _rabbit_calculate_forces(GameObject* self,bool gravity_only){
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;

	Vector2 result = {0.0f,0.0f};

	// Constantly move right
	result = vec2_add(result, vec2(d->speed,0.0f) );

	// Boost
	if(d->combo_counter >= 3)
		result = vec2_add(result, vec2(200.0f * d->combo_counter, 0.0f) );

	// Gravity
	result = vec2_add(result, vec2(0.0f, 5000.0f) );

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

	// Rocket
	if(d->rocket_time > 0.0f){
		float mult = d->rocket_time - TIME_S;
		result = vec2_add(result, vec2(7500.0f * mult, 0.0f) );
		result.y = 0.0f;
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

	if(!d->input_disabled && !hud_click){
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
			start = vec2(pos.x + 50.0f + p->vel.x * 0.5f,pos.y - 90.0f);
			
			RectF rec = {
				.left = start.x - 35.0f,
				.top = v_height - 300.0f,
				.right = start.x + rabbit_hitbox_width,
				.bottom = v_height - 300.0f + rabbit_hitbox_height
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
			float x = pos.x + (p->vel.x * 0.2f) *(ground_y-pos.y)/ground_y;
			float y = pos.y+rabbit_hitbox_height + 30.0f;
			start = vec2(x,y);

			RectF rec = {
				.left = start.x - 20.0f,
				.top = v_height - 268.0f,
				.right = start.x + rabbit_hitbox_width,
				.bottom = v_height - 268.0f + rabbit_hitbox_height
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

static Vector2 _predict_landing(ObjRabbit* rabbit, Vector2 force){
	GameObject* self = (GameObject*) rabbit;
	PhysicsComponent* p = rabbit->header.physics;
	ObjRabbitData* d = rabbit->data;	

	bool jumped = false;
	bool hit = false;
	Vector2 acc = vec2_add(p->acc,force);
	Vector2 vel = p->vel;
	Vector2 landing = p->cd_obj->pos;

	// save state
	float rocket_time = d->rocket_time;

	// modify state for prediction
	d->rocket_time = 0;

	uint iterations = 0;

	// predict landing
	while(!hit || !jumped){
		
		acc = vec2_add(acc, _rabbit_calculate_forces(self,true) );

		// physics tick
		Vector2 a = vec2_scale(acc, p->inv_mass * PHYSICS_DT);
		vel = vec2_add(vel, a);
		acc = vec2(0.0f, 0.0f);
		landing = vec2_add(landing, vec2_scale(vel, PHYSICS_DT));

		// damping
		vel = _rabbit_damping(vel);

		int obj_type = OBJ_GROUND_TYPE | OBJ_MUSHROOM_TYPE | OBJ_BRANCH_TYPE | OBJ_SPRING_BRANCH_TYPE | OBJ_SPIKE_BRANCH_TYPE;
		obj_type &= ~collision_flag;

		if(jumped){

			RectF rec = {
				.left = landing.x,
				.top = landing.y,
				.right = landing.x + rabbit_hitbox_width,
				.bottom = landing.y + rabbit_hitbox_height
			};

			GameObject* result[3] = {0};
			objects_aabb_query(&rec,&result[0],3);

			for(uint i = 0; i < 3; i++)	{

				if(result[i] && ((result[i]->type & ~collision_flag ) & obj_type))
				{
					if((result[i]->type & ~collision_flag ) & (OBJ_GROUND_TYPE & ~collision_flag) ){
						if(landing.y < result[i]->physics->cd_obj->pos.y){

							hit = true;
						}else if(landing.x < result[i]->physics->cd_obj->pos.x){

							landing.x = result[i]->physics->cd_obj->pos.x - rabbit_hitbox_width;
							landing.y = v_height - rabbit_hitbox_height;
							hit = true;
						}

					} else if(landing.y < result[i]->physics->cd_obj->pos.y)	
						hit = true;
				} 
			}

			if(landing.y > v_height){
				hit = true;
				landing.y = v_height - rabbit_hitbox_height;
			}	

			if(landing.y < rabbit_hitbox_height){
				hit = true;
			}

		} else {
			if(vel.y > 0.0f) jumped = true;
		}

		if(++iterations > 1000) 
			printf("D landing[%f;%f] velocity[%f;%f] \n",landing.x, landing.y,vel.x,vel.y);

	}

	// restore state
	d->rocket_time = rocket_time;

	return landing;
}

static Vector2 _predict_diving(ObjRabbit* rabbit){
	GameObject* self = (GameObject*) rabbit;
	PhysicsComponent* p = rabbit->header.physics;
	ObjRabbitData* d = rabbit->data;	

	bool hit = false;
	Vector2 acc = p->acc;
	Vector2 vel = p->vel;
	Vector2 landing = p->cd_obj->pos;	

	// save state
	bool jumped = d->jumped;
	bool dived = d->dived;
	bool kd = d->virtual_key_down;
	bool kp = d->virtual_key_pressed;
	float rocket_time = d->rocket_time;

	// modify state for prediction
	d->jumped = false;
	d->dived = true;
	d->virtual_key_down = false;
	d->virtual_key_pressed = true;
	d->rocket_time = 0;

	uint iterations = 0;

	// predict landing
	while(!hit){
		
		acc = vec2_add(acc, _rabbit_calculate_forces(self,false) );

		// physics tick
		Vector2 a = vec2_scale(acc, p->inv_mass * PHYSICS_DT);
		vel = vec2_add(vel, a);
		acc = vec2(0.0f, 0.0f);
		landing = vec2_add(landing, vec2_scale(vel, PHYSICS_DT));

		// damping
		vel = _rabbit_damping(vel);

		int obj_type = OBJ_GROUND_TYPE | OBJ_MUSHROOM_TYPE | OBJ_BRANCH_TYPE | OBJ_SPRING_BRANCH_TYPE | OBJ_SPIKE_BRANCH_TYPE;
		obj_type &= ~collision_flag;

		RectF rec = {
			.left = landing.x,
			.top = landing.y,
			.right = landing.x + rabbit_hitbox_width,
			.bottom = landing.y + rabbit_hitbox_height
		};

		GameObject* result[3] = {0};
		objects_aabb_query(&rec,&result[0],3);
		for(uint i = 0; i < 3; i++)	{
			if(result[i] && ((result[i]->type & ~collision_flag ) & obj_type) && result[i] != d->previuos_hit) hit = true;
		}

		if(landing.y > v_height){
			hit = true;
			landing.y = v_height - rabbit_hitbox_height;
		}			

		if(landing.y < rabbit_hitbox_height){
			hit = true;
		}

		if(++iterations > 1000) 
			printf("D landing[%f;%f] velocity[%f;%f] \n",landing.x, landing.y,vel.x,vel.y);

	}

	// restore state
	d->jumped = jumped;
	d->dived = dived;
	d->virtual_key_down = kd;
	d->virtual_key_pressed = kp;
	d->rocket_time = rocket_time;	

	if(landing.x < p->cd_obj->pos.x) landing.x = p->cd_obj->pos.x;

	if(landing.y - p->cd_obj->pos.y + p->cd_obj->offset.y < rabbit_hitbox_height) landing.y = p->cd_obj->pos.y + p->cd_obj->offset.y + rabbit_hitbox_height;

	return landing;
}

static void obj_rabbit_update(GameObject* self, float ts, float dt) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;

	if(!d->is_dead){

		PhysicsComponent* p = self->physics;
		d->jumped = false;
		d->dived = false;

		if(camera_follow && !d->jump_out)
			rabbit->control(self);	
		else{
			d->virtual_key_up = false;
			d->virtual_key_down = false;
			d->virtual_key_pressed = false;
		}

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
		
		if(d->rocket_time == 0.0f){

			if(d->touching_ground) {	
				d->force_dive = false;	
				d->is_diving = false;
				d->trampoline_placed = false;			
				d->combo_counter = 0;
				d->boost = 0;

				d->land = p->cd_obj->pos;

				// Jump
				if(d->virtual_key_down || d->force_jump){
					d->force_jump = false;
					d->touching_ground = false;
					d->jump_off_mushroom = false;
					d->jump_time = ts;
					d->jumped = true;
					Vector2 force = vec2(d->xjump*d->xjump, -d->yjump*d->yjump);
					anim_play_ex(rabbit->anim, "jump", TIME_S);
					
					if(!d->player_control) d->land = _predict_landing(rabbit,force);

					d->combo_counter = 0;
					
					if(r->was_visible){			
						ObjParticleAnchor* anchor = (ObjParticleAnchor*)objects_create(&obj_particle_anchor_desc, pos, NULL);
						mfx_trigger_follow("jump",&anchor->screen_pos,NULL);
					}

					d->shield_dh = -20.0f;
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

					if(effect && r->was_visible)
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
					d->jump_off_mushroom = false;

					if(fabsf(d->mushroom_hit_time - d->last_keypress_t) < 0.2f)
						d->jump_off_mushroom = true;

					if(fabsf(d->mushroom_hit_time - d->last_keyrelease_t) < 0.2f)
						d->jump_off_mushroom = true;
						
				}
				else {
					if( (!d->is_diving && d->virtual_key_down) || d->force_dive ) {
						// Dive	
						d->is_diving = true;
						d->dived = true;
						anim_play_ex(rabbit->anim, "dive", TIME_S);
					}
					else if( (d->is_diving && d->virtual_key_pressed) || d->force_dive ) {
						d->dived = true;
					}
					else if( d->is_diving && !d->virtual_key_pressed) {
						d->is_diving = false;
						anim_play_ex(rabbit->anim, "glide", TIME_S);
					}
				}
			}	

		} else {

			p->vel.y = 0.0f;
			
			// Ending rocket powerup
			if(TIME_S > d->rocket_time){
				d->rocket_time = 0.0f;
				d->touching_ground = false;
				anim_play_ex(rabbit->anim, "down", TIME_S);
			}	

		}

		// Rocket starting if activated on ground/gap
		if(d->rocket_start || d->rocket_time != 0.0f){

			if(d->touching_ground || p->cd_obj->pos.y > 579.0f){
				p->vel.y = 0.0f;
				d->touching_ground = false;
				d->rocket_start = true;
				objects_apply_force(self, vec2(10000.0f, -160000.0f) );
			}

			if(d->rocket_time != 0.0f){
				//float mult = (d->rocket_time - TIME_S );
				float t = TIME_S;
				t = normalize(t,d->rocket_time-3.0f,d->rocket_time);
				float delta = sin(30.0 * t) * (1.0-t) * 5.0f;

				p->cd_obj->offset.y += delta;
			}
			// Spawn rocket particles
			if(d->boost == 0){
				if(r->was_visible){
					Vector2 rocket_pos = vec2_add(screen_pos,vec2(13.0f,2.0f));
					mfx_trigger_ex("rocket", rocket_pos, 0.0f);
				}	
				d->boost = 3;
			} else d->boost--;

			if(p->vel.y > 0.0f){
				anim_play_ex(rabbit->anim, "rocket_ride", TIME_S);
				d->rocket_start = false;
				d->rocket_time = TIME_S + 2.0f;
			}
		}

		// Prevent player from moving out of screen on bomb/cactus hit
		if(screen_pos.x < 0.0f && p->vel.x < 0.0f){
			p->vel.x = 0.0f;
		}

		if(!d->game_over) d->rabbit_time += time_delta() / 1000.0f;
		
		d->on_water = false;

		objects_apply_force(self,_rabbit_calculate_forces(self,false));	
		p->vel = _rabbit_damping(p->vel);

		if(p->vel.y > 0.0f) d->touching_ground = false;


		if(d->jump_out && d->touching_ground)
			d->jump_out = false;

		if(p->cd_obj->pos.y > v_height + 33.0f && !d->game_over && !d->jump_out){
			p->vel.x = 0.0f;
			p->vel.y = 0.0f;
			p->cd_obj->pos.y = v_height + 33.0f;
			d->combo_counter = 0;
			d->boost = 0;				

			if(d->respawn > 0.0f && TIME_S > d->respawn){

				if(levels_current_desc()->season == AUTUMN){
					// Find next ground tile
					Vector2 start = vec2(p->cd_obj->pos.x,v_height - 10.0f);
					Vector2 end = vec2_add(start,vec2(400.0f,0.0f));

					Vector2 hitpoint;
					CDObj* cdobj = coldet_cast_segment(
						objects_cdworld, start, end, OBJ_GROUND_TYPE & ~collision_flag, &hitpoint
					);

					GameObject* obj = NULL;

					if(cdobj)
						obj = (GameObject*)cdobj->userdata;

					assert(obj);

					// Jump the rabbit out of the gap
					p->cd_obj->pos.x = obj->physics->cd_obj->pos.x - rabbit_hitbox_width*2.0f;
					p->cd_obj->pos.y = v_height;
					p->cd_obj->dirty = true;

					objects_apply_force(self, 
						vec2(11500.0f, -200000.0f)
					);
					anim_play_ex(rabbit->anim, "jump", TIME_S);
					d->jump_out = true;	

				} else {
					p->cd_obj->pos.y = 0.0f;
					p->cd_obj->dirty = true;				
				}

			} else if(d->respawn == 0.0f) {
				// time to spend in gap
				d->respawn = TIME_S + 1.0f;
			}

		} else {
			d->respawn = 0.0f;
		}

		if(!d->player_control){
			if(!d->touching_ground)
				d->dive = _predict_diving(rabbit);
			else
				d->dive = p->cd_obj->pos;
		}

		if(d->game_over && !r->was_visible && !d->is_dead){
			d->is_dead = true;
			p->vel = vec2(0.0f,0.0f);
			self->render->spr = empty_spr;
		}

		if(d->collision_update)
			d->collision_update = false;
		else
			d->over_branch = false;

	} else {
		self->physics->vel = vec2(0.0f,0.0f);
	}

}

static void obj_rabbit_post_render(GameObject* self){
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;
	
	if(d->has_powerup[SHIELD] && !d->is_dead){

		PhysicsComponent* p = self->physics;

		Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);
		pos.x += 37.0f;
		
		float f = 30.0f * (shield_height - d->shield_h);
		d->shield_dh += f * (time_delta()/1000.0f);
		d->shield_h += (d->shield_dh * 20.0f) * (time_delta()/1000.0f);
		d->shield_dh *= 0.9f;

		RectF rec = {
			.left = pos.x - shield_width / 2.0f, 
			.top = (pos.y + shield_height / 2.0f) - d->shield_h,
			.right = pos.x + shield_width / 2.0f,
			.bottom = pos.y + shield_height / 2.0f
		};
		RectF result = objects_world2screen(rec,0);

		RenderComponent* render = self->render;
		spr_draw_h(shield_spr, render->layer,result,COLOR_WHITE);

	}

}

static void obj_rabbit_update_pos(GameObject* self) {
	// Update render data
	RenderComponent* r = self->render;
	PhysicsComponent* p = self->physics;
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;	
	Vector2 pos = vec2_add(p->cd_obj->pos, p->cd_obj->offset);
	pos = vec2_add(pos, vec2(rabbit_hitbox_width / 2.0f, rabbit_hitbox_height / 2.0f));
	pos = vec2_add(pos,vec2(d->sprite_offset,0.0f)); //Sprite offset from hitbox	
	r->world_dest = rectf_centered(
		pos, rectf_width(&r->world_dest), rectf_height(&r->world_dest)
	);
}

static void obj_rabbit_became_visible(GameObject* self) {
}

static void obj_rabbit_became_invisible(GameObject* self) {
}

static void _rabbit_delayed_bounce(void* r) {
	ObjRabbit* rabbit = r;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = rabbit->header.physics;

	if(p->acc.y >= 0.0f && !d->touching_ground && (d->jump_off_mushroom || d->is_diving) ) {

		d->jump_out = false;
		d->touching_ground = false;
		if(!d->player_control) d->land = _predict_landing(rabbit,d->bounce_force);

		if(d->player_control) tutorial_event(BOUNCE_PERFORMED);
		d->force_dive = false;
		d->is_diving = false;
		d->trampoline_placed = false;
		GameObject* self = r;
		objects_apply_force(self, d->bounce_force); 
		d->jump_off_mushroom = false;
		
		RenderComponent* render = rabbit->header.render;

		if(render->was_visible){
			ObjParticleAnchor* anchor = (ObjParticleAnchor*)objects_create(&obj_particle_anchor_desc, p->cd_obj->pos, NULL);
			mfx_trigger_follow("jump",&anchor->screen_pos,NULL);
		}

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


			if(render->was_visible)
				mfx_trigger_ex("boost_explosion",screen_pos,0.0f);
		} 

		d->combo_counter++;
		 
		if(d->player_control) hud_trigger_combo(d->combo_counter);

		d->shield_dh = -20.0f;
	}
	else
		d->combo_counter = 0;

	d->bounce_force = vec2(0.0f, 0.0f);
}

static void obj_rabbit_collide(GameObject* self, GameObject* other) {
	ObjRabbit* rabbit = (ObjRabbit*)self;
	ObjRabbitData* d = rabbit->data;
	PhysicsComponent* p = self->physics;
	Vector2 vel = self->physics->vel;

	d->collision_update = true;

	// Collision with ground
	if(other->type == OBJ_GROUND_TYPE) {

		d->spike_hit = false;

		CDObj* cd_rabbit = self->physics->cd_obj;
		CDObj* cd_ground = other->physics->cd_obj;
		float rabbit_bottom = cd_rabbit->pos.y + cd_rabbit->size.size.y;
		float ground_top = cd_ground->pos.y;
		float penetration_y = (rabbit_bottom + cd_rabbit->offset.y) - ground_top;
		if(penetration_y > 0.0f && cd_rabbit->pos.y < ground_top && p->vel.y > 0.0f) {

			self->physics->vel.y = 0.0f;
			if(!d->touching_ground) {
				if(d->player_control) hud_trigger_combo(0);
				anim_play_ex(rabbit->anim, "land", TIME_S);
				d->shield_dh = -20.0f;
			}
			d->touching_ground = true;
			cd_rabbit->offset = vec2_add(
				cd_rabbit->offset, 
				vec2(0.0f, -penetration_y)
			);
		} else if(cd_rabbit->pos.y > ground_top && !(d->has_powerup[TRAMPOLINE] || d->trampoline_placed) ) {

			float rabbit_right = cd_rabbit->pos.x + cd_rabbit->size.size.x;
			float ground_left = cd_ground->pos.x;;
			float penetration_x = (rabbit_right + cd_rabbit->offset.x) - ground_left;
			
			if(penetration_x > 0.0f) {
				cd_rabbit->offset = vec2_add(
					cd_rabbit->offset, 
					vec2(-penetration_x, 0.0f)
				);
			}

		}


	}
	// Collision with spring branch (bounce)
	else if(other->type == OBJ_SPRING_BRANCH_TYPE && !d->touching_ground &&
		vel.y > 500.0f && d->bounce_force.y == 0.0f) {

		d->mushroom_hit_time = time_s();
		anim_play_ex(rabbit->anim, "bounce", TIME_S);
		vel.y = -vel.y;

		Vector2 f = {
			.x = MIN(vel.x*d->xjump, 110000.0f),
			.y = MAX(vel.y*d->yjump,-250000.0f)
		};

		d->bounce_force = vec2_scale(f,1.2f);


		// Slow down vertical movement
		self->physics->vel.y *= 0.2f;

		// Delay actual bouncing 0.1s
		async_schedule(_rabbit_delayed_bounce, 20, self);
	}

	// Collision with branches (run)
	else if(other->type == OBJ_BRANCH_TYPE ||

		(other->type == OBJ_SPIKE_BRANCH_TYPE && d->spike_hit) ||

		(other->type == OBJ_SPRING_BRANCH_TYPE && d->bounce_force.y == 0.0f)

		) {

		d->previuos_hit = other;

		if(other->type != OBJ_SPIKE_BRANCH_TYPE) d->spike_hit = false;

		// Branch run
		CDObj* cd_rabbit = self->physics->cd_obj;
		CDObj* cd_ground = other->physics->cd_obj;
		float rabbit_bottom = cd_rabbit->pos.y + cd_rabbit->size.size.y;
		float ground_top = cd_ground->pos.y;

		if(p->vel.y >= 0.0f){

			float penetration = (rabbit_bottom + cd_rabbit->offset.y) - ground_top;
			if(penetration > 0.0f && cd_rabbit->pos.y < ground_top && !d->over_branch  ) {

				self->physics->vel.y = 0.0f;
				d->over_branch = false;
				if(!d->touching_ground) {
					if(d->player_control) hud_trigger_combo(0);
					anim_play_ex(rabbit->anim, "land", TIME_S);
					d->shield_dh = -20.0f;
				}
				d->touching_ground = true;
				cd_rabbit->offset = vec2_add(
					cd_rabbit->offset, 
					vec2(0.0f, -penetration)
				);
			}

		} else {
			d->over_branch = true;
		}


	}

	// Collision with fall trigger
	else if(other->type == OBJ_FALL_TRIGGER_TYPE) {
		d->touching_ground = false;	

		// Trampoline
		if(p->cd_obj->pos.y > ground_y && d->has_powerup[TRAMPOLINE] && !d->trampoline_placed && !rabbit->data->game_over){


			// Find next ground tile
			Vector2 start = vec2(p->cd_obj->pos.x,v_height - 10.0f);
			Vector2 end = vec2_add(start,vec2(400.0f,0.0f));

			Vector2 hitpoint;
			CDObj* cdobj = coldet_cast_segment(
				objects_cdworld, start, end, OBJ_GROUND_TYPE & ~collision_flag, &hitpoint
			);

			GameObject* right = NULL;

			if(cdobj)
				right = (GameObject*)cdobj->userdata;

			assert(right);

			end = vec2_add(start,vec2(-400.0f,0.0f));

			cdobj = coldet_cast_segment(
				objects_cdworld, start, end, OBJ_GROUND_TYPE & ~collision_flag, &hitpoint
			);

			GameObject* left = NULL;

			if(cdobj)
				left = (GameObject*)cdobj->userdata;

			assert(left);	

			d->trampoline_placed = true;
			d->has_powerup[TRAMPOLINE] = false;

			// Trampoline sprite
			SprHandle sprt = sprsheet_get_handle("trampoline_obj");
			Vector2 size = sprsheet_get_size_h(sprt);
			float width = size.x;

			Vector2 pos = vec2(0.0f,v_height + 15.0f);
			pos.x = (left->physics->cd_obj->pos.x + left->physics->cd_obj->size.size.x + right->physics->cd_obj->pos.x - width) / 2.0f;		

			// Create Trampoline
			ObjTrampoline* trampoline = (ObjTrampoline*) objects_create(&obj_trampoline_desc, pos, (void*)sprt);
			trampoline->owner = self;
		}

	}

	// Collision with speed trigger
	else if(other->type == OBJ_SPEED_TRIGGER_TYPE && d->rocket_time == 0.0f ) {
		if(!d->has_powerup[SHIELD]){
			ObjSpeedTrigger* t = (ObjSpeedTrigger*)other;
			objects_apply_force(self, 
				vec2(-self->physics->vel.x * t->drag_coef, 0.0f)
			);
		}
		d->on_water = true;
	}

	// Collision with mushroom
	else if(other->type == OBJ_MUSHROOM_TYPE && !d->touching_ground &&
		vel.y > 500.0f && d->bounce_force.y == 0.0f) {

		d->previuos_hit = other;

		d->mushroom_hit_time = time_s();
		anim_play_ex(rabbit->anim, "bounce", TIME_S);
		vel.y = -vel.y;

		Vector2 f = {
			.x = MIN(vel.x*d->xjump, 110000.0f),
			.y = MAX(vel.y*d->yjump,-250000.0f)
		};

		d->bounce_force = f;

		// Slow down vertical movement
		self->physics->vel.y *= 0.2f;

		// Delay actual bouncing 0.1s
		async_schedule(_rabbit_delayed_bounce, 100, self);
	}

	// Collision with trampoline
	else if(other->type == OBJ_TRAMPOLINE_TYPE) {
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
	CharacterParams* c = (CharacterParams*)user_data;

	ObjRabbit* rabbit = (ObjRabbit*)self;
	rabbit->anim = anim_new_ex(c->animation, TIME_S);
	rabbit->control = c->control;
	// Init physics
	PhysicsComponent* physics = self->physics;
	RectF rect = rectf_centered(pos, rabbit_hitbox_width, rabbit_hitbox_height);
	physics->cd_obj = coldet_new_aabb(objects_cdworld, &rect, OBJ_RABBIT_TYPE, NULL);
	float mass = 3.0f;
	physics->inv_mass = 1.0f / mass;
	physics->vel = vec2(0.0f, 0.0f);
	physics->hit_callback = obj_rabbit_collide;

	physics->cd_obj->pos.y = ground_y;

	// Init render
	Vector2 size = sprsheet_get_size_h(c->sprite);
	RenderComponent* render = self->render;
	render->world_dest = rectf_centered(pos, size.x, size.y);
	render->angle = 0.0f;
	render->anim_frame = 0;
	render->update_pos = obj_rabbit_update_pos;
	render->post_render = obj_rabbit_post_render;
	render->became_visible = obj_rabbit_became_visible;
	render->became_invisible = obj_rabbit_became_invisible;
	render->spr = c->sprite;

	// Init update
	UpdateComponent* update = self->update;
	update->update = obj_rabbit_update;

	// additional rabbit data
	rabbit->data = MEM_ALLOC(sizeof(ObjRabbitData));
	ObjRabbitData* d = rabbit->data;
	memset(d, 0, sizeof(ObjRabbitData));
	// Everything is initialized to zero, except these:
	d->sprite_offset = c->sprite_offset;
	d->rabbit_name = c->name;
	d->speed = c->speed;
	d->xjump = c->xjump;
	d->yjump = c->yjump;
	d->ai_max_combo = c->ai_max_combo;
	d->player_control = rabbit->control == obj_rabbit_player_control;

	if(d->player_control){
		render->layer = rabbit_layer;

		// Transfer shop powerups to rabbit
		for(uint i = 0; i < POWERUP_COUNT;i++){
			rabbit->data->has_powerup[i] = powerups[i];
		}

	}
	else
		render->layer = ai_rabbit_layer;

	// Load once
	if(shield_width == 0.0f){
		shield_spr = sprsheet_get_handle("bubble_obj");
		size = sprsheet_get_size_h(shield_spr);
		shield_width = size.x;
		shield_height = size.y;
	}
	if(ground_y == 0) ground_y = v_height - 128.0f;
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