#include "datastruct.h"

typedef struct {
	int n;
	ListHead list;
} ListEntry;

TEST_(list) {
	ListHead l;

	list_init(&l);
	ASSERT_(l.prev == &l);
	ASSERT_(l.next == &l);
	ASSERT_(list_empty(&l) == true);

	ListEntry a, b, c;
	a.n = 1;
	b.n = 2;
	c.n = 3;

	list_push_back(&l, &a.list);
	ASSERT_(list_empty(&l) == false);
	ASSERT_(a.list.next == &l);
	ASSERT_(a.list.prev == &l);
	ASSERT_(list_first_entry(&l, ListEntry, list) == &a);
	ASSERT_(list_last_entry(&l, ListEntry, list) == &a);

	ListEntry* pos;
	uint count = 0;
	list_for_each_entry(pos, &l, list) {
		ASSERT_(pos->n == 1);
		count++;
	}
	ASSERT_(count == 1);

	list_push_back(&l, &b.list);
	list_push_front(&l, &c.list);

	ASSERT_(list_first_entry(&l, ListEntry, list) == &c);

	ASSERT_(list_entry(&b.list, ListEntry, list) == &b);
	int n[] = {3, 1, 2};	
	int* p = n;
	count = 0;
	list_for_each_entry(pos, &l, list) {
		ASSERT_(*p == pos->n);
		p++;
		count++;
	}
	ASSERT_(count == 3);

	list_pop_front(&l);
	ASSERT_(list_first_entry(&l, ListEntry, list) == &a);

	list_remove(&b.list);

	ASSERT_(list_first_entry(&l, ListEntry, list) == &a);
	ASSERT_(list_last_entry(&l, ListEntry, list) == &a);

	list_pop_back(&l);

	ASSERT_(list_empty(&l) == true);
}

TEST_(heap) {
	Heap h;

	heap_init(&h);
	ASSERT_(heap_size(&h) == 0);

	heap_push(&h, 1, NULL);
	ASSERT_(heap_size(&h) == 1);
	ASSERT_(heap_peek(&h, NULL) == 1);

	heap_push(&h, 4, NULL);
	heap_push(&h, 3, NULL);
	heap_push(&h, 2, NULL);
	ASSERT_(heap_size(&h) == 4);
	ASSERT_(heap_peek(&h, NULL) == 1);

	ASSERT_(heap_pop(&h, NULL) == 1); 
	ASSERT_(heap_peek(&h, NULL) == 2);
	ASSERT_(heap_size(&h) == 3);

	ASSERT_(heap_pop(&h, NULL) == 2);
	ASSERT_(heap_pop(&h, NULL) == 3);
	ASSERT_(heap_pop(&h, NULL) == 4);

	ASSERT_(heap_size(&h) == 0);

	heap_free(&h);
}

TEST_(heapsort) {
	Heap h;

	int m[] = {2, 2, 1, 6, 1, 9, -4, 3, 8, 6, 0, 2, 4, -5, -2, 8, 4};
	int sorted[] = {-5, -4, -2, 0, 1, 1, 2, 2, 2, 3, 4, 4, 6, 6, 8, 8, 9};
	int n = ARRAY_SIZE(m);

	heap_init(&h);

	for(uint i = 0; i < n; ++i) {
		heap_push(&h, m[i], &m[i]);
	}

	ASSERT_(heap_size(&h) == n);

	for(uint i = 0; i < n; ++i) {
		int* data;
		ASSERT_(heap_pop(&h, (void**)&data) == sorted[i]); 
		ASSERT_(*data == sorted[i]);
	}

	ASSERT_(heap_size(&h) == 0);

	heap_free(&h);
}

TEST_(aatree_empty) {
	AATree t;

	aatree_init(&t);
	
	ASSERT_(aatree_size(&t) == 0);
	ASSERT_(aatree_find(&t, 16) == NULL);
	ASSERT_(aatree_find(&t, 0) == NULL);

	aatree_free(&t);
}

TEST_(aatree_insert) {
	AATree t;

	char* a = "karass";
	char* b = "granfaloon";
	char* c = "duprass";

	aatree_init(&t);

	ASSERT_(aatree_insert(&t, 7, a) == true);
	ASSERT_(aatree_size(&t) == 1);

	void* p = NULL;
	ASSERT_(aatree_min(&t, &p) == 7);
	ASSERT_(p == a);
	p = NULL;
	ASSERT_(aatree_max(&t, &p) == 7);
	ASSERT_(p == a);
	ASSERT_(aatree_find(&t, 7) == a);

	ASSERT_(aatree_insert(&t, 19, b) == true);
	ASSERT_(aatree_insert(&t, 37, c) == true);
	ASSERT_(aatree_insert(&t, 7, a) == false);

	ASSERT_(aatree_size(&t) == 3);
	ASSERT_(aatree_min(&t, NULL) == 7);
	ASSERT_(aatree_max(&t, NULL) == 37);

	ASSERT_(aatree_find(&t, 8) == NULL);
	ASSERT_(aatree_find(&t, 19) == b);

	aatree_free(&t);
}

TEST_(aatree_insert_stress) {
	AATree t;

	aatree_init(&t);

	for(uint i = 0; i < 10000; ++i) {
		ASSERT_(aatree_insert(&t, (i+1) * 3, &t) == true);
	}

	ASSERT_(aatree_size(&t) == 10000);
	ASSERT_(aatree_min(&t, NULL) == 3);
	ASSERT_(aatree_max(&t, NULL) == 30000);

	for(uint i = 1; i <= 3000; ++i) {
		ASSERT_(aatree_find(&t, i) == (i % 3 ? NULL : &t));
	}

	ASSERT_(aatree_insert(&t, 303, NULL) == false);
	ASSERT_(aatree_insert(&t, 15, NULL) == false);
	ASSERT_(aatree_insert(&t, 7, NULL) == true);

	aatree_clear(&t);
	ASSERT_(aatree_size(&t) == 0);
	ASSERT_(aatree_insert(&t, 7, NULL) == true);
	ASSERT_(aatree_insert(&t, 3, NULL) == true);
	ASSERT_(aatree_insert(&t, 7, NULL) == false);
	ASSERT_(aatree_size(&t) == 2);

	aatree_free(&t);
}

TEST_(aatree_remove_simple) {
	AATree t;

	aatree_init(&t);

	ASSERT_(aatree_insert(&t, 1, &t) == true);
	ASSERT_(aatree_remove(&t, 1) == &t);

	ASSERT_(aatree_insert(&t, 3, &t) == true);
	ASSERT_(aatree_insert(&t, 2, &t) == true);
	ASSERT_(aatree_insert(&t, 1, &t) == true);
	ASSERT_(aatree_remove(&t, 2) == &t);
	ASSERT_(aatree_remove(&t, 1) == &t);
	ASSERT_(aatree_remove(&t, 3) == &t);
	ASSERT_(aatree_size(&t) == 0);

	aatree_free(&t);
}

TEST_(aatree_remove) {
	AATree t;

	uint data[] = {4, 2, 8, 6, -3, 5, 7, 9, -11, 19, 14, 10}; 
	uint n = ARRAY_SIZE(data);

	aatree_init(&t);

	for(uint i = 0; i < n; ++i) {
		ASSERT_(aatree_insert(&t, data[i], &data[i]) == true);
	}

	ASSERT_(aatree_size(&t) == n);
	ASSERT_(aatree_remove(&t, 5) != NULL);
	ASSERT_(aatree_remove(&t, 1) == NULL);
	ASSERT_(aatree_size(&t) == n-1);

	ASSERT_(aatree_remove(&t, -11) != NULL);
	ASSERT_(aatree_remove(&t, -3) != NULL);
	ASSERT_(aatree_size(&t) == n-3);
	ASSERT_(aatree_min(&t, NULL) == 2);
	ASSERT_(aatree_find(&t, -3) == NULL);
	ASSERT_(aatree_find(&t, 7) != NULL);
	ASSERT_(aatree_remove(&t, 7) != NULL);
	ASSERT_(aatree_remove(&t, 7) == NULL);
	ASSERT_(aatree_find(&t, 7) == NULL);

	aatree_free(&t);
}

bool is_prime(int n) {
	if(n == 2)
		return true;
	if(n % 2 == 0 || n < 2)
		return false;

	for(uint d = 3; d*d <= n; d += 2) {
		if(n % d == 0)
			return false;
	}

	return true;
}

TEST_(aatree_remove_stress) {
	AATree t;

	aatree_init(&t);
		
	for(uint i = 0; i < 10000; ++i) {
		ASSERT_(aatree_insert(&t, i, &t) == true);
	}

	for(uint i = 0; i < 10000; ++i) {
		if(!is_prime(i)) {
			ASSERT_(aatree_remove(&t, i) == &t);
		}
	}

	ASSERT_(aatree_size(&t) == 1229);
	ASSERT_(aatree_min(&t, NULL) == 2);
	ASSERT_(aatree_max(&t, NULL) == 9973);

	for(uint i = 8; i < 10000; i += 3) {
		bool prime = (aatree_find(&t, i) != NULL);
		ASSERT_(prime == is_prime(i));
	}

	uint first_primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
	uint n = ARRAY_SIZE(first_primes);
	for(uint i = 0; i < n; ++i) {
		int p = first_primes[i];
		ASSERT_(aatree_min(&t, NULL) == p);
		ASSERT_(aatree_remove(&t, (p+1) * 15) == NULL);
		ASSERT_(aatree_remove(&t, p) == &t);
	}

	ASSERT_(aatree_size(&t) == 1229 - n);
		
	aatree_free(&t);
}

TEST_(dict_empty) {
	Dict d;

	dict_init(&d);

	ASSERT_(d.items == 0);
	ASSERT_(d.mask);

	dict_free(&d);
}


TEST_(dict_simple) {
	Dict d;

	dict_init(&d);

	ASSERT_(dict_insert(&d, "viens", &d));
	ASSERT_(dict_insert(&d, "du", &d));
	ASSERT_(dict_insert(&d, "du", &d) == false);
	ASSERT_(dict_insert(&d, "trys", &d + 2));
	dict_set(&d, "grazi", &d + 3);
	dict_set(&d, "lietuva", &d + 4);
	dict_set(&d, "du", &d + 1);

	ASSERT_(dict_get(&d, "trys") == &d + 2);
	ASSERT_(dict_get(&d, "du") == &d + 1);
	ASSERT_(dict_get(&d, "grazi") == &d + 3);
	ASSERT_(dict_delete(&d, "grazi") == &d + 3);
	ASSERT_(dict_get(&d, "vienas") == NULL);
	ASSERT_(dict_get(&d, "grazi") == NULL);

	dict_set(&d, "grazi", &d);
	ASSERT_(dict_insert(&d, "vienas", &d));
	ASSERT_(dict_get(&d, "grazi") == &d);

	dict_free(&d);
}

TEST_(dict_stress) {
	Dict d;

	char numbers[1024*1024];
	char* p = numbers;	

	dict_init(&d);

	for(size_t i = 0; i < 10000; ++i) {
		sprintf(p, "%zd", i);
		ASSERT_(dict_insert(&d, p, (void*)i));
		p += strlen(p)+1;
	}

	char scratch[10];
	for(size_t i = 0; i < 10000; ++i) {
		sprintf(scratch, "%zd", i);
		ASSERT_(dict_get(&d, scratch) == (void*)i);	
	}

	for(size_t i = 0; i < 10000; ++i) {
		if(!is_prime(i)) {
			sprintf(scratch, "%zd", i);
			ASSERT_(dict_delete(&d, scratch) == (void*)i);
		}
	}

	for(size_t i = 0; i < 10000; ++i) {
		sprintf(scratch, "%zd", i);
		if(is_prime(i)) {
			ASSERT_(dict_get(&d, scratch) == (void*)i);	
		}
		sprintf(scratch, "%zd", (i+2)*7);
		ASSERT_(dict_get(&d, scratch) == NULL);	
	}

	dict_free(&d);
}

TEST_(set_stress) {
	Dict d;

	char numbers[1024*1024];
	char* p = numbers;	

	dict_init(&d);

	for(size_t i = 0; i < 10000; ++i) {
		sprintf(p, "%zd", i);
		dict_set(&d, p, (void*)i);
		p += strlen(p)+1;
	}

	char scratch[10];
	for(size_t i = 0; i < 10000; ++i) {
		sprintf(scratch, "%zd", i);
		ASSERT_(dict_get(&d, scratch) == (void*)i);	
	}

	for(size_t i = 0; i < 10000; ++i) {
		if(!is_prime(i)) {
			sprintf(scratch, "%zd", i);
			ASSERT_(dict_delete(&d, scratch) == (void*)i);
		}
	}

	for(size_t i = 0; i < 10000; ++i) {
		sprintf(scratch, "%zd", i);
		if(is_prime(i)) {
			ASSERT_(dict_get(&d, scratch) == (void*)i);	
		}
		sprintf(scratch, "%zd", (i+2)*7);
		ASSERT_(dict_get(&d, scratch) == NULL);	
	}

	dict_free(&d);
}

