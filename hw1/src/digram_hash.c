#include "const.h"
#include "sequitur.h"


/*
 * Digram hash table.
 *
 * Maps pairs of symbol values to first symbol of digram.
 * Uses open addressing with linear probing.
 * See, e.g. https://en.wikipedia.org/wiki/Open_addressing
 */

/**
 * Clear the digram hash table.
 */
void init_digram_hash(void) {
    for(int i = 0; i < MAX_DIGRAMS; i++)
    {
        (*(digram_table + i)) = NULL;
    }
}

/**
 * Look up a digram in the hash table.
 *
 * @param v1  The symbol value of the first symbol of the digram.
 * @param v2  The symbol value of the second symbol of the digram.
 * @return  A pointer to a matching digram (i.e. one having the same two
 * symbol values) in the hash table, if there is one, otherwise NULL.
 */
SYMBOL *digram_get(int v1, int v2) {

    int first_index = DIGRAM_HASH(v1, v2);
    int i = 0;

    SYMBOL **hash_symbol_cursor;
    hash_symbol_cursor = digram_table + first_index;

    int found = 0;

    while(((*hash_symbol_cursor) != NULL) && (i < MAX_DIGRAMS))
    {
        if((*hash_symbol_cursor) == TOMBSTONE)
        {
            //printf("%s\n", "TOMBSTONE");
        }
        else if((((*hash_symbol_cursor)->value) == v1) && ((((*hash_symbol_cursor)->next)->value) == v2))
        {
            found = 1;
            break;
        }

        i++;
        hash_symbol_cursor = (digram_table + ((i + first_index) % MAX_DIGRAMS));
    }

    if(found == 1)
    {
        return *hash_symbol_cursor;
    }
    else
    {
        return NULL;
    }
}

/**
 * Delete a specified digram from the hash table.
 *
 * @param digram  The digram to be deleted.
 * @return 0 if the digram was found and deleted, -1 if the digram did
 * not exist in the table.
 *
 * Note that deletion in an open-addressed hash table requires that a
 * special "tombstone" value be left as a replacement for the value being
 * deleted.  Tombstones are treated as vacant for the purposes of insertion,
 * but as filled for the purpose of lookups.
 *
 * Note also that this function will only delete the specific digram that is
 * passed as the argument, not some other matching digram that happens
 * to be in the table.  The reason for this is that if we were to delete
 * some other digram, then somebody would have to be responsible for
 * recycling the symbols contained in it, and we do not have the information
 * at this point that would allow us to be able to decide whether it makes
 * sense to do it here.
 */
int digram_delete(SYMBOL *digram) {
    if(digram == NULL)
    {
        return -1;
    }
    if((digram->next) == NULL)
    {
        return -1;
    }

    int index = DIGRAM_HASH((digram->value), ((digram->next)->value));
    int i = 0;

    SYMBOL **hash_symbol_cursor;
    hash_symbol_cursor = digram_table + index;

    int deleted = -1;

    while(((*hash_symbol_cursor) != NULL) && (i < MAX_DIGRAMS))
    {
        if((*hash_symbol_cursor) == (digram))
        {
            deleted = 0;
            (*hash_symbol_cursor) = TOMBSTONE;
            break;
        }

        i++;
        hash_symbol_cursor = (digram_table + ((i + index) % MAX_DIGRAMS));
    }

    return deleted;
}


/**
 * Attempt to insert a digram into the hash table.
 *
 * @param digram  The digram to be inserted.
 * @return  0 in case the digram did not previously exist in the table and
 * insertion was successful, 1 if a matching digram already existed in the
 * table and no change was made, and -1 in case of an error, such as the hash
 * table being full or the given digram not being well-formed.
 */
int digram_put(SYMBOL *digram) {
    if(digram == NULL)
    {
        return -1;
    }
    if((digram->next) == NULL)
    {
        return -1;
    }

    int first_index = DIGRAM_HASH((digram->value), ((digram->next)->value));
    int i = 0;

    SYMBOL **hash_symbol_cursor;
    hash_symbol_cursor = digram_table + first_index;

    int placed = -1;

    while(i < MAX_DIGRAMS)
    {
        if((*hash_symbol_cursor) == digram)
        {
            placed = 1;
            break;
        }

        if((*hash_symbol_cursor == NULL) || (*hash_symbol_cursor == TOMBSTONE))
        {
            *hash_symbol_cursor = digram;
            placed = 0;
            break;
        }

        i++;
        hash_symbol_cursor = digram_table + ((i + first_index) % MAX_DIGRAMS);
    }
    return placed;
}