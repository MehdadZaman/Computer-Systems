/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "debug.h"
#include "sfmm.h"

#define ULONG_MAX  ((unsigned long)-1)
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) ((p->header) & (~0x3))

static int number_of_malloc_calls = 0;

void remove_from_free_list(sf_block *temp_block);
void split_block(unsigned long blocksize, sf_block *big_block, int is_wildblock);
void initialize_senteniel_nodes();
int find_fib_index(unsigned long size_class);

void *sf_malloc(size_t size) {
    if(number_of_malloc_calls == 0)
    {
        //First memory initializations
        sf_mem_grow();

        //Initializing the senteniel nodes in the list to point back at themselves
        initialize_senteniel_nodes();

        //Setting up the prologue block in the heap
        char *temp_ptr = sf_mem_start();
        temp_ptr += 56;
        unsigned long *prologue_block = (unsigned long *) temp_ptr;
        (*prologue_block) = 64;
        (*prologue_block) |= 3;
        temp_ptr += 56;
        prologue_block = (unsigned long *) temp_ptr;
        (*prologue_block) = 64;
        (*prologue_block) |= 3;

        //Setting up wild block in array of free lists
        sf_block *first_wild_block = (sf_block *)temp_ptr;
        //Setup blocksize in first block header
        (first_wild_block->header) = (4096 - (56 + 64 + 8));
        //Set free block's prev alloc bit to 1, because header block is allocated
        (first_wild_block->header) |= 0x2;
        //Add this large free block to the last index of the segregated free lists
        sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = first_wild_block;
        sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev = first_wild_block;
        (first_wild_block->body).links.next = &sf_free_list_heads[NUM_FREE_LISTS - 1];
        (first_wild_block->body).links.prev = &sf_free_list_heads[NUM_FREE_LISTS - 1];

        //Setting up the footer of the new LARGE block in the heap
        temp_ptr = sf_mem_end();
        temp_ptr -= 16;
        unsigned long *new_footer_block = (unsigned long *) temp_ptr;
        (*new_footer_block) = (first_wild_block->header);

        //Setting up the epilogue block in the heap
        temp_ptr = sf_mem_end();
        temp_ptr -= 8;
        unsigned long *epilogue_block = (unsigned long *) temp_ptr;
        (*epilogue_block) = 1;
    }

    //Increment the number of malloc calls
    number_of_malloc_calls++;

    //If size is 0, return NULL
    if(size == 0)
    {
        return NULL;
    }

    //Find the correct alignment by incrementing until blocksize is a multiple of 4
    unsigned long blocksize = 8UL + size;

    unsigned long required_margin_size = blocksize;
    while((required_margin_size % 64UL) != 0UL)
    {
        required_margin_size++;
    }

    //Set blocksize equal to margin with padding
    blocksize = required_margin_size;

    //Checking if blocksize is 0 again because of overflow
    if(blocksize == 0)
    {
        sf_errno = ENOMEM;
        return NULL;
    }

    //Size class and size class index
    unsigned long size_class = blocksize / 64UL;

    //Retrieving correct index for size_class_index
    int size_class_index = find_fib_index(size_class);

    //look for the block in the different segregated lists
    int valid_block_found = 0;
    sf_block *temp_block;

    for(int i = size_class_index; i < (NUM_FREE_LISTS - 1); i++)
    {
        //Start at the next node after the senteniel node
        temp_block = sf_free_list_heads[i].body.links.next;
        //while not equal to the senteniel node
        while(temp_block != (&sf_free_list_heads[i]))
        {
            //If less than or matches, it is found
            if(blocksize <= GET_SIZE(temp_block))
            {
                valid_block_found = 1;
                break;
            }
            temp_block = (*temp_block).body.links.next;
        }
        //valid block was found, so exit
        if (valid_block_found == 1)
        {
            break;
        }
    }

    //valid block was found, so split it
    if (valid_block_found == 1)
    {
        //Split the block
        split_block(blocksize, temp_block, 0);
    }
    else
    {
        //if there is no wildblock node in the last list
        if(sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev == (&sf_free_list_heads[NUM_FREE_LISTS - 1]))
        {
            //Get epilogue block (THIS WILL BE THE HEAD OF THE NEW BLOCK)
            char *block_header = sf_mem_end();
            block_header -= 16;
            temp_block = (sf_block *)block_header;

            unsigned long wildblock_blocksize = 0;
            while(blocksize > wildblock_blocksize)
            {
                char *ret_grow = sf_mem_grow();

                //If no more space is available in the heap
                if(ret_grow == NULL)
                {
                    sf_errno = ENOMEM;
                    return NULL;
                }

                wildblock_blocksize += 4096UL;

                //Start coalescing returne page and current block
                //Set up large block and epilogue
                (temp_block->header) &= 0x3;
                (temp_block->header) |= wildblock_blocksize;
                //Set allocated bit to 0
                (temp_block->header) &= (~0x1);

                //Set up footer of large block
                char *epilogue_block_temp = sf_mem_end();
                epilogue_block_temp -= 16;
                unsigned long *epilogue_block_temp2 = (unsigned long *)epilogue_block_temp;
                (*epilogue_block_temp2) = (temp_block->header);

                //Set up epilogue
                epilogue_block_temp = sf_mem_end();
                epilogue_block_temp -= 8;
                epilogue_block_temp2 = (unsigned long *)epilogue_block_temp;
                (*epilogue_block_temp2) = 1;

                //Add new block to wildblock segregated free list
                sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = temp_block;
                sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev = temp_block;
                (temp_block->body).links.next = &sf_free_list_heads[NUM_FREE_LISTS - 1];
                (temp_block->body).links.prev = &sf_free_list_heads[NUM_FREE_LISTS - 1];
            }
            //clearing top 62 bits and oring it with the new wildblocksize
            (temp_block->header) &= 0x3;
            (temp_block->header) |= wildblock_blocksize;
        }
        else
        {
            unsigned long wildblock_blocksize = GET_SIZE(sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next);
            temp_block = sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next;
            while(blocksize > wildblock_blocksize)
            {
                char *ret_grow = sf_mem_grow();

                //If no more space is available in the heap
                if(ret_grow == NULL)
                {
                    sf_errno = ENOMEM;
                    return NULL;
                }

                wildblock_blocksize += 4096UL;

                //Start coalescing returne page and current block
                //Set up large block and epilogue
                (temp_block->header) &= 0x3;
                (temp_block->header) |= wildblock_blocksize;
                //Set allocated bit to 0
                (temp_block->header) &= (~0x1);

                //Set up footer of large block
                char *epilogue_block_temp = sf_mem_end();
                epilogue_block_temp -= 16;
                unsigned long *epilogue_block_temp2 = (unsigned long *)epilogue_block_temp;
                (*epilogue_block_temp2) = (temp_block->header);

                //Set up epilogue
                epilogue_block_temp = sf_mem_end();
                epilogue_block_temp -= 8;
                epilogue_block_temp2 = (unsigned long *)epilogue_block_temp;
                (*epilogue_block_temp2) = 1;
            }
            //clearing top 62 bits and oring it with the new wildblocksize
            (temp_block->header) &= 0x3;
            (temp_block->header) |= wildblock_blocksize;
        }
        //Recreating the epilogue block
        char *epilogue_block_temp = sf_mem_end();
        epilogue_block_temp -= 8;
        unsigned long *epilogue_block_temp2 = (unsigned long *)epilogue_block_temp;
        (*epilogue_block_temp2) = 1;

        //Now we can split the block
        split_block(blocksize, temp_block, 1);
    }

    remove_from_free_list(temp_block);

    //return a void point to its body
    return ((void *)(temp_block->body).payload);
}

void initialize_senteniel_nodes()
{
    for(int i = 0; i < NUM_FREE_LISTS; i++)
    {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

void split_block(unsigned long blocksize, sf_block *big_block, int is_wildblock)
{
    int unsplittable_flag = 0;
    unsigned long smaller_blocksize = 0;

    //Checking if splitting block would produce splinters
    if((((big_block->header) & (~0x3)) - blocksize) < 64UL)
    {
        unsplittable_flag = 1;
        blocksize = ((big_block->header) & (~0x3));
    }
    else
    {
        smaller_blocksize = ((big_block->header) & (~0x3)) - blocksize;
    }

    //placing size in header
    (big_block->header) &= 0x3;
    (big_block->header) |= blocksize;
    //toggling allocated bit to 1
    (big_block->header) |= 0x1;

    //Go to the prev_footer field of the next block
    char *byte_cursor = (char *)big_block;
    byte_cursor += blocksize;

    //creating the next new block (or an already created one if there is a splinter)
    sf_block *new_block_ptr = (sf_block *)byte_cursor;

    //if not-splittable, just toggle next prevalloc bit
    if(unsplittable_flag == 1)
    {
        (new_block_ptr->header) |= 0x2;
        return;
    }
    //else just make it a free block
    else
    {
        (new_block_ptr->header) = smaller_blocksize;
        //Set prev allocated bit
        (new_block_ptr->header) |= 0x2;
    }

    //Set up information of the block after the newly split block
    char *temp_ptr2 = (char *)new_block_ptr;
    temp_ptr2 += smaller_blocksize;

    sf_block *block_after_new = (sf_block *)temp_ptr2;
    (block_after_new->prev_footer) = (new_block_ptr->header);

    //toggle prevalloc bit of next block to 0 because new block is free
    (block_after_new->header) &= (~0x2);

    //Add to free list
    unsigned long size_class = smaller_blocksize / 64UL;

    //Retrieving correct index for size_class_index
    int size_class_index = find_fib_index(size_class);

    if(is_wildblock == 1)
    {
        size_class_index = NUM_FREE_LISTS - 1;
    }

    //Circular linked-list update (LIFO STRUCTURE)
    (new_block_ptr->body).links.next = sf_free_list_heads[size_class_index].body.links.next;
    ((new_block_ptr->body).links.next)->body.links.prev = new_block_ptr;

    (new_block_ptr->body).links.prev = (&sf_free_list_heads[size_class_index]);
    sf_free_list_heads[size_class_index].body.links.next = new_block_ptr;
}

void remove_from_free_list(sf_block *temp_block)
{
    (*((*temp_block).body.links.next)).body.links.prev = (*temp_block).body.links.prev;
    (*((*temp_block).body.links.prev)).body.links.next = (*temp_block).body.links.next;
}
void add_to_free_list(sf_block *temp_block);

void sf_free(void *pp) {
    //Checking for invalid pointers
    if(pp == NULL)
    {
        abort();
    }
    long unsigned var = (long unsigned)pp;
    if((var % 64) != 0)
    {
        abort();
    }

    //Go to the header of the block
    char *pp2 = pp;
    pp2 -= 8;

    //Skip padding and go to the end of the end of the prologue
    char *prologue_end = sf_mem_start();
    prologue_end += 56;
    prologue_end += 64;

    //header is less than the ending address of prologue
    if(pp2 < prologue_end)
    {
        abort();
    }

    //Going to the footer of the block and getting the size
    pp2 -= 8;
    sf_block *current_block_check = (sf_block *)pp2;
    int current_block_size = GET_SIZE(current_block_check);

    //Go the end of the footer
    pp2 += 8;
    pp2 += current_block_size;

    //Go to the beginning of the epilogue
    char *epilogue_start = sf_mem_end();
    epilogue_start -= 8;

    //greater than the starting address of epilogue
    if(pp2 > epilogue_start)
    {
        abort();
    }

    //going to header
    char *temp_ptr = pp;
    temp_ptr -= 8;

    //if allocated bit is equal to 0
    unsigned long *header_ptr = (unsigned long *)temp_ptr;
    if(((*header_ptr) & (0x1)) == 0)
    {
        abort();
    }

    //prev_alloc field is 0, indicating that the previous block is free,
    //but the alloc field of the previous block header is not 0.
    if(((*header_ptr) & (0x2)) == 0)
    {
        //Go to footer of previous block
        temp_ptr -= 8;
        unsigned long *prev_footer_ptr = (unsigned long *)temp_ptr;
        //Get the blocksize of the previous block
        unsigned long prev_size = ((*prev_footer_ptr) & (~0x3));
        //Go to header of previous block
        temp_ptr += 8;
        temp_ptr -= prev_size;
        unsigned long *prev_header_ptr = (unsigned long *)temp_ptr;

        //footer and header are not equal to eachother
        if((*prev_header_ptr) != (*prev_footer_ptr))
        {
            abort();
        }

        if(((*prev_footer_ptr) & (0x1)) == 1)
        {
            abort();
        }
    }

    //Go to next block
    unsigned long size_value = ((*header_ptr) & (~0x3));
    temp_ptr = (char *)header_ptr;
    temp_ptr += size_value;

    //Go to footer of the same block
    temp_ptr -= 8;
    unsigned long *footer_ptr = (unsigned long *)temp_ptr;

    //increment ptr to go to next block's head
    temp_ptr += 8;
    //if next blocks prev alloc bit is equal to 0 (block is already free)
    unsigned long *next_header_ptr = (unsigned long *)temp_ptr;
    if(((*next_header_ptr) & (0x2)) == 0)
    {
        abort();
    }

    //Now start checking cases for coalescing
    //Case 1 -- Previous block and next block are both free
    if((((*header_ptr) & (0x2)) == 0) && (((*next_header_ptr) & (0x1)) == 0))
    {
        //Go to footer of previous block
        temp_ptr = pp;
        temp_ptr -= 16;
        unsigned long *prev_footer_ptr = (unsigned long *)temp_ptr;
        //Get the blocksize of the previous block
        unsigned long prev_size = ((*prev_footer_ptr) & (~0x3));
        //Go to header of previous block
        temp_ptr += 8;
        temp_ptr -= prev_size;
        unsigned long *prev_header_ptr = (unsigned long *)temp_ptr;

        //set new header to the combined size
        (*prev_header_ptr) &= 0x3;
        unsigned long full_size = prev_size + ((*header_ptr) & (~0x3)) + ((*next_header_ptr) & (~0x3));
        (*prev_header_ptr) |= full_size;

        //set up the footer of the new free block
        temp_ptr = (char *)prev_header_ptr;
        temp_ptr += full_size;
        temp_ptr -= 8;
        footer_ptr = (unsigned long *)temp_ptr;
        (*footer_ptr) = (*prev_header_ptr);

        //remove the original previous blocks from the free lists
        remove_from_free_list((sf_block *)(prev_header_ptr - 1));
        remove_from_free_list((sf_block *)(next_header_ptr - 1));

        //add to new block to free list
        add_to_free_list((sf_block *)(prev_header_ptr - 1));

        //change next block's prevalloc bit
        //Go to next block
        temp_ptr = (char *)prev_header_ptr;
        temp_ptr += full_size;

        //toggling prev_alloc bit in next block
        unsigned long *next_block_in_heap = (unsigned long *)temp_ptr;
        (*next_block_in_heap) &= (~0x2);
    }

    //Case 2 -- Only previous block is free
    else if(((*header_ptr) & (0x2)) == 0)
    {
        //Go to footer of previous block
        temp_ptr = pp;
        temp_ptr -= 16;
        unsigned long *prev_footer_ptr = (unsigned long *)temp_ptr;
        //Get the blocksize of the previous block
        unsigned long prev_size = ((*prev_footer_ptr) & (~0x3));
        //Go to header of previous block
        temp_ptr += 8;
        temp_ptr -= prev_size;
        unsigned long *prev_header_ptr = (unsigned long *)temp_ptr;

        //set new header to the combined size
        (*prev_header_ptr) &= 0x3;
        unsigned long full_size = prev_size + ((*header_ptr) & (~0x3));
        (*prev_header_ptr) |= full_size;

        //set up the footer of the new free block
        temp_ptr = (char *)prev_header_ptr;
        temp_ptr += full_size;
        temp_ptr -= 8;
        footer_ptr = (unsigned long *)temp_ptr;
        (*footer_ptr) = (*prev_header_ptr);

        //remove the original previous block from the free list
        remove_from_free_list((sf_block *)(prev_header_ptr - 1));

        //add to new block to free list
        add_to_free_list((sf_block *)(prev_header_ptr - 1));

        //change next block's prevalloc bit
        //Go to next block
        temp_ptr = (char *)prev_header_ptr;
        temp_ptr += full_size;

        //toggling prev_alloc bit in next block
        unsigned long *next_block_in_heap = (unsigned long *)temp_ptr;
        (*next_block_in_heap) &= (~0x2);
    }

    //Case 3 -- Only next block is free
    else if(((*next_header_ptr) & (0x1)) == 0)
    {
        //set new header to the combined size
        unsigned long full_size = ((*header_ptr) & (~0x3)) + ((*next_header_ptr) & (~0x3));
        (*header_ptr) &= 0x3;
        (*header_ptr) |= full_size;
        (*header_ptr) &= (~0x1);

        //set up the footer of the new free block
        temp_ptr = (char *)header_ptr;
        temp_ptr += full_size;
        temp_ptr -= 8;
        footer_ptr = (unsigned long *)temp_ptr;
        (*footer_ptr) = (*header_ptr);

        //remove the original previous block from the free list
        remove_from_free_list((sf_block *)(next_header_ptr - 1));

        //add to new block to free list
        add_to_free_list((sf_block *)(header_ptr - 1));

        //change next block's prevalloc bit
        //Go to next block
        temp_ptr = (char *)header_ptr;
        temp_ptr += full_size;

        //toggling prev_alloc bit in next block
        unsigned long *next_block_in_heap = (unsigned long *)temp_ptr;
        (*next_block_in_heap) &= (~0x2);
    }

    //Case 4 -- Neither next nor previous block is free
    else
    {
        //set new header to the combined size
        unsigned long full_size = ((*header_ptr) & (~0x3));
        (*header_ptr) &= 0x3;
        (*header_ptr) |= full_size;
        (*header_ptr) &= (~0x1);

        //set up the footer of the new free block
        temp_ptr = (char *)header_ptr;
        temp_ptr += full_size;
        temp_ptr -= 8;
        footer_ptr = (unsigned long *)temp_ptr;
        (*footer_ptr) = (*header_ptr);

        //add to new block to free list
        add_to_free_list((sf_block *)(header_ptr - 1));

        //change next block's prevalloc bit
        //Go to next block
        temp_ptr = (char *)header_ptr;
        temp_ptr += full_size;

        //toggling prev_alloc bit in next block
        unsigned long *next_block_in_heap = (unsigned long *)temp_ptr;
        (*next_block_in_heap) &= (~0x2);
    }
}

void add_to_free_list(sf_block *temp_block)
{
    //Get the size of the current block
    unsigned long blocksize = GET_SIZE(temp_block);
    char *byte_cursor = (char *)temp_block;

    //Go to the footer of the current block to create block struct for the next block
    byte_cursor += blocksize;

    //Make a block pointer out of the next block
    sf_block *next_block = (sf_block *)byte_cursor;

    //Get the size of the next block
    int next_block_blocksize = GET_SIZE(next_block);

    //If the next block is the epilogue block
    if(next_block_blocksize == 0)
    {
        sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = temp_block;
        sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev = temp_block;
        (temp_block->body).links.next = &sf_free_list_heads[NUM_FREE_LISTS - 1];
        (temp_block->body).links.prev = &sf_free_list_heads[NUM_FREE_LISTS - 1];
    }
    else
    {
        //Add to free list
        unsigned long size_class = blocksize / 64UL;

        //Retrieving correct index for size_class_index
        int size_class_index = find_fib_index(size_class);

        //Circular linked-list update (LIFO STRUCTURE)
        (temp_block->body).links.next = sf_free_list_heads[size_class_index].body.links.next;
        ((temp_block->body).links.next)->body.links.prev = temp_block;

        (temp_block->body).links.prev = (&sf_free_list_heads[size_class_index]);
        sf_free_list_heads[size_class_index].body.links.next = temp_block;
    }
}

int find_fib_index(unsigned long size_class)
{
    unsigned long fib1 = 1;
    unsigned long fib2 = 1;

    //The index that will be returned
    int size_class_index = 0;

    while(size_class > fib2)
    {
        //If reached the largest fib size, then break
        if(size_class_index == (NUM_FREE_LISTS - 2))
        {
            break;
        }
        //increment the index
        size_class_index++;

        //update the 2 fibonacci numbers
        unsigned long temp = fib2;
        fib2 = fib2 + fib1;
        fib1 = temp;
    }

    return size_class_index;
}

int is_invalid_mallocd_ptr(void *pp);
void split_and_free_small_block(sf_block *current_block, unsigned long size);

void *sf_realloc(void *pp, size_t rsize) {
    //Checking if pointer is invalid
    int invalid_ptr_ret_value = is_invalid_mallocd_ptr(pp);

    //If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
    if(invalid_ptr_ret_value == 1)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    //checking if size is equal to 0
    //If sf_realloc is called with a valid pointer and a size of 0 it should free
    if(rsize == 0)
    {
        sf_free(pp);
        return NULL;
    }

    //Going to the footer of the previous block to create an sf_block pointer
    char *temp_ptr = pp;
    temp_ptr -= 16;

    sf_block *current_block = (sf_block *)temp_ptr;
    unsigned long current_block_size = GET_SIZE(current_block);

    unsigned long new_block_size = rsize;
    //header size is added to rsize
    new_block_size += 8;
    //look for closest multiple of 64 for new block
    while((new_block_size % 64UL) != 0UL)
    {
        new_block_size++;
    }

    char *new_block_ptr;

    //if rsize is greater than current blocksize
    if(current_block_size < new_block_size)
    {
        new_block_ptr = sf_malloc(rsize);

        //check if return of sf_malloc is NULL
        if(new_block_ptr == NULL)
        {
            return NULL;
        }

        //payload size is the entire blocksize minus the header
        unsigned long payload_size = current_block_size - 8;
        memcpy(new_block_ptr, pp, payload_size);

        //free the current block
        sf_free(pp);

        //return new malloc'd pointer
        return new_block_ptr;
    }
    //if rsize is less or equal to current blocksize
    else
    {
        //checking if the next block is the epilogue or not
        //Pass in the new blocksize with the added header and padding
        split_and_free_small_block(current_block, rsize);

        //returning current given pointer
        return pp;
    }
}

int is_invalid_mallocd_ptr(void *pp)
{
    //Checking for invalid pointers
    if(pp == NULL)
    {
        return 1;
    }
    long unsigned var = (long unsigned)pp;
    if((var % 64) != 0)
    {
        return 1;
    }

    //Go to the header of the block
    char *pp2 = pp;
    pp2 -= 8;

    //Skip padding and go to the end of the end of the prologue
    char *prologue_end = sf_mem_start();
    prologue_end += 56;
    prologue_end += 64;

    //header is less than the ending address of prologue
    if(pp2 < prologue_end)
    {
        return 1;
    }

    //Going to the footer of the block and getting the size
    pp2 -= 8;
    sf_block *current_block_check = (sf_block *)pp2;
    int current_block_size = GET_SIZE(current_block_check);

    //Go the end of the footer
    pp2 += 8;
    pp2 += current_block_size;

    //Go to the beginning of the epilogue
    char *epilogue_start = sf_mem_end();
    epilogue_start -= 8;

    //greater than the starting address of epilogue
    if(pp2 > epilogue_start)
    {
        return 1;
    }

    //going to header
    char *temp_ptr = pp;
    temp_ptr -= 8;

    //if allocated bit is equal to 0
    unsigned long *header_ptr = (unsigned long *)temp_ptr;
    if(((*header_ptr) & (0x1)) == 0)
    {
        return 1;
    }

    //prev_alloc field is 0, indicating that the previous block is free,
    //but the alloc field of the previous block header is not 0.
    if(((*header_ptr) & (0x2)) == 0)
    {
        //Go to footer of previous block
        temp_ptr -= 8;
        unsigned long *prev_footer_ptr = (unsigned long *)temp_ptr;
        //Get the blocksize of the previous block
        unsigned long prev_size = ((*prev_footer_ptr) & (~0x3));
        //Go to header of previous block
        temp_ptr += 8;
        temp_ptr -= prev_size;
        unsigned long *prev_header_ptr = (unsigned long *)temp_ptr;

        //footer and header are not equal to eachother
        if((*prev_header_ptr) != (*prev_footer_ptr))
        {
            return 1;
        }

        if(((*prev_footer_ptr) & (0x1)) == 1)
        {
            return 1;
        }
    }

    //Go to next block
    unsigned long size_value = ((*header_ptr) & (~0x3));
    temp_ptr = (char *)header_ptr;
    temp_ptr += size_value;

    //Go to footer of the same block
    temp_ptr -= 8;

    //increment ptr to go to next block's head
    temp_ptr += 8;
    //if next blocks prev alloc bit is equal to 0 (block is already free)
    unsigned long *next_header_ptr = (unsigned long *)temp_ptr;
    if(((*next_header_ptr) & (0x2)) == 0)
    {
        return 1;
    }

    //passed all of the cases
    return 0;
}

void split_and_free_small_block(sf_block *current_block, unsigned long new_size)
{
    //header size is added to rsize
    new_size += 8;
    //look for closest multiple of 64 for new block
    while((new_size % 64UL) != 0UL)
    {
        new_size++;
    }

    unsigned long smaller_blocksize = 0;

    //Checking if splitting block would produce splinters
    if((((current_block->header) & (~0x3)) - new_size) < 64UL)
    {
        return;
    }

    //find the small blocksize
    smaller_blocksize = ((current_block->header) & (~0x3)) - new_size;

    //Working on modifying the newly allocated block

    //placing size in header
    (current_block->header) &= 0x3;
    (current_block->header) |= new_size;

    //Go to the new block and allocate it, so that you can free it
    char *byte_cursor = (char *)current_block;
    byte_cursor += new_size;

    //Get to the new block
    sf_block *new_block_ptr = (sf_block *)byte_cursor;

    (new_block_ptr->header) = smaller_blocksize;
    //Set prev allocated bit
    (new_block_ptr->header) |= 0x2;
    //Set its allocated bit so that we can free it later
    (new_block_ptr->header) |= 0x1;

    //The next block already has information that the current block was allocated, so no changes
    //need to be made

    //Now go to the payload area of the block, so that it can be freed
    char *temp_ptr = (char *)new_block_ptr;
    temp_ptr += 16;
    sf_free(temp_ptr);
}

int is_power_of_2(unsigned long size);

void *sf_memalign(size_t size, size_t align) {

    //If align is less than the minimum block size, then NULL is returned and sf_errno is set to EINVAL.
    if(align < 64UL)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    //If align is not a power of two, then NULL is returned and sf_errno is set to EINVAL.
    int is_power_of_2_ret = is_power_of_2(align);
    if(is_power_of_2_ret == 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    //If size is 0, then NULL is returned without setting sf_errno.
    if(size == 0)
    {
        return NULL;
    }

    //requested size, plus the alignment size, plus the minimum block size, plus the size required for a block header
    unsigned long minimum_size = size + align + 64UL;

    char *large_block = sf_malloc(minimum_size);

    //Get the current sf_block that holds the information
    char *byte_cursor = large_block;
    byte_cursor -= 16;
    sf_block *current_block = (sf_block *)byte_cursor;
    unsigned long current_block_size = GET_SIZE(current_block);

    unsigned long current_payload_size = current_block_size - 8UL;

    unsigned long ptr_alignment = (unsigned long)large_block;

    //number of left over bytes
    unsigned long num_leftover_bytes;
    (void)num_leftover_bytes;

    //block to be returned
    sf_block *block_to_be_returned = NULL;

    //returned pointer is not aligned to the desired alignment
    if((ptr_alignment % align) != 0)
    {
        byte_cursor = large_block;
        unsigned long increment_counter = 0;
        while(increment_counter < current_payload_size)
        {
            if((((ptr_alignment + increment_counter) % align) == 0) && ((current_payload_size - increment_counter) >= size) && (increment_counter >= 64UL))
            {
                //Set up the new block that will be returned
                char *temp_ptr = large_block;
                temp_ptr += increment_counter;
                temp_ptr -= 16;

                //set the block to be returned equal to the correct size, add 8 because of the header
                block_to_be_returned = (sf_block *)temp_ptr;
                (block_to_be_returned->header) = (current_payload_size - increment_counter + 8);

                //toggle the prev alloc bit so that the previous block can be freed later on
                (block_to_be_returned->header) |= 0x2;

                //toggle the alloc bit because you are allocating it
                (block_to_be_returned->header) |= 0x1;

                //Now work on freeing the previous block

                //Go to the header of the returned pointer
                temp_ptr = large_block;
                temp_ptr -= 16;

                //Set the header equal to the number of bytes traversed
                unsigned long previous_block_size = increment_counter;

                sf_block* previous_block = (sf_block *)temp_ptr;
                //clear out the original size
                (previous_block->header) &= (0x3);
                (previous_block->header) |= previous_block_size;

                //now free the previous block
                sf_free(large_block);

                break;
            }
            increment_counter++;
        }
    }
    else
    {
        //Set up the new block that will be returned
        char *temp_ptr = large_block;
        temp_ptr -= 16;

        block_to_be_returned = (sf_block *)temp_ptr;
    }

    //Now split the block to be returned
    split_and_free_small_block(block_to_be_returned, size);

    return ((void *)(*block_to_be_returned).body.payload);
}

int is_power_of_2(unsigned long size)
{
    unsigned long number = size;

    //number of 1s in binary representation of number
    int number_of_ones_inbinary = 0;

    //while the number is not equal to 0
    while(number != 0)
    {
        //check if the last bit is 1
        if((number & (0x1)) == 1)
        {
            number_of_ones_inbinary++;
        }

        //if there is more than one 1 in the binary representation of the number
        //then it is not a power of 2
        if(number_of_ones_inbinary > 1)
        {
            return 0;
        }

        //shift number left by 1
        number = (number >> 1);
    }

    //if there is one 1 in the binary representation of the number, then it is a power of 2
    if(number_of_ones_inbinary == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}