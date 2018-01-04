#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;


//queue fnodes for lru and fifo
struct fnode{
	pgtbl_entry_t * entry;
	struct fnode * next;
	struct fnode * prev;
};

struct fnode * fhead = NULL;
struct fnode * ftail = NULL;

/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	struct fnode * victim = ftail;
	ftail = victim->prev;
	ftail->next = NULL;
	int frame = (victim->entry->frame) >> PAGE_SHIFT;
	free(victim);	
	return frame;
}

int in_list(pgtbl_entry_t * p){
	
	struct fnode * cur_fnode = fhead;
	while(cur_fnode != NULL){
		if(cur_fnode->entry == p){
			return 1;
		}
		cur_fnode = cur_fnode->next;
	}

	return 0;
}


/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {
	if((fhead == NULL) && (ftail == NULL)){
		struct fnode * first = malloc(sizeof(struct fnode));
		first->entry = p;
		first->next = NULL;
		first->prev = NULL;
		fhead = first;
		ftail = first;
	}

	else if(!(in_list(p))){
		struct fnode * new_fnode = malloc(sizeof(struct fnode));
		new_fnode->entry = p;
		new_fnode->next = fhead;
		fhead->prev = new_fnode;
		new_fnode->prev = NULL;
		fhead = new_fnode;		
	}
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {
}
