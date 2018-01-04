#include <assert.h>
#include <string.h> 
#include "sim.h"
#include "pagetable.h"

// The top-level page table (also known as the 'page directory')
pgdir_entry_t pgdir[PTRS_PER_PGDIR]; 

// Counters for various events.
// Your code must increment these when the related events occur.
int hit_count = 0;
int miss_count = 0;
int ref_count = 0;
int evict_clean_count = 0;
int evict_dirty_count = 0;

/*
 * Allocates a frame to be used for the virtual page represented by p.
 * If all frames are in use, calls the replacement algorithm's evict_fcn to
 * select a victim frame.  Writes victim to swap if needed, and updates 
 * pagetable entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 *
 * Counters for evictions should be updated appropriately in this function.
 */
int allocate_frame(pgtbl_entry_t *p) {
	int i;
	int frame = -1;
	for(i = 0; i < memsize; i++) {
		if(!coremap[i].in_use) {
			frame = i;
			break;
		}
	}
	if(frame == -1) { // Didn't find a free page.
		// Call replacement algorithm's evict function to select victim
		frame = evict_fcn(); //index in coremap

		// All frames were in use, so victim frame must hold some page
		// Write victim page to swap, if needed, and update pagetable
		// IMPLEMENTATION NEEDED
		pgtbl_entry_t *entry = (coremap[frame].pte);
				int fn = entry->frame >> PAGE_SHIFT;
				
		if(entry->swap_off == INVALID_SWAP){
			//hasn't been written to swap yet, so write
			entry->swap_off = swap_pageout(fn, entry->swap_off);
			evict_dirty_count++;
		}
		else if(entry->frame & PG_DIRTY){
			//it is dirtied so rewrite to swap
			entry->swap_off = swap_pageout(fn, entry->swap_off);
			evict_dirty_count++;
		}
		else{
			//it has not changed since it was last written so don't write again
			evict_clean_count++; 
		}

		if(entry->swap_off == INVALID_SWAP){
			fprintf(stderr, "error loading data into swap");
		}

		entry->frame = entry->frame &(~(PG_VALID)); //set valid = 0
		entry->frame = entry->frame | PG_ONSWAP; //at swap
		entry->frame = entry->frame &(~(PG_REF));//set ref=0
		entry->frame = entry->frame &(~(PG_DIRTY)); //not dirty as we have written to swap		

	}

	// Record information for virtual page that will now be stored in frame
	coremap[frame].in_use = 1;
	coremap[frame].pte = p;

	return frame;
}

/*
 * Initializes the top-level pagetable.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is 
 * being simulated, so there is just one top-level page table (page directory).
 * To keep things simple, we use a global array of 'page directory entries'.
 *
 * In a real OS, each process would have its own page directory, which would
 * need to be allocated and initialized as part of process creation.
 */
void init_pagetable() {
	int i;
	// Set all entries in top-level pagetable to 0, which ensures valid
	// bits are all 0 initially.
	for (i=0; i < PTRS_PER_PGDIR; i++) {
		pgdir[i].pde = 0;
	}
}

// For simulation, we get second-level pagetables from ordinary memory
pgdir_entry_t init_second_level() {
	int i;
	pgdir_entry_t new_entry;
	pgtbl_entry_t *pgtbl;

	// Allocating aligned memory ensures the low bits in the pointer must
	// be zero, so we can use them to store our status bits, like PG_VALID
	if (posix_memalign((void **)&pgtbl, PAGE_SIZE, 
			   PTRS_PER_PGTBL*sizeof(pgtbl_entry_t)) != 0) {
		perror("Failed to allocate aligned memory for page table");
		exit(1);
	}

	// Initialize all entries in second-level pagetable
	for (i=0; i < PTRS_PER_PGTBL; i++) {
		pgtbl[i].frame = 0; // sets all bits, including valid, to zero
		pgtbl[i].swap_off = INVALID_SWAP;
	}
    //Mark new entry as valid
	new_entry.pde = (uintptr_t)pgtbl | PG_VALID;
    return new_entry;
}

/* 
 * Initializes the content of a (simulated) physical memory frame when it 
 * is first allocated for some virtual address.  Just like in a real OS,
 * we fill the frame with zero's to prevent leaking information across
 * pages. 
 * 
 * In our simulation, we also store the the virtual address itself in the 
 * page frame to help with error checking.
 *
 */
void init_frame(int frame, addr_t vaddr) {
	// Calculate pointer to start of frame in (simulated) physical memory
	char *mem_ptr = &physmem[frame*SIMPAGESIZE];
	// Calculate pointer to location in page where we keep the vaddr
        addr_t *vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int));
	
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
	*vaddr_ptr = vaddr;             // record the vaddr for error checking

	return;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the entry is invalid and not on swap, then this is the first reference 
 * to the page and a (simulated) physical frame should be allocated and 
 * initialized (using init_frame).  
 *
 * If the entry is invalid and on swap, then a (simulated) physical frame
 * should be allocated and filled by reading the page data from swap.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
char *find_physpage(addr_t vaddr, char type) {
	pgtbl_entry_t *p=NULL; // pointer to the full page table entry for vaddr
	unsigned idx = PGDIR_INDEX(vaddr); // get index into page directory

	// IMPLEMENTATION NEEDED
	// Use top-level page directory to get pointer to 2nd-level page table
	//(void)idx; // To keep compiler happy - remove when you have a real use.
	pgdir_entry_t entry;
	entry = pgdir[idx];

	//check if the pgdir entry is not valid
	if(!(entry.pde &  PG_VALID)){
	   pgdir[idx] = init_second_level();
	}
        entry = pgdir[idx]; 
	// Use vaddr to get index into 2nd-level page table and initialize 'p'
	pgtbl_entry_t * ptfn = (pgtbl_entry_t *)(entry.pde &(~(0x1)));//page table start   
	unsigned long tidx = PGTBL_INDEX(vaddr); //index from the start of pgtb
	p = &(ptfn[tidx]);

	// Check if p is valid or not, on swap or not, and handle appropriately
    if(!(p->frame &  PG_VALID)){
		miss_count++;
		if(!(p->frame & PG_ONSWAP)){
			//first call as page is invalid and not in swap
			assert(p->swap_off == INVALID_SWAP); //sanity check
			p->frame = allocate_frame(p); 
			init_frame(p->frame, vaddr);
		}
		else{
			//frame in swap
			int frame_num = allocate_frame(p); 
			p->frame = (unsigned int)frame_num;
			if(swap_pagein(frame_num, p->swap_off) != 0){
				fprintf(stderr, "cannot read from swap\n");
            }
                        
		}
		p->frame = (p->frame << PAGE_SHIFT); //make room for reference bits
        p->frame = (p->frame | PG_VALID); //set it to be valid		
	}
	
	else{
		hit_count++;
	}

	//by now frame is valid
	if(!(p->frame & PG_VALID)){
		//should not reach here, just to double-check
		fprintf(stderr, "valid bit is not properly set \n");
	}

	// Make sure that p is marked valid and referenced. Also mark it
	// dirty if the access type indicates that the page will be written to.

	p->frame = (p->frame | PG_REF);
        ref_count++;
	if (type == 'S' || type == 'M') {
		p->frame = (p->frame | PG_DIRTY);
	}
	
	// Call replacement algorithm's ref_fcn for this page
	ref_fcn(p);

	// Return pointer into (simulated) physical memory at start of frame
	return  &physmem[(p->frame >> PAGE_SHIFT)*SIMPAGESIZE];
}

void print_pagetbl(pgtbl_entry_t *pgtbl) {
	int i;
	int first_invalid, last_invalid;
	first_invalid = last_invalid = -1;

	for (i=0; i < PTRS_PER_PGTBL; i++) {
		if (!(pgtbl[i].frame & PG_VALID) && 
		    !(pgtbl[i].frame & PG_ONSWAP)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("\t[%d] - [%d]: INVALID\n",
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			printf("\t[%d]: ",i);
			if (pgtbl[i].frame & PG_VALID) {
				printf("VALID, ");
				if (pgtbl[i].frame & PG_DIRTY) {
					printf("DIRTY, ");
				}
				printf("in frame %d\n",pgtbl[i].frame >> PAGE_SHIFT);
			} else {
				assert(pgtbl[i].frame & PG_ONSWAP);
				printf("ONSWAP, at offset %lu\n",pgtbl[i].swap_off);
			}			
		}
	}
	if (first_invalid != -1) {
		printf("\t[%d] - [%d]: INVALID\n", first_invalid, last_invalid);
		first_invalid = last_invalid = -1;
	}
}

void print_pagedirectory() {
	int i; // index into pgdir
	int first_invalid,last_invalid;
	first_invalid = last_invalid = -1;

	pgtbl_entry_t *pgtbl;

	for (i=0; i < PTRS_PER_PGDIR; i++) {
		if (!(pgdir[i].pde & PG_VALID)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("[%d]: INVALID\n  to\n[%d]: INVALID\n", 
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			pgtbl = (pgtbl_entry_t *)(pgdir[i].pde & PAGE_MASK);
			printf("[%d]: %p\n",i, pgtbl);
			print_pagetbl(pgtbl);
		}
	}
}
