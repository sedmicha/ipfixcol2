#ifndef IPX_PROFILES_H
#define IPX_PROFILES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <ipfixcol2/api.h>

struct ipx_profile_channel {
    size_t channel_idx;
    size_t bit_offset;

    char *name;

    size_t sources_cnt;
    struct ipx_profile_channel **sources;

    char *filter;

    struct ipx_profile *profile;

    size_t listeners_cnt;
    struct ipx_profile_channel **listeners;
};

enum ipx_profile_type {
    IPX_PT_UNASSIGNED,
    IPX_PT_NORMAL,
    IPX_PT_SHADOW
};

struct ipx_profile {
    size_t bit_offset;

    enum ipx_profile_type type; 
    char *name;
    char *directory;
    
    size_t subprofile_idx;
    struct ipx_profile *parent;
    
    size_t channels_cnt;
    struct ipx_profile_channel **channels;

    size_t subprofiles_cnt;
    struct ipx_profile **subprofiles;
};

struct ipx_profile_tree {
    struct ipx_profile *root;
    
    size_t profiles_cnt;
    size_t channels_cnt;
};


struct ipx_pmatcher_result {
    uint64_t *channels;
    uint64_t *profiles;
};

typedef struct ipx_pmatcher ipx_pmatcher_t;

union ipx_pevents_target {
    struct ipx_profile *profile;
    struct ipx_profile_channel *channel;
};

struct ipx_pevents_ctx {
    union {
        struct ipx_profile *profile;
        struct ipx_profile_channel *channel;
    };
    void *local;
    void *global;
};

typedef void ipx_pevents_create_cb(struct ipx_pevents_ctx *ctx);

typedef void ipx_pevents_delete_cb(struct ipx_pevents_ctx *ctx);

typedef void ipx_pevents_update_cb(struct ipx_pevents_ctx *ctx, union ipx_pevents_target old);

typedef void ipx_pevents_data_cb(struct ipx_pevents_ctx *ctx, void *record);

struct ipx_pevents_cb_set {
    ipx_pevents_create_cb *on_create;
    ipx_pevents_delete_cb *on_delete;
    ipx_pevents_update_cb *on_update;
    ipx_pevents_data_cb *on_data;
};

typedef struct ipx_pevents ipx_pevents_t;

IPX_API int
ipx_profiles_parse_xml(const char *file_path, struct ipx_profile_tree **profile_tree);

IPX_API void
ipx_profiles_destroy(struct ipx_profile_tree *profile_tree);

IPX_API int
ipx_profiles_copy(struct ipx_profile_tree *profile_tree,
                  struct ipx_profile_tree **profile_tree_copy);

IPX_API const char *
ipx_profiles_get_xml_path();

IPX_API struct ipx_pmatcher *
ipx_pmatcher_create(struct ipx_profile *live, fds_iemgr_t *iemgr);

IPX_API void
ipx_pmatcher_match(struct ipx_pmatcher *matcher, void *data,
                     struct ipx_pmatcher_result result);

IPX_API void
ipx_pmatcher_destroy(struct ipx_pmatcher *matcher);

IPX_API ipx_pevents_t *
ipx_pevents_create(struct ipx_pevents_cb_set profile_cbs,
                   struct ipx_pevents_cb_set channel_cbs);

IPX_API int
ipx_pevents_process(ipx_pevents_t *events, struct ipx_profile_tree *ptree, 
                    struct ipx_pmatcher_result result, void *record);

IPX_API void
ipx_pevents_destroy(ipx_pevents_t *events);

IPX_API void
ipx_pevents_global_set(ipx_pevents_t *events, void *global);

IPX_API void
ipx_pevents_global_get(ipx_pevents_t *events);


#ifdef __cplusplus
}
#endif

#endif // IPX_PROFILES_H