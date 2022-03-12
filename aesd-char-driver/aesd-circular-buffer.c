/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer implementation
 * 
 * KJ: Modified for completion of assignment 7.
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h> // kfree
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer. 
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{
    uint8_t offset = 0;
    size_t total_size = 0;
    size_t last_size = 0;

    // Validate arguments
    if ((buffer == NULL) || (entry_offset_byte_rtn == NULL))
        return NULL;

    // Loop through all entries until matching offset is found
    offset = buffer->out_offs; // Start with next entry to be popped
    do
    {
        last_size = total_size;
        total_size += buffer->entry[offset].size;

        if (char_offset <= (total_size - 1))
        {
            *entry_offset_byte_rtn = char_offset - last_size;
            return &buffer->entry[offset];
        }

        // Increment to next position
        if ((++offset) >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            offset = 0;

    } while (offset != buffer->in_offs);

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char *pBuf = NULL;
    
    // Verify arguments
    if ((buffer == NULL) || (add_entry == NULL) || (add_entry->buffptr == NULL) || (add_entry->size == 0))
        return pBuf;

    // Check if buffer is already full
    if (buffer->full)
    {
        // Save memory address pointed to by the out offset
        pBuf = buffer->entry[buffer->out_offs].buffptr;
        if ((++buffer->out_offs) >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            buffer->out_offs = 0;
    }

    // Add new entry to buffer
    buffer->entry[buffer->in_offs] = *add_entry;
    if ((++buffer->in_offs) >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        buffer->in_offs = 0;

    // Check whether buffer is still full or not full
    buffer->full = (buffer->in_offs == buffer->out_offs) ? true : false;

    return pBuf;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}

// See aesd-circular-buffer.h for documentation
void aesd_circular_buffer_deinit(struct aesd_circular_buffer *buffer)
{
    uint8_t index;
    struct aesd_buffer_entry *pEntry;

    // Loop through each slot and free allocated memory
    AESD_CIRCULAR_BUFFER_FOREACH(pEntry, buffer, index)
    {
        if (pEntry->buffptr == NULL)
            continue; // Memory already freed

// Use kernel to determine if kfree or free is needed
#ifdef __KERNEL__
        // Kernel space
        kfree(pEntry->buffptr);
#else
        // User space
        free(pEntry->buffptr);
#endif
    }
}
