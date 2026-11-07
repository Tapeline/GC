#ifndef GC_H
#define GC_H

#define GC_DEBUG 1

#include <stddef.h>

#define M_UNVISITED 0
#define M_VISITING 1
#define M_VISITED 2

typedef struct {
	size_t size;
    size_t fieldsCount;
    size_t fieldsOffset;
} GcTypeInfo;

typedef struct {
	GcTypeInfo* type;
	void* nextObj;
	void* nextWorkObj;
	bool isStaticRoot;
	unsigned char mark;
} GcCollectible;

#define GC_COLLECTIBLE GcCollectible gcHeader;

#define DECLARE_COLLECTIBLE(typename, fieldcount) \
	GcTypeInfo gcTypeInfo_##typename = { \
		.size = sizeof(typename), \
		.fieldsCount = fieldcount, \
		.fieldsOffset = offsetof(typename, gcHeader) + sizeof(GcCollectible) \
	}

#define NEW(typename) ((typename*) allocateNew(&gcTypeInfo_##typename))

void gcInit(void* sp);
GcCollectible* allocateNew(GcTypeInfo* type);
void gc();
void markFromStack();
void mark();
void sweep();
void dumpObjInfo(GcCollectible* obj);
bool isInObjectChain(GcCollectible* obj);

#ifdef GC_DEBUG
#define GC_TRACE(...) printf(__VA_ARGS__)
#define GC_DUMP_OBJ(obj) dumpObjInfo((GcCollectible*) obj)
#else
#define GC_TRACE(...) (void)0
#define GC_DUMP_OBJ(obj) (void)0
#endif

#define IS_STATIC_ROOT(obj) ((GcCollectible*) obj)->isStaticRoot

#define GC_INIT() \
	int __gcInitDummy; \
	gcInit((void*) &__gcInitDummy)

#endif 
