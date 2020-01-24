#include <ipfixcol2.h>
#include <libfds.h>
#include <stdlib.h>
#include <arpa/inet.h> // ntohs
#include <endian.h> // be64toh
#include <assert.h>

#include "config.h"
#include "msg_builder.h"

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    .type = IPX_PT_INTERMEDIATE,
    .name = "filter",
    .dsc = "Data record filtering plugin",
    .flags = 0,
    .version = "0.0.1",
    .ipx_min = "2.0.0"
};

typedef struct {
    struct fds_iemgr_elem *elem;
} lookup_item_s;

typedef struct {
    config_s *config;
    fds_filter_t *filter;
    ipx_ctx_t *ipx_ctx;
    fds_iemgr_t *iemgr;
    int lookup_cnt;
    lookup_item_s *lookup_tab;
} plugin_ctx_s;

plugin_ctx_s *
create_plugin_ctx()
{
    plugin_ctx_s *pctx = calloc(1, sizeof(plugin_ctx_s));
    if (!pctx) {
        return NULL;
    }
    return pctx;
}

void
destroy_plugin_ctx(plugin_ctx_s *pctx)
{
    if (!pctx) {
        return;
    }
    free(pctx->lookup_tab);
    config_destroy(pctx->config);
    fds_filter_destroy(pctx->filter);
    free(pctx);
}

int
add_lookup_item(plugin_ctx_s *pctx, struct fds_iemgr_elem *elem)
{
    void *tmp = realloc(pctx->lookup_tab, pctx->lookup_cnt + 1);
    if (!tmp) {
        return -1;
    }
    pctx->lookup_cnt++;
    pctx->lookup_tab = tmp;
    pctx->lookup_tab[pctx->lookup_cnt - 1].elem = elem;
    return pctx->lookup_cnt - 1;
}

static inline int
get_filter_datatype(struct fds_iemgr_elem *elem)
{
    switch (elem->data_type) {
    case FDS_ET_UNSIGNED_8:
    case FDS_ET_UNSIGNED_16:
    case FDS_ET_UNSIGNED_32:
    case FDS_ET_UNSIGNED_64:
        return FDS_FILTER_DT_UINT;
    case FDS_ET_SIGNED_8:
    case FDS_ET_SIGNED_16:
    case FDS_ET_SIGNED_32:
    case FDS_ET_SIGNED_64:
        return FDS_FILTER_DT_INT;
    case FDS_ET_BOOLEAN:
        return FDS_FILTER_DT_BOOL;
    case FDS_ET_IPV4_ADDRESS:
    case FDS_ET_IPV6_ADDRESS:
        return FDS_FILTER_DT_IP;
    case FDS_ET_MAC_ADDRESS:
        return FDS_FILTER_DT_MAC;
    case FDS_ET_STRING:
        return FDS_FILTER_DT_STR;
    }
    return FDS_FILTER_DT_NONE;
}

static inline bool
drec_belongs_to_set(struct fds_ipfix_set_hdr *set, struct fds_drec *drec)
{
    uint8_t *set_begin = (uint8_t *) set;
    uint8_t *set_end = set_begin + ntohs(set->length);
    uint8_t *drec_begin = drec->data;

    return drec_begin >= set_begin && drec_begin < set_end;
}

int
lookup_callback(void *user_ctx, const char *name, int *out_id, int *out_datatype, int *out_flags)
{
    plugin_ctx_s *pctx = (plugin_ctx_s *) user_ctx;

    struct fds_iemgr_elem *elem = fds_iemgr_elem_find_name(pctx->iemgr, name);
    if (!elem) {
        return FDS_ERR_NOTFOUND;
    }

    *out_datatype = get_filter_datatype(elem);
    if (*out_datatype == FDS_FILTER_DT_NONE) {
        IPX_CTX_ERROR(pctx->ipx_ctx, "Cannot represent value as filter datatype");
        return FDS_ERR_DENIED;
    }
    
    int item_id = add_lookup_item(pctx, elem);
    if (item_id == -1) {
        IPX_CTX_ERROR(pctx->ipx_ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        return FDS_ERR_NOMEM;
    }
    *out_id = item_id;
    IPX_CTX_INFO(pctx->ipx_ctx, "Added %s, lookup id: %d, elem id: %d:%d",
        name, item_id, elem->scope->pen, elem->id);

    return FDS_OK;
}

void
const_callback(void *user_ctx, int id, fds_filter_value_u *out_value)
{
    (void)(id);
    (void)(out_value);
    assert(0);
}

int
data_callback(void *user_ctx, bool reset_ctx, int id, void *data, fds_filter_value_u *out_value)
{
    plugin_ctx_s *pctx = (plugin_ctx_s *) user_ctx;
    struct ipx_ipfix_record *drec = data;
    lookup_item_s *item = &pctx->lookup_tab[id];
    
    struct fds_drec_iter iter;
    fds_drec_iter_init(&iter, &drec->rec, FDS_DREC_UNKNOWN_SKIP);
    if (fds_drec_iter_find(&iter, item->elem->scope->pen, item->elem->id) == FDS_EOC) {
        memset(out_value, 0, sizeof(fds_filter_value_u));
        return FDS_ERR_NOTFOUND;
    }

    switch (item->elem->data_type) {
        case FDS_ET_UNSIGNED_8:
        case FDS_ET_UNSIGNED_16:
        case FDS_ET_UNSIGNED_32:
        case FDS_ET_UNSIGNED_64:
            fds_get_uint_be(iter.field.data, iter.field.size, &out_value->u);
            break;

        case FDS_ET_SIGNED_8:
        case FDS_ET_SIGNED_16:
        case FDS_ET_SIGNED_32:
        case FDS_ET_SIGNED_64:
            fds_get_int_be(iter.field.data, iter.field.size, &out_value->u);
            break;
        
        case FDS_ET_FLOAT_32:
        case FDS_ET_FLOAT_64:
            fds_get_float_be(iter.field.data, iter.field.size, &out_value->f);
            break;

        case FDS_ET_BOOLEAN:
            fds_get_bool(iter.field.data, iter.field.size, &out_value->b);
            break;

        case FDS_ET_IPV4_ADDRESS:
            out_value->ip.prefix = 32;
            out_value->ip.version = 4;
            fds_get_ip(iter.field.data, iter.field.size, out_value->ip.addr);
            break;

        case FDS_ET_IPV6_ADDRESS:
            out_value->ip.prefix = 128;
            out_value->ip.version = 6;
            fds_get_ip(iter.field.data, iter.field.size, out_value->ip.addr);
            break;

        case FDS_ET_MAC_ADDRESS:
            fds_get_mac(iter.field.data, iter.field.size, out_value->mac.addr);
            break;

        case FDS_ET_STRING:
            out_value->str.len = iter.field.size;
            out_value->str.chars = iter.field.data;
            break;
        
        default: assert(0);
    }
    
    
    IPX_CTX_INFO(pctx->ipx_ctx, "Data callback for %s %d:%d", 
        item->elem->name, item->elem->scope->pen, item->elem->id);

    return FDS_OK;
}

int
ipx_plugin_init(ipx_ctx_t *ipx_ctx, const char *params)
{
    // Create the plugin context
    plugin_ctx_s *pctx = create_plugin_ctx();
    if (!pctx) {
        return IPX_ERR_DENIED;
    }

    pctx->ipx_ctx = ipx_ctx;
    pctx->iemgr = ipx_ctx_iemgr_get(ipx_ctx);
    
    // Parse config
    pctx->config = config_parse(ipx_ctx, params);
    if (!pctx->config) {
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }

    // Create the opts
    fds_filter_opts_t *opts = fds_filter_create_default_opts();
    if (!opts) {
        IPX_CTX_ERROR(ipx_ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }
    fds_filter_opts_set_user_ctx(opts, pctx);
    fds_filter_opts_set_lookup_cb(opts, lookup_callback);
    fds_filter_opts_set_data_cb(opts, data_callback);

    // Create the filter
    int rc = fds_filter_create(pctx->config->expr, opts, &pctx->filter);
    //fds_filter_destroy_opts(opts); FIXME
    if (rc != FDS_OK) {
        fds_filter_error_s *error = fds_filter_get_error(pctx->filter);
        IPX_CTX_ERROR(ipx_ctx, "Error creating filter: %s", error->msg);
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ipx_ctx, pctx);
    
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ipx_ctx, void *data)
{
    destroy_plugin_ctx(data);
}

int
ipx_plugin_process(ipx_ctx_t *ipx_ctx, void *data, ipx_msg_t *base_msg)
{
    // We only care about IPFIX messages
    if (ipx_msg_get_type(base_msg) != IPX_MSG_IPFIX) {
        ipx_ctx_msg_pass(ipx_ctx, base_msg);
        return IPX_OK;
    }

    plugin_ctx_s *pctx = (plugin_ctx_s *) data;

    // Get the ipfix message
    ipx_msg_ipfix_t *orig_msg = ipx_msg_base2ipfix(base_msg);

    // Initialize message builder
    msg_builder_s mb;
    int rc = msg_builder_init(&mb, ipx_ctx, orig_msg);
    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ipx_ctx, "Error initializing message builder");
        return rc;
    }    

    // Get ipfix sets
    struct ipx_ipfix_set *sets;
    int set_cnt;
    ipx_msg_ipfix_get_sets(orig_msg, &sets, &set_cnt);

    // Build new message
    int drec_idx = 0;
    for (int set_idx = 0; set_idx < set_cnt; set_idx++) {
        struct ipx_ipfix_set set = sets[set_idx]; 
        uint16_t set_id = ntohs(set.ptr->flowset_id);

        // If it's not a data set simply copy it over
        if (set_id < FDS_IPFIX_SET_MIN_DSET) {
                rc = msg_builder_copy_set(&mb, &set);
            if (rc != IPX_OK) {
                IPX_CTX_ERROR(ipx_ctx, "Error copying set");
                ipx_msg_ipfix_destroy(mb.msg);
                return rc;
            }
            continue;
        }

        msg_builder_begin_dset(&mb, set_id);

        // If it's a data set copy over all its data records that pass the filter
        for (;;) {
            struct ipx_ipfix_record *drec = ipx_msg_ipfix_get_drec(orig_msg, drec_idx);

            if (drec == NULL || !drec_belongs_to_set(set.ptr, &drec->rec)) {
                break;
            }

            if (fds_filter_eval(pctx->filter, drec)) {
                rc = msg_builder_copy_drec(&mb, drec);
                if (rc != IPX_OK) {
                    IPX_CTX_ERROR(ipx_ctx, "Error copying data record");
                    ipx_msg_ipfix_destroy(mb.msg);
                    return rc;
                }
            }

            drec_idx++;
        }


        rc = msg_builder_end_dset(&mb);
        if (rc != IPX_OK) {
            IPX_CTX_ERROR(ipx_ctx, "Error ending data set");
            ipx_msg_ipfix_destroy(mb.msg);
            return rc;
        }
    }
    assert(drec_idx == ipx_msg_ipfix_get_drec_cnt(orig_msg));

    // Finish building the message
    msg_builder_finish(&mb);

    // Destroy the original
    ipx_msg_ipfix_destroy(orig_msg);

    // If the message is empty throw it away
    if (msg_builder_is_empty_msg(&mb)) {
        ipx_msg_ipfix_destroy(mb.msg);
    } else {
        ipx_ctx_msg_pass(ipx_ctx, mb.msg);
    }
    
    return IPX_OK;
}