
#include "stdio.h"
#include "gc.h"
#include "stdlib.h"

typedef struct {
	GC_COLLECTIBLE
	int someting;
} MyObject2;

DECLARE_COLLECTIBLE(MyObject2, 0);

typedef struct {
	GC_COLLECTIBLE
	MyObject2* a;
	MyObject2* b;
	int someting;
} MyObject;

DECLARE_COLLECTIBLE(MyObject, 2);

MyObject2* staticA;

MyObject2* makeGarbage() {
	MyObject2* a = NEW(MyObject2);
	a->someting = 2;
	MyObject2* b = NEW(MyObject2);
	MyObject* c = NEW(MyObject);
	c->b = a;
	c->a = a;
	staticA =a;
	return a;
}

int main() {
	GC_INIT();
	makeGarbage();
	makeGarbage();
	makeGarbage();
	gc();
	return 0;
}
