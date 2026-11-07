#include "gc.h" 
#include "stdio.h"
#include "stdlib.h"
#include "setjmp.h"

void* baseStackPtr = NULL;
GcCollectible* worklist = NULL;
GcCollectible* allObjects = NULL;
GcCollectible* minAllocatedAddress = NULL;
GcCollectible* maxAllocatedAddress = NULL;

GcCollectible* allocateNew(GcTypeInfo* type) {
	GcCollectible* ptr = (GcCollectible*) malloc(type->size);
	if (ptr == NULL) {
		gc();
		ptr = malloc(type->size);
		if (ptr == NULL) {
			exit(67);
		}
	}
	if (minAllocatedAddress == NULL || ptr < minAllocatedAddress) minAllocatedAddress = ptr;
	if (maxAllocatedAddress == NULL || ptr > maxAllocatedAddress) maxAllocatedAddress = ptr;
	ptr->nextObj = allObjects;
	allObjects = ptr;
	ptr->mark = M_UNVISITED;
	ptr->nextWorkObj = NULL;
	ptr->type = type;
	return ptr;
}

void gc() {
	markFromStack();
	sweep();
}

void markFromStack() {
	worklist = NULL;
	// spill registers onto stack
	jmp_buf jb;
	setjmp(jb);
	GC_TRACE("Marking from stack [%p; %p]\n", baseStackPtr, (void*) &jb);
	if (baseStackPtr == NULL) {
		GC_TRACE("Gc wasn't initialised");
		exit(67);
	}
	for (GcCollectible** ptrPtr = (GcCollectible**) &jb; ptrPtr < baseStackPtr; ptrPtr++) {
		GcCollectible* maybePtr = *ptrPtr;
		if (maybePtr < minAllocatedAddress || maybePtr > maxAllocatedAddress) continue;
		if (!isInObjectChain(maybePtr)) continue;
		if (maybePtr->mark == M_UNVISITED) {
			GC_TRACE("stack root %p found at %p\n", (void*) maybePtr, (void*) ptrPtr);
			maybePtr->nextWorkObj = worklist;
			worklist = maybePtr;
			maybePtr->mark = M_VISITING;
			mark();
		}
	}
}

bool isInObjectChain(GcCollectible* maybePtr) {
	GcCollectible* elem = allObjects;
	while (elem != NULL) {
		if (elem == maybePtr) return true;
		elem = elem->nextObj;
	}
	return false;
}

void mark() {
	GC_TRACE("mark(), worklist %p\n", (void*)worklist);
	while (worklist	!= NULL) {
		GcCollectible* obj = worklist;
		worklist = worklist->nextWorkObj;
		GC_DUMP_OBJ(obj);
		size_t offset = ((size_t) obj) + obj->type->fieldsOffset;
		GC_TRACE("Tracing children of %p, offset at %p\n", (void*)obj, (void*)offset);
		for (size_t i = 0; i < obj->type->fieldsCount; i++) {
			GcCollectible* child = *((GcCollectible**) (offset + i * sizeof(GcCollectible*)));
			GC_TRACE("Tracing child #%d (%p)\n", i, (void*) child);
			GC_DUMP_OBJ(child);
			if (child != NULL && child->mark == M_UNVISITED) {
				GC_TRACE("Adding child #%d (%p) to worklist\n", i, (void*) child);
				child->mark = M_VISITING;
				child->nextWorkObj = worklist;
				worklist = child;
			}
		}
		GC_TRACE("Marking %p\n", (void*)obj);
		obj->mark = M_VISITED;
	}
	GC_TRACE("mark() finished\n");
}

void sweep() {
	GcCollectible* current = allObjects;
	GcCollectible* next = NULL;
	#ifdef GC_DEBUG_SWEEP_ONLY
	#define GC_DEBUG
	#endif
	GC_TRACE("Sweeping\n");
	while (current != NULL) {
		next = current->nextObj;
		if (current->mark == M_VISITED) {
			GC_TRACE("Kept %p\n", (void*) current);
			current->mark = M_UNVISITED;
		} else {
			GC_TRACE("Swept %p\n", (void*) current);
			free(current);
		}
		current = next;
	}
	#ifdef GC_DEBUG_SWEEP_ONLY
	#undef GC_DEBUG
	#endif
}

void dumpObjInfo(GcCollectible* obj) {
	if (obj == NULL) printf("Null\n");
	else
	printf("Gc collectible at %p\nmark    \t= %d\nnext    \t= %p\nnextWork\t= %p\ntype    \t= sz %d\tfc %d\tfo %d\n",
		(void*) obj, obj->mark, obj->nextObj, obj->nextWorkObj, 
		obj->type->size, obj->type->fieldsCount, obj->type->fieldsOffset);
}

void gcInit(void* sp) {
	baseStackPtr = sp;
}
