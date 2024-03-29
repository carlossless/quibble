#ifndef COLDET_H
#define COLDET_H

#include "utils.h"
#include "darray.h"
#include "datastruct.h"
#include "mempool.h"

// Broad-phase collission detection system, using spatial hash map.
// Supports circle, AABB and OBB primitives.

typedef struct {
	int x, y;
	ListHead objs;
} CDCell;

typedef struct {
	// Spatial hash map
	float cell_size;
	uint occupied_cells;
	uint reserved_cells;
	CDCell* cells;

	// Wrapping
	bool horiz_wrap;
	bool vert_wrap;
	float width, height;

	// Objects
	MemPool allocator;
	ListHead reinsert_list;

#ifndef NO_DEVMODE	
	// Devmode stats
	uint last_process_hittests;
	uint last_process_reinserts;
	uint max_objs_in_cell;
#endif
} CDWorld;

typedef enum {
	CD_CIRCLE = 0,
	CD_AABB,
	CD_OBB
} CDObjType;

typedef struct {
	CDObjType type;	
	uint mask;

	Vector2 pos;
	union {
		Vector2 size;
		float radius;
	} size;
	
	// Meaningfull only for OBB, must set 'dirty' flag if modified
	float angle;

	// Moving is done by setting offset and calling coldet_process
	Vector2 offset;

	// If object is changed in any other way than setting offset,
	// dirty flag must be set
	bool dirty;

	void* userdata;

	ListHead list;
} CDObj;

// Query callback functions, gets called once for each object.
typedef void (*CDQueryCallback)(CDObj* obj);

// Collission callback, called once for each colliding pair
typedef void (*CDCollissionCallback)(CDObj* a, CDObj* b);

// Init collission detection world 
// max_obj_size is grid cell size, no object can have larger linear dimensions!
void coldet_init(CDWorld* cd, float max_obj_size);

// Same as above, also lets to setup tube or torus shaped wraparound worlds
void coldet_init_ex(CDWorld* cd, float max_obj_size,
		float width, float height, bool horiz_wrap, bool vert_wrap);

// Frees internal resources
void coldet_close(CDWorld* cd);

// Creates a new circle object, adds it to the world.
// mask is bitmask for filtering queries and collissions
CDObj* coldet_new_circle(CDWorld* cd, Vector2 center, float radius,
		uint mask, void* userdata);

// Creates a new AABB object, adds it to the world
CDObj* coldet_new_aabb(CDWorld* cd, const RectF* rect, uint mask,
		void* userdata);

// Creates a new OBB objects, adds it to the world
CDObj* coldet_new_obb(CDWorld* cd, const RectF* rect, float angle,
		uint mask, void* userdata);

// Removes any object. Pointers to it become invalid!
// Calling this in query/collission callback results in
// undefined behaviour.
void coldet_remove_obj(CDWorld* cd, CDObj* obj);

// Circle query. Radius can be bigger than world->cell_size.
// Performs a callback for each object which intersects circle and
// expression obj->mask & mask != 0 is true.
// Returns number of times callback was invoked.
uint coldet_query_circle(CDWorld* cd, Vector2 center, float radius, uint mask,
	CDQueryCallback callback);

// AABB query. Same rules as coldet_query_circle.
uint coldet_query_aabb(CDWorld* cd, const RectF* rect, uint mask,
	CDQueryCallback callback);

// Casts a segment from start to end, returns first collision with object.
// If hitpoint is not NULL, it's set to collission point. 
CDObj* coldet_cast_segment(CDWorld* cd, Vector2 start, Vector2 end, uint mask,
		Vector2* hitpoint);

// Moves all objects along offset vectors, invokes callback for each
// intersecting pair of objects a and b, if a->mask & b->mask != 0.
void coldet_process(CDWorld* cd, CDCollissionCallback callback);

#endif
