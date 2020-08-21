/**
 * \file src/core/profiles/profile.c
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz> 
 * \brief Profile structures
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
#include <string.h>

#include <ipfixcol2.h>
#include "common.h"

enum {
    PROFILE,
    PROFILE_NAME,
    PROFILE_TYPE,
    PROFILE_DIRECTORY,
    PROFILE_CHANNEL_LIST,
    PROFILE_SUBPROFILE_LIST,
    CHANNEL,
    CHANNEL_NAME,
    CHANNEL_SOURCE_LIST,
    CHANNEL_FILTER,
    SOURCE
};

static const struct fds_xml_args args_source_list[] = {
    FDS_OPTS_ELEM(SOURCE, "source", FDS_OPTS_T_STRING, FDS_OPTS_P_MULTI | FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

static const struct fds_xml_args args_channel[] = {
    FDS_OPTS_ATTR(CHANNEL_NAME, "name", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(CHANNEL_FILTER, "filter", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(CHANNEL_SOURCE_LIST, "sourceList", args_source_list, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const struct fds_xml_args args_channel_list[] = {
    FDS_OPTS_NESTED(CHANNEL, "channel", args_channel, FDS_OPTS_P_MULTI | FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

static const struct fds_xml_args args_subprofile_list[2];

static const struct fds_xml_args args_profile[] = {
    FDS_OPTS_ATTR(PROFILE_NAME, "name", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(PROFILE_TYPE, "type", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PROFILE_DIRECTORY, "directory", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(PROFILE_CHANNEL_LIST, "channelList", args_channel_list, 0),
    FDS_OPTS_NESTED(PROFILE_SUBPROFILE_LIST, "subprofileList", args_subprofile_list, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

static const struct fds_xml_args args_subprofile_list[] = {
    FDS_OPTS_NESTED(PROFILE, "profile", args_profile, FDS_OPTS_P_MULTI | FDS_OPTS_P_OPT),
    FDS_OPTS_END        
};

static const struct fds_xml_args args_main[] = {
    FDS_OPTS_ROOT("profileTree"), // workaround because profile cannot be root element
    FDS_OPTS_NESTED(PROFILE, "profile", args_profile, 0),
    FDS_OPTS_END
};

/**
 * Destroy a profile and all its subprofiles
 * 
 * \param[in] profile  The pointer to the profile
 */
static void
destroy_profiles(struct ipx_profile *profile)
{
    free(profile->name);
    free(profile->directory);
    free(profile->path);

    for (size_t i = 0; i < profile->channels_cnt; i++) {
        struct ipx_profile_channel *channel = profile->channels[i];
        free(channel->name);
        free(channel->sources);
        free(channel->filter);
        free(channel->listeners);
        free(channel);
    }
    free(profile->channels);

    for (size_t i = 0; i < profile->subprofiles_cnt; i++) {
        struct ipx_profile *subprofile = profile->subprofiles[i];
        destroy_profiles(subprofile);
    }
    free(profile->subprofiles);
    free(profile);
}

/**
 * Destroy a profile tree
 * 
 * \param[in] ptree  The pointer to the profile tree
 */
static void
destroy_profile_tree(struct ipx_profile_tree *ptree)
{
    if (ptree == NULL) {
        return;
    }
    destroy_profiles(ptree->root);
    free(ptree);
}

/**
 * Create XML parser for the profile config file
 * 
 * \return Pointer to the XML parser on success, NULL on failure 
 */
static fds_xml_t *
create_parser()
{
    fds_xml_t *parser = fds_xml_create();
    if (parser == NULL) {
        PROFILES_MEMORY_ERROR();
        return NULL;
    }

    if (fds_xml_set_args(parser, args_main) != FDS_OK) {
        PROFILES_ERROR("cannot create parser: %s", fds_xml_last_err(parser));
        fds_xml_destroy(parser);
        return NULL;
    }
    
    return parser;
}

/**
 * Find channel in profile by name
 */
static struct ipx_profile_channel *
find_channel_by_name(struct ipx_profile *profile, const char *name)
{
    for (int i = 0; i < profile->channels_cnt; i++) {
        if (strcmp(profile->channels[i]->name, name) == 0) {
            return profile->channels[i];
        }
    }
    return NULL;
}

/**
 * Check if channel contains source
 */
static bool
channel_contains_source(struct ipx_profile_channel *channel, struct ipx_profile_channel *source)
{
    for (int i = 0; i < channel->sources_cnt; i++) {
        if (channel->sources[i] == source) {
            return true;
        }
    }
    return false;
}

/**
 * Add source to a channels list of sources, also check if it doesn't already exist
 */
static int
add_channel_source(struct ipx_profile_channel *channel, struct ipx_profile_channel *source)
{
    if (channel_contains_source(channel, source)) {
        // don't care if its defined multiple times, just don't add it again
        return IPX_OK;
    }

    // add pointer to the channel found in parent to the list of sources of this channel
    struct ipx_profile_channel **source_ref = array_push(
        &channel->sources, &channel->sources_cnt, sizeof(struct ipx_profile_channel *));     
    if (source_ref == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }
    *source_ref = source;

    // add pointer to this channel to the list of listeners of the source channel
    struct ipx_profile **listener_ref = array_push(
        &source->listeners, &source->listeners_cnt, sizeof(struct ipx_profile_channel *));
    if (listener_ref == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }
    *listener_ref = channel;

    return IPX_OK;
}

/**
 * Parse <sourceList> and the <source> elements within
 * 
 * \param[in]     xml_ctx  The xml context of the <sourceList> node
 * \param[in,out] channel  Pointer to the channel struct to be filled with the sources
 * \return IPX_OK on success, IPX error code otherwise
 */
static int
parse_source_list(fds_xml_ctx_t *xml_ctx, struct ipx_profile_channel *channel)
{
    // loop over all sources in sourceList
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        if (content->id != SOURCE) {
            //TODO:???
            continue;
        }

        struct ipx_profile *parent = channel->profile->parent;

        if (strcmp(content->ptr_string, "*") == 0) {
            if (parent == NULL) {
                // this is the root "live" profile and "*" is special
                //TODO: ensure that the live profile indeed contained "*" source?
                continue;
            }

            // add all the parent channels as source
            for (int i = 0; i < parent->channels_cnt; i++) {
                int rc = add_channel_source(channel, parent->channels[i]);
                if (rc != IPX_OK) {
                    return rc;
                }
            }
        } else {
            if (parent == NULL) {
                PROFILES_ERROR("live profile can only contain the special source '*'");
                return IPX_ERR_FORMAT;
            }
        
            struct ipx_profile_channel *source = find_channel_by_name(parent, content->ptr_string);
            if (source == NULL) {
                //TODO: improve error message
                PROFILES_ERROR("channel '%s' not found in parent profile", content->ptr_string);
                return IPX_ERR_FORMAT;
            }
            int rc = add_channel_source(channel, source);
            if (rc != IPX_OK) {
                return rc;
            }
        }
    }
    return IPX_OK;
}

/**
 * Parse the <channel> element
 * 
 * \param[in]     xml_ctx  The xml context of the <channel> node
 * \param[in,out] channel  Pointer to the channel struct to be filled
 * \return IPX_OK on success, IPX error code otherwise
 */
static int
parse_channel(struct ipx_profile_tree *ptree, fds_xml_ctx_t *xml_ctx,
              struct ipx_profile_channel *channel)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case CHANNEL_NAME:
            channel->name = strdup(content->ptr_string);
            if (channel->name == NULL) {
                PROFILES_MEMORY_ERROR();
                return IPX_ERR_NOMEM;
            }
            break;

        case CHANNEL_FILTER:
            channel->filter = strdup(content->ptr_string);
            if (channel->filter == NULL) {
                PROFILES_MEMORY_ERROR();
                return IPX_ERR_NOMEM;
            }
            //TODO: compile the filter?
            break;
        
        case CHANNEL_SOURCE_LIST: {
            int rc = parse_source_list(content->ptr_ctx, channel);
            if (rc != IPX_OK) {
                return rc;
            }
            break;
        }

        }
    }

    return IPX_OK;
}

/**
 * Add new channel to profile
 */
static struct ipx_profile_channel *
add_channel(struct ipx_profile_tree *ptree, struct ipx_profile *profile)
{
    struct ipx_profile_channel *channel = calloc(1, sizeof(struct ipx_profile_channel));
    if (channel == NULL) {
        PROFILES_MEMORY_ERROR();
        return NULL;
    }

    struct ipx_profile_channel **channel_ptr =
        array_push(&profile->channels, &profile->channels_cnt, sizeof(struct ipx_profile_channel *));
    if (channel_ptr == NULL) {
        PROFILES_MEMORY_ERROR();
        free(channel);
        return NULL;
    }
    *channel_ptr = channel;

    channel->profile = profile;
    channel->channel_idx = profile->channels_cnt - 1;
    channel->bit_offset = ptree->channels_cnt;
    ptree->channels_cnt++;

    return channel;
}

/**
 * Parse the <channelList> element
 * 
 * \param[in]     xml_ctx  The xml context of the <sourceList> node
 * \param[in,out] profile  Pointer to the profile struct to be filled with the channels
 * \return IPX_OK on success, IPX error code otherwise
 */
static int
parse_channel_list(struct ipx_profile_tree *ptree, fds_xml_ctx_t *xml_ctx, struct ipx_profile *profile)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        if (content->id != CHANNEL) {
            //TODO:???
            continue;
        }

        struct ipx_profile_channel *channel = add_channel(ptree, profile);
        if (channel == NULL) {
            return IPX_ERR_NOMEM;
        }

        int rc = parse_channel(ptree, content->ptr_ctx, channel);
        if (rc != IPX_OK) {
            return rc;
        }
    }
    return IPX_OK;
}

/**
 * Add new subprofile to profile
 */
static struct ipx_profile *
add_subprofile(struct ipx_profile_tree *ptree, struct ipx_profile *profile)
{
    struct ipx_profile *subprofile = calloc(1, sizeof(struct ipx_profile));
    if (subprofile == NULL) {
        PROFILES_MEMORY_ERROR();
        return NULL;
    }

    struct ipx_profile **subprofile_ptr =
        array_push(&profile->subprofiles, &profile->subprofiles_cnt, sizeof(struct ipx_profile *));
    if (subprofile == NULL) {
        PROFILES_MEMORY_ERROR();
        free(subprofile);
        return NULL;
    }
    *subprofile_ptr = subprofile;

    subprofile->path = malloc(strlen(profile->path) + strlen(profile->name) + 2);
    if (subprofile->path == NULL) {
        // Do not free subprofile because it is already attached to the parent profile
        return NULL;
    }
    sprintf(subprofile->path, "%s/%s", profile->path, profile->name);
    subprofile->parent = profile;
    subprofile->subprofile_idx = profile->subprofiles_cnt - 1;
    subprofile->bit_offset = ptree->profiles_cnt;
    ptree->profiles_cnt++;

    return subprofile;
}

static int
parse_profile(struct ipx_profile_tree *ptree, fds_xml_ctx_t *xml_ctx, struct ipx_profile *profile);

/**
 * Parse the <subprofileList> element
 * 
 * \param[in]     xml_ctx  The xml context of the <sourceList> node
 * \param[in,out] profile  Pointer to the profile struct to be filled with the channels
 * \return IPX_OK on success, IPX error code otherwise
 */
static int
parse_subprofile_list(struct ipx_profile_tree *ptree, fds_xml_ctx_t *xml_ctx, struct ipx_profile *profile)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        if (content->id != PROFILE) {
            //TODO:???
            continue;
        }

        struct ipx_profile *subprofile = add_subprofile(ptree, profile);
        if (subprofile == NULL) {
            return IPX_ERR_NOMEM;
        }

        int rc = parse_profile(ptree, content->ptr_ctx, subprofile);
        if (rc != IPX_OK) {
            return rc;
        }
    }
    return IPX_OK;

}

/**
 * Parse the <profile> element
 * 
 * \param[in]     xml_ctx  The xml context of the <profile> node
 * \param[in,out] profile  Pointer to the profile struct to be filled
 * \return IPX_OK on success, IPX error code otherwise
 */
static int
parse_profile(struct ipx_profile_tree *ptree, fds_xml_ctx_t *xml_ctx, struct ipx_profile *profile)
{
    // default values
    //TODO:???
    profile->type = IPX_PT_NORMAL;

    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {

        case PROFILE_NAME:
            profile->name = strdup(content->ptr_string);
            if (profile->name == NULL) {
                PROFILES_MEMORY_ERROR();
                return IPX_ERR_NOMEM;
            }
            break;

        case PROFILE_TYPE:
            if (strcmp(content->ptr_string, "normal") == 0) {
                profile->type = IPX_PT_NORMAL;
            } else if (strcmp(content->ptr_string, "shadow") == 0) {
                profile->type = IPX_PT_SHADOW;
            } else {
                PROFILES_ERROR("invalid profile type '%s'", content->ptr_string);
                return IPX_ERR_FORMAT;
            }
            break;
        
        case PROFILE_DIRECTORY: {
            profile->directory = strdup(content->ptr_string);
            if (profile->directory == NULL) {
                PROFILES_MEMORY_ERROR();
                return IPX_ERR_NOMEM;
            }
            // If the directory ends with trailing / remove it
            size_t len = strlen(profile->directory);
            if (len > 0 && profile->directory[len - 1] == '/') {
                profile->directory[len - 1] = '\0';
            }
            break;
        }
        
        case PROFILE_CHANNEL_LIST: {
            int rc = parse_channel_list(ptree, content->ptr_ctx, profile);
            if (rc != IPX_OK) {
                return rc;
            }
            break;
        }
        
        case PROFILE_SUBPROFILE_LIST: {
            int rc = parse_subprofile_list(ptree, content->ptr_ctx, profile);
            if (rc != IPX_OK) {
                return rc;
            }
            break;
        }
        
        }
    }

    // Generate a directory from the parent directory if it wasn't explicitly set
    // in the format `parent directory`/`this profile name`
    if (profile->directory == NULL) {
        if (profile->parent == NULL) {
            PROFILES_ERROR("directory is required for live profile");
            return IPX_ERR_FORMAT;
        }

        profile->directory = malloc(strlen(profile->parent->directory) + 1 + strlen(profile->name) + 1);
        if (profile->directory == NULL) {
            PROFILES_MEMORY_ERROR();
            return IPX_ERR_NOMEM;
        }

        sprintf(profile->directory, "%s/%s", profile->parent->directory, profile->name);
    } 

    return IPX_OK;
}

/**
 * Parse <profiles>
 */
static int
parse_profile_tree(fds_xml_ctx_t *xml_ctx, struct ipx_profile_tree *ptree)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        if (content->id != PROFILE) {
            continue;
        }

        int rc = parse_profile(ptree, content->ptr_ctx, ptree->root);
        return rc;
    }
    return IPX_ERR_FORMAT;
}

/**
 * Create a new profile tree
 */
static struct ipx_profile_tree *
create_profile_tree()
{
    struct ipx_profile_tree *ptree = calloc(1, sizeof(struct ipx_profile_tree));
    if (ptree == NULL) {
        PROFILES_MEMORY_ERROR();
        return NULL;
    }

    ptree->root = calloc(1, sizeof(struct ipx_profile));
    if (ptree->root == NULL) {
        PROFILES_MEMORY_ERROR();
        free(ptree);
        return NULL;
    }
    ptree->root->path = malloc(1);
    if (ptree->root->path == NULL) {
        free(ptree->root);
        free(ptree);
        return NULL;
    }
    ptree->root->path[0] = '\0';
    ptree->root->bit_offset = 0;
    ptree->profiles_cnt++;

    return ptree;
}

/**
 * Copy profile and its channels, also recursively copy all its subprofiles
 */
static int
copy_profile(struct ipx_profile_tree *ptree, struct ipx_profile *srcprof, struct ipx_profile *dstprof)
{
    // Copy attributes
    dstprof->type = srcprof->type;
    
    dstprof->name = strdup(srcprof->name);
    if (dstprof->name == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }

    dstprof->path = strdup(srcprof->path);
    if (dstprof->path == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }

    dstprof->directory = strdup(srcprof->directory);
    if (dstprof->directory == NULL) {
        PROFILES_MEMORY_ERROR();
        return IPX_ERR_NOMEM;
    }

    // copy channels
    for (size_t i = 0; i < srcprof->channels_cnt; i++) {
        struct ipx_profile_channel *srcchan = srcprof->channels[i];
        struct ipx_profile_channel *dstchan = add_channel(ptree, dstprof);
        
        if (dstchan == NULL) {
            PROFILES_MEMORY_ERROR();
            return IPX_ERR_NOMEM;
        }

        // copy channel sources
        for (size_t j = 0; j < srcchan->sources_cnt; j++) {
            int rc = add_channel_source(
                dstchan,
                dstchan->profile->parent->channels[srcchan->sources[j]->channel_idx]
            );
            if (rc != IPX_OK) {
                return rc;
            }
        }

        // copy the remaining fields
        dstchan->name = strdup(srcchan->name);
        if (dstchan->name == NULL) {
            PROFILES_MEMORY_ERROR();
            return IPX_ERR_NOMEM;
        }
        dstchan->filter = strdup(srcchan->filter);
        if (dstchan->filter == NULL) {
            PROFILES_MEMORY_ERROR();
            return IPX_ERR_NOMEM;
        }
    }

    // copy subprofiles
    for (size_t i = 0; i < srcprof->subprofiles_cnt; i++) {
        struct ipx_profile *subprof = add_subprofile(ptree, dstprof);
        if (subprof == NULL) {
            return IPX_ERR_NOMEM;
        }
        int rc = copy_profile(ptree, srcprof->subprofiles[i], subprof);
        if (rc != IPX_OK) {
            return IPX_ERR_NOMEM;
        }
    }


    return IPX_OK;
}

/**
 * Make a copy of a profile tree
 */
static int
copy_profile_tree(struct ipx_profile_tree *orig, struct ipx_profile_tree **copy)
{
    struct ipx_profile_tree *ptree = create_profile_tree();
    if (ptree == NULL) {
        return IPX_ERR_NOMEM;
    }

    int rc = copy_profile(ptree, orig->root, ptree->root);
    if (rc != IPX_OK) {
        destroy_profile_tree(ptree);
        return rc;
    }

    *copy = ptree;
    return IPX_OK;
}

/**
 * Parse a XML file with profile definitions
 * 
 * \param[in]  file_path    Path to the XML file
 * \param[out] profile_tree  Pointer to where the resulting profile tree pointer will be stored
 * \return IPX_OK on success, else IPX error code
 */
int
ipx_profiles_parse_xml(const char *file_path, struct ipx_profile_tree **profile_tree)
{
    // open the file
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        PROFILES_ERROR("cannot open profiles file '%s'", file_path);
        return IPX_ERR_DENIED;
    }

    // create xml parser
    fds_xml_t *parser = create_parser();
    if (parser == NULL) {
        fclose(file);
        return IPX_ERR_NOMEM;
    }

    // create profile tree
    struct ipx_profile_tree *ptree = create_profile_tree();
    if (ptree == NULL) {
        PROFILES_MEMORY_ERROR();
        fds_xml_destroy(parser);
        fclose(file);
        return IPX_ERR_NOMEM;
    }

    // parse xml file
    fds_xml_ctx_t *root_ctx = fds_xml_parse_file(parser, file, false);
    if (root_ctx == NULL) {
        PROFILES_ERROR("cannot parse profiles file: %s", fds_xml_last_err(parser));
        destroy_profile_tree(ptree);
        fds_xml_destroy(parser);
        fclose(file);
        return IPX_ERR_FORMAT;
    }

    // parse the xml data into profile tree
    int rc = parse_profile_tree(root_ctx, ptree);
    if (rc != IPX_OK) {
        destroy_profile_tree(ptree);
        fds_xml_destroy(parser);
        fclose(file);
        return rc;
    }
    
    *profile_tree = ptree;
    fds_xml_destroy(parser);
    fclose(file);
    return IPX_OK;
}

/**
 * Destroy profile tree
 */
void
ipx_profiles_destroy(struct ipx_profile_tree *profile_tree)
{
    destroy_profile_tree(profile_tree);
}

/**
 * Make a copy of a profile tree
 */
int
ipx_profiles_copy(struct ipx_profile_tree *profile_tree,
                  struct ipx_profile_tree **profile_tree_copy)
{
    return copy_profile_tree(profile_tree, profile_tree_copy);
}

IPX_API size_t
ipx_profiles_calc_ext_size(struct ipx_profile_tree *ptree)
{
    return sizeof(struct ipx_profiles_ext) 
        + ((ptree->profiles_cnt + 63) / 64) * 8 
        + ((ptree->channels_cnt + 63) / 64) * 8;
}

IPX_API struct ipx_pmatcher_result
ipx_profiles_get_matches(struct ipx_profiles_ext *ext)
{
    return (struct ipx_pmatcher_result) {
        .profiles = ext->matches,
        .channels = ext->matches + ((ext->ptree->profiles_cnt + 63) / 64) * 8
    };
}
