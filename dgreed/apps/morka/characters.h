#ifndef GAME_CHARACTER_H
#define GAME_CHARACTER_H

#define character_count 4

#include "obj_types.h"

typedef struct {
	const char* name;
	const char* icon;
	const char* spr_handle;
	const char* animation;
	const char* description;
	uint cost;
	float speed;
	float xjump;
	float yjump;
	float sprite_offset;
} CharacterDefaultParams;

typedef struct {
	const char* name;
	const char* animation;	
	SprHandle sprite;
	float speed;
	float xjump;
	float yjump;
	ControlCallback control;
	uint ai_max_combo;
	float sprite_offset;
} CharacterParams;

extern CharacterDefaultParams default_characters[];

#endif
