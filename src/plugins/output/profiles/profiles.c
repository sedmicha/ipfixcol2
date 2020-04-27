#include <ipfixcol2.h>
#include <stdio.h>

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    .type = IPX_PT_OUTPUT,
    .name = "profiles-output",
    .dsc = "",
    .flags = 0,
    .version = "0.0.1",
    .ipx_min = "2.0.0"
};

struct plugin_data {
    ipx_ctx_ext_t *ext;
    ipx_pevents_t *pevents;
};


void
channel_create_cb(struct ipx_pevents_ctx *ctx)
{
    printf("XXXX: Channel %s:%s created\n", ctx->ptr.channel->profile->name, ctx->ptr.channel->name);
}

void
channel_delete_cb(struct ipx_pevents_ctx *ctx)
{
    printf("XXXX: Channel %s:%s deleted\n", ctx->ptr.channel->profile->name, ctx->ptr.channel->name);
}

void
channel_update_cb(struct ipx_pevents_ctx *ctx, union ipx_pevents_target old)
{
    printf("XXXX: Channel %s:%s updated\n", ctx->ptr.channel->profile->name, ctx->ptr.channel->name);
}

void
channel_data_cb(struct ipx_pevents_ctx *ctx, void *record)
{
    printf("XXXX: Channel %s:%s data %p\n", ctx->ptr.channel->profile->name, ctx->ptr.channel->name, record);
}

void
profile_create_cb(struct ipx_pevents_ctx *ctx)
{
    printf("XXXX: Profile %s created\n", ctx->ptr.profile->name);
}

void
profile_delete_cb(struct ipx_pevents_ctx *ctx)
{
    printf("XXXX: Profile %s deleted\n", ctx->ptr.profile->name);
}

void
profile_update_cb(struct ipx_pevents_ctx *ctx, union ipx_pevents_target old)
{
    printf("XXXX: Profile %s updated\n", ctx->ptr.profile->name);
}

void
profile_data_cb(struct ipx_pevents_ctx *ctx, void *record)
{
    printf("XXXX: Profile %s data %p\n", ctx->ptr.profile->name, record);
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

    // Create pevents and set-up callbacks
    struct ipx_pevents_cb_set prof_cbs;
    prof_cbs.on_create = profile_create_cb;
    prof_cbs.on_update = profile_update_cb;
    prof_cbs.on_delete = profile_delete_cb;
    prof_cbs.on_data = profile_data_cb;

    struct ipx_pevents_cb_set chan_cbs;
    chan_cbs.on_create = channel_create_cb;
    chan_cbs.on_update = channel_update_cb;
    chan_cbs.on_delete = channel_delete_cb;
    chan_cbs.on_data = channel_data_cb;

    pd->pevents = ipx_pevents_create(prof_cbs, chan_cbs);
    if (pd->pevents == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Register producer for extension
    rc = ipx_ctx_ext_consumer(ipx_ctx, "profiles-v1", "main_profiles", &pd->ext);
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
    ipx_pevents_destroy(pd->pevents);
    free(pd);
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

    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (uint32_t idx = 0; idx < drec_cnt; idx++) {
        struct ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(msg, idx);

        // Get extension data
        struct ipx_profiles_ext *ext_data;
        size_t ext_size;
        int rc = ipx_ctx_ext_get(pd->ext, rec, &ext_data, &ext_size);
        if (rc != IPX_OK) {
            IPX_CTX_ERROR(ipx_ctx, "error getting extension data");
            continue;
        }
        assert(ext_size == ipx_profiles_calc_ext_size(ext_data->ptree));

        // Call callbacks based on the results
        ipx_pevents_process(pd->pevents, &rec->rec, ext_data);
    }

}

