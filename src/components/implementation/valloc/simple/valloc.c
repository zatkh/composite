/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cos_component.h>
#include <cos_synchronization.h>
#include <cos_alloc.h>
#include <cos_vect.h>
#include <vas_mgr.h>
#include <valloc.h>
#include <cinfo.h>
#include <bitmap.h>

/* vector of vas vectors for spds */
COS_VECT_CREATE_STATIC(spd_vect);
cos_lock_t valloc_lock;
#define LOCK()   lock_take(&valloc_lock)
#define UNLOCK() lock_release(&valloc_lock)

#define WORDS_PER_PAGE (PAGE_SIZE/sizeof(u32_t))
#define MAP_MAX WORDS_PER_PAGE
#define VAS_SPAN (sizeof(u32_t) * WORDS_PER_PAGE)

/* describes 2^(12+12+3 = 27) bytes */
struct spd_vas_occupied {
	u32_t pgd_occupied[WORDS_PER_PAGE];
};

/* #if sizeof(struct spd_vas_occupied) != PAGE_SIZE */
/* #error "spd_vas_occupied not sized to a page" */
/* #endif */

struct vas_extent {
	void *start, *end;
};

struct spd_vas_tracker {
	struct cos_component_information *ci;
	struct vas_extent extents[MAX_SPD_VAS_LOCATIONS];
	/* should be an array to track more than 2^27 bytes */
	struct spd_vas_occupied *map; 
};

int valloc_init(spdid_t spdid)
{
	int ret = -1;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	struct cos_component_information *ci;
	unsigned long page_off;
	void *hp;

	LOCK();
	if (cos_vect_lookup(&spd_vect, spdid)) goto success;
	trac = malloc(sizeof(struct spd_vas_tracker));
	if (!trac) goto done;

	occ = alloc_page();
	if (!occ) goto err_free1;
	
	ci = cos_get_vas_page();
	if (cinfo_map(cos_spd_id(), (vaddr_t)ci, spdid)) goto err_free2;
	hp = (void*)ci->cos_heap_ptr;

	trac->ci               = ci;
	trac->map              = occ;
	trac->extents[0].start = (void*)round_to_pgd_page(hp);
	trac->extents[0].end   = (void*)round_up_to_pgd_page(hp);
	page_off = ((unsigned long)hp - (unsigned long)round_to_pgd_page(hp))/PAGE_SIZE;
	bitmap_set_contig(&occ->pgd_occupied[0], page_off, (PGD_SIZE/PAGE_SIZE)-page_off, 1);

	cos_vect_add_id(&spd_vect, trac, spdid);

	bitmap_print(&occ->pgd_occupied[0], 128);
success:
	ret = 0;
done:	UNLOCK();
	return ret;
err_free2:
	cos_set_heap_ptr_conditional((char*)ci+PAGE_SIZE, ci);
	free_page(occ);
err_free1:
	free(trac);
	goto done;
}

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages)
{
	void *ret = NULL;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	long off;

	LOCK();
	trac = cos_vect_lookup(&spd_vect, spdid);
	if (!trac) goto done;
	occ = trac->map;
	assert(occ);
	off = bitmap_extent_find_set(&occ->pgd_occupied[0], 0, npages, MAP_MAX);
	if (off < 0) goto done;
	ret = ((char *)trac->extents[0].start) + (off * PAGE_SIZE);
done:   
	UNLOCK();
	return ret;
}

int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages)
{
	int ret = -1;
	struct spd_vas_tracker *trac;
	struct spd_vas_occupied *occ;
	unsigned long off;

	LOCK();
	trac = cos_vect_lookup(&spd_vect, dest);
	if (!trac) goto done;
	occ = trac->map;
	assert(occ);
	off = ((char *)addr - (char *)trac->extents[0].start)/PAGE_SIZE;
	assert(off+npages < MAP_MAX*sizeof(u32_t));
	bitmap_set_contig(&occ->pgd_occupied[0], off, npages, 1);
	ret = 0;

done:	UNLOCK();
	return ret;
}

static void init(void)
{
	lock_static_init(&valloc_lock);
	cos_vect_init_static(&spd_vect);
}

void cos_init(void *arg)
{
	static volatile int first = 1;

	if (first) {
		first = 0;
		init();
	} else {
		prints("vas_mgr: not expecting more than one bootstrap.");
	}
}