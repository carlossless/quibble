#include "physics.h"
#include <gfx_utils.h>
#include <particles.h>
#include "arena.h"
#include "sounds.h"
#include "ai_precalc.h"
#include "chipmunk/chipmunk.h"

#define SHIP_COLLISION 1
#define WALL_COLLISION 2
#define BULLET_COLLISION 3
#define PLATFORM_COLLISION 4

#define DEBUG_DRAW_LAYER 7

PhysicsState physics_state;

typedef struct {
	cpBody* body;
	cpShape* shape;
	bool remove;
} Bullet;

typedef struct {
	cpBody* body;
	cpShape* shape;
} Ship;	

typedef struct {
	cpBody* body;
	cpShape* shape;
} Platform;	

// Physics state
cpSpace* space;
Ship ships[MAX_SHIPS];
Bullet bullets[MAX_BULLETS];
Platform platforms[MAX_PLATFORMS];
cpBody* static_body;

// Tweakables
float ship_min_mass = 1.0f;
float ship_max_mass = 3.0f;
float ship_elasticity = 0.8f;
float ship_friction = 0.1f;
float ship_acceleration = 800.0f;
float ship_turn_speed = 1.0f;
float ship_turn_damping = 0.9f;
float ship_turn_friction = 0.99f; 
float ship_zrot_min_speed = 720.0f;
float ship_zrot_acceleration = 100.0f;
float ship_zrot_damping = 0.98f;
float ship_velocity_limit = 1000.0f;
float ship_damping = 0.997f;
float ship_walldmg_velocity = 250.0f;
float wall_elasticity = 0.8f;
float wall_friction = 0.1f;
float bullet_speed = 400.0f;
float bullet_mass = 1.0f;
float bullet_inertia = 0.1f;
float bullet_radius = 5.0f;
float platform_radius = 20.0f;
float platform_neutral_force = -500.0f;
float platform_active_force = 500.0f;
float dmap_linear_factor = 0.95f;
float dmap_angular_factor = 0.95f;
float shockwave_push_radius = 160.0f;
float shockwave_push_force = 10000.0f;

#define GAME_TWEAK(name, min, max) \
	tweaks_float(tweaks, #name, &name, min, max)
	
void physics_register_tweaks(Tweaks* tweaks) {
	tweaks_group(tweaks, "physics");
	GAME_TWEAK(ship_min_mass, 0.1f, 10.0f);
	GAME_TWEAK(ship_max_mass, 0.1f, 10.0f);
	GAME_TWEAK(ship_elasticity, 0.01f, 0.99f);
	GAME_TWEAK(ship_friction, 0.01f, 0.99f);
	GAME_TWEAK(ship_acceleration, 100.0f, 4000.0f);
	GAME_TWEAK(ship_turn_speed, 0.1f, PI);
	GAME_TWEAK(ship_turn_damping, 0.8f, 0.99f);
	GAME_TWEAK(ship_turn_friction, 0.95f, 0.999f);
	GAME_TWEAK(ship_zrot_min_speed, 100.0f, 2000.0f);
	GAME_TWEAK(ship_zrot_damping, 0.9f, 0.99f);
	GAME_TWEAK(ship_velocity_limit, 100.0f, 2000.0f);
	GAME_TWEAK(ship_damping, 0.98f, 0.999f);
	GAME_TWEAK(ship_walldmg_velocity, 10.0f, 1000.0f);
	GAME_TWEAK(wall_elasticity, 0.01f, 0.99f);
	GAME_TWEAK(bullet_speed, 50.0f, 1000.0f);
	GAME_TWEAK(bullet_mass, 0.1f, 5.0f);
	GAME_TWEAK(bullet_inertia, 0.05f, 0.4f);
	GAME_TWEAK(bullet_radius, 1.0f, 10.0f);
	GAME_TWEAK(platform_radius, 5.0f, 32.0f);
	GAME_TWEAK(platform_neutral_force, -1000.0f, 1000.0f);
	GAME_TWEAK(platform_active_force, -1000.0f, 1000.0f);
	GAME_TWEAK(dmap_linear_factor, 0.8f, 1.0f);
	GAME_TWEAK(dmap_angular_factor, 0.8f, 1.0f);
	GAME_TWEAK(shockwave_push_radius, 1.0f, 200.0f);
	GAME_TWEAK(shockwave_push_force, 0.0f, 50000.0f);
}

extern float ship_min_size;
extern float ship_max_size;
extern float ship_circle_radius;

Vector2 _wrap_around_gv(Vector2 p, const RectF* rect); 

// Helpers to convert between chipmunk and greed vectors
Vector2 cpv_to_gv(cpVect v) {
	return vec2(v.x, v.y);
}

cpVect gv_to_cpv(Vector2 v) {
	return cpv(v.x, v.y);
}	

cpVect ship_shape_points[] = {
	{0.0f, 35.0f},
	{13.0f, 9.0f},
	{20.0f, -6.0f},
	{21.0f, -15.0f},
	{17.0f, -25.0f},
	{4.0f, -34.0f},
	{-4.0f, -34.0f},
	{-17.0f, -25.0f},
	{-21.0f, -15.0f},
	{-20.0f, -6.0f},
	{-13.0f, 9.0f}};

cpVect ship_gun_pos = {0.0f, -36.0f};	

const RectF screen_bounds = {0.0f, 0.0f, 480.0f, 320.0f};

// Collision callbacks	

int ship2ship_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {

	sounds_event(COLLISION_SHIP_SHIP);

	return 1;
}	

int ship2wall_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {

	cpShape* ship_shape = a->collision_type == SHIP_COLLISION ?
		a : b;
	cpBody* ship = ship_shape->body;

	// TODO: Optimize this
	uint ship_id;
	for(ship_id = 0; ship_id < n_ships; ++ship_id) {
		if(ships[ship_id].body == ship)
			break;
	}
	assert(ship_id != n_ships);

	float ship_speed_sq = vec2_length_sq(physics_state.ships[ship_id].vel);

	if(ship_speed_sq >= ship_walldmg_velocity * ship_walldmg_velocity) {
		Vector2 pos = cpv_to_gv(contacts[0].p);
		Vector2 ship_pos = physics_state.ships[ship_id].vel;

		float dir = vec2_dir(vec2_sub(ship_pos, pos)) + PI/2.0f;

		particles_spawn("wall1", &pos, dir);
	}	

	sounds_event(COLLISION_SHIP_WALL);

	return 1;
}	

int ship2bullet_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {
	cpShape* bullet_shape = a->collision_type == BULLET_COLLISION ?
		a : b;
	cpBody* bullet = bullet_shape->body;

	cpShape* ship_shape = bullet_shape == a ? b : a;
	cpBody* ship = ship_shape->body;

	// TODO: Optimize this
	uint i, n = physics_state.n_bullets;
	for(i = 0; i < n; ++i) {
		if(bullets[i].body == bullet)
			break;
	}		
	assert(i != n);
	bullets[i].remove = true;

	n = physics_state.n_ships;
	for(i = 0; i < n; ++i) {
		if(ships[i].body == ship)
			break;
	}
	assert(i != n);

	// Spawn particles
	Vector2 pos = cpv_to_gv(contacts[0].p);
	float dir = vec2_dir(cpv_to_gv(contacts[0].n));
	particles_spawn("sparks1", &pos, dir); 
	particles_spawn("sparks2", &pos, dir);

	game_bullet_hit(i);
	sounds_event(COLLISION_BULLET_SHIP);

	return 1;
}

int bullet2wall_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {
	cpShape* bullet_shape = a->collision_type == BULLET_COLLISION ?
		a : b;
	cpBody* bullet = bullet_shape->body;

	// TODO: Optimize this
	uint i, n = physics_state.n_bullets;
	for(i = 0; i < n; ++i) {
		if(bullets[i].body == bullet)
			break;
	}		
	assert(i != n);
	bullets[i].remove = true;

	// Spawn particles
	Vector2 pos = cpv_to_gv(contacts[0].p);
	float dir = physics_state.bullets[i].rot + PI * 0.5f;
	particles_spawn("sparks1", &pos, dir); 
	particles_spawn("sparks2", &pos, dir);

	sounds_event(COLLISION_BULLET_WALL);

	return 0;
}	

int bullet2bullet_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {
	return 0;
}	

int platform2bullet_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {
	return 0;
}

int platform2ship_callback(cpShape* a, cpShape* b, cpContact* contacts,
	int num_contacts, cpFloat normal_coeff, void* data) {
	cpShape* ship_shape = a->collision_type == SHIP_COLLISION ?
		a : b;
	cpShape* platform_shape = a->collision_type == PLATFORM_COLLISION ?
		a : b;
	
	cpBody* ship = ship_shape->body;
	cpBody* platform = platform_shape->body;

	// TODO: Optimize this
	uint ship_id;
	for(ship_id = 0; ship_id < n_ships; ++ship_id) {
		if(ships[ship_id].body == ship)
			break;
	}
	assert(ship_id != n_ships);

	uint platform_id;
	for(platform_id = 0; platform_id < n_platforms; ++platform_id) {
		if(platforms[platform_id].body == platform)
			break;
	}
	assert(platform_id != n_platforms);

	PlatformState* pstate = &platform_states[platform_id];
	if(pstate->color != ship_id) {
		// Do nothing is platform is already in transition
		if(pstate->color == pstate->last_color) {
			platform_states[platform_id].color = ship_id;
			platform_states[platform_id].activation_t = time_ms() / 1000.0f;

			game_platform_taken(ship_id, platform_id);
		}	
	}	

	// Push/pull force
	cpVect platform_to_ship = 
		cpvsub(ships[ship_id].body->p, platforms[platform_id].body->p);
	float distance = cpvlength(platform_to_ship);	
	float max_distance = platform_radius + ship_circle_radius;
	platform_to_ship = cpvnormalize(platform_to_ship);	

	float t = 1.0f - normalize(distance, 0.0f, max_distance);
	float force = t*t * 
		(platform_states[platform_id].last_color == MAX_UINT32 ?
		platform_neutral_force : platform_active_force);

	cpVect force_vec = cpvmult(platform_to_ship, force);

	cpBodyApplyForce(ships[ship_id].body, force_vec, cpvzero);	

	return 0;
} 

void physics_init(void) {
	cpInitChipmunk();
}

void _space_init(void) {
	cpResetShapeIdCounter();
	space = cpSpaceNew();
	space->worldWidth = screen_bounds.right;
	space->worldHeight = screen_bounds.bottom;
	space->iterations = 4;
	space->elasticIterations = 4;


	// TODO: Tweak these numbers
	cpSpaceResizeStaticHash(space, 32.0f, 1000);
	cpSpaceResizeActiveHash(space, 32.0f, 100);

	space->gravity = cpvzero;

	cpSpaceAddCollisionPairFunc(space, 
		SHIP_COLLISION, SHIP_COLLISION, ship2ship_callback, NULL);
	cpSpaceAddCollisionPairFunc(space,
		SHIP_COLLISION, WALL_COLLISION, ship2wall_callback, NULL);
	cpSpaceAddCollisionPairFunc(space,
		SHIP_COLLISION, BULLET_COLLISION, ship2bullet_callback, NULL);
	cpSpaceAddCollisionPairFunc(space,
		BULLET_COLLISION, WALL_COLLISION, bullet2wall_callback, NULL);
	cpSpaceAddCollisionPairFunc(space, 
		BULLET_COLLISION, BULLET_COLLISION, bullet2bullet_callback, NULL);
	cpSpaceAddCollisionPairFunc(space,
		PLATFORM_COLLISION, BULLET_COLLISION, platform2bullet_callback, NULL);
	cpSpaceAddCollisionPairFunc(space,
		PLATFORM_COLLISION, SHIP_COLLISION, platform2ship_callback, NULL);
	
	physics_state.n_bullets = 0;
}	

void _space_close(void) {
	if(!space)
		return;
	if(static_body)
		cpBodyFree(static_body);
	cpSpaceFreeChildren(space);
	cpSpaceFree(space);
	space = NULL;
}	

void physics_close(void) {
	_space_close();
	cpCloseChipmunk();
}
void physics_reset(uint n_ships) {
	assert(n_ships);
	assert(n_ships <= MAX_SHIPS);

	_space_close();
	_space_init();
	physics_state.n_ships = n_ships;

	// TODO: Make moment change when mass changes
	float ship_moment = cpMomentForPoly(ship_min_mass, 3, ship_shape_points, cpvzero);

	// Add ships
	uint i;
	for(i = 0; i < n_ships; ++i) {
		physics_state.ships[i].exploded = false;
		physics_state.ships[i].pos = current_arena_desc.spawnpoints[i]; 
		physics_state.ships[i].vel = vec2(0.0f, 0.0f);
		physics_state.ships[i].target_rot = -1.0f;
		physics_state.ships[i].rot = 0.0f;
		physics_state.ships[i].ang_vel = 0.0f;
		physics_state.ships[i].scale = ship_min_size;
		physics_state.ships[i].mass = ship_min_mass;
		physics_state.ships[i].zrot = 0.0f;
		physics_state.ships[i].zrot_vel = ship_zrot_min_speed;

		ships[i].body = cpBodyNew(ship_min_mass, ship_moment);
		ships[i].body->p = 
			gv_to_cpv(current_arena_desc.spawnpoints[i]);
		ships[i].shape = cpPolyShapeNew(ships[i].body,
			ARRAY_SIZE(ship_shape_points), ship_shape_points, cpvzero);
		ships[i].shape->e = ship_elasticity;
		ships[i].shape->u = ship_friction;
		ships[i].shape->collision_type = SHIP_COLLISION;

		cpSpaceAddBody(space, ships[i].body);
		cpSpaceAddShape(space, ships[i].shape);

		physics_set_ship_size(i, ship_min_size);
	}	

	// Add arena geometry
	cpVect verts[3];	
	static_body = cpBodyNew(INFINITY, INFINITY);
	for(i = 0; i < current_arena_desc.n_tris; ++i) {
		verts[0] = gv_to_cpv(current_arena_desc.collision_tris[i].p1);	
		verts[1] = gv_to_cpv(current_arena_desc.collision_tris[i].p3);	
		verts[2] = gv_to_cpv(current_arena_desc.collision_tris[i].p2);	
		cpShape* tri = cpPolyShapeNew(static_body, 3, verts, cpvzero);
		
		tri->e = wall_elasticity;
		tri->u = wall_friction;
		tri->collision_type = WALL_COLLISION;
		cpSpaceAddStaticShape(space, tri);
	}

	// Add platforms
	assert(current_arena_desc.platforms);
	for(i = 0; i < current_arena_desc.n_platforms; ++i) {
		platforms[i].body = cpBodyNew(INFINITY, INFINITY);
		platforms[i].body->p = 
			gv_to_cpv(current_arena_desc.platforms[i]);
		platforms[i].shape = 
			cpCircleShapeNew(platforms[i].body, platform_radius, cpvzero);
		platforms[i].shape->collision_type = PLATFORM_COLLISION;

		cpSpaceAddBody(space, platforms[i].body);
		cpSpaceAddShape(space, platforms[i].shape);
	}	
	
}	

void physics_spawn_bullet(uint ship) {
	assert(ship < physics_state.n_ships);

	if(physics_state.ships[ship].exploded)
		return;

	if(physics_state.n_bullets == MAX_BULLETS)
		LOG_ERROR("Bullet buffer overflow");

	cpVect scaled_gun_pos = cpvmult(ship_gun_pos, physics_state.ships[ship].scale);
	cpVect bullet_pos = cpBodyLocal2World(ships[ship].body, scaled_gun_pos); 	
	cpVect bullet_dir = cpv(sinf(ships[ship].body->a), -cosf(ships[ship].body->a));
	float ship_speed = cpvdot(ships[ship].body->v, bullet_dir);
	bullet_dir = cpvmult(bullet_dir, ship_speed + bullet_speed);

	uint i = physics_state.n_bullets;
	bullets[i].body = cpBodyNew(bullet_mass, bullet_inertia);	
	bullets[i].body->p = bullet_pos;
	bullets[i].body->v = bullet_dir;
	bullets[i].shape = cpCircleShapeNew(bullets[i].body, bullet_radius, cpvzero);
	bullets[i].shape->collision_type = BULLET_COLLISION;
	bullets[i].remove = false;

	cpSpaceAddBody(space, bullets[i].body);
	cpSpaceAddShape(space, bullets[i].shape);

	physics_state.bullets[i].rot = ships[ship].body->a;

	Vector2 pos = _wrap_around_gv(cpv_to_gv(bullet_pos), &screen_bounds);
	float dir = physics_state.bullets[i].rot - PI * 0.5f;
	particles_spawn("shot", &pos, dir); 

	physics_state.n_bullets++;
}	

void physics_control_ship(uint ship, bool rot_left, bool rot_right, bool acc) {
	assert(ship < physics_state.n_ships);

	if(physics_state.ships[ship].exploded)
		return;

	float vel = vec2_length_sq(physics_state.ships[ship].vel);

	cpVect world_force = cpBodyLocal2World(ships[ship].body, 
		cpv(0.0f, -ship_acceleration));
	world_force = cpvsub(world_force, ships[ship].body->p);
	if(acc && vel < ship_velocity_limit*ship_velocity_limit) {
		cpBodyApplyForce(ships[ship].body, world_force, cpvzero);
		physics_state.ships[ship].zrot_vel += ship_zrot_acceleration;
	}	
	if(rot_right)
		ships[ship].body->w += ship_turn_speed;
	if(rot_left)
		ships[ship].body->w -= ship_turn_speed;
}

void physics_control_ship_ex(uint ship, float target_angle, float acc) {
	assert(ship < physics_state.n_ships);

	if(physics_state.ships[ship].exploded)
		return;
	
	acc = clamp(0.0f, 1.0f, acc);

	float vel_sq = vec2_length_sq(physics_state.ships[ship].vel);
	cpVect world_force = cpBodyLocal2World(ships[ship].body,
		cpv(0.0f, -ship_acceleration * acc));
	world_force = cpvsub(world_force, ships[ship].body->p);
	if(vel_sq < ship_velocity_limit*ship_velocity_limit) {
		cpBodyApplyForce(ships[ship].body, world_force, cpvzero);
		physics_state.ships[ship].zrot_vel += ship_zrot_acceleration * acc;
	}

	float ta = target_angle;
	if(ta == -1.0f) 
		return;

	float a = ships[ship].body->a;

	// Find optimal target angle
	float d = fabsf(ta - a);
	float two_pi = 2.0f * PI;
	if(fabs(ta + two_pi - a) < d) {
		ta += two_pi;
		d = fabs(ta + two_pi - a);
	}	
	if(fabs(ta - two_pi - a) < d) {
		ta -= two_pi;
	}	

	// HACK, heuristical rotation dif. equation solving
	const float dt = 1.0f / 60.0f;

	if(fabsf(ta - a) < 0.08f) {
		ships[ship].body->w *= 0.5f;
		return;
	}

	float w = ships[ship].body->w;
	float turn = clamp(-ship_turn_speed, ship_turn_speed,
		(ta - a) / (ship_turn_damping * dt) - w);

	float nw = (w + turn) * ship_turn_damping;
	float na = a + nw * dt;
	float nnw = (nw + turn) * ship_turn_damping;
	float nna = na + nnw * dt;

	float flip = 1.0f;

	// a < na < nna < ta
	if(a > ta) {
		a *= -1.0f;
		ta *= -1.0f;
		na *= -1.0f;
		nna *= -1.0f;
		flip *= -1.0f;
	}

	if(ta > nna) {
		ships[ship].body->w += turn;
		return;
	}

	ships[ship].body->w -= turn;
}

void physics_set_ship_size(uint ship, float size) {
	assert(ship < physics_state.n_ships);

	// Determine mass by simple proportion
	float mass = (ship_min_mass * size) / ship_min_size;

	// Set new size and mass
	physics_state.ships[ship].scale = size;
	physics_state.ships[ship].mass = mass;
	cpPolyShape* ship_shape = (cpPolyShape*)ships[ship].shape;
	cpBody* ship_body = (cpBody*)ships[ship].body;
	ship_shape->scale = size;
	cpBodySetMass(ship_body, mass);
}	

void physics_shockwave(uint ship) {
	assert(ship < physics_state.n_ships);

	Vector2 pos = physics_state.ships[ship].pos;

	for(uint i = 0; i < physics_state.n_ships; ++i) {
		if(i == ship)
			continue;

		Vector2 npos = physics_state.ships[i].pos;

		// Use ai routines to get shortest path
		Segment path = ai_shortest_path(pos, npos);
		Segment seg1, seg2;
		float dist;
		if(ai_split_path(path, &seg1, &seg2)) {
			dist = segment_length(seg1);
			dist += segment_length(seg2);
		}
		else {
			seg2 = path;
			dist = segment_length(seg2);
		}

		// Push if within radius
		if(dist < shockwave_push_radius) {
			float force = shockwave_push_force * (dist / shockwave_push_radius);
			Vector2 dir = vec2_normalize(vec2_sub(seg2.p2, seg2.p1));
			cpVect push = gv_to_cpv(vec2_scale(dir, force));
			cpBodyApplyForce(ships[i].body, push, cpvzero);
		}
	}
}

cpVect _wrap_around(cpVect p, const RectF* rect) {
	if(p.x > rect->right) 
		p.x -= rect->right - rect->left;
	if(p.x < rect->left)
		p.x += rect->right - rect->left;
	if(p.y > rect->bottom)
		p.y -= rect->bottom - rect->top;
	if(p.y < rect->top)
		p.y += rect->bottom - rect->top;
	return p;	
}		

Vector2 _wrap_around_gv(Vector2 p, const RectF* rect) {
	if(p.x > rect->right) 
		p.x -= rect->right - rect->left;
	if(p.x < rect->left)
		p.x += rect->right - rect->left;
	if(p.y > rect->bottom)
		p.y -= rect->bottom - rect->top;
	if(p.y < rect->top)
		p.y += rect->bottom - rect->top;
	return p;	
}		

uint _dmap_grid_cell(Vector2 p) {
	assert(p.x >= 0.0f && p.x <= SCREEN_WIDTH);
	assert(p.y >= 0.0f && p.y <= SCREEN_HEIGHT);

	float dmap_cell_width = (float)SCREEN_WIDTH / (float)DENSITY_MAP_WIDTH;
	float dmap_cell_height = (float)SCREEN_HEIGHT / (float)DENSITY_MAP_HEIGHT;
	uint x = (uint)floorf(p.x / dmap_cell_width);
	assert(x < DENSITY_MAP_WIDTH);
	uint y = (uint)floorf(p.y / dmap_cell_height);
	assert(y < DENSITY_MAP_HEIGHT);

	return y * DENSITY_MAP_WIDTH + x;
}

void physics_update(float dt) {

	// Remove destroyed bullets
	uint i;
	for(i = 0; i < physics_state.n_bullets; ++i) {

		if(bullets[i].remove) {
			cpShape* shape = bullets[i].shape;
			cpBody* body = bullets[i].body;

			cpSpaceRemoveShape(space, shape);
			cpSpaceRemoveBody(space, body);

			cpShapeFree(shape);
			cpBodyFree(body);

			bullets[i] = bullets[physics_state.n_bullets-1];
			i--;
			physics_state.n_bullets--;
		}
	}

	// Remove destroyed ships
	for(i = 0; i < physics_state.n_ships; ++i) {
		if(ship_states[i].is_exploding &&
			!physics_state.ships[i].exploded) {

			cpShape* shape = ships[i].shape;
			cpBody* body = ships[i].body;

			cpSpaceRemoveShape(space, shape);
			cpSpaceRemoveBody(space, body);

			cpShapeFree(shape);
			cpBodyFree(body);

			physics_state.ships[i].exploded = true;
		}
	}

	// Damp ship rotations
	for(i = 0; i < physics_state.n_ships; ++i) { 
		if(!physics_state.ships[i].exploded)
			ships[i].body->w *= ship_turn_damping;	
	}	

	// Simulate next step
	cpSpaceStep(space, dt);

	// Update public state
	for(i = 0; i < physics_state.n_ships; ++i) {
		if(physics_state.ships[i].exploded)
			continue;
		
		// Wrap around if ship is out of screen
		Vector2 ship_pos = cpv_to_gv(ships[i].body->p);
		bool out_of_screen = !rectf_contains_point(&screen_bounds, &ship_pos);
		if(out_of_screen) 
			ships[i].body->p = _wrap_around(ships[i].body->p, &screen_bounds);

		// Simulate friction by decreasing speed
		ships[i].body->v.x *= ship_damping;
		ships[i].body->v.y *= ship_damping;
		ships[i].body->w *= ship_turn_friction;

		// More friction depending on density map
		float* dmap = current_arena_desc.density_map;
		if(dmap) {
			uint idx = _dmap_grid_cell(vec2(ships[i].body->p.x, ships[i].body->p.y));
			float dmap_linear_damping = lerp(dmap_linear_factor, 1.0f, dmap[idx]); 
			float dmap_angular_damping = lerp(dmap_angular_factor, 1.0f, dmap[idx]);

			ships[i].body->v.x *= dmap_linear_damping;
			ships[i].body->v.y *= dmap_linear_damping;
			ships[i].body->w *= dmap_angular_damping;
		}

		physics_state.ships[i].pos = cpv_to_gv(ships[i].body->p);
		physics_state.ships[i].vel = cpv_to_gv(ships[i].body->v);
		physics_state.ships[i].rot = ships[i].body->a;
		physics_state.ships[i].ang_vel = ships[i].body->w;
		physics_state.ships[i].mass = ships[i].body->m;
		physics_state.ships[i].zrot += physics_state.ships[i].zrot_vel * dt;
		if(physics_state.ships[i].zrot > 360.0f)
			physics_state.ships[i].zrot -= 360.0f;
		physics_state.ships[i].zrot_vel *= ship_zrot_damping;
		physics_state.ships[i].zrot_vel = MAX(ship_zrot_min_speed,
			physics_state.ships[i].zrot_vel);

		cpBodyResetForces(ships[i].body);
	}	
	for(i = 0; i < physics_state.n_bullets; ++i) {
		Vector2 bullet_pos = cpv_to_gv(bullets[i].body->p);
		bool out_of_screen = !rectf_contains_point(&screen_bounds, &bullet_pos);
		if(out_of_screen)
			bullets[i].body->p = _wrap_around(bullets[i].body->p, &screen_bounds);

		physics_state.bullets[i].pos = cpv_to_gv(bullets[i].body->p);
	}	
}	

void physics_debug_draw(void) {
	Vector2 ship_vertices[ARRAY_SIZE(ship_shape_points)];

	// Draw ships
	uint i, j;
	for(i = 0; i < physics_state.n_ships; ++i) {
		if(physics_state.ships[i].exploded)
			continue;

		// Copy untransformed ship vertices
		for(j = 0; j < ARRAY_SIZE(ship_vertices); ++j)
			ship_vertices[j] = cpv_to_gv(ship_shape_points[j]);
		// Transform	
		gfx_transform(ship_vertices, ARRAY_SIZE(ship_vertices), &(physics_state.ships[i].pos),
			physics_state.ships[i].rot, physics_state.ships[i].scale);
		// Draw	
		gfx_draw_poly(DEBUG_DRAW_LAYER, ship_vertices, ARRAY_SIZE(ship_vertices), COLOR_WHITE);
		
		// Enclosing circle
		gfx_draw_circle(DEBUG_DRAW_LAYER, &physics_state.ships[i].pos, 
			physics_state.ships[i].scale * ship_circle_radius, COLOR_WHITE);
	}	

	// Draw bullets
	for(i = 0; i < physics_state.n_bullets; ++i) { 
		gfx_draw_circle_ex(DEBUG_DRAW_LAYER, &physics_state.bullets[i].pos,
			bullet_radius, COLOR_WHITE, 5);
	}	

	// Draw walls
	//assert(current_arena_desc.collision_tris);
	for(i = 0; i < current_arena_desc.n_tris; ++i) {
		gfx_draw_tri(DEBUG_DRAW_LAYER, &(current_arena_desc.collision_tris[i]),
			COLOR_WHITE);
	}		

	// Draw platfoms
	for(i = 0; i < current_arena_desc.n_platforms; ++i) {
		gfx_draw_circle(DEBUG_DRAW_LAYER, &current_arena_desc.platforms[i],
			platform_radius, COLOR_WHITE);
	}

	// Density map
	float* dmap = current_arena_desc.density_map;
	if(dmap) {
		for(uint y = 0; y < DENSITY_MAP_HEIGHT; ++y) {
			for(uint x = 0; x < DENSITY_MAP_WIDTH; ++x) {
				float s = (float)SCREEN_WIDTH / (float)DENSITY_MAP_WIDTH;
				Vector2 pos = vec2((float)x+0.5f, (float)y+0.5f);
				pos = vec2_scale(pos, s);

				uint idx = _dmap_grid_cell(pos);
				if(dmap[idx] < 0.8f) {
					float r = lerp(10.0f, 0.5f, dmap[idx]); 	
					gfx_draw_circle_ex(DEBUG_DRAW_LAYER+1, &pos, r,
							COLOR_RGBA(128, 128, 128, 255), 3);
				}		
			}
		}
	}	
}	

Vector2 physics_wraparound(Vector2 in) {
	if(!rectf_contains_point(&screen_bounds, &in)) 
		return _wrap_around_gv(in, &screen_bounds);	
	else
		return in;
}
	
