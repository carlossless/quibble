#ifndef ARENA_H
#define ARENA_H

#include "utils.h"
#include "system.h"
#include "game.h"

typedef struct {
	TexHandle img;
	Vector2 shadow_shift;
	Vector2 spawnpoints[MAX_SHIPS];
	uint n_platforms;
	Vector2* platforms;

	uint n_tris;
	Triangle* collision_tris;
} ArenaDesc;

extern ArenaDesc current_arena_desc;

void arena_init(void);
void arena_close(void);
void arena_reset(const char* filename, uint n_ships);

void arena_update(float dt);
void arena_draw(void);

#endif
