#include <ipfixcol2.h>
#include <libfds.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"

struct matcher_channel {
    fds_ipfix_filter_t *filter;
    uint64_t source_mask;
    int bit_offset;
};

struct matcher_profile {
    struct matcher_profile *parent;
    uint64_t matched_channels;
    int bit_offset;

    int channels_cnt;
    struct matcher_channel *channels;

    int children_cnt;
    struct matcher_profile *children;
};

struct ipx_pmatcher {
    struct matcher_profile root;
    
    struct {
        fds_iemgr_t *iemgr;
        void *data;
        struct ipx_pmatcher_result result;
    } aux;
};

/**
 * Construct matcher channel from the profile tree channel
 */
static int
make_matcher_channel(struct ipx_pmatcher *matcher, struct ipx_profile_channel *chan, struct matcher_channel *mchan)
{
    // calculate the source mask
    assert(chan->sources_cnt < 64);
    mchan->source_mask = 0;
    for (int i = 0; i < chan->sources_cnt; i++) {
        int bit_offset = chan->sources[i]->bit_offset;
        mchan->source_mask |= 1 << bit_offset;
    }
    mchan->bit_offset = chan->bit_offset;

    //TODO: compile the filter
    int rc = fds_ipfix_filter_create(&mchan->filter, matcher->aux.iemgr, chan->filter);
    if (rc != FDS_OK) {
        PROFILES_ERROR("filter create failed: %s", fds_ipfix_filter_get_error(mchan->filter));
        return rc;
    }
    
    return IPX_OK;
}

/**
 * Construct matcher profile from the profile tree profile
 */
static int
make_matcher_profile(struct ipx_pmatcher *matcher, struct ipx_profile *prof, struct matcher_profile *mprof)
{
    // make matcher channels for profile channels
    mprof->channels_cnt = prof->channels_cnt; 
    mprof->bit_offset = prof->bit_offset;
    mprof->channels = calloc(prof->channels_cnt, sizeof(struct matcher_channel));
    if (mprof->channels == NULL) {
        return IPX_ERR_NOMEM;
    }
    for (int i = 0; i < prof->channels_cnt; i++) {
        int rc = make_matcher_channel(matcher, prof->channels[i], &mprof->channels[i]);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    // make matcher profiles for subprofiles
    mprof->children_cnt = prof->subprofiles_cnt;
    mprof->children = calloc(prof->subprofiles_cnt, sizeof(struct matcher_profile));
    for (int i = 0; i < prof->subprofiles_cnt; i++) {
        mprof->children[i].parent = mprof;
        int rc = make_matcher_profile(matcher, prof->subprofiles[i], &mprof->children[i]);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    return IPX_OK;
}

/**
 * Destroy matcher profile and all its subprofiles and channels, recursively
 */
static void
destroy_matcher_profile(struct matcher_profile *mprof)
{
    for (size_t i = 0; i < mprof->children_cnt; i++) {
        destroy_matcher_profile(&mprof->children[i]);
    }
    for (size_t i = 0; i < mprof->channels_cnt; i++) {
        fds_ipfix_filter_destroy(mprof->channels[i].filter);
    }
    free(mprof->channels);
    free(mprof->children);
}

/**
 * Destroy the matcher
 */
void
ipx_pmatcher_destroy(struct ipx_pmatcher *matcher)
{
    destroy_matcher_profile(&matcher->root);
    free(matcher);
}

/**
 * Create matcher from profiles tree
 */
struct ipx_pmatcher *
ipx_pmatcher_create(struct ipx_profile *live, fds_iemgr_t *iemgr)
{
    struct ipx_pmatcher *matcher = calloc(1, sizeof(struct ipx_pmatcher));
    matcher->aux.iemgr = iemgr;
    int rc = make_matcher_profile(matcher, live, &matcher->root);
    if (rc != IPX_OK) {
        ipx_pmatcher_destroy(matcher);
        return NULL;
    }
    return matcher;
}

/**
 * Check if the channel source data passed
 */
static inline bool
source_matched(struct matcher_profile *prof, struct matcher_channel *chan)
{
    return prof->parent == NULL
        || (prof->parent->matched_channels & chan->source_mask) == 0;
}

/**
 * Check if data record passes the IPFIX filter
 */
static inline bool
filter_passes(struct ipx_pmatcher *matcher, struct matcher_channel *chan)
{
    return fds_ipfix_filter_eval(chan->filter, matcher->aux.data);
}

/**
 * Match the data against a matcher profile and its channels, recursively match
 * all the subprofiles too
 */
static void
match_profile(struct ipx_pmatcher *matcher, struct matcher_profile *prof)
{
    // match channels
    bool any_match = false;
    for (size_t i = 0; i < prof->channels_cnt; i++) {
        struct matcher_channel *chan = &prof->channels[i];
        if (source_matched(prof, chan) && filter_passes(matcher, chan)) {
            set_bit(&prof->matched_channels, i);
            set_bit(matcher->aux.result.channels, chan->bit_offset);
            any_match = true;
        } else {
            clear_bit(&prof->matched_channels, i);
            clear_bit(matcher->aux.result.channels, chan->bit_offset);
        }
    }

    // if any of the channels matched then the profile matched too
    if (any_match) {
        set_bit(matcher->aux.result.profiles, prof->bit_offset);
    } else {
        clear_bit(matcher->aux.result.profiles, prof->bit_offset);
    }

    // proceed to the children
    for (size_t i = 0; i < prof->children_cnt; i++) {
        match_profile(matcher, &prof->children[i]);
    }

}

/**
 * Process IPFIX record data and return a bitmask result indicating which 
 * channels and profiles matched
 */
void
ipx_pmatcher_match(struct ipx_pmatcher *matcher, void *data,
                   struct ipx_pmatcher_result result)
{
    matcher->aux.data = data;
    matcher->aux.result = result;
    match_profile(matcher, &matcher->root);
}
