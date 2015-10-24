/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
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
 */

#ifndef __MEDIA_CODEC_INI_C__
#define __MEDIA_CODEC_INI_C__

/* includes here */
#include <glib.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <mm_debug.h>
#include <mm_error.h>
#include "iniparser.h"
#include <media_codec_ini.h>
#include <media_codec_port.h>

#define DEFAULT_VALUE ""

#define DEFAULT_HW_DECODER_NAME ""
#define DEFAULT_HW_DECODER_MIME ""
#define DEFAULT_HW_DECODER_FORMAT ""

#define DEFAULT_HW_ENCODER_NAME ""
#define DEFAULT_HW_ENCODER_MIME ""
#define DEFAULT_HW_ENCODER_FORMAT ""

#define DEFAULT_SW_DECODER_NAME ""
#define DEFAULT_SW_DECODER_MIME ""
#define DEFAULT_SW_DECODER_FORMAT ""

#define DEFAULT_SW_ENCODER_NAME ""
#define DEFAULT_SW_ENCODER_MIME ""
#define DEFAULT_SW_ENCODER_FORMAT ""


#define CNAME_SIZE 512

typedef struct {
    gchar cname[MEDIA_CODEC_INI_MAX_STRLEN];
    mediacodec_codec_type_e ctype;
}codec_list_t;

static codec_list_t general_codec_list[]={
    {"h261", MEDIACODEC_H261},
    {"h263", MEDIACODEC_H263},
    {"h264", MEDIACODEC_H264},
    {"mjpeg", MEDIACODEC_MJPEG},
    {"mpeg1", MEDIACODEC_MPEG1},
    {"mpeg2", MEDIACODEC_MPEG2},
    {"mpeg4", MEDIACODEC_MPEG4},
    {"hevc", MEDIACODEC_HEVC},
    {"vp8", MEDIACODEC_VP8},
    {"vp9", MEDIACODEC_VP9},
    {"vc1", MEDIACODEC_VC1},
    {"aac_lc", MEDIACODEC_AAC_LC},
    {"aac_he", MEDIACODEC_AAC_HE},
    {"aac_he_ps", MEDIACODEC_AAC_HE_PS},
    {"mp3", MEDIACODEC_MP3},
    {"amr_nb", MEDIACODEC_AMR_NB},
    {"amr_wb", MEDIACODEC_AMR_WB},
    {"vorbis", MEDIACODEC_VORBIS},
    {"flac", MEDIACODEC_FLAC},
    {"wmav1", MEDIACODEC_WMAV1},
    {"wmav2", MEDIACODEC_WMAV2},
    {"wmapro", MEDIACODEC_WMAPRO},
};

/* internal functions, macros here */
#ifdef MEDIA_CODEC_DEFAULT_INI
static gboolean _generate_default_ini(void);
#endif

static void _mc_ini_check_ini_status(void);

/* macro */
#define MEDIA_CODEC_INI_GET_STRING( x_dict, x_item, x_ini, x_default ) \
    do \
{ \
    gchar* str = iniparser_getstring(x_dict, x_ini, x_default); \
    \
    if ( str &&  \
            ( strlen( str ) > 0 ) && \
            ( strlen( str ) < MEDIA_CODEC_INI_MAX_STRLEN ) ) \
    { \
        strncpy ( x_item, str, strlen(str)+1 ); \
    } \
    else \
    { \
        strncpy ( x_item, x_default, strlen(x_default)+1 ); \
    } \
}while(0)

#define MEDIA_CODEC_INI_GET_STRING_FROM_LIST( x_dict, x_list, x_ini, x_default ) \
    do \
{ \
    char *token = NULL; \
    char *usr_ptr = NULL; \
    int index = 0; \
    const char *delimiters = " ,"; \
    gchar temp_arr[MEDIA_CODEC_INI_MAX_STRLEN] = {0}; \
    MEDIA_CODEC_INI_GET_STRING(x_dict, temp_arr, x_ini, x_default); \
    token = strtok_r( temp_arr, delimiters, &usr_ptr ); \
    while (token) \
    { \
        if (index == 0) \
        strncpy(x_list.name,token, MEDIA_CODEC_INI_MAX_STRLEN-1 );\
        else if (index == 1) \
        strncpy(x_list.mime,token, MEDIA_CODEC_INI_MAX_STRLEN-1 );\
        else if (index == 2) \
        strncpy(x_list.format,token, MEDIA_CODEC_INI_MAX_STRLEN-1 );\
        index++;\
        token = strtok_r( NULL, delimiters, &usr_ptr ); \
    } \
}while(0)

#define MEDIA_CODEC_INI_GET_COLOR( x_dict, x_item, x_ini, x_default ) \
    do \
{ \
    gchar* str = iniparser_getstring(x_dict, x_ini, x_default); \
    \
    if ( str &&  \
            ( strlen( str ) > 0 ) && \
            ( strlen( str ) < MEDIA_CODEC_INI_MAX_STRLEN ) ) \
    { \
        x_item = (guint) strtoul(str, NULL, 16); \
    } \
    else \
    { \
        x_item = (guint) strtoul(x_default, NULL, 16); \
    } \
}while(0)

/* x_ini is the list of index to set TRUE at x_list[index] */
#define MEDIA_CODEC_INI_GET_BOOLEAN_FROM_LIST( x_dict, x_list, x_list_max, x_ini, x_default ) \
    do \
{ \
    int index = 0; \
    const char *delimiters = " ,"; \
    char *usr_ptr = NULL; \
    char *token = NULL; \
    gchar temp_arr[MEDIA_CODEC_INI_MAX_STRLEN] = {0}; \
    MEDIA_CODEC_INI_GET_STRING( x_dict, temp_arr, x_ini, x_default); \
    token = strtok_r( temp_arr, delimiters, &usr_ptr ); \
    while (token) \
    { \
        index = atoi(token); \
        if (index < 0 || index > x_list_max -1) \
        { \
            LOGW("%d is not valid index\n", index); \
        } \
        else \
        { \
            x_list[index] = TRUE; \
        } \
        token = strtok_r( NULL, delimiters, &usr_ptr ); \
    } \
}while(0)

/* x_ini is the list of value to be set at x_list[index] */
#define MEDIA_CODEC_INI_GET_INT_FROM_LIST( x_dict, x_list, x_list_max, x_ini, x_default ) \
    do \
{ \
    int index = 0; \
    int value = 0; \
    const char *delimiters = " ,"; \
    char *usr_ptr = NULL; \
    char *token = NULL; \
    gchar temp_arr[MEDIA_CODEC_INI_MAX_STRLEN] = {0}; \
    MEDIA_CODEC_INI_GET_STRING(x_dict, temp_arr, x_ini, x_default); \
    token = strtok_r( temp_arr, delimiters, &usr_ptr ); \
    while (token) \
    { \
        if ( index > x_list_max -1) \
        { \
            LOGE("%d is not valid index\n", index); \
            break; \
        } \
        else \
        { \
            value = atoi(token); \
            x_list[index] = value; \
            index++; \
        } \
        token = strtok_r( NULL, delimiters, &usr_ptr ); \
    } \
}while(0)

#define MEDIA_CODEC_GET_DEFAULT_LIST( x_list, x_default ) \
    do \
{ \
    strncpy( x_list, x_default, MEDIA_CODEC_INI_MAX_STRLEN - 1);\
}while(0)
#define MEDIA_CODEC_PRINT_LIST( x_list,  x_message ) \
    do \
{ \
    codec_info_t codec_list= x_list;\
    printf("%s =", x_message);\
    printf("%s %s %s\n", codec_list.name, codec_list.mime, codec_list.format);\
}while(0)

media_format_mimetype_e _mc_convert_media_format_str_to_int(char *sformat )
{

    media_format_mimetype_e iformat = MEDIA_FORMAT_I420;
    if (!strcmp(sformat,"I420")) {
        iformat = MEDIA_FORMAT_I420;
        goto endf;
    } else if (!strcmp(sformat,"NV12")) {
        iformat = MEDIA_FORMAT_NV12;
        goto endf;
    } else if (!strcmp(sformat,"NV12T")) {
        iformat = MEDIA_FORMAT_NV12T;
        goto endf;
    } else if (!strcmp(sformat,"YV12")) {
        iformat = MEDIA_FORMAT_YV12;
        goto endf;
    } else if (!strcmp(sformat,"NV21")) {
        iformat = MEDIA_FORMAT_NV21;
        goto endf;
    } else if (!strcmp(sformat,"NV16")) {
        iformat = MEDIA_FORMAT_NV16;
    } else if (!strcmp(sformat,"YUYV")) {
        iformat = MEDIA_FORMAT_YUYV;
        goto endf;
    } else if (!strcmp(sformat,"UYVY")) {
        iformat = MEDIA_FORMAT_UYVY;
        goto endf;
    } else if (!strcmp(sformat,"422P")) {
        iformat = MEDIA_FORMAT_422P;
        goto endf;
    } else if (!strcmp(sformat,"RGB565")) {
        iformat = MEDIA_FORMAT_RGB565;
        goto endf;
    } else if (!strcmp(sformat,"RGB888")) {
        iformat = MEDIA_FORMAT_RGB888;
        goto endf;
    } else if (!strcmp(sformat,"RGBA")) {
        iformat = MEDIA_FORMAT_RGBA;
        goto endf;
    } else if (!strcmp(sformat,"ARGB")) {
        iformat = MEDIA_FORMAT_ARGB;
        goto endf;
    } else if (!strcmp(sformat,"PCM")) {
        iformat = MEDIA_FORMAT_PCM;
        goto endf;
    } else if (!strcmp(sformat,"H261")) {
        iformat = MEDIA_FORMAT_H261;
        goto endf;
    } else if (!strcmp(sformat,"H263")) {
        iformat = MEDIA_FORMAT_H263;
        goto endf;
    } else if (!strcmp(sformat,"H263P")) {
        iformat = MEDIA_FORMAT_H263P;
        goto endf;
    } else if (!strcmp(sformat,"H264_SP")) {
        iformat = MEDIA_FORMAT_H264_SP;
        goto endf;
    } else if (!strcmp(sformat,"H264_MP")) {
        iformat = MEDIA_FORMAT_H264_MP;
        goto endf;
    } else if (!strcmp(sformat,"H264_HP")) {
        iformat = MEDIA_FORMAT_H264_HP;
        goto endf;
    } else if (!strcmp(sformat,"MPEG4_SP")) {
        iformat = MEDIA_FORMAT_MPEG4_SP;
        goto endf;
    } else if (!strcmp(sformat,"MPEG4_ASP")) {
        iformat = MEDIA_FORMAT_MPEG4_ASP;
        goto endf;
    } else if (!strcmp(sformat,"AMR_NB")) {
        iformat = MEDIA_FORMAT_AMR_NB;
        goto endf;
    } else if (!strcmp(sformat,"AMR_WB")) {
        iformat = MEDIA_FORMAT_AMR_WB;
        goto endf;
    } else if (!strcmp(sformat,"AAC_LC")) {
        iformat = MEDIA_FORMAT_AAC_LC;
        goto endf;
    } else if (!strcmp(sformat,"AAC_HE")) {
        iformat = MEDIA_FORMAT_AAC_HE;
        goto endf;
    } else if (!strcmp(sformat,"AAC_HE_PS")) {
        iformat = MEDIA_FORMAT_AAC_HE_PS;
        goto endf;
    } else if (!strcmp(sformat,"MP3")) {
        iformat = MEDIA_FORMAT_MP3;
        goto endf;
    } else if (!strcmp(sformat,"VORBIS")) {
        iformat = MEDIA_FORMAT_VORBIS;
        goto endf;
    } else if (!strcmp(sformat,"FLAC")) {
        iformat = MEDIA_FORMAT_FLAC;
        goto endf;
    } else if (!strcmp(sformat,"WMAV1")) {
        iformat = MEDIA_FORMAT_WMAV1;
        goto endf;
    } else if (!strcmp(sformat,"WMAV2")) {
        iformat = MEDIA_FORMAT_WMAV2;
        goto endf;
    } else if (!strcmp(sformat,"WMAPRO")) {
        iformat = MEDIA_FORMAT_WMAPRO;
        goto endf;
    }

endf:
    LOGD("sformat : %x", iformat);
    return iformat;
}

int mc_ini_load(mc_ini_t *ini)
{
    gchar cname[CNAME_SIZE];
    int i = 0;
    dictionary *dict = NULL;

    static const int codec_list = sizeof(general_codec_list) / sizeof(general_codec_list[0]);

    _mc_ini_check_ini_status();

    /* first, try to load existing ini file */
    dict = iniparser_load(MEDIA_CODEC_INI_DEFAULT_PATH);

    /* if no file exists. create one with set of default values */
    if (!dict) {
#ifdef MEDIA_CODEC_DEFAULT_INI
        LOGD("No inifile found. codec will create default inifile.\n");
        if (FALSE == _generate_default_ini()) {
            LOGW("Creating default inifile failed. Media Codec will use default values.\n");
        } else {
            /* load default ini */
            dict = iniparser_load(MEDIA_CODEC_INI_DEFAULT_PATH);
        }
#else
        LOGD("No ini file found. \n");
        return LOGERROR_FILE_NOT_FOUND;
#endif
    }

    /* get ini values */
    memset(ini, 0, sizeof(mc_ini_t));

    if (dict) {/* if dict is available */
        /* general */
        MEDIA_CODEC_INI_GET_STRING( dict, ini->port_name, "port_in_use:media_codec_port",DEFAULT_PORT);
        /* codec */
        for (i = 0; i < codec_list; i++) {
            memset(cname, 0x00, CNAME_SIZE);
            snprintf(cname, CNAME_SIZE, "%s", general_codec_list[i].cname);
            int len = strlen(cname);

            ini->codec[i].codec_id =  general_codec_list[i].ctype;
            snprintf(cname+len, CNAME_SIZE-len, "%s", ":hw_decoder");
            MEDIA_CODEC_INI_GET_STRING_FROM_LIST( dict, ini->codec[i].codec_info[0], cname, DEFAULT_VALUE);
            snprintf(cname+len, CNAME_SIZE-len, "%s",":hw_encoder");
            MEDIA_CODEC_INI_GET_STRING_FROM_LIST( dict, ini->codec[i].codec_info[1], cname, DEFAULT_VALUE);
            snprintf(cname+len, CNAME_SIZE-len, "%s",":sw_decoder");
            MEDIA_CODEC_INI_GET_STRING_FROM_LIST( dict, ini->codec[i].codec_info[2], cname, DEFAULT_VALUE);
            snprintf(cname+len, CNAME_SIZE-len, "%s",":sw_encoder");
            MEDIA_CODEC_INI_GET_STRING_FROM_LIST( dict, ini->codec[i].codec_info[3], cname, DEFAULT_VALUE);
        }
    } else {/* if dict is not available just fill the structure with default value */

        LOGW("failed to load ini. using hardcoded default\n");
        /* general */
        snprintf(ini->port_name, sizeof(ini->port_name), "%s", DEFAULT_PORT);
        for (i = 0;i < codec_list; i++) {
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[0].name,   DEFAULT_HW_DECODER_NAME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[0].mime,   DEFAULT_HW_DECODER_MIME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[0].format, DEFAULT_HW_DECODER_FORMAT);

            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[1].name,   DEFAULT_HW_ENCODER_NAME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[1].mime,   DEFAULT_HW_ENCODER_MIME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[1].format, DEFAULT_HW_ENCODER_FORMAT);

            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[2].name,   DEFAULT_SW_DECODER_NAME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[2].mime,   DEFAULT_SW_DECODER_MIME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[2].format, DEFAULT_SW_DECODER_FORMAT);

            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[3].name,   DEFAULT_SW_ENCODER_NAME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[3].mime,   DEFAULT_SW_ENCODER_MIME);
            MEDIA_CODEC_GET_DEFAULT_LIST(  ini->codec[i].codec_info[3].format, DEFAULT_SW_ENCODER_FORMAT);
        }
    }

    if (0 == strcmp(ini->port_name, "GST_PORT"))
        ini->port_type = GST_PORT;
    else {
        LOGE("Invalid port is set to [%s] [%d]\n", ini->port_name,ini->port_type);
        iniparser_freedict(dict);
        goto ERROR;
    }
    LOGD("The port is set to [%s] [%d]\n", ini->port_name, ini->port_type);

    for (i = 0;i < codec_list; i++) {
        memset(cname, 0x00, CNAME_SIZE);
        snprintf(cname, CNAME_SIZE, "%s", general_codec_list[i].cname);
        int len = strlen(cname);

        snprintf(cname+len, CNAME_SIZE-len, "%s",":hw_decoder");
        MEDIA_CODEC_PRINT_LIST(ini->codec[i].codec_info[0],cname);
        snprintf(cname+len, CNAME_SIZE-len, "%s",":hw_encoder");
        MEDIA_CODEC_PRINT_LIST(ini->codec[i].codec_info[1],cname);
        snprintf(cname+len, CNAME_SIZE-len, "%s",":sw_decoder");
        MEDIA_CODEC_PRINT_LIST(ini->codec[i].codec_info[2],cname);
        snprintf(cname+len, CNAME_SIZE-len, "%s",":sw_encoder");
        MEDIA_CODEC_PRINT_LIST(ini->codec[i].codec_info[3],cname);
    }

    /* free dict as we got our own structure */
    iniparser_freedict(dict);

    /* dump structure */
    LOGD("codec settings -----------------------------------\n");

    /* general */
    LOGD("port_name: %s\n", ini->port_name);
    LOGD("port_type : %d\n", ini->port_type);

    return MC_ERROR_NONE;
ERROR:
    return MC_COURRPTED_INI;

}

static void _mc_ini_check_ini_status(void)
{
    struct stat ini_buff;

    if (g_stat(MEDIA_CODEC_INI_DEFAULT_PATH, &ini_buff) < 0) {
        LOGW("failed to get codec ini status\n");
    } else {
        if (ini_buff.st_size < 5) {
            LOGW("codec.ini file size=%d, Corrupted! So, Removed\n", (int)ini_buff.st_size);

            if (g_remove(MEDIA_CODEC_INI_DEFAULT_PATH) == -1) {
                LOGE("failed to delete corrupted ini");
            }
        }
    }
}

#ifdef MEDIA_CODEC_DEFAULT_INI
static gboolean _generate_default_ini(void)
{
    FILE *fp = NULL;
    gchar *default_ini = MEDIA_CODEC_DEFAULT_INI;

    /* create new file */
    fp = fopen(MEDIA_CODEC_INI_DEFAULT_PATH, "wt");

    if (!fp) {
        return FALSE;
    }

    /* writing default ini file */
    if (strlen(default_ini) !=
            fwrite(default_ini, 1, strlen(default_ini), fp)) {
        fclose(fp);
        return FALSE;
    }

    fclose(fp);
    return TRUE;
}
#endif

void _mc_create_decoder_map_from_ini(mc_handle_t *mediacodec)
{
    int indx = 0, count = 0;
    int codec_list = sizeof(general_codec_list) / sizeof(general_codec_list[0]);
    for (indx=0;indx <codec_list;indx++) {
        if (strcmp(mediacodec->ini.codec[indx].codec_info[0].name, "")) {
            mediacodec->decoder_map[count].id = mediacodec->ini.codec[indx].codec_id;
            mediacodec->decoder_map[count].hardware = 1; // hardware
            mediacodec->decoder_map[count].type.factory_name = mediacodec->ini.codec[indx].codec_info[0].name;
            mediacodec->decoder_map[count].type.mime = mediacodec->ini.codec[indx].codec_info[0].mime;
            mediacodec->decoder_map[count].type.out_format=  _mc_convert_media_format_str_to_int(mediacodec->ini.codec[indx].codec_info[0].format);
            count++;
        }

        if (strcmp(mediacodec->ini.codec[indx].codec_info[2].name, "")) {
            mediacodec->decoder_map[count].id = mediacodec->ini.codec[indx].codec_id;
            mediacodec->decoder_map[count].hardware = 0; // software
            mediacodec->decoder_map[count].type.factory_name = mediacodec->ini.codec[indx].codec_info[2].name;
            mediacodec->decoder_map[count].type.mime = mediacodec->ini.codec[indx].codec_info[2].mime;
            mediacodec->decoder_map[count].type.out_format=  _mc_convert_media_format_str_to_int(mediacodec->ini.codec[indx].codec_info[2].format);
            count++;
        }
    }
    mediacodec->num_supported_decoder = count;
    return;

}

void _mc_create_encoder_map_from_ini(mc_handle_t *mediacodec)
{
    int indx = 0, count = 0;
    int codec_list = sizeof(general_codec_list) / sizeof(general_codec_list[0]);

    for (indx=0;indx <codec_list;indx++) {
        if (strcmp(mediacodec->ini.codec[indx].codec_info[1].name, "")) {
            mediacodec->encoder_map[count].id = mediacodec->ini.codec[indx].codec_id;
            mediacodec->encoder_map[count].hardware = 1;
            mediacodec->encoder_map[count].type.factory_name = mediacodec->ini.codec[indx].codec_info[1].name;
            mediacodec->encoder_map[count].type.mime = mediacodec->ini.codec[indx].codec_info[1].mime;
            mediacodec->encoder_map[count].type.out_format=  _mc_convert_media_format_str_to_int(mediacodec->ini.codec[indx].codec_info[1].format);
            count++;
        }

        if (strcmp(mediacodec->ini.codec[indx].codec_info[3].name, "")) {
            mediacodec->encoder_map[count].id = mediacodec->ini.codec[indx].codec_id;
            mediacodec->encoder_map[count].hardware = 0;
            mediacodec->encoder_map[count].type.factory_name = mediacodec->ini.codec[indx].codec_info[3].name;
            mediacodec->encoder_map[count].type.mime = mediacodec->ini.codec[indx].codec_info[3].mime;
            mediacodec->encoder_map[count].type.out_format=  _mc_convert_media_format_str_to_int(mediacodec->ini.codec[indx].codec_info[3].format);
            count++;
        }
    }
    mediacodec->num_supported_encoder = count;
    return;

}
void _mc_create_codec_map_from_ini(mc_handle_t *mediacodec, mc_codec_spec_t *spec_emul)
{
    int indx = 0, count = 0;
    int codec_list = sizeof(general_codec_list) / sizeof(general_codec_list[0]);
    for (indx=0;indx <codec_list;indx++) {
        if (strcmp(mediacodec->ini.codec[indx].codec_info[0].name, "")) {
            spec_emul[count].codec_id = mediacodec->ini.codec[indx].codec_id;
            spec_emul[count].codec_type =  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW;
            spec_emul[count].port_type =  MEDIACODEC_PORT_TYPE_GST;
            count++;
        }
        if (strcmp(mediacodec->ini.codec[indx].codec_info[1].name, "")) {
            spec_emul[count].codec_id = mediacodec->ini.codec[indx].codec_id;
            spec_emul[count].codec_type =  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_HW;
            spec_emul[count].port_type =  MEDIACODEC_PORT_TYPE_GST;
            count++;
        }
        if (strcmp(mediacodec->ini.codec[indx].codec_info[2].name, "")) {
            spec_emul[count].codec_id = mediacodec->ini.codec[indx].codec_id;
            spec_emul[count].codec_type =  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW;
            spec_emul[count].port_type =  MEDIACODEC_PORT_TYPE_GST;
            count++;
        }
        if (strcmp(mediacodec->ini.codec[indx].codec_info[3].name, "")) {
            spec_emul[count].codec_id = mediacodec->ini.codec[indx].codec_id;
            spec_emul[count].codec_type =  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW;
            spec_emul[count].port_type =  MEDIACODEC_PORT_TYPE_GST;
            count++;
        }
    }

    mediacodec->num_supported_codecs = count;
    return;
}

#endif /* #ifdef _MEDIA_CODEC_INI_C_ */
