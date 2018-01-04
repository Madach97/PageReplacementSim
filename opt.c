#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

extern unsigned memsize;

extern int debug;

extern struct frame *coremap;

struct time_element{
	addr_t vaddr;
	struct time_element * next;
	struct evict_queue * match;
};

struct evict_queue{
	addr_t vaddr;
	pgtbl_entry_t * ptentry;
	struct evict_queue * next;
	struct evict_queue * prev;
}; 

struct time_element * first = NULL;
struct time_element * end = NULL;
struct time_element * nearest_future = NULL;

struct evict_queue* ehead = NULL;
struct evict_queue* etail = NULL;


addr_t get_vaddr(int frame){
     frame = frame >> PAGE_SHIFT;
    char *mem_ptr = &physmem[frame*SIMPAGESIZE];
	// Calculate pointer to location in page where we keep the vaddr
        addr_t *vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int));
     return (*(vaddr_ptr));

}

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {

	if(etail == NULL){
		fprintf(stderr, "the tail is null!\n");
		exit(1);
	}
	struct evict_queue * victim = etail;
        etail = victim->prev;
	etail->next = NULL;
	int frame = (victim->ptentry->frame) >> PAGE_SHIFT;
	free(victim);	
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
    
	if(p == NULL){
		fprintf(stderr, "a null pointer is passed");
		exit(1);
	}
    int found_spot = 0; //has the frame been appended to the list
	if(nearest_future != NULL){
		//nearest_future = NULL only for the last reference
		nearest_future = nearest_future->next;
	}

	if((ehead == NULL) && (etail == NULL)){
		struct evict_queue * item = malloc(sizeof(struct evict_queue));
		item->ptentry = p;
		item->vaddr = get_vaddr(p->frame);
		item->next = NULL;
		item->prev = NULL;
		ehead = item;
		etail = item;
		return;
	}

	struct evict_queue * item = malloc(sizeof(struct evict_queue));
	item->ptentry = p;
	item->vaddr = get_vaddr(p->frame);
        struct time_element * current = nearest_future;
	item->next = ehead;
        //ehead->prev = item;
	item->prev = NULL;
	while(current != NULL){

		if(item->next == NULL){
                        found_spot = 1;
			//this is the last element
		         break;
		}

		if(item->next->vaddr == item->vaddr){
                        found_spot = 1;
			//this item already exists in the queue
			if(item->next == ehead && item->next == etail){
				//only element in the list
				etail = item;
				ehead = item;	
                                break;			
			}

			else if(item->next == ehead){
				ehead = item;
				assert(item->prev == NULL); //sanity check	
			}
                        struct evict_queue * orig_next = item->next;
			item->next = orig_next->next; //remove the entry already in the queue as
		        if(item->next != NULL){
                            item->next->prev = item;
                         }	                       //item must be the most recent edition
			item->prev = orig_next->prev;
                        if(item->prev != NULL){
                            item->prev->next = item;
                         }

			if(item->next == NULL){ //the new next is null
				etail = item; //then this is the last element
			}
			
		}

		if(current->vaddr == item->vaddr){
                        found_spot = 1;
			if(item->next == ehead){
				ehead->prev = item;
				assert(item->prev == NULL);//sanity check
				ehead = item;
			}
			break; //we are in the right spot
		}

		else if(current->vaddr == item->next->vaddr){
                        found_spot = 1;
			//the next appears before this item, so item must go after it
			struct evict_queue * orig_next = item->next;
                        struct evict_queue * orig_prev = item->prev;
                                             
                        if(item == ehead){
                           ehead = orig_next;
                        }

                        orig_next->prev = item->prev;
                        if(orig_prev != NULL){
                           orig_prev->next = item->next;
                        }

                        item->next = orig_next->next;
                        item->prev = orig_next;

                        if(item->next != NULL){
                            item->next->prev = item;
                        }
                        
			if(orig_next == etail){
				etail = item;
			}
			orig_next->next = item;
			
		}	

		current = current->next;		
		

	}
        if(found_spot == 0){
          //neither this item nor others in the list repeat themseleves anymore
             etail->next = item;
             item->prev = etail;
             item->next = NULL;
             etail = item;
        }
        return;
}

void add_val(addr_t addr){
	struct time_element * entry = malloc(sizeof(struct time_element));
	entry->vaddr = addr;
        if((first == NULL) && (end == NULL)){
		first = entry;
		end = entry;
		entry->next = NULL;
	}
	else{
                assert(end != NULL);
		end->next = entry;
		end = entry;
		entry->next = NULL;
	}
    
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
	FILE *fp = stdin;
	if(tracefile != NULL) {
		if((fp = fopen(tracefile, "r")) == NULL) {
			perror("Error opening tracefile:");
			exit(1);
		}
	}
	char buf[MAXLINE];
	addr_t vaddr = 0;
	char type;

	while(fgets(buf, MAXLINE, fp) != NULL) {
		if(buf[0] != '=') {
			sscanf(buf, "%c %lx", &type, &vaddr);
			if(debug)  {
				printf("%c %lx\n", type, vaddr);
			}
			add_val(vaddr);
		} else {
			continue;
		}

	}
	nearest_future = first;
}

