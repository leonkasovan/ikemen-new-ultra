// Micro-test: Reference::refnew / listnew must release the old HeapObj
// when overwriting an already-allocated reference.
//
// Build (from repo root, w64devkit g++):
//   g++ -std=c++17 -O0 -g -I./main -o test_refnew_leak test_refnew_leak.cpp
//
// Expected with the fix: live bytes stay bounded (one HeapObj + payload).
// Expected without the fix: live bytes grow by ~(payload+head) per iteration.

#include "ssz/sszdef.h"
#include "ssz/arrayandref.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// ---- tracking allocator (stands in for ssz.cpp's MemoryKakuho/Kaihou) ----
static int64_t t_liveBytes  = 0;
static int64_t t_totalAlloc = 0;
static int64_t t_totalFree  = 0;

static void* SSZ_STDCALL TAlloc(intptr_t size)
{
	int8_t* raw = (int8_t*)malloc(size + 16);
	*(intptr_t*)raw = size;
	t_liveBytes += size;
	t_totalAlloc++;
	return raw + 16;
}
static void SSZ_STDCALL TFree(void* p)
{
	if(p == nullptr) return;
	int8_t* raw = (int8_t*)p - 16;
	t_liveBytes -= *(intptr_t*)raw;
	t_totalFree++;
	free(raw);
}

// ssz.cpp defines these; in this standalone TU we provide them.
void* (SSZ_STDCALL *sszrefnewfunc)(intptr_t) = TAlloc;
void (SSZ_STDCALL *sszrefdeletefunc)(void*) = TFree;

static const int64_t LIMIT = 4 * 1024 * 1024; // 4 MB bound

int main()
{
	// --- refnew: repeated re-allocation on the same Reference ---
	{
		Reference r;
		r.init();
		for(int i = 0; i < 1000; i++){
			r.refnew(1024, 1);
			assert(!r.null());
		}
		printf("refnew   : live=%lld KB  allocs=%lld frees=%lld\n",
			(long long)(t_liveBytes/1024), (long long)t_totalAlloc, (long long)t_totalFree);
		if(t_liveBytes > LIMIT){
			printf("FAIL: refnew leaks (live=%lld bytes after 1000 x 1KB reallocs)\n",
				(long long)t_liveBytes);
			return 1;
		}
		r.releaseanddelete();
	}

	// --- listnew: same pattern ---
	{
		Reference r;
		r.init();
		for(int i = 0; i < 1000; i++){
			r.listnew(1024, 1);
			assert(!r.null());
		}
		printf("listnew  : live=%lld KB  allocs=%lld frees=%lld\n",
			(long long)(t_liveBytes/1024), (long long)t_totalAlloc, (long long)t_totalFree);
		if(t_liveBytes > LIMIT){
			printf("FAIL: listnew leaks (live=%lld bytes after 1000 x 1KB reallocs)\n",
				(long long)t_liveBytes);
			return 1;
		}
		r.releaseanddelete();
	}

	// --- shared ref must NOT be freed underneath another holder ---
	{
		Reference a, b;
		a.init();
		b.init();
		a.refnew(64, 1);
		b.copy(a); // b shares a's HeapObj, refcount = 2
		HeapObj* shared = a.obj();
		a.refnew(64, 1); // a reallocates; old obj must survive (b holds it)
		assert(b.obj() == shared && b.obj()->rcount() == 1);
		b.releaseanddelete(); // old obj freed here
		a.releaseanddelete();
		printf("share    : OK (shared HeapObj survived refnew, freed by last holder)\n");
	}

	// --- addsize on existing ref must not regress (grows in place) ---
	{
		Reference r;
		r.init();
		r.refnew(16, 1);
		for(int i = 0; i < 10; i++) r.addsize(16, 1);
		assert(r.size() == 16 * 11);
		r.releaseanddelete();
		printf("addsize  : OK (size=%lld)\n", (long long)r.size());
	}

	int64_t residual = t_liveBytes;
	printf("residual live bytes: %lld\n", (long long)residual);
	if(residual != 0){
		printf("FAIL: residual live memory = %lld bytes\n", (long long)residual);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
