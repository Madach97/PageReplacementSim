#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

struct node{
	pgtbl_entry_t * entry;
	struct node * next;
	struct node * prev;
};


struct node * head = NULL;
struct node * tail = NULL;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	if(tail == NULL)
	{
		exit(1);
	}
	struct node * victim = tail;
              
	tail = victim->prev;
	tail->next = NULL;
	int frame = (victim->entry->frame) >> PAGE_SHIFT;
	free(victim);	
	return frame;
}

struct node* inside_list(pgtbl_entry_t * p){
	
	struct node * cur_node = head;
	while(cur_node != NULL){
		if(cur_node->entry == p){
			return cur_node;
		}
		cur_node = cur_node->next;
	}

	return NULL;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	if(p == NULL){
		fprintf(stderr, "a null pointer is passed");
		exit(1);
	}

	if((head == NULL) && (tail == NULL)){
		struct node * first = malloc(sizeof(struct node));
		first->entry = p;
		first->next = NULL;
		first->prev = NULL;
		head = first;
		tail = first;
		return;
	}
	struct node * in = inside_list(p);

	if(in == NULL){
		struct node * new_node = malloc(sizeof(struct node));
		new_node->entry = p;
		new_node->next = head;
		head->prev = new_node;
		new_node->prev = NULL;
		head = new_node;		
	}

	else{
         
		if((in->prev != NULL) && (in->next != NULL)){
		    in->prev->next = in->next;
                    in->next->prev = in->prev;
                    in->next = head;
                    head->prev = in;
                    in->prev = NULL;
                    head = in;
                
	        }
            
                else if((in->next != NULL) && (in->prev == NULL)){
                   assert(in == head); //sanity check
                   //already at top priority do nothing                 
                }
 
                else{
                  // next = NULL and prev not null
                  assert(in == tail); //sanity check
                  tail = in->prev;
                  in->prev->next = NULL;
                  in->next = head;
                  head->prev = in;
                  in->prev = NULL;               
                  head = in;
               }
           
	}
	
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
}
