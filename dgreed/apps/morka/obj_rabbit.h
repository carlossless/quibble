#ifndef OBJ_RABBIT_H
#define OBJ_RABBIT_H

#include "objects.h"

#define OBJ_RABBIT_TYPE 1

typedef struct {
	GameObject header;
} ObjRabbit;

extern GameObjectDesc obj_rabbit_desc;

#endif
