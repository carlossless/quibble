#include "coldet.h"

uint query_count = 0;
uint collission_count = 0;

void query_cb(CDObj* obj) {
	query_count++;
}

void collission_cb(CDObj* a, CDObj* b) {
	collission_count++;
}

TEST_(empty) {
	CDWorld cd;

	coldet_init(&cd, 10.0f);

	RectF r = {-100.0f, -100.0f, 100.0f, 100.0f};
	coldet_query_aabb(&cd, &r, 0xFFFFFFFF, query_cb); 
	ASSERT_(query_count == 0);

	coldet_query_circle(&cd, vec2(13.0f, 2.2f), 100.0f,
		0xFFFFFFFF, query_cb);
	ASSERT_(query_count == 0);

	Vector2 hp = {1.0f, 2.0f};
	ASSERT_(coldet_cast_segment(&cd, vec2(-24.0f, -15.0f), vec2(42.0f, 81.3f),
		0xFFFFFFFF, &hp) == NULL);
	ASSERT_(hp.x == 1.0f && hp.y == 2.0f);

	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 0);

	coldet_close(&cd);
}

TEST_(insert) {
	CDWorld cd;

	coldet_init(&cd, 10.0f);

	RectF rects[] = {
		{16.0f, 31.5f, 21.0f, 40.0f},
		{-2.0f, 10.0f, 1.0f, 17.2f},
		{-9.0f, 15.0f, -7.0f, 17.0f}
	};

	CDObj* box_a = coldet_new_aabb(&cd, &rects[0], 0xFFFFFFFF, &rects[0]);
	ASSERT_(box_a);
	ASSERT_(box_a->pos.x == 16.0f && box_a->pos.y == 31.5f);
	ASSERT_(box_a->size.size.x == 5.0f && box_a->size.size.y == 8.5f);
	ASSERT_(box_a->offset.x == 0.0f && box_a->offset.y == 0.0f);
	ASSERT_(box_a->dirty == false);
	ASSERT_(box_a->type == CD_AABB);
	ASSERT_(box_a->mask == 0xFFFFFFFF);
	ASSERT_(box_a->userdata == &rects[0]);

	CDObj* box_b = coldet_new_aabb(&cd, &rects[1], 0xFFFFFFFF, &rects[1]);
	ASSERT_(box_b);
	CDObj* box_c = coldet_new_aabb(&cd, &rects[2], 0xFFFFFFFF, &rects[2]);
	ASSERT_(box_c);

	CDObj* cr_a = coldet_new_circle(&cd, vec2(51.0f, -4.0f), 4.0f, 0xFFFFFFFF, NULL);  
	ASSERT_(cr_a);
	ASSERT_(cr_a->pos.x == 51.0f && cr_a->pos.y == -4.0f);
	ASSERT_(cr_a->size.radius == 4.0f);
	ASSERT_(cr_a->offset.x == 0.0f && cr_a->offset.y == 0.0f);
	ASSERT_(cr_a->dirty == false);
	ASSERT_(cr_a->type == CD_CIRCLE);
	ASSERT_(cr_a->mask == 0xFFFFFFFF);
	ASSERT_(cr_a->userdata == NULL);

	CDObj* cr_b = coldet_new_circle(&cd, vec2(11.0f, 21.3f), 5.0f, 0xFFFFFFFF, NULL);  
	ASSERT_(cr_b && cr_b != cr_a);
	CDObj* cr_c = coldet_new_circle(&cd, vec2(-2.0f, 18.0f), 1.2f, 0xFFFFFFFF, NULL);  
	ASSERT_(cr_c && cr_c != cr_b);

	coldet_close(&cd);
}

RectF rects[] = {
	{1.0f, 1.0f, 2.0f, 3.0f},
	{1.0f, 2.0f, 4.0f, 5.0f},
	{-6.0f, -2.0f, -2.0f, 2.0f},
	{-4.0f, 5.0f, -1.0f, 7.0f},
	{10.0f, 5.0f, 11.0f, 8.0f}
};

struct {
	Vector2 c;
	float r;
} circles[] = {
	{{2.0f, -1.0f}, 1.0f},
	{{2.0f, 6.0f}, 1.0f},
	{{7.0f, 1.0f}, 2.0f},
	{{5.0f, 8.0f}, 1.0f}
};


TEST_(queries_aabb) {
	CDWorld cd;

	coldet_init(&cd, 4.0f);

	for(uint i = 0; i < ARRAY_SIZE(rects); ++i)
		ASSERT_(coldet_new_aabb(&cd, &rects[i], 0xFFFFFFFF, NULL));

	for(uint i = 0; i < ARRAY_SIZE(circles); ++i)
		ASSERT_(coldet_new_circle(&cd, circles[i].c, circles[i].r, 0xFFFFFFFF, NULL));

	RectF query_rect = {0.0f, 0.0f, 0.9f, 0.9f};
	ASSERT_(coldet_query_aabb(&cd, &query_rect, 0xFFFFFFFF, query_cb) == 0);  
	query_rect = rectf(5.0f, 4.0f, 9.0f, 6.0f);
	ASSERT_(coldet_query_aabb(&cd, &query_rect, 0xFFFFFFFF, query_cb) == 0);
	query_rect = rectf(0.0f, 1.0f, 4.5f, 7.0f);
	ASSERT_(query_count == 0);
	ASSERT_(coldet_query_aabb(&cd, &query_rect, 0xFFFFFFFF, query_cb) == 3);
	ASSERT_(query_count == 3);
	query_rect = rectf(-10.0f, -10.0f, 10.0f, 10.0f);
	ASSERT_(coldet_query_aabb(&cd, &query_rect, 0xFFFFFFFF, query_cb) == 9);
	ASSERT_(query_count == 12);
	query_count = 0;

	coldet_close(&cd);
}

TEST_(queries_circle) {
	CDWorld cd;

	coldet_init(&cd, 4.0f);

	for(uint i = 0; i < ARRAY_SIZE(rects); ++i)
		ASSERT_(coldet_new_aabb(&cd, &rects[i], 0xFFFFFFFF, NULL));

	for(uint i = 0; i < ARRAY_SIZE(circles); ++i)
		ASSERT_(coldet_new_circle(&cd, circles[i].c, circles[i].r, 0xFFFFFFFF, NULL));

	// Circle
	ASSERT_(coldet_query_circle(&cd, vec2(7.0f, 5.0f), 1.5f, 0xFFFFFFFF, query_cb) == 0);
	ASSERT_(coldet_query_circle(&cd, vec2(5.0f, -2.0f), 3.0f, 0xFFFFFFFF, query_cb) == 2);
	ASSERT_(coldet_query_circle(&cd, vec2(-6.0f, 9.0f), 3.0f, 0xFFFFFFFF, query_cb) == 1);
	ASSERT_(query_count == 3);
	ASSERT_(coldet_query_circle(&cd, vec2(3.0f, 4.0f), 2.0f, 0xFFFFFFFF, query_cb) == 3);
	ASSERT_(query_count == 6);
	ASSERT_(coldet_query_circle(&cd, vec2(3.0f, 4.0f), 20.0f, 0xFFFFFFFF, query_cb) == 9);
	ASSERT_(query_count == 15);
	query_count = 0;

	coldet_close(&cd);
}

TEST_(segment_cast) {
	CDWorld cd;

	coldet_init(&cd, 4.0f);

	for(uint i = 0; i < ARRAY_SIZE(rects); ++i)
		ASSERT_(coldet_new_aabb(&cd, &rects[i], 0xFFFFFFFF, NULL));

	for(uint i = 0; i < ARRAY_SIZE(circles); ++i)
		ASSERT_(coldet_new_circle(&cd, circles[i].c, circles[i].r, 0xFFFFFFFF, NULL));
	
	Vector2 hp;

	ASSERT_(coldet_cast_segment(&cd, vec2(0.0f, 0.0f), vec2(-3.0f, 0.0f), 0xFFFFFFFF, &hp));
	ASSERT_(hp.x == -2.0f && hp.y == 0.0f);
	ASSERT_(coldet_cast_segment(&cd, vec2(0.0f, 0.0f), vec2(-1.0f, 4.0f), 0xFFFFFFFF, &hp) == NULL);
	ASSERT_(hp.x == -2.0f && hp.y == 0.0f);

	CDObj* circle = coldet_cast_segment(&cd, vec2(10.0f, 9.0f), vec2(8.0f, -1.0f),
		0xFFFFFFFF, &hp);
	ASSERT_(circle);
	ASSERT_(circle->type == CD_CIRCLE);
	ASSERT_(circle->pos.x == 7.0f && circle->pos.y == 1.0f);

	CDObj* aabb = coldet_cast_segment(&cd, vec2(1.0f, 0.0f), vec2(-2.0, 8.0f), 
		0xFFFFFFFF, &hp);
	ASSERT_(aabb);
	ASSERT_(aabb->type == CD_AABB);
	ASSERT_(aabb->pos.x == -4.0f && aabb->pos.y == 5.0f);

	coldet_close(&cd);

}

TEST_(stress) {
	CDWorld cd;

	coldet_init(&cd, 1.0f);

	for(int y = -100; y <= 100; ++y) {
		for(int x = -100; x <= 100; ++x) {
			Vector2 c = {
				(float)x + 0.5f,
				(float)y + 0.5f
			};

			RectF rect = {
				c.x - 0.4f,
				c.y - 0.4f,
				c.x + 0.4f,
				c.y + 0.4f
			};

			ASSERT_(coldet_new_aabb(&cd, &rect, 0xFFFFFFFF, NULL));
			ASSERT_(coldet_new_circle(&cd, c, 0.3f, 0xFFFFFFFF, NULL));
		}
	}

	RectF rect = {-101.0f, -101.0f, 101.0f, 101.0f}; 
	ASSERT_(coldet_query_aabb(&cd, &rect, 0xFFFFFFFF, query_cb) == 201*201*2);
	ASSERT_(coldet_query_circle(&cd, vec2(0.0f, 0.0f), 150.0f, 0xFFFFFFFF, query_cb) == 201*201*2);
	query_count = 0;

	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 201*201);
	collission_count = 0;

	coldet_close(&cd);
}

TEST_(collission_simple) {
	CDWorld cd;

	coldet_init(&cd, 4.0f);

	for(uint i = 0; i < ARRAY_SIZE(rects); ++i)
		ASSERT_(coldet_new_aabb(&cd, &rects[i], 0xFFFFFFFF, NULL));

	for(uint i = 0; i < ARRAY_SIZE(circles); ++i)
		ASSERT_(coldet_new_circle(&cd, circles[i].c, circles[i].r, 0xFFFFFFFF, NULL));
	
	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 2);
	collission_count = 0;

	ASSERT_(coldet_new_circle(&cd, vec2(0.0f, 6.0f), 1.1f, 0xFFFFFFFF, NULL));
	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 4);
	collission_count = 0;

	coldet_close(&cd);
}

TEST_(wraparound_h) {
	CDWorld cd;

	coldet_init_ex(&cd, 2.0f, 6.0f, 0.0f, true, false);

	ASSERT_(coldet_new_circle(&cd, vec2(1.0f, 1.0f), 0.5f, 1, NULL));
	ASSERT_(coldet_new_circle(&cd, vec2(3.0f, 2.0f), 0.5f, 1, NULL));
	ASSERT_(coldet_new_circle(&cd, vec2(5.0f, 3.0f), 0.5f, 1, NULL));
	ASSERT_(coldet_new_circle(&cd, vec2(0.0f, 2.0f), 0.25f, 1, NULL));

	RectF r = rectf(0.5f, 2.5f, 1.5f, 3.5f);
	ASSERT_(coldet_new_aabb(&cd, &r, 1, NULL));
	r = rectf(4.5f, 0.5f, 5.5f, 1.5f);
	ASSERT_(coldet_new_aabb(&cd, &r, 1, NULL));

	// No collissions
	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 0);

	// Circle queries
	ASSERT_(coldet_query_circle(&cd, vec2(0.0f, 0.0f), 1.0f, 1, query_cb) == 2);
	ASSERT_(coldet_query_circle(&cd, vec2(6.0f, 0.0f), 1.0f, 1, query_cb) == 2);
	ASSERT_(query_count == 4);
	query_count = 0;

	ASSERT_(coldet_query_circle(&cd, vec2(3.0f, 3.0f), 1.0f, 1, query_cb) == 1);
	ASSERT_(coldet_query_circle(&cd, vec2(5.9f, 1.0f), 0.3f, 1, query_cb) == 0);
	ASSERT_(query_count == 1);
	query_count = 0;

	ASSERT_(coldet_query_circle(&cd, vec2(0.0f, 3.0f), 6.0f, 1, query_cb) == 13);
	ASSERT_(query_count == 13);
	query_count = 0;

	// AABB queries
	r = rectf(-1.0f, -1.0f, 2.0f, 2.0f);
	ASSERT_(coldet_query_aabb(&cd, &r, 1, query_cb) == 3);
	r = rectf(-1.0f, 0.0f, 1.0f, 4.0f);
	ASSERT_(coldet_query_aabb(&cd, &r, 1, query_cb) == 5);
	r = rectf(2.0f, -3.0f, 4.0f, 1.0f);
	ASSERT_(coldet_query_aabb(&cd, &r, 1, query_cb) == 0);
	ASSERT_(query_count == 8);
	query_count = 0;

	// Collission
	ASSERT_(coldet_new_circle(&cd, vec2(0.0f, 2.0f), 1.0f, 1, NULL));
	coldet_process(&cd, collission_cb);
	ASSERT_(collission_count == 5);

	coldet_close(&cd);
}

