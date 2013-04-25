#include "characters.h"

CharacterDefaultParams default_characters[character_count] = {
	{
	"Dash",
	"rabbit_icon",
	"rabbit",
	"rabbit",
	"Runs faster",
	0,
	600.0f,
	100.0f,
	400.0f,
	0.0f
	},

	{
	"Skippy",
	"squirrel_icon",
	"squirrel",
	"squirrel",
	"Jumps higher",
	49,	
	500.0f,
	120.0f,
	420.0f,
	-30.0f	
	},

	{
	"Stinky",
	"skunk_icon",
	"skunk",
	"squirrel",
	"Slows everyone",
	149,	
	510.0f,
	110.0f,
	410.0f,
	-30.0f		
	},

	{
	"Richman",
	"hare_icon",
	"golden_rabbit",
	"rabbit",
	"Attracts coins",
	99,	
	490.0f,
	110.0f,
	420.0f,
	0.0f		
	}			
};