/*
 * NASA Docket No. GSC-18,587-1 and identified as “The Bundle Protocol Core Flight
 * System Application (BP) v6.5”
 *
 * Copyright © 2020 United States Government as represented by the Administrator of
 * the National Aeronautics and Space Administration. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef V7_MPOOL_H
#define V7_MPOOL_H

#include <string.h>

#include "bplib_api_types.h"
#include "v7_types.h"
#include "v7_rbtree.h"
#include "crc.h"

/*
 * A note about encoded chunk size -
 * All data chunks, including logical (non-encoded) primary and canonical block info,
 * as well as encoded blobs, get stored in the same size record.  These are then chained
 * together to hold larger items.
 */
#define BP_MPOOL_MAX_ENCODED_CHUNK_SIZE 320

/*
 * The basic types of blocks which are cacheable in the mpool
 */
typedef struct bplib_mpool_primary_block   bplib_mpool_primary_block_t;
typedef struct bplib_mpool_canonical_block bplib_mpool_canonical_block_t;
typedef struct bplib_mpool_refptr          bplib_mpool_refptr_t;

typedef struct bplib_mpool_subq bplib_mpool_subq_t;
typedef struct bplib_mpool_flow bplib_mpool_flow_t;

typedef enum bplib_mpool_blocktype
{
    bplib_mpool_blocktype_undefined = 0,
    bplib_mpool_blocktype_head      = 1,
    bplib_mpool_blocktype_ref       = 2,

    /*
     * Note, the enum value here does matter -- these block types
     * are all refcount-capable, and thus are grouped together so this
     * can be implemented as a range check.  Do not change the
     * order of enum values without also updating the checks.
     */
    bplib_mpool_blocktype_cbor_data      = 3,
    bplib_mpool_blocktype_service_object = 4,
    bplib_mpool_blocktype_primary        = 5,
    bplib_mpool_blocktype_canonical      = 6,
    bplib_mpool_blocktype_flow           = 7,

    bplib_mpool_blocktype_max = 8, /* placeholder for the max "regular" block type */

    /*
     * A secondary link is one that is not at the beginning of the structure.
     * The offset from the beginning of the block is added to this value, so
     * that the original block type can be easily obtained.
     */
    bplib_mpool_blocktype_secondary_link_base = 1000

} bplib_mpool_blocktype_t;

struct bplib_mpool_block
{
    bplib_mpool_blocktype_t   type;
    struct bplib_mpool_block *next;
    struct bplib_mpool_block *prev;
};

/*
 * Enumeration that defines the various possible routing table events.  This enum
 * must always appear first in the structure that is the argument to the event handler,
 * and indicates the actual event that has occurred
 */
typedef enum
{
    bplib_mpool_eventid_undefined,
    bplib_mpool_eventid_recycle,
    bplib_mpool_eventid_max

} bplib_mpool_eventid_t;

typedef void (*bplib_mpool_event_func_t)(bplib_mpool_eventid_t, bplib_mpool_block_t *);
typedef void (*bplib_mpool_callback_func_t)(void *, bplib_mpool_block_t *);

typedef struct bplib_mpool_delivery_data
{
    bplib_policy_delivery_t delivery_policy;
    bp_handle_t             ingress_intf_id;
    bp_handle_t             egress_intf_id;
    bp_handle_t             storage_intf_id;
    bp_sid_t                committed_storage_id;
    uint64_t                local_retx_interval;
    bp_dtntime_t            ingress_time;
    bp_dtntime_t            egress_time;
} bplib_mpool_delivery_data_t;

struct bplib_mpool_primary_block
{
    bplib_mpool_block_t         cblock_list;
    bplib_mpool_block_t         chunk_list;
    size_t                      block_encode_size_cache;
    size_t                      bundle_encode_size_cache;
    bp_primary_block_t          pri_logical_data;
    bplib_mpool_delivery_data_t delivery_data;
};

struct bplib_mpool_canonical_block
{
    bplib_mpool_block_t          chunk_list;
    bplib_mpool_primary_block_t *bundle_ref;
    size_t                       block_encode_size_cache;
    size_t                       encoded_content_offset;
    size_t                       encoded_content_length;
    bp_canonical_block_buffer_t  canonical_logical_data;
};

struct bplib_mpool_subq
{
    bplib_mpool_block_t block_list;
    bplib_q_stats_t     stats;
    size_t              current_depth_limit;
};

struct bplib_mpool_flow
{
    bp_handle_t           external_id;
    bplib_mpool_subq_t    input;
    bplib_mpool_subq_t    output;
    bplib_mpool_refptr_t *parent;
};

/**
 * @brief Gets the actual block from a reference block (dereference)
 *
 * @param refptr
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_reference_target(bplib_mpool_refptr_t *refptr)
{
    return (bplib_mpool_block_t *)refptr;
}

/**
 * @brief Gets the logical information associated with a primary block
 *
 * @param cpb
 * @return bp_primary_block_t*
 */
static inline bp_primary_block_t *bplib_mpool_get_pri_block_logical(bplib_mpool_primary_block_t *cpb)
{
    return &cpb->pri_logical_data;
}

/**
 * @brief Gets the list of encoded chunks associated with a primary block
 *
 * @param cpb
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_pri_block_encoded_chunks(bplib_mpool_primary_block_t *cpb)
{
    return &cpb->chunk_list;
}

/**
 * @brief Gets the list of canonical blocks associated with a primary block
 *
 * @param cpb
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_canonical_block_list(bplib_mpool_primary_block_t *cpb)
{
    return &cpb->cblock_list;
}

/**
 * @brief Gets the logical data associated with a canonical block
 *
 * @param ccb
 * @return bp_canonical_block_buffer_t*
 */
static inline bp_canonical_block_buffer_t *bplib_mpool_get_canonical_block_logical(bplib_mpool_canonical_block_t *ccb)
{
    return &ccb->canonical_logical_data;
}

/**
 * @brief Gets the list of encoded chunks associated with a canonical block
 *
 * @param ccb
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_canonical_block_encoded_chunks(bplib_mpool_canonical_block_t *ccb)
{
    return &ccb->chunk_list;
}

/**
 * @brief Sets the offset and length of the content part of an encoded canonical block
 *
 * This refers to the position within the CBOR data of the actual content
 *
 * @param ccb
 * @param offset
 * @param length
 */
static inline void bplib_mpool_set_canonical_block_encoded_content_detail(bplib_mpool_canonical_block_t *ccb,
                                                                          size_t offset, size_t length)
{
    ccb->encoded_content_offset = offset;
    ccb->encoded_content_length = length;
}

/**
 * @brief Gets the length of the content part of the encoded canonical block
 *
 * @param ccb
 * @return size_t
 */
static inline size_t bplib_mpool_get_canonical_block_encoded_content_length(const bplib_mpool_canonical_block_t *ccb)
{
    return ccb->encoded_content_length;
}

/**
 * @brief Gets the offset of the content part of the encoded canonical block
 *
 * @param ccb
 * @return size_t
 */
static inline size_t bplib_mpool_get_canonical_block_encoded_content_offset(const bplib_mpool_canonical_block_t *ccb)
{
    return ccb->encoded_content_offset;
}

/**
 * @brief Gets the next block in a list of blocks
 *
 * @param cb
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_next_block(bplib_mpool_block_t *cb)
{
    return cb->next;
}

/**
 * @brief Gets the previous block in a list of blocks
 *
 * @param cb
 * @return bplib_mpool_block_t*
 */
static inline bplib_mpool_block_t *bplib_mpool_get_prev_block(bplib_mpool_block_t *cb)
{
    return cb->prev;
}

/**
 * @brief Checks if this block is part of a list
 *
 * @param list
 * @return true If this is in a list
 * @return false If the block is a singleton
 */
static inline bool bplib_mpool_is_link_attached(const bplib_mpool_block_t *list)
{
    return (list->next != list);
}

/**
 * @brief Checks if this block is a singleon
 *
 * @param list
 * @return true If the block is a singleton
 * @return false If the block is part of a list
 */
static inline bool bplib_mpool_is_link_unattached(const bplib_mpool_block_t *list)
{
    return (list->next == list);
}

/**
 * @brief Checks if this block is the head of a list
 *
 * This is both a start condition and an end condition when iterating a list
 *
 * @param list
 * @return true If the block is a head node
 * @return false If the block is not a head node
 */
static inline bool bplib_mpool_is_list_head(const bplib_mpool_block_t *list)
{
    return (list->type == bplib_mpool_blocktype_head);
}

/**
 * @brief Checks if this block is an empty list
 *
 * @param list
 * @return true If the list is empty
 * @return false If the list is not empty, nor not a list head node
 */
static inline bool bplib_mpool_is_empty_list_head(const bplib_mpool_block_t *list)
{
    return (bplib_mpool_is_list_head(list) && bplib_mpool_is_link_unattached(list));
}

/**
 * @brief Checks if the block is some form of binary data block
 *
 * @param cb
 * @return true
 * @return false
 */
static inline bool bplib_mpool_is_generic_data_block(const bplib_mpool_block_t *cb)
{
    return (cb->type == bplib_mpool_blocktype_cbor_data || cb->type == bplib_mpool_blocktype_service_object);
}

/**
 * @brief Checks if the block is indirect (a reference)
 *
 * @param cb
 * @return true
 * @return false
 */
static inline bool bplib_mpool_is_indirect_block(const bplib_mpool_block_t *cb)
{
    return (cb->type == bplib_mpool_blocktype_ref);
}

/**
 * @brief Checks if the block is any valid content type
 *
 * This indicates blocks that have actual content
 * This is any block type other than a list head or free block, reference, or
 * secondary index.
 *
 * @param cb
 * @return true
 * @return false
 */
static inline bool bplib_mpool_is_any_content_node(const bplib_mpool_block_t *cb)
{
    return (cb->type > bplib_mpool_blocktype_ref && cb->type < bplib_mpool_blocktype_max);
}

/**
 * @brief Initialize a secondary link for block indexing purposes
 *
 * A secondary link may be required for cases where there needs to be more than one way of
 * indexing data blocks, for example in a case where blocks may need to be located by
 * DTN time or node number.
 *
 * The secondary index may be passed in place of the primary index to any "cast" function,
 * and it will automatically be converted back to a pointer to the same block.
 *
 * @param base_block
 * @param secondary_link
 */
void bplib_mpool_init_secondary_link(bplib_mpool_block_t *base_block, bplib_mpool_block_t *secondary_link);

/**
 * @brief Go to the real block pointer, depending on what type of link this is
 *
 * If the block points to a secondary link structure, convert it back to the
 * primary link structure.
 *
 * If the block is a reference, then dereference it to get to the real block.
 *
 * If the block already refers to a primary link it is passed through.
 *
 * In all cases, a pointer to the original base block is returned.
 *
 * @param cb
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_obtain_base_block(bplib_mpool_block_t *cb);

/**
 * @brief Gets the capacity (maximum size) of the generic data block
 *
 * This value is currently fixed at compile time, but the actual object definition
 * is not available outside the mpool module, so this call can be used in place of sizeof()
 *
 * @param cb
 * @return size_t
 */
size_t bplib_mpool_get_generic_data_capacity(const bplib_mpool_block_t *cb);

/* basic list ops */

/**
 * @brief Sets the node to be an empty list head node
 *
 * Initialize a new list head object.
 *
 * Any existing content will be discarded, so this should typically only
 * be used on new blocks (such as a temporary list created on the stack)
 * where it is guaranteed to NOT have any valid content, and the object
 * is in an unknown/undefined state.
 *
 * To clear a list that has already been initialized once, use
 * bplib_mpool_recycle_all_blocks_in_list()
 *
 * @param link
 * @param type
 */
void bplib_mpool_init_list_head(bplib_mpool_block_t *head);

/**
 * @brief Inserts a node after the given position or list head
 *
 * Note when inserting at a list head, it is essentially a mirror image (outside looking in)
 * Therefore this call will place a node at the _beginning_ of the list (prepend)
 *
 * @param list
 * @param node
 */
void bplib_mpool_insert_after(bplib_mpool_block_t *list, bplib_mpool_block_t *node);

/**
 * @brief Inserts a node before the given position or list head
 *
 * Note when inserting at a list head, it is essentially a mirror image (outside looking in)
 * Therefore this call will place a node at the _end_ of the list (append)
 *
 * @param list
 * @param node
 */
void bplib_mpool_insert_before(bplib_mpool_block_t *list, bplib_mpool_block_t *node);

/**
 * @brief Removes a node from its list
 *
 * The node becomes a singleton.
 * It is OK to invoke this on a node which is already a singleton, it will have no effect.
 *
 * @param node
 */
void bplib_mpool_extract_node(bplib_mpool_block_t *node);

/**
 * @brief Merge two lists together
 *
 * Note that the entire content is merged, including the head node.  After using this,
 * the bplib_mpool_extract_node() should be used to remove one of the head nodes,
 * depending on which one should keep the contents.
 *
 * @param dest
 * @param src
 */
void bplib_mpool_merge_listx(bplib_mpool_block_t *dest, bplib_mpool_block_t *src);

/**
 * @brief Cast a block to a primary type
 *
 * Confirms that the block is actually a primary block (of any flavor) and returns
 * a pointer to that block, or returns NULL if not convertible to a primary block type.
 *
 * @param cb
 * @return bplib_mpool_primary_block_t*
 */
bplib_mpool_primary_block_t *bplib_mpool_cast_primary(bplib_mpool_block_t *cb);

/**
 * @brief Cast a block to a canonical type
 *
 * Confirms that the block is actually a canonical block (of any flavor) and returns
 * a pointer to that block, or returns NULL if not convertible to a canonical block type.
 *
 * @param cb
 * @return bplib_mpool_canonical_block_t*
 */
bplib_mpool_canonical_block_t *bplib_mpool_cast_canonical(bplib_mpool_block_t *cb);

/**
 * @brief Cast a block to a CBOR data type
 *
 * Confirms that the block is a CBOR data block, and returns a pointer to that block
 *
 * @param cb
 * @return bplib_mpool_generic_data_t*
 */
void *bplib_mpool_cast_cbor_data(bplib_mpool_block_t *cb);

/**
 * @brief Cast a block to generic user data type
 *
 * User blocks require a match on on the "magic number", which may be a random 32-bit integer.
 * This is intended as a data integrity check to confirm the block is indeed the correct flavor.
 *
 * @param cb
 * @param required_magic
 * @return bplib_mpool_generic_data_t*
 */
void *bplib_mpool_cast_generic_data(bplib_mpool_block_t *cb, uint32_t required_magic);

/**
 * @brief Cast a block to a flow type
 *
 * @param cb
 * @return bplib_mpool_flow_t*
 */
bplib_mpool_flow_t *bplib_mpool_cast_flow(bplib_mpool_block_t *cb);

/**
 * @brief Convert a user data pointer back to the parent block pointer
 *
 * User blocks require a match on on the "magic number", which may be a random 32-bit integer.
 * This is intended as a data integrity check to confirm the block is indeed the correct flavor.
 *
 * @param ptr
 * @param required_magic
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_get_generic_block_from_pointer(void *ptr, uint32_t required_magic);

void bplib_mpool_set_cbor_content_size(bplib_mpool_block_t *cb, size_t user_content_size);

/**
 * @brief Gets the size of the user buffer associated with a data block
 *
 * For a CBOR block this is the length of the CBOR data within this block.  For a user
 * data block, this refers to the size of the actual user object stored here.
 *
 * @param ceb
 * @return size_t
 */
size_t bplib_mpool_get_user_content_size(const bplib_mpool_block_t *ceb);

/**
 * @brief Reads the reference count of the object
 *
 * Primary and canonical blocks have a reference count, allowing them to be quickly
 * duplicated (such as to keep one copy in a storage, while another sent to a CLA) without
 * actually copying the data itself.  The content blocks will be kept in the pool until
 * the refcount reaches zero, and then the memory blocks will be recycled.
 *
 * If this returns "1" it means that the given block pointer is currently the only reference to
 * this particular block (that is, it is not also present in an interface queue somewhere else)
 *
 * @param cb
 * @return size_t
 */
size_t bplib_mpool_get_read_refcount(const bplib_mpool_block_t *cb);

/**
 * @brief Allocate a new primary block
 *
 * @param pool
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_alloc_primary_block(bplib_mpool_t *pool);

/**
 * @brief Allocate a new canonical block
 *
 * @param pool
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_alloc_canonical_block(bplib_mpool_t *pool);

bplib_mpool_block_t *bplib_mpool_alloc_flow(bplib_mpool_t *pool, uint32_t magic_number, size_t req_capacity);

/**
 * @brief Allocate a new CBOR data block
 *
 * @param pool
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_alloc_cbor_data_block(bplib_mpool_t *pool);

/**
 * @brief Allocate a new user data block
 *
 * Note that the maximum block capacity is set at compile time.  This limits the size
 * of user data objects that can be stored by this mechanism.
 *
 * @param pool
 * @param magic_number
 * @param req_capacity
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_alloc_generic_block(bplib_mpool_t *pool, uint32_t magic_number, size_t req_capacity);

/**
 * @brief Append CBOR data to the given list
 *
 * @param head
 * @param blk
 */
void bplib_mpool_append_cbor_block(bplib_mpool_block_t *head, bplib_mpool_block_t *blk);

/**
 * @brief Append a canonical block to the bundle
 *
 * @param cpb
 * @param ccb
 */
void bplib_mpool_store_canonical_block(bplib_mpool_primary_block_t *cpb, bplib_mpool_block_t *ccb);

/**
 * @brief Recycle a single block which is no longer needed
 *
 * @param pool
 * @param blk
 */
void bplib_mpool_recycle_block(bplib_mpool_t *pool, bplib_mpool_block_t *blk);

/**
 * @brief Recycle an entire list of blocks which are no longer needed
 *
 * @param pool
 * @param list
 */
void bplib_mpool_recycle_all_blocks_in_list(bplib_mpool_t *pool, bplib_mpool_block_t *list);

/**
 * @brief Process every item in the list in sequential order
 *
 * The callback function will be invoked for every item in the list, except for the head node.
 *
 * If "always_remove" is true, the item will be removed from the list prior to invoking the call.
 * In this case, the callback function must guarantee to place the block onto another list (or
 * some other tracking facility) to not leak blocks.
 *
 * @param list
 * @param always_remove
 * @param callback_fn
 * @param callback_arg
 * @return int Number of items that were in the list
 */
int bplib_mpool_foreach_item_in_list(bplib_mpool_block_t *list, bool always_remove,
                                     bplib_mpool_callback_func_t callback_fn, void *callback_arg);

/**
 * @brief Process (forward) all active flows in the system
 *
 * @param pool
 * @param callback_fn
 * @param callback_arg
 * @return int
 */
int bplib_mpool_process_all_flows(bplib_mpool_t *pool, bplib_mpool_callback_func_t callback_fn, void *callback_arg);

/**
 * @brief Append a bundle to the given queue (flow)
 *
 * @param subq
 * @param cpb
 */
void bplib_mpool_append_subq_bundle(bplib_mpool_subq_t *subq, bplib_mpool_block_t *cpb);

/**
 * @brief Mark a given flow as active
 *
 * This marks it so it will be processed during the next call to forward data
 *
 * @param pool
 * @param flow
 */
void bplib_mpool_mark_flow_active(bplib_mpool_t *pool, bplib_mpool_flow_t *flow);

/**
 * @brief Get the next bundle from the given queue (flow)
 *
 * @param subq
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_shift_subq_bundle(bplib_mpool_subq_t *subq);

/**
 * @brief Drop all encode data (CBOR) from a primary block
 *
 * This would be used if the logical data has been changed, necessitating re-encoding of the data.
 * The old data blocks are returned to the pool, and the updated contents can replace it.
 *
 * @param pool
 * @param cpb
 */
void bplib_mpool_pri_drop_encode_data(bplib_mpool_t *pool, bplib_mpool_primary_block_t *cpb);

/**
 * @brief Drop all encode data (CBOR) from a canonical block
 *
 * This would be used if the logical data has been changed, necessitating re-encoding of the data.
 * The old data blocks are returned to the pool, and the updated contents can replace it.
 *
 * @param pool
 * @param ccb
 */
void bplib_mpool_canonical_drop_encode_data(bplib_mpool_t *pool, bplib_mpool_canonical_block_t *ccb);

/**
 * @brief Creates a lightweight reference to the data block
 *
 * This creates an opaque pointer that can be stored into another user object
 * A light reference increses the reference count on the target object, but does
 * not consume any pool memory directly itself.  It will prevent the target
 * object from being garbage collected.
 *
 * References must be explicitly tracked and released by the user when
 * no longer needed, using bplib_mpool_release_light_reference()
 *
 * References may also be duplicated via bplib_mpool_duplicate_light_reference()
 *
 * After this call, the passed in blk becomes managed by the pool.
 *
 * @note If this function returns non-NULL, the calling application should no longer directly
 * use the blk pointer that was passed in.  It should only use the reference pointers.
 *
 * @param pool
 * @param blk
 * @return bplib_mpool_refptr_t*
 */
bplib_mpool_refptr_t *bplib_mpool_make_dynamic_object(bplib_mpool_t *pool, bplib_mpool_block_t *blk);

bplib_mpool_refptr_t *bplib_mpool_duplicate_light_reference(bplib_mpool_refptr_t *refptr);

/**
 * @brief Release a lightweight reference
 *
 * This must be invoked once a reference that was previously created by bplib_mpool_make_dynamic_object()
 * or bplib_mpool_duplicate_light_reference() is no longer needed by the software application.
 *
 * This decrements the reference count, and if the reference count reaches 0, it also recycles the
 * original object.
 *
 * @param pool
 * @param refptr
 */
void bplib_mpool_release_light_reference(bplib_mpool_t *pool, bplib_mpool_refptr_t *refptr);

/**
 * @brief Creates a separate block reference to the data block
 *
 * A block is allocated from the pool which refers to the original data block, and can stand in place
 * of the actual data block wherever a data block is expected, such as for queuing and storage.
 *
 * A reference of this type does not need to be explicitly released by the user, as it
 * will be automatically  released when the block is recycled via bplib_mpool_recycle_block()
 * or bplib_mpool_recycle_all_blocks_in_list()
 *
 * @note This increments the refcount, so the calling application should call
 * bplib_mpool_release_light_reference() on the original ref if it does not keep it.
 *
 * @param pool
 * @param blk
 * @param notify_on_discard
 * @param notify_arg
 * @return bplib_mpool_block_t*
 */
bplib_mpool_block_t *bplib_mpool_make_block_ref(bplib_mpool_t *pool, bplib_mpool_refptr_t *refptr,
                                                bplib_mpool_callback_func_t notify_on_discard, void *notify_arg);

bplib_mpool_refptr_t *bplib_mpool_duplicate_block_reference(bplib_mpool_block_t *rblk);

/**
 * @brief Copy an entire chain of encoded blocks to a target buffer
 *
 * @param list
 * @param out_ptr
 * @param max_out_size
 * @param seek_start
 * @param max_count
 * @return size_t
 */
size_t bplib_mpool_copy_block_chain(bplib_mpool_block_t *list, void *out_ptr, size_t max_out_size, size_t seek_start,
                                    size_t max_count);

/**
 * @brief Run basic maintenance on the memory pool
 *
 * Mainly, this performs any garbage-collection tasks on recycled memory blocks, returning the memory
 * back to the free pool so it can be used again.
 *
 * @param pool
 */
void bplib_mpool_maintain(bplib_mpool_t *pool);

/**
 * @brief Creates a memory pool object using a preallocated memory block
 *
 * @param pool_mem  Pointer to pool memory
 * @param pool_size Size of pool memory
 * @return bplib_mpool_t*
 */
bplib_mpool_t *bplib_mpool_create(void *pool_mem, size_t pool_size);

/* DEBUG/TEST verification routines */

void bplib_mpool_debug_scan(bplib_mpool_t *pool);
void bplib_mpool_debug_print_queue_stats(bplib_mpool_block_t *list, const char *label);

#endif /* V7_MPOOL_H */
