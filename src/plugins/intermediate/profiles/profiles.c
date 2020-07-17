#include <ipfixcol2.h>
#include <stdio.h>
#include "config.h"

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    .type = IPX_PT_INTERMEDIATE,
    .name = "profiles",
    .dsc = "Data record profiling plugin",
    .flags = 0,
    .version = "0.0.1",
    .ipx_min = "2.0.0"
};

struct plugin_data {
    struct ipx_profile_tree *ptree;
    ipx_pmatcher_t *pmatcher;
    ipx_ctx_ext_t *ext;
    struct config *config;
};


void
print_profiles(struct ipx_profile *profile)
{
    int indent = 0;
    struct ipx_profile *p = profile;
    while (p->parent != NULL) {
        indent++;
        p = p->parent;
    }
    #define INDENT for (int _i = 0; _i < indent; _i++) printf("      ");
    if (profile->parent == NULL) {
        INDENT printf("root:\n");
    }
    INDENT printf("  profile idx: %d\n", profile->subprofile_idx);
    INDENT printf("  name: %s\n", profile->name);
    INDENT printf("  path: %s/\n", profile->path);
    INDENT printf("  directory: %s\n", profile->directory);
    INDENT printf("  type: %d\n", profile->type);
    INDENT printf("  channels (%d):\n", profile->channels_cnt);
    for (size_t i = 0; i < profile->channels_cnt; i++) {
        struct ipx_profile_channel *chan = profile->channels[i];
        INDENT printf("    #%d:\n", i);
        INDENT printf("      channel idx: %d\n", chan->channel_idx);
        INDENT printf("      name: %s\n", chan->name);
        INDENT printf("      path: %s/%s/\n", chan->profile->path, chan->profile->name);
        INDENT printf("      filter: %s\n", chan->filter);
        INDENT printf("      sources (%d):\n", chan->sources_cnt);
        for (size_t j = 0; j < chan->sources_cnt; j++) {
            INDENT printf("        source name: %s\n", chan->sources[j]->name);
        }
        INDENT printf("      listeners (%d):\n", chan->listeners_cnt);
        for (size_t j = 0; j < chan->listeners_cnt; j++) {
            INDENT printf("        listener name: %s\n", chan->listeners[j]->name);
        }
    }
    INDENT printf("    subprofiles (%d):\n", profile->subprofiles_cnt);
    for (size_t i = 0; i < profile->subprofiles_cnt; i++) {
        INDENT printf("      #%d:\n", i);
        print_profiles(profile->subprofiles[i]);
    }

    #undef INDENT
}

int
ipx_plugin_init(ipx_ctx_t *ipx_ctx, const char *params)
{
    int rc;
    
    // Create plugin context
    struct plugin_data *pd = calloc(1, sizeof(struct plugin_data));
    if (pd == NULL) {
        IPX_CTX_ERROR(ipx_ctx, "out of memory");
        return IPX_ERR_NOMEM;
    }
    ipx_ctx_private_set(ipx_ctx, pd);

    pd->config = config_parse(ipx_ctx, params);
    if (pd->config == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Parse profiles xml and build profile tree    
    rc = ipx_profiles_parse_xml(pd->config->profiles_filename, &pd->ptree);
    if (rc != IPX_OK) {
        return rc;
    }
    // print_profiles(pd->ptree->root);

    // Create matcher from the profile tree
    pd->pmatcher = ipx_pmatcher_create(pd->ptree->root, ipx_ctx_iemgr_get(ipx_ctx));
    if (pd->pmatcher == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Register producer for extension
    rc = ipx_ctx_ext_producer(ipx_ctx, "profiles-v1", "main_profiles", 
                              ipx_profiles_calc_ext_size(pd->ptree), &pd->ext);
    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ipx_ctx, "error registering ext producer");
        return rc;
    }

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ipx_ctx, void *data)
{
    struct plugin_data *pd = data;  
    //ipx_pmatcher_destroy(pd->pmatcher);
    //ipx_profiles_destroy(pd->ptree);
    //config_destroy(pd->config);
    //free(pd);
}

static inline bool
test_bit(uint64_t *bitset, int idx)
{
    return bitset[idx >> 6] & (1 << (idx & 0b111111));
}

int
ipx_plugin_process(ipx_ctx_t *ipx_ctx, void *data, ipx_msg_t *base_msg)
{
    struct plugin_data *pd = data;  

    // We only care about IPFIX messages
    if (ipx_msg_get_type(base_msg) != IPX_MSG_IPFIX) {
        ipx_ctx_msg_pass(ipx_ctx, base_msg);
        return IPX_OK;
    }

    ipx_msg_ipfix_t *msg = ipx_msg_base2ipfix(base_msg);

    size_t profiles_bytes_cnt = pd->ptree->profiles_cnt + 63 / 64 * 8;
    size_t channels_bytes_cnt = pd->ptree->channels_cnt + 63 / 64 * 8;
    uint8_t *bytes = malloc(sizeof(struct ipx_profiles_tree *) + profiles_bytes_cnt + channels_bytes_cnt);
    *((struct ipx_profiles_tree **) bytes) = pd->ptree;

    struct ipx_pmatcher_result match_result; 
    match_result.profiles = bytes + sizeof(struct ipx_profiles_tree *);
    match_result.channels = bytes + sizeof(struct ipx_profiles_tree *) + profiles_bytes_cnt;

    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (uint32_t idx = 0; idx < drec_cnt; idx++) {
        struct ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(msg, idx);
        
        // Get extension data to fill out
        struct ipx_profiles_ext *ext_data;
        size_t ext_size;
        int rc = ipx_ctx_ext_get(pd->ext, rec, &ext_data, &ext_size);
        if (rc != IPX_OK) {
            IPX_CTX_ERROR(ipx_ctx, "error getting extension data");
            continue;
        }
        assert(ext_size == ipx_profiles_calc_ext_size(pd->ptree));
        ext_data->ptree = pd->ptree;

        // Fill it out with matches
        struct ipx_pmatcher_result match_result = ipx_profiles_get_matches(ext_data);
        ipx_pmatcher_match(pd->pmatcher, &rec->rec, match_result);

        //printf("XXXX: match result:\n");
        //printf("XXXX: profiles ");
        //for (int i = 0; i < ext_data->ptree->profiles_cnt; i++) {
        //    if (test_bit(match_result.profiles, i)) {
        //        printf("1");
        //    } else {
        //        printf("0");
        //    }
        //}
        //printf("\n");

        //printf("XXXX: channels ");
        //for (int i = 0; i < ext_data->ptree->channels_cnt; i++) {
        //    if (test_bit(match_result.channels, i)) {
        //        printf("1");
        //    } else {
        //        printf("0");
        //    }
        //}
        //printf("\n");

        ipx_ctx_ext_set_filled(pd->ext, rec);
    }

    ipx_ctx_msg_pass(ipx_ctx, base_msg);

    return IPX_OK;
}

