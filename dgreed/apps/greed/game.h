#ifndef GAME_H
#define GAME_H

#include "utils.h"

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define MAX_SHIPS 4
#define MAX_BULLETS 128
#define MAX_PLATFORMS 16

typedef struct {
	float energy;
	float last_bullet_t;
	float vortex_opacity;
	float vortex_angle;
	bool is_accelerating;
} ShipState;	

#define PLATFORM_NEUTRAL MAX_UINT32

typedef struct {
	// Color is ship number, MAX_UINT32 is neutral;
	uint color;
	uint last_color;
	float color_fade;
	float activation_t;
	float ring_angle;
} PlatformState;

extern uint n_ships;
extern uint n_platforms;
extern ShipState ship_states[MAX_SHIPS];
extern PlatformState platform_states[MAX_PLATFORMS];

void game_init(void);
void game_close(void);

void game_reset(const char* arena, uint n_players);
void game_update(void);
void game_platform_taken(uint ship, uint platform);
void game_render(void);

#endif
