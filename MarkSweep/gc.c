#include "gc.h" 
#include "stdio.h"
#include "stdlib.h"
#include "setjmp.h"

uintptr_t baseStackPtr = 0;
GcCollectible* worklist = NULL;
GcCollectible* allObjects = NULL;
uintptr_t minAllocatedAddress = 0;
uintptr_t maxAllocatedAddress = 0;

GcCollectible* allocateNew(GcTypeInfo* type) {
	GcCollectible* ptr = (GcCollectible*) malloc(type->size);
	if (ptr == NULL) {
		gc();
		ptr = malloc(type->size);
		if (ptr == NULL) {
			exit(67);
		}
	}
	if (minAllocatedAddress == 0 || (uintptr_t) ptr < minAllocatedAddress) minAllocatedAddress = (uintptr_t) ptr;
	if (maxAllocatedAddress == 0 || (uintptr_t) ptr > maxAllocatedAddress) maxAllocatedAddress = (uintptr_t) ptr;
	ptr->nextObj = allObjects;
	allObjects = ptr;
	ptr->mark = M_UNVISITED;
	ptr->nextWorkObj = NULL;
	ptr->type = type;
	ptr->isStaticRoot = false;
	return ptr;
}

void gc() {
	markFromStack();
	sweep();
}

__attribute__((noinline)) void markFromStack() {
	worklist = NULL;
	// spill registers onto stack
	jmp_buf jb;
	setjmp(jb);
	GC_TRACE("Marking from stack [%p; %p]\n", baseStackPtr, (void*) &jb);
	if (baseStackPtr == 0) {
		GC_TRACE("Gc wasn't initialised");
		exit(67);
	}
	uintptr_t from, to;
	if (baseStackPtr < (uintptr_t) &jb) {
		from = baseStackPtr;
		to = (uintptr_t) &jb;
	} else {
		from = (uintptr_t) &jb;
		to  = baseStackPtr;
	}
	to -= sizeof(GcCollectible*);
	GcCollectible* maybePtr;
	for (uintptr_t ptrPtr = from; ptrPtr < to; ptrPtr+=sizeof(GcCollectible*)) {
		if ((uintptr_t) maybePtr < minAllocatedAddress || (uintptr_t) maybePtr > maxAllocatedAddress) continue;
		if (!isInObjectChain(maybePtr)) continue;
		if (maybePtr->mark == M_UNVISITED) {
			GC_TRACE("stack root %p found at %p\n", (void*) maybePtr, (void*) ptrPtr);
			maybePtr->nextWorkObj = worklist;
			worklist = maybePtr;
			maybePtr->mark = M_VISITING;
			mark();
		}
	}
	maybePtr = allObjects;
	while (maybePtr != NULL) {
		if (maybePtr->isStaticRoot && maybePtr->mark == M_UNVISITED) {
			GC_TRACE("marking static root %p\n", (void*) maybePtr);
			maybePtr->nextWorkObj = worklist;
			worklist = maybePtr;
			maybePtr->mark = M_VISITING;
			mark();
		}
		maybePtr = maybePtr->nextObj;
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
	GcCollectible* prev = NULL;
	#ifdef GC_DEBUG_SWEEP_ONLY
	#define GC_DEBUG
	#endif
	GC_TRACE("Sweeping\n");
	while (current != NULL) {
		next = current->nextObj;
		if (current->mark == M_VISITED) {
			GC_TRACE("Kept %p\n", (void*) current);
			current->mark = M_UNVISITED;
			prev = current;
			current = next;
		} else {
			GC_TRACE("Swept %p\n", (void*) current);
			if (prev != NULL) prev->nextObj = next;
			else allObjects = next;
			free(current);
			current = next;
		}
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
	baseStackPtr = (uintptr_t) sp;
}
