/**
 * \file src/core/profiles/pevents.c
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz> 
 * \brief Profiling events
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <ipfixcol2.h>
#include "common.h"

struct pevents_profile_item {
    struct ipx_profile *ptr;
    void *local;
};

struct pevents_channel_item {
    struct ipx_profile_channel *ptr;
    void *local;
};

struct pevents_mapping {
    struct ipx_profile_tree *ptree_copy;

    size_t profiles_cnt;
    struct pevents_profile_item *profiles;

    size_t channels_cnt;
    struct pevents_channel_item *channels;
};

struct pevents_diff {
    enum pevents_diff_action {
        // This also specifies the order in which the callbacks for the actions are called
        CHAN_DELETE,
        PROF_DELETE,
        CHAN_UPDATE,
        PROF_UPDATE,
        PROF_CREATE,
        CHAN_CREATE,
    } action;
    union {
        struct {
           struct ipx_profile *old_profile; 
           struct ipx_profile *new_profile; 
        };
        struct {
           struct ipx_profile_channel *old_channel; 
           struct ipx_profile_channel *new_channel; 
        };
    };
};

struct pevents_diff_list {
    size_t diffs_cnt;
    struct pevents_diff *diffs;
};

struct ipx_pevents {
    struct ipx_profile_tree *ptree;

    struct pevents_mapping mapping;

    struct ipx_pevents_cb_set profile_cbs;
    struct ipx_pevents_cb_set channel_cbs;

    void *global;
};

// static const union ipx_pevents_target NULL_TARGET = (union ipx_pevents_target) {}; 

/**
 * Walk the profiles tree and assign profile/channel pointers to their 
 * corresponding pevents items
 */
static void
map_items(struct pevents_mapping *mapping, struct ipx_profile *prof)
{
    mapping->profiles[prof->bit_offset].ptr = prof;
    for (size_t i = 0; i < prof->channels_cnt; i++) {
        struct ipx_profile_channel *chan = prof->channels[i];
        mapping->channels[chan->bit_offset].ptr = prof->channels[i]; 
    }
    for (size_t i = 0; i < prof->subprofiles_cnt; i++) {
        map_items(mapping, prof->subprofiles[i]); 
    }
}

static void
destroy_mapping(struct pevents_mapping *mapping)
{
    ipx_profiles_destroy(mapping->ptree_copy);
    free(mapping->channels);
    free(mapping->profiles);
}

/**
 * Create empty pevents items for the profiles/channels
 */
static int
create_mapping(struct pevents_mapping *mapping, struct ipx_profile_tree *ptree)
{
    mapping->channels = calloc(ptree->channels_cnt, sizeof(struct pevents_channel_item));
    if (mapping->channels == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }
    mapping->channels_cnt = ptree->channels_cnt;

    mapping->profiles = calloc(ptree->profiles_cnt, sizeof(struct pevents_profile_item));
    if (mapping->profiles == NULL) {
        PROFILES_MEMORY_ERROR();
        free(mapping->channels);
        return IPX_ERR_NOMEM;
    }
    mapping->profiles_cnt = ptree->profiles_cnt;
    
    int rc = ipx_profiles_copy(ptree, &mapping->ptree_copy);
    if (rc != IPX_OK) {
        free(mapping->channels);
        free(mapping->profiles);
        return rc;
    }

    map_items(mapping, ptree->root);
    
    return IPX_OK;
}

//static pevents_item *
//find_profile_by_name(struct pevents_mapping *mapping, const char *name)
//{
//    for (size_t i = 0; i < mapping->profiles_cnt; i++) {
//        if (strcmp())
//    }
//}

static inline bool
profile_name_matches(struct pevents_profile_item *a, struct pevents_profile_item *b)
{
    return a->ptr != NULL
        && b->ptr != NULL
        && strcmp(a->ptr->name, b->ptr->name) == 0; 
}

static inline bool
channel_name_matches(struct pevents_profile_item *a, struct pevents_profile_item *b)
{
    return a->ptr != NULL
        && b->ptr != NULL
        && strcmp(a->ptr->name, b->ptr->name) == 0; 
}

/**
 * Find a matching profile in the old mapping for the profile from the new mapping
 */
static struct pevents_item *
map_new_to_old_profile(struct pevents_mapping *old_mapping,
                       struct pevents_profile_item *new_profile)
{
    // first try if it isn't on the same index
    size_t offset = new_profile->ptr->bit_offset;
    if (offset < old_mapping->profiles_cnt) {
        if (profile_name_matches(new_profile, &old_mapping->profiles[offset])) {
            return &old_mapping->profiles[offset];
        }
    }

    // it's not on the same index so search by name
    for (size_t i = 0; i < old_mapping->profiles_cnt; i++) {
        if (profile_name_matches(new_profile, &old_mapping->profiles[i])) {
            return &old_mapping->profiles[i];
        }
    }

    return NULL;
}

/**
 * Find a matching channel in the old mapping for the channel from the new mapping
 */
static struct pevents_item *
map_new_to_old_channel(struct pevents_mapping *old_mapping,
                       struct pevents_profile_item *old_profile,
                       struct pevents_channel_item *new_channel)
{
    // first try if it isn't on the same index
    size_t offset = new_channel->ptr->bit_offset;
    if (offset < old_mapping->profiles_cnt) {
        if (channel_name_matches(new_channel, &old_mapping->profiles[offset])) {
            return &old_mapping->profiles[offset];
        }
    }

    // it's not on the same index so search by name in the old profile
    for (size_t i = 0; i < old_profile->ptr->channels_cnt; i++) {
        struct pevents_item *ch = 
            &old_mapping->channels[old_profile->ptr->channels[i]->bit_offset];
        if (channel_name_matches(new_channel, ch)) {
            return ch;
        }
    }

    return NULL;
}

/**
 * Add diff to a list of diffs
 */
static struct pevents_diff *
add_diff(struct pevents_diff_list *diff_list)
{
    struct pevents_diff *diff = 
        array_push(&diff_list->diffs, &diff_list->diffs_cnt, sizeof(struct pevents_diff));
    if (diff == NULL) {
        PROFILES_MEMORY_ERROR();
        return NULL;
    }
    *diff = (struct pevents_diff) {};
    return diff;
}

/**
 * Check if profile config changed
 */
static inline bool
has_profile_changed(struct pevents_profile_item *a, struct pevents_profile_item *b)
{
    //TODO:?
    return strcmp(a->ptr->directory, b->ptr->directory) != 0
        || a->ptr->type != b->ptr->type;
}

/**
 * Check if channel config changed
 */
static inline bool
has_channel_changed(struct pevents_channel_item *a, struct pevents_channel_item *b)
{
    //TODO:?
    return strcmp(a->ptr->filter, b->ptr->filter) != 0;
}

/**
 * Sorting function for diff list sorting
 */
static int
diff_list_sort_compare(struct pevents_diff *a, struct pevents_diff *b)
{
    // delete first, then update, then create
    // channels first, then profiles for delete and update
    // profiles first, then channels for create
    // if everything above is the same then sort by the bit offset

    if (a->action < b->action) {
        return -1;
    } else if (a->action > b->action) {
        return 1;
    }

    switch (a->action) {
    case CHAN_DELETE:
        return a->old_channel->bit_offset - b->old_channel->bit_offset;
    case PROF_DELETE:
        return a->old_profile->bit_offset - b->old_profile->bit_offset;
    case CHAN_UPDATE:
        return a->old_channel->bit_offset - b->old_channel->bit_offset;
    case PROF_UPDATE:
        return a->old_profile->bit_offset - b->old_profile->bit_offset;
    case CHAN_CREATE:
        return a->new_channel->bit_offset - b->new_channel->bit_offset;
    case PROF_CREATE:
        return a->new_profile->bit_offset - b->new_profile->bit_offset;
    }
}

/**
 * Update the mappings with the new ones and fill a diff list with the changes
 */ 
static int
update_mappings(struct pevents_mapping *old_mapping, struct pevents_mapping *new_mapping, 
                struct pevents_diff_list *diff_list)
{
    memset(diff_list, 0, sizeof(struct pevents_diff_list));
    struct pevents_diff *diff;

    // go through the mapping items and build a list of differences
    for (size_t i = 0; i < new_mapping->profiles_cnt; i++) {
        struct pevents_profile_item *new_prof = &new_mapping->profiles[i];
        struct pevents_profile_item *old_prof = map_new_to_old_profile(old_mapping, new_prof);

        if (old_prof == NULL) {
            // no mapping to old profile found -> newly created profile
            
            // add create event for the profile
            diff = add_diff(diff_list);
            if (diff == NULL) {
                return IPX_ERR_NOMEM;
            }
            diff->action = PROF_CREATE;
            diff->new_profile = new_prof->ptr;

            // add create events for all its channels too
            for (size_t j = 0; j < new_prof->ptr->channels_cnt; j++) {
                diff = add_diff(diff_list);
                if (diff == NULL) {
                    return IPX_ERR_NOMEM;
                }
                diff->action = CHAN_CREATE;
                diff->new_channel = 
                    new_mapping->channels[new_prof->ptr->channels[j]->bit_offset].ptr;
            }

        } else {
            // mapping found -> check for changes between old and new
            if (has_profile_changed(old_prof, new_prof)) {
                // add update event for the profile
                diff = add_diff(diff_list);
                if (diff == NULL) {
                    return IPX_ERR_NOMEM;
                }
                diff->action = PROF_UPDATE;
                diff->old_profile = old_prof->ptr;
                diff->new_profile = new_prof->ptr;
            }

            // check the profile channels and try to map them to the old ones too
            // also migrate the local data to the new profile and clear the old item
            for (size_t j = 0; j < new_prof->ptr->channels_cnt; j++) {
                size_t offset = new_prof->ptr->channels[j]->bit_offset;
                struct pevents_channel_item *new_chan = &new_mapping->channels[offset];
                struct pevents_channel_item *old_chan = map_new_to_old_channel(old_mapping, old_prof, new_chan);
                if (old_chan == NULL) {
                    // no mapping found -> new channel
                    // add create event for the channel
                    diff = add_diff(diff_list);
                    if (diff == NULL) {
                        return IPX_ERR_NOMEM;
                    }
                    diff->action = CHAN_CREATE;
                    diff->new_channel = new_chan->ptr;
                } else {
                    // mapping found -> check for changes
                    if (has_channel_changed(old_chan, new_chan)) {        
                        // add change event for the channel
                        diff = add_diff(diff_list);
                        if (diff == NULL) {
                            return IPX_ERR_NOMEM;
                        }
                        diff->action = CHAN_UPDATE;
                        diff->old_channel = old_chan->ptr;
                        diff->new_channel = new_chan->ptr;
                    }
                    // migrate the data and clear the old channel
                    new_chan->local = old_chan->local;
                    old_chan->local = NULL;
                    old_chan->ptr = NULL;
                }
            }

            // migrate the data and clear the old profile
            new_prof->local = old_prof->local;
            old_prof->local = NULL;
            old_prof->ptr = NULL;
        }
    }

    // go through the old profiles and channels to find those that haven't been cleared,
    // those must have been deleted
    for (size_t i = 0; i < old_mapping->profiles_cnt; i++) {
        struct pevents_profile_item *p = &old_mapping->profiles[i];
        if (p->ptr == NULL && p->local == NULL) {
            // has been cleared, nothing to do
            continue;
        }
        // add delete event for the profile
        diff = add_diff(diff_list);
        if (diff == NULL) {
            return IPX_ERR_NOMEM;
        }
        diff->action = PROF_DELETE;
        diff->old_profile = p->ptr;
    }
    for (size_t i = 0; i < old_mapping->channels_cnt; i++) {
        struct pevents_profile_item *ch = &old_mapping->channels[i];
        if (ch->ptr == NULL) {
            // has been cleared, nothing to do
            continue;
        }
        // add delete event for the profile
        diff = add_diff(diff_list);
        if (diff == NULL) {
            return IPX_ERR_NOMEM;
        }
        diff->action = CHAN_DELETE;
        diff->old_profile = ch->ptr;
    }

    // sort the diff list events
    qsort(diff_list->diffs, diff_list->diffs_cnt, sizeof(struct pevents_diff), diff_list_sort_compare);

    return IPX_OK;
}


/**
 * Call data callbacks for the profiles/channels that were matched in the bitset
 */
static void
call_data_callbacks(struct ipx_pevents *pevents, struct ipx_pmatcher_result result, void *record)
{
    if (pevents->profile_cbs.on_data != NULL) {
        for (size_t i = 0; i < pevents->mapping.profiles_cnt; i++) {
            if (test_bit(result.profiles, i)) {
                struct pevents_profile_item *prof = &pevents->mapping.profiles[i];
                struct ipx_pevents_ctx ctx;
                ctx.ptr.profile = prof->ptr;
                ctx.user.local = prof->local;
                ctx.user.global = pevents->global;
                pevents->profile_cbs.on_data(&ctx, record);
            }
        }
    }

    if (pevents->channel_cbs.on_data != NULL) {
        for (size_t i = 0; i < pevents->mapping.channels_cnt; i++) {
            // printf("XXXX: channel %d: %s\n", i, test_bit(result.channels, i) ? "yes" : "no");
            if (test_bit(result.channels, i)) {
                struct pevents_channel_item *chan = &pevents->mapping.channels[i];
                struct ipx_pevents_ctx ctx;
                ctx.ptr.channel = chan->ptr;
                ctx.user.local = chan->local;
                ctx.user.global = pevents->global;
                pevents->channel_cbs.on_data(&ctx, record);
            }
        }
    }
}

/**
 * Reconfigure the pevents with new profiles tree
 */
static int
reconfigure(struct ipx_pevents *pevents, struct ipx_profile_tree *ptree)
{
    int rc;

    // create new mappings
    struct pevents_mapping new_mapping;
    rc = create_mapping(&new_mapping, ptree);
    if (rc != IPX_OK) {
        return rc;
    }

    // update the mappings and return a diff list
    struct pevents_diff_list diff_list;
    rc = update_mappings(&pevents->mapping, &new_mapping, &diff_list);
    if (rc != IPX_OK) {
        destroy_mapping(&new_mapping);
        return rc;
    }
    
    // call the appropriate callbacks 
    for (size_t i = 0; i < diff_list.diffs_cnt; i++) {
        struct pevents_diff *diff = &diff_list.diffs[i];
        struct ipx_pevents_ctx ctx;
        ctx.user.global = pevents->global; 
        union ipx_pevents_target target;
        
        switch (diff->action) {
        case CHAN_DELETE:
            ctx.ptr.channel = diff->old_channel;
            ctx.user.local = pevents->mapping.channels[diff->old_channel->bit_offset].local;
            if (pevents->channel_cbs.on_delete != NULL) {
                pevents->channel_cbs.on_delete(&ctx);
            }
            break;
        case PROF_DELETE:
            ctx.ptr.profile = diff->old_profile;
            ctx.user.local = pevents->mapping.profiles[diff->old_profile->bit_offset].local;
            if (pevents->profile_cbs.on_delete != NULL) {
                pevents->profile_cbs.on_delete(&ctx);
            }
            break;
        case CHAN_UPDATE:
            ctx.ptr.channel = diff->new_channel;
            ctx.user.local = new_mapping.channels[diff->new_channel->bit_offset].local;
            target.channel = diff->old_channel;
            if (pevents->channel_cbs.on_update != NULL) {
                pevents->channel_cbs.on_update(&ctx, target);
            }
            break;
        case PROF_UPDATE:
            ctx.ptr.profile = diff->new_profile;
            ctx.user.local = new_mapping.profiles[diff->new_profile->bit_offset].local;
            target.profile = diff->old_profile;
            if (pevents->profile_cbs.on_update != NULL) {
                pevents->profile_cbs.on_update(&ctx, target);
            }
            break;
        case CHAN_CREATE:
            ctx.ptr.channel = diff->new_channel;
            if (pevents->channel_cbs.on_create != NULL) {
                void *local = pevents->channel_cbs.on_create(&ctx);
                new_mapping.channels[diff->new_channel->bit_offset].local = local;
            }
            break;
        case PROF_CREATE:
            ctx.ptr.profile = diff->new_profile;
            if (pevents->profile_cbs.on_create != NULL) {
                void *local = pevents->profile_cbs.on_create(&ctx);
                new_mapping.profiles[diff->new_profile->bit_offset].local = local;
            }
            break;
        default:
            assert(0 && "invalid diff action");
        }
    }

    // destroy the old mappings and assign the new ones
    destroy_mapping(&pevents->mapping);
    pevents->mapping = new_mapping;
    pevents->ptree = ptree;

    return IPX_OK;
}

ipx_pevents_t *
ipx_pevents_create(struct ipx_pevents_cb_set profile_cbs,
                   struct ipx_pevents_cb_set channel_cbs)
{
    struct ipx_pevents *pevents = calloc(1, sizeof(struct ipx_pevents));
    if (pevents == NULL) {
        return NULL;
    }
    pevents->profile_cbs = profile_cbs;
    pevents->channel_cbs = channel_cbs;
    return pevents;
}

int
ipx_pevents_process(ipx_pevents_t *pevents, 
                    void *record, void *ext_data)
{

    struct ipx_profiles_ext *ext = ext_data;

    struct ipx_profiles_tree *ptree = ext->ptree;
    struct ipx_pmatcher_result result = ipx_profiles_get_matches(ext);

    if (pevents->ptree != ptree) {
        int rc = reconfigure(pevents, ptree);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    call_data_callbacks(pevents, result, record);

    return IPX_OK;
}

void
ipx_pevents_destroy(ipx_pevents_t *pevents)
{
    destroy_mapping(&pevents->mapping);
    free(pevents);
}

void
ipx_pevents_global_set(ipx_pevents_t *pevents, void *global)
{
    pevents->global = global;
}

void
ipx_pevents_global_get(ipx_pevents_t *pevents)
{
    return pevents->global;
}

void
ipx_pevents_for_each(ipx_pevents_t *pevents, ipx_pevents_fn *prof_fn, ipx_pevents_fn *chan_fn)
{
    struct pevents_mapping *mapping = &pevents->mapping;

    if (prof_fn != NULL) {
        for (size_t i = 0; i < pevents->mapping.profiles_cnt; i++) {
            struct ipx_pevents_ctx ctx;
            ctx.user.global = pevents->global;
            ctx.user.local = mapping->profiles[i].local;
            ctx.ptr.profile = mapping->profiles[i].ptr;
            prof_fn(&ctx);
        }
    }

    if (chan_fn != NULL) {
        for (size_t i = 0; i < pevents->mapping.channels_cnt; i++) {
            struct ipx_pevents_ctx ctx;
            ctx.user.global = pevents->global;
            ctx.user.local = mapping->channels[i].local;
            ctx.ptr.channel = mapping->channels[i].ptr;
            chan_fn(&ctx);
        }
    }
}
