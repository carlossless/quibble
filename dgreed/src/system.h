#ifndef SYSTEM_H
#define SYSTEM_H

#include "utils.h"

/*
----------------
--- Graphics ---
----------------
*/

#ifndef NO_DEVMODE
// This is quite implementation-specific,
// put somewhere else in future
typedef struct {
	uint frame;
	uint active_textures;

	// Per-frame stats
	uint frame_layer_sorts;
	uint frame_batches;
	uint frame_rects;
	uint frame_lines;
	uint frame_texture_switches;

	// Per-layer stats
	uint n_layers;
	uint* layer_rects;
	uint* layer_lines;
} VideoStats;

const VideoStats* video_stats(void);
#endif

typedef enum {
	BM_NORMAL,
	BM_ADD,
	BM_MULTIPLY
} BlendMode;

// Returns native screen resolution, video does not need 
// to be initialized for this
void video_get_native_resolution(uint* width, uint* height);
// Initializes video
void video_init(uint width, uint height, const char* name); 
// Initializes video, sets virtual resolution
void video_init_ex(uint width, uint height, uint v_width, uint v_height, const
	char* name, bool fullscreen);
// Same as above, but without texture filtering for "retro" look
void video_init_exr(uint width, uint height, uint v_width, uint v_height, const
	char* name, bool fullscreen);
// Deinitializes video
void video_close(void);
// Sets clear color, black by default
void video_clear_color(Color c);
// Flips backbuffer and frontbuffer, sleeps to hit 60fps.
// Returns false if app is closed, true otherwise.
void video_present(void);
// Returns number of frame which is currently being rendered
uint video_get_frame(void);
// Sets a blendmode for a layer
void video_set_blendmode(uint layer, BlendMode bmode);
// Sets a custom 3x2 affine transform matrix for a layer,
// NULL means identity.
void video_set_transform(uint layer, float* matrix);

typedef uint TexHandle;
typedef void (*TexFilter)(Color* texels, uint w, uint h);

// Creates empty texture, filled with transparent white
TexHandle tex_create(uint width, uint height);
// Loads texture from .png file, it must have pow2 dimensions
TexHandle tex_load(const char* filename);
// Loads texture and applies custom filtering
TexHandle tex_load_filter(const char* filename, TexFilter filter); 
// Blits image into texture. You are responsible for clipping!
void tex_blit(TexHandle tex, Color* data, uint x, uint y, uint w, uint h);
// Returns width and height of texture in pixels
void tex_size(TexHandle tex, uint* width, uint* height); 
// Frees texture which was loaded with tex_load
void tex_free(TexHandle tex);
// Pretend that texture is s times bigger than it actually is
void tex_scale(TexHandle tex, float s);

// Draws textured rectangle. Source can be null rectangle,
// destination can have 0 for right and bottom.
void video_draw_rect(TexHandle tex, uint layer, 
	const RectF* source, const RectF* dest, Color tint);
// Draws rotated textured rectangle. 
// Same rules for source/dest applies as for video_draw_rect.
void video_draw_rect_rotated(TexHandle tex, uint layer,
	const RectF* source, const RectF* dest, float rotation, Color tint);
// Draws a line 
void video_draw_line(uint layer,
	const Vector2* start, const Vector2* end, Color color);

/*
-------------
--- Sound ---
-------------
*/

#ifndef NO_DEVMODE
typedef struct {
	uint sample_count;
	uint stream_count;
	uint playing_samples;
	uint playing_streams;
} SoundStats;	

const SoundStats* sound_stats(void);
#endif

// Initializes sound
void sound_init(void);
// Deinitializes sound
void sound_close(void);
// Must be called once each frame
void sound_update(void);

typedef uint SoundHandle;

// Loads sound sample from .wav file
SoundHandle sound_load_sample(const char* filename);
// Prepares to steam sound from .ogg file
SoundHandle sound_load_stream(const char* filename);

// Frees resources used by sound
void sound_free(SoundHandle handle);

// Plays stream or sample
void sound_play(SoundHandle handle);
// Stops sounds stream which is being played. Does nothing for samples.
void sound_stop(SoundHandle handle);

// Sets volume of sound. For samples, it takes effect next time sample is played.
void sound_set_volume(SoundHandle handle, float volume);
// Returns volume of sound
float sound_get_volume(SoundHandle handle);
// Returns length of sound in seconds
float sound_get_length(SoundHandle handle);

// Extented API for controlling specific sound sources

typedef uint SourceHandle;

// Plays stream or sample, optionally looping it, returns source.
// If result is null - sound was skipped.
SourceHandle sound_play_ex(SoundHandle handle, bool loop);
// Pauses a specific source, if it is playing
void sound_pause_ex(SourceHandle source);
// Resumes a paused source
void sound_resume_ex(SourceHandle source);
// Stops a source, SourceHandle becomes invalid
void sound_stop_ex(SourceHandle source);
// Sets a volume for a source
void sound_set_volume_ex(SourceHandle source, float volume);
// Returns current volume of a source
float sound_get_volume_ex(SourceHandle source);
// Returns play cursor position in seconds
float sound_get_pos_ex(SourceHandle source);
// Sets play cursor position
void sound_set_pos_ex(SourceHandle source, float pos);

/*
--------------------------
--- Device orientation ---
--------------------------
*/

typedef enum {
	ORIENT_LANDSCAPE_LEFT = 1,
	ORIENT_LANDSCAPE_RIGHT = 2,
	ORIENT_PORTRAIT = 4,
	ORIENT_PORTRAIT_UPSIDE_DOWN = 8
} DevOrient;

// Returns current device orientation
DevOrient orientation_current(void);

// Returns true if orientation did just change, outputs
// new orientation, animation start time and length.
bool orientation_change(DevOrient* _new, float* anim_start, float* anim_len);

/*
---------------------
--- Running state ---
---------------------
*/

typedef void (*RunStateCallback)(void);

// Sets a callback which will be invoked when
// app enters background state. Can be NULL
// if you want to disable previous callback.
void runstate_background_cb(RunStateCallback cb);

// Sets a callback which will be invoked when
// app enters foreground state. Can be NULL
// if you want to disable previous callback.
void runstate_foreground_cb(RunStateCallback cb);

/*
---------------------
--- Accelerometer ---
---------------------
*/

typedef void (*ShakeCallback)(void);

// Sets a callback which will be invoked when
// device is shaked. Can be NULL.
void acc_shake_cb(ShakeCallback cb);

/*
-------------
--- Input ---
-------------
*/

typedef enum {
	KEY_UP = 0,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_A,
	KEY_B,
	KEY_PAUSE,
	KEY_QUIT,
	KEY_COUNT
} Key;	

typedef enum {

	MBTN_PRIMARY = 0,
	MBTN_SECONDARY = 1,
	MBTN_MIDDLE,
	
	// Left/right is deprecated
	MBTN_LEFT = 0,
	MBTN_RIGHT = 1,
	
	MBTN_COUNT = 3
} MouseButton;	

// Returns true if key is being held down
bool key_pressed(Key key);
bool char_pressed(char c);

// Returns true when key was just pressed down
bool key_down(Key key);
bool char_down(char c);

// Returns true when key is released
bool key_up(Key key);
bool char_up(char c);

// Same as key_***, just for mouse
bool mouse_pressed(MouseButton button);
bool mouse_down(MouseButton button);
bool mouse_up(MouseButton button);

// Returns absolute mouse position
void mouse_pos(uint* x, uint* y);
Vector2 mouse_vec(void);

typedef struct {
	float hit_time;
	Vector2 hit_pos;
	Vector2 pos;
} Touch;

// Returns number of fingers currently touching the screen
uint touches_count(void);

// Returns array to all touches, NULL if no touches
Touch* touches_get(void);

/*
------------------
--- Text Input ---
------------------
*/

// Start capturing text. Shows on-screen keyboard if
// running on keyboardless devices.
void txtinput_start(void);
// Currently entered text in utf-8
const char* txtinput_get(void);
// Overrides entered text
void txtinput_set(const char* text);
// Returns string if user ended text capture, NULL otherwise
const char* txtinput_did_end(void);
// Stop capturing text, hide keyboard
const char* txtinput_end(void);
// Clear currently entered text
void txtinput_clear(void);

/*
-----------
--- Time --
-----------
*/

float time_s(void);
float time_ms(void);
float time_delta(void);
uint time_fps(void);

// This returns precise current time, updated continiously
uint time_ms_current(void);

/*
------------
--- Misc ---
------------
*/

bool system_update(void);

#endif

