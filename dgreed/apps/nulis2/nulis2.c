#include <malka/malka.h>
#include <malka/ml_states.h>
#include <system.h>
#include <sprsheet.h>

#include "game.h"

void dgreed_init(int argc, const char** argv) {
	log_init("nulis2.log", LOG_LEVEL_INFO);
	rand_init(432);

	video_init(1024, 768, "Nulis");

	malka_init();
	malka_params(argc, argv);

	sprsheet_init("nulis2_assets/sprsheet_768p.mml");

	malka_states_register("game", &game_state);
	malka_states_push("game");
	malka_states_init("nulis2_scripts/main.lua");
}

void dgreed_close(void) {
	malka_states_close();

	sprsheet_close();

	malka_close();
	video_close();
	log_close();
}

bool dgreed_update(void) {
	if(key_up(KEY_QUIT))
		return false;

	return true;
}

bool dgreed_render(void) {
	return malka_states_step();
}

#ifndef TARGET_IOS
int dgreed_main(int argc, const char** argv) {
	params_init(argc, argv);

	dgreed_init(argc, argv);
	while(true) {
		if(!dgreed_update())
			break;
		if(!dgreed_render())
			break;
	}
	dgreed_close();

	return 0;
}
#endif

