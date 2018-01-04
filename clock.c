#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

 struct cnode{
	pgtbl_entry_t * entry;
	struct cnode * next;
	struct cnode * prev;
};

 struct cnode * clock = NULL;

int clock_evict() {
	while(clock->entry->frame & PG_REF){
		//turn off the ref bit
		clock->entry->frame = clock->entry->frame & (~(PG_REF));
		clock = clock->next;
	}	
	struct cnode * victim = clock;
	victim->prev->next = victim->next;
	victim->next->prev = victim->prev;
	clock = clock->next;
	int frame = (victim->entry->frame) >> PAGE_SHIFT;
	free(victim);
	return frame;
}

struct cnode* list_contains(pgtbl_entry_t * p){
	
	struct cnode * cur_cnode = clock;
	do{
		if(cur_cnode->entry == p){
			return cur_cnode;
		}
		cur_cnode = cur_cnode->next;
	}while(cur_cnode != clock);

	return NULL;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

	if(p == NULL){
		fprintf(stderr, "a null pointer is passed");
		exit(1);
	}

	if(clock == NULL){
		//new entry is put before the clock location so it is visited last
		struct cnode * first = malloc(sizeof(struct cnode));
		first->entry = p;
		first->next = first; //for circular data structure
		first->prev = first; //for circular data structure
		clock = first;
		return;
	}
	struct cnode * in =  list_contains(p);

	if(in == NULL){
		struct cnode * new_cnode = malloc(sizeof(struct cnode));
		new_cnode->entry = p;
		new_cnode->next = clock;
		new_cnode->prev = clock->prev;
		clock->prev->next = new_cnode;
		clock->prev = new_cnode;		
	}	
	
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
}
