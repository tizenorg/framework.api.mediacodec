/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <Elementary.h>
#include <appcore-efl.h>

#include <media_codec.h>
#include <media_packet.h>
#include <media_codec_queue.h> // !!!! remove it
#include <media_codec_port.h> // !!!! remove it

//#include <media_codec_private.h>
//#include <media_codec_port_general.h>
//#include <media_codec_port_omx.h>
//#include <media_codec_port.h>
//#include <media_codec_util.h>

#define PACKAGE "media_codec_test"
#define TEST_FILE_SIZE	(10 * 1024 * 1024)		//10M - test case
#define MAX_STRING_LEN	256
#define MAX_HANDLE			10
#define DEFAULT_OUT_BUF_WIDTH   640
#define DEFAULT_OUT_BUF_HEIGHT  480
#define OUTBUF_SIZE (DEFAULT_OUT_BUF_WIDTH * DEFAULT_OUT_BUF_HEIGHT * 3 / 2)

#define DEFAULT_SAMPPLERATE   44100
#define DEFAULT_CHANNEL		    2
#define DEFAULT_BIT			    16
#define DEFAULT_BITRATE             128
#define DEFAULT_SAMPLEBYTE	    1024
#define ADTS_HEADER_SIZE            7

#define DUMP_OUTBUF         1
#define MAX_INPUT_BUF_NUM   20
#define USE_INPUT_QUEUE     1


//extern int file_handle_args (int argc, char ** argv, int flag);

/*
 * Test MAIN
 */

enum
{
    CURRENT_STATUS_MAINMENU,
    CURRENT_STATUS_FILENAME,
    CURRENT_STATUS_CREATE,
    CURRENT_STATUS_DESTROY,
    CURRENT_STATUS_SET_CODEC,
    CURRENT_STATUS_SET_VDEC_INFO,
    CURRENT_STATUS_SET_VENC_INFO,
    CURRENT_STATUS_SET_ADEC_INFO,
    CURRENT_STATUS_SET_AENC_INFO,
    CURRENT_STATUS_PREPARE,
    CURRENT_STATUS_UNPREPARE,
    CURRENT_STATUS_PROCESS_INPUT,
    CURRENT_STATUS_GET_OUTPUT,
    CURRENT_STATUS_RESET_OUTPUT_BUFFER,
    CURRENT_STATUS_SET_SIZE,
};

int g_menu_state = CURRENT_STATUS_MAINMENU;
int g_handle_num = 1;
static mediacodec_h g_media_codec[MAX_HANDLE] = {0};
char g_uri[MAX_STRING_LEN];
FILE *fp_src = NULL;
media_format_h input_fmt = NULL;
#if USE_INPUT_QUEUE
media_packet_h *input_buf = NULL;
#else
media_packet_h in_buf = NULL;
#endif
media_packet_h output_buf = NULL;
async_queue_t *input_avaliable = NULL;

GThread *pa_thread;
gint pa_running = 0;
uint64_t pts = 0;

static int width = DEFAULT_OUT_BUF_WIDTH;
static int height = DEFAULT_OUT_BUF_HEIGHT;
static float fps = 0;
static int target_bits = 0;

static int samplerate = DEFAULT_SAMPPLERATE;
static int channel = DEFAULT_CHANNEL;
static int bit = DEFAULT_BIT;
static int bitrate = DEFAULT_BITRATE;
static int samplebyte = DEFAULT_SAMPLEBYTE;
unsigned char buf_adts[ADTS_HEADER_SIZE];

media_format_mimetype_e mimetype;

int use_video = 0;
int use_encoder = 0;
int frame_count = 0;

#if DUMP_OUTBUF
FILE *fp_out = NULL;
#endif
static gpointer _feed_pa(gpointer data);
static void display_sub_basic();
static int _mediacodec_get_output(void);
static void _mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data);
static void _mediacodec_fill_buffer_cb(media_packet_h pkt, void *user_data);

static int _create_app(void *data)
{
    printf("My app is going alive!\n");
    return 0;
}

static int _terminate_app(void *data)
{
    printf("My app is going gone!\n");
    return 0;
}


struct appcore_ops ops = {
    .create = _create_app,
    .terminate = _terminate_app,
};

unsigned int bytestream2nalunit(FILE *fd, unsigned char* nal)
{
    int nal_length = 0;
    size_t result;
    int read_size = 1;
    unsigned char buffer[1000000];
    unsigned char val, zero_count, i;

    zero_count = 0;
    if (feof(fd))
        return 0;

    result = fread(buffer, 1, read_size, fd);
    if(result != read_size)
    {
        exit(1);
    }
    val = buffer[0];
    while (!val)
    {
        if ((zero_count == 2 || zero_count == 3) && val == 1)
        {
            break;
        }
        zero_count++;
        result = fread(buffer, 1, read_size, fd);

        if(result != read_size)
        {
            exit(1);
        }
        val = buffer[0];
    }
    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 1;
    zero_count = 0;
    while(1)
    {
        if (feof(fd))
            return nal_length;

        result = fread(buffer, 1, read_size, fd);
        if(result != read_size)
        {
            exit(1);
        }
        val = buffer[0];

        if (!val)
        {
            zero_count++;
        }
        else
        {
            if ((zero_count == 2 || zero_count == 3 || zero_count == 4) && (val == 1))
            {
                break;
            }
            else
            {
                for (i = 0; i<zero_count; i++)
                {
                    nal[nal_length++] = 0;
                }
                nal[nal_length++] = val;
                zero_count = 0;
            }
        }
    }

    fseek(fd, -(zero_count + 1), SEEK_CUR);

    return nal_length;
}

unsigned int bytestream2yuv420(FILE *fd, unsigned char* yuv)
{
    size_t result;
    int read_size;
    unsigned char buffer[1000000];

    if (feof(fd))
        return 0;

    read_size = width*height*3/2;

    result = fread(buffer, 1,read_size, fd);
    if(result != read_size)
    {
        exit(1);
    }

    memcpy(yuv, buffer, width*height*3/2);

    return width*height*3/2;
}

unsigned int extract_input_aacdec(FILE *fd, unsigned char* aacdata)
{
    int readsize;
    size_t result;
    unsigned char buffer[1000000];

    if (feof(fd))
        return 0;

    result = fread(buffer, 1,6, fd);
    if(result != 6)
    {
        exit(1);
    }

    if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
        readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
        result = fread(buffer+6, 1,(readsize - 6), fd);
        memcpy(aacdata, buffer,readsize);
    } else {
        readsize = 0;
        g_print("[FAIL] Not found aac frame sync.....\n");
    }

    return readsize;
}

unsigned int extract_input_aacenc(FILE *fd, unsigned char* rawdata)
{
    int readsize;
    size_t result;
    unsigned char buffer[1000000];

    if (feof(fd))
        return 0;

    readsize =  ((samplebyte*channel)*(bit/8));
    result = fread(buffer, 1, readsize, fd);
    if(result != readsize)
    {
        exit(1);
    }

    memcpy(rawdata, buffer,readsize);

    return readsize;
}

/**
 *  Add ADTS header at the beginning of each and every AAC packet.
 *  This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *  Note the packetLen must count in the ADTS header itself.
 **/
void add_adts_to_packet(unsigned char *buffer, int packetLen) {
    int profile = 2;    //AAC LC (0x01)
    int freqIdx = 3;    //48KHz (0x03)
    int chanCfg = 2;    //CPE (0x02)

    if (samplerate == 96000) freqIdx = 0;
    else if (samplerate == 88200) freqIdx = 1;
    else if (samplerate == 64000) freqIdx = 2;
    else if (samplerate == 48000) freqIdx = 3;
    else if (samplerate == 44100) freqIdx = 4;
    else if (samplerate == 32000) freqIdx = 5;
    else if (samplerate == 24000) freqIdx = 6;
    else if (samplerate == 22050) freqIdx = 7;
    else if (samplerate == 16000) freqIdx = 8;
    else if (samplerate == 12000) freqIdx = 9;
    else if (samplerate == 11025) freqIdx = 10;
    else if (samplerate == 8000) freqIdx = 11;

    if ((channel == 1) || (channel == 2))
        chanCfg = channel;

    // fill in ADTS data
    buffer[0] = (char)0xFF;
    buffer[1] = (char)0xF1;
    buffer[2] = (char)(((profile-1)<<6) + (freqIdx<<2) +(chanCfg>>2));
    buffer[3] = (char)(((chanCfg&3)<<6) + (packetLen>>11));
    buffer[4] = (char)((packetLen&0x7FF) >> 3);
    buffer[5] = (char)(((packetLen&7)<<5) + 0x1F);
    buffer[6] = (char)0xFC;
}

static void input_filepath(char *filename)
{
    int len = strlen(filename);
    int i = 0;

    if(len < 0 || len > MAX_STRING_LEN)
        return;

    for(i = 0; i < g_handle_num; i++)
    {
        if(g_media_codec[i] != NULL)
        {
            mediacodec_unprepare(g_media_codec[i]);
            mediacodec_destroy(g_media_codec[i]);
            g_media_codec[i] = NULL;
        }

        if (mediacodec_create(&g_media_codec[i]) != MEDIACODEC_ERROR_NONE)
        {
            g_print("mediacodec create is failed\n");
        }
    }
    //input_fmt = (media_format_s *) malloc(sizeof(media_format_s));
    //memset(input_fmt, 0, sizeof(media_format_s));
    media_format_create(&input_fmt);

#if DUMP_OUTBUF
    fp_out = fopen("/opt/usr/media/codec_dump.out", "wb");
#endif

    strncpy (g_uri, filename, len);

    return;
}

void _allocate_buf(void)
{
#if USE_INPUT_QUEUE
    int i = 0;

    // !!!! remove dependency on internal headers.
    input_avaliable = mc_async_queue_new();
    input_buf = (media_packet_h *)malloc(sizeof(media_packet_h)*MAX_INPUT_BUF_NUM);

    for (i = 0; i < MAX_INPUT_BUF_NUM; i++)
    {
        media_packet_create_alloc(input_fmt, NULL, NULL, &input_buf[i]);
        g_print("input queue buf = %p\n", input_buf[i]);
        mc_async_queue_push(input_avaliable, input_buf[i]);
    }
#else
    media_packet_create_alloc(input_fmt, NULL, NULL, &in_buf);
    //media_format_unref(input_fmt);
    g_print("input queue buf = %p\n", in_buf);
#endif
    return;
}

#if USE_INPUT_QUEUE
void _free_buf(void)
{
    int i = 0;

    if (input_avaliable)
        mc_async_queue_free(input_avaliable);

    if (input_buf)
    {
        for (i = 0; i < MAX_INPUT_BUF_NUM; i++)
        {
            if(input_buf[i])
            {
                media_packet_destroy(input_buf[i]);
            }
        }
        media_format_unref(input_fmt);
        input_fmt = NULL;
        free(input_buf);
        input_buf = NULL;
    }
    return;
}
#endif

static void _mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data)
{
    if (pkt != NULL)
    {
#if USE_INPUT_QUEUE
        media_packet_unset_flags(pkt, MEDIA_PACKET_CODEC_CONFIG);
        mc_async_queue_push(input_avaliable, pkt);
        g_print("availablebuf = %p\n", pkt);
#else
        g_print("Used input buffer = %p\n", pkt);
        media_packet_destroy(pkt);
#endif
    }
    return;
}

static void _mediacodec_fill_buffer_cb(media_packet_h pkt, void *user_data)
{
    if (pkt != NULL)
    {
        _mediacodec_get_output();
    }
    return;
}

static void _mediacodec_eos_cb(void *user_data)
{
    g_print("event : eos\n");
}

void _mediacodec_destroy(void)
{
    int i = 0;
    g_print("mediacodec_destroy\n");

    g_atomic_int_set(&pa_running, 0);
    g_thread_join(pa_thread);

    for (i = 0; i < g_handle_num; i++)
    {
        if(g_media_codec[i] != NULL)
        {
            //mediacodec_unprepare(g_media_codec[i]);
            mediacodec_destroy(g_media_codec[i]);
            g_media_codec[i] = NULL;
        }
    }
#if USE_INPUT_QUEUE
    _free_buf();
#endif
    if (fp_src)
        fclose(fp_src);
#if DUMP_OUTBUF
    if (fp_out)
        fclose(fp_out);
#endif
    return;
}

void _mediacodec_set_codec(int codecid, int flag)
{
    int encoder = 0;
    g_print("_mediacodec_configure\n");
    g_print("codecid = %x, flag = %d\n", codecid, flag);
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_codec(g_media_codec[0], (mediacodec_codec_type_e)codecid, flag);
        encoder = GET_IS_ENCODER(flag) ? 1 : 0;
        if (use_video)
        {
            if (encoder)
            {
                //input_fmt->mimetype |= MEDIA_FORMAT_RAW;
                //input_fmt->mimetype |= MEDIA_FORMAT_NV12;
                mimetype |= MEDIA_FORMAT_RAW;
                mimetype |= MEDIA_FORMAT_I420;
            }
            else
            {
                //input_fmt->mimetype |= MEDIA_FORMAT_H264_SP;
                mimetype |= MEDIA_FORMAT_H264_SP;
            }
            mimetype |= MEDIA_FORMAT_VIDEO;
        }
        else
        {
            if (encoder)
            {
                mimetype |= MEDIA_FORMAT_RAW;
                mimetype |= MEDIA_FORMAT_PCM;
            }
            else
            {
                mimetype |= MEDIA_FORMAT_AAC;
            }
            mimetype |= MEDIA_FORMAT_AUDIO;
            g_print(" [audio test] mimetype (0x%x)\n", mimetype);
        }
    }
    else
    {
        g_print("mediacodec handle is not created\n");
    }
    return;
}

void _mediacodec_set_vdec_info(int width, int height)
{
    g_print("_mediacodec_set_vdec_info\n");
    g_print("width = %d, height = %d\n", width, height);
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_vdec_info(g_media_codec[0], width, height);
        //input_fmt->detail.video.width = width;
        //input_fmt->detail.video.height = height;
    }
    else
    {
        g_print("mediacodec handle is not created\n");
    }
    return;
}

void _mediacodec_set_venc_info(int width, int height, float fps, int target_bits)
{
    g_print("_mediacodec_set_venc_info\n");
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_venc_info(g_media_codec[0], width, height, fps, target_bits);
        //input_fmt->detail.video.width = width;
        //input_fmt->detail.video.height = height;
    }
    else
    {
        g_print("mediacodec handle is not created\n");
    }
    return;
}

void _mediacodec_set_adec_info(int samplerate, int chnnel, int bit)
{
    g_print("_mediacodec_set_adec_info\n");
    g_print("samplerate = %d, channel = %d, bit = %d\n", samplerate, chnnel, bit);
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_adec_info(g_media_codec[0], samplerate, chnnel, bit);
    }
    else
    {
        g_print("mediacodec handle is not created\n");
    }
    return;
}

void _mediacodec_set_aenc_info(int samplerate, int chnnel, int bit, int bitrate)
{
    g_print("_mediacodec_set_aenc_info\n");
    g_print("samplerate = %d, channel = %d, bit = %d, bitrate = %d\n", samplerate, chnnel, bit, bitrate);
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_aenc_info(g_media_codec[0], samplerate, chnnel, bit, bitrate);
    }
    else
    {
        g_print("mediacodec handle is not created\n");
    }
    return;
}


void _mediacodec_prepare(void)
{
    int i = 0;
    int err = 0;
    if (use_video)
    {
        media_format_set_video_mime(input_fmt, mimetype);
        media_format_set_video_width(input_fmt, width);
        media_format_set_video_height(input_fmt, height);
        media_format_set_video_avg_bps(input_fmt, target_bits);
    }
    else
    {
        g_print(" [audio test] mimetype (0x%x), channel(%d), samplerate (%d), bit (%d)\n", mimetype, channel, samplerate, bit);
        media_format_set_audio_mime(input_fmt, mimetype);
        media_format_set_audio_channel(input_fmt, channel);
        media_format_set_audio_samplerate(input_fmt, samplerate);
        media_format_set_audio_bit(input_fmt, bit);
    }

    for (i=0; i < g_handle_num; i++)
    {
        if(g_media_codec[i] != NULL)
        {
            mediacodec_set_input_buffer_used_cb(g_media_codec[i], _mediacodec_empty_buffer_cb, g_media_codec[i]);
            mediacodec_set_output_buffer_available_cb(g_media_codec[i], _mediacodec_fill_buffer_cb, g_media_codec[i]);
            mediacodec_set_eos_cb(g_media_codec[i], _mediacodec_eos_cb, g_media_codec[i]);

            err = mediacodec_prepare(g_media_codec[i]);

            if (err != MEDIACODEC_ERROR_NONE)
            {
                g_print("mediacodec_prepare failed error = %d \n", err);
            }
#if USE_INPUT_QUEUE
            _allocate_buf();
#endif
        }
        else
        {
            g_print("mediacodec handle is not created\n");
        }
    }
    frame_count = 0;

    return;
}



void _mediacodec_unprepare(void)
{
    int i = 0;
    int err = 0;
    g_print("_mediacodec_unprepare\n");

    for (i=0; i < g_handle_num; i++)
    {
        if(g_media_codec[i] != NULL)
        {
            mediacodec_unset_input_buffer_used_cb(g_media_codec[i]);
            mediacodec_unset_output_buffer_available_cb(g_media_codec[i]);

            err = mediacodec_unprepare(g_media_codec[i]);
            if (err != MEDIACODEC_ERROR_NONE)
            {
                g_print("mediacodec_unprepare failed error = %d \n", err);
            }
        }
        else
        {
            g_print("mediacodec handle is not created\n");
        }
    }
    frame_count = 0;
    return;
}

int _mediacodec_process_input(void)
{
    g_print("_mediacodec_process_input\n");
    unsigned int buf_size = 0;
#if USE_INPUT_QUEUE
    media_packet_h in_buf = NULL;
#endif
    void *data = NULL;
    int ret = 0;

    if (g_media_codec[0] == NULL)
    {
        g_print("mediacodec handle is not created\n");
        return MEDIACODEC_ERROR_INVALID_PARAMETER;
    }

    if (fp_src == NULL)
    {
        fp_src = fopen(g_uri, "r");
        if (fp_src == NULL)
        {
            g_print("%s file open failed\n", g_uri);
            return MEDIACODEC_ERROR_INVALID_PARAMETER;
        }
    }
#if USE_INPUT_QUEUE
    in_buf = mc_async_queue_pop(input_avaliable);
#else
    _allocate_buf();
#endif


    if (in_buf != NULL)
    {
        media_packet_get_buffer_data_ptr(in_buf, &data);
        if(data == NULL)
            return MEDIACODEC_ERROR_INVALID_PARAMETER;

        if (use_encoder)
        {
            if (use_video)
            {
                buf_size = bytestream2yuv420(fp_src, data);
                media_packet_set_pts (in_buf, pts);
                g_print("input pts = %llu\n", pts);
                if (fps != 0)
                {
                    pts += (GST_SECOND / fps);
                }
            }
            else
            {
                buf_size = extract_input_aacenc(fp_src, data);
                media_packet_set_pts (in_buf, pts);
                g_print("input pts = %llu\n", pts);
                if (samplerate != 0)
                {
                    pts += ((GST_SECOND / samplerate) * samplebyte);
                }
            }
        }
        else
        {
            if (use_video)
            {
                if(frame_count == 0)
                    ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                else if(frame_count == 1258)
                    ret = media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);

                buf_size = bytestream2nalunit(fp_src, data);
            }
            else
                buf_size = extract_input_aacdec(fp_src, data);
        }

        if (buf_size == 0)
        {
          g_print("input file read failed\n");
          return MEDIACODEC_ERROR_INVALID_PARAMETER;
        }

        media_packet_set_buffer_size(in_buf, buf_size);
        g_print("%s - input_buf size = %d  (0x%x), frame_count : %d\n",__func__, buf_size, buf_size, frame_count);
        ret = mediacodec_process_input (g_media_codec[0], in_buf, 0);
        frame_count++;
        return ret;
    }

    return MEDIACODEC_ERROR_NONE;
}

int _mediacodec_get_output(void)
{
    int err = 0;
    uint64_t buf_size = 0;
    void *data = NULL;
    g_print("_mediacodec_get_output\n");
    if (g_media_codec[0] == NULL)
    {
        g_print("mediacodec handle is not created\n");
        return MEDIACODEC_ERROR_INVALID_PARAMETER;
    }

    err = mediacodec_get_output(g_media_codec[0], &output_buf, 0);
    if( err == MEDIACODEC_ERROR_NONE)
    {
        media_packet_get_buffer_size(output_buf, &buf_size);
        g_print("%s - output_buf size = %lld\n",__func__, buf_size);
#if DUMP_OUTBUF
        media_packet_get_buffer_data_ptr(output_buf, &data);
        if ((!use_video) && (use_encoder))
        {
            if (buf_size > 0)
            {
                add_adts_to_packet(buf_adts, (buf_size+ADTS_HEADER_SIZE));
                fwrite(&buf_adts[0], 1, ADTS_HEADER_SIZE, fp_out);
            }
        }

        if (data != NULL)
            fwrite(data, 1, buf_size, fp_out);
#endif
//        printf("%s - output_buf : %p\n",__func__, output_buf);
//        mediacodec_reset_output_buffer(g_media_codec[0], &output_buf);
        media_packet_destroy(output_buf);
    }
    else
    {
        g_print("get_output failed err = %d\n", err);
        return err;
    }

    return MEDIACODEC_ERROR_NONE;
}

void _mediacodec_reset_output_buffer(void)
{
    g_print("_media_codec_reset_output_buffer\n");
    if (g_media_codec[0] == NULL)
    {
        g_print("mediacodec handle is not created\n");
        return;
    }
/*
    if(output_buf == NULL)
    {
        g_print("output buffer is NULL");
        return;
    }
*/
    //mediacodec_reset_output_buffer(g_media_codec[0], &output_buf);
    return;
}

void _mediacodec_process_input_n(int num)
{
    int i = 0;
    int ret = 0;
    for (i =0; i < num; i++)
    {
        ret = _mediacodec_process_input();
        if (ret != 0)
            g_print ("_mediacodec_process_input err = %d\n", ret);
    }
    return;
}

void _mediacodec_get_output_n(int num)
{
    int i = 0;
    int ret = 0;
    for (i =0; i < num; i++)
    {
        ret = _mediacodec_get_output();
        if (ret != 0)
            g_print ("_mediacodec_get_output err = %d\n", ret);
    }
    return;
}

int _mediacodec_pa_runcheck(void)
{
    return pa_running;
}

static gpointer _feed_pa(gpointer data)
{
    int ret = 0;

    while (1) //FIXME crash g_atomic_int_get(&pa_running)
    {
        if (!_mediacodec_pa_runcheck())
            break;
        ret = _mediacodec_process_input();
        if (ret == MEDIACODEC_ERROR_INVALID_PARAMETER)
            break;
//        ret = _mediacodec_get_output();
        usleep(100);
    }
/*
    while (1) //FIXME crash g_atomic_int_get(&pa_running)
    {
        if (!_mediacodec_pa_runcheck())
            break;
        ret = _mediacodec_get_output();
        usleep(100);
    }
*/
    g_print("_feed_pa task finished\n");
    return NULL;
}

void _mediacodec_process_all(void)
{
    g_print("_mediacodec_process_all\n");
    pa_running = 1;
    pa_thread = g_thread_create(_feed_pa, GINT_TO_POINTER(1), TRUE, NULL);
    return;
}


void quit_program(void)
{
    int i = 0;

    for (i = 0; i < g_handle_num; i++)
    {
        if(g_media_codec[i]!=NULL)
        {
            mediacodec_unprepare(g_media_codec[i]);
            mediacodec_destroy(g_media_codec[i]);
            g_media_codec[i] = NULL;
        }
    }
    elm_exit();
}

void reset_menu_state()
{
    g_menu_state = CURRENT_STATUS_MAINMENU;
    return;
}

void _interpret_main_menu(char *cmd)
{
    int len =  strlen(cmd);
    if (len == 1)
    {
        if (strncmp(cmd, "a", 1) == 0)
        {
            g_menu_state = CURRENT_STATUS_FILENAME;
        }
        else if (strncmp(cmd, "o", 1) == 0)
        {
            g_menu_state = CURRENT_STATUS_GET_OUTPUT;
        }
        else if (strncmp(cmd, "q", 1) == 0)
        {
            quit_program();
        }
        else
        {
            g_print("unknown menu \n");
        }
    }
    else if (len == 2)
    {
        if (strncmp(cmd, "pr", 2) == 0)
        {
            _mediacodec_prepare();
        }
        else if (strncmp(cmd, "sc", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_SET_CODEC;
        }
        else if (strncmp(cmd, "vd", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_SET_VDEC_INFO;
        }
        else if (strncmp(cmd, "ve", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_SET_VENC_INFO;
        }
        else if (strncmp(cmd, "ad", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_SET_ADEC_INFO;
        }
        else if (strncmp(cmd, "ae", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_SET_AENC_INFO;
        }
        else if (strncmp(cmd, "pi", 2) == 0)
        {
            g_menu_state = CURRENT_STATUS_PROCESS_INPUT;
        }
        else if (strncmp(cmd, "rb", 2) == 0)
        {
            _mediacodec_reset_output_buffer();
        }
        else if (strncmp(cmd, "pa", 2) == 0)
        {
            _mediacodec_process_all();
        }
        else if (strncmp(cmd, "un", 2) == 0)
        {
            _mediacodec_unprepare();
        }
        else if (strncmp(cmd, "dt", 2) == 0)
        {
            _mediacodec_destroy();
        }
        else
        {
            g_print("unknown menu \n");
        }
    }
    else
    {
        g_print("unknown menu \n");
    }
    return;
}

static void displaymenu(void)
{
    if (g_menu_state == CURRENT_STATUS_MAINMENU)
    {
        display_sub_basic();
    }
    else if (g_menu_state == CURRENT_STATUS_FILENAME)
    {
        g_print("*** input mediapath.\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_CODEC)
    {
        g_print("*** Codec id : L16  = 1         Flags : MEDIACODEC_ENCODER = 1\n");
        g_print("               ALAW = 2                 MEDIACODEC_DECODER = 2\n");
        g_print("               ULAW = 3                 MEDIACODEC_SUPPORT_TYPE_HW = 4,\n");
        g_print("               AMR  = 4                 MEDIACODEC_SUPPORT_TYPE_SW = 8,\n");
        g_print("               G729 = 5                 MEDIACODEC_SUPPORT_TYPE_OMX = 16\n");
        g_print("               AAC  = 6                 MEDIACODEC_SUPPORT_TYPE_GEN = 32,\n");
        g_print("               MP3  = 7\n");
        g_print("               H261  = 101\n");
        g_print("               H263  = 102\n");
        g_print("               H264  = 103\n");
        g_print("               MJPEG = 104\n");
        g_print("               MPEG1 = 105\n");
        g_print("               MPEG2 = 106\n");
        g_print("               MPEG4 = 107\n");
        g_print("*** input codec id, falgs.\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_VDEC_INFO)
    {
        g_print("*** input video decode configure.(width, height)\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_VENC_INFO)
    {
        g_print("*** input video encode configure.(width, height, fps, target_bits)\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_ADEC_INFO)
    {
        g_print("*** input audio decode configure.(samplerate, channel, bit)\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_AENC_INFO)
    {
        g_print("*** input audio encode configure.(samplerate, channel, bit, bitrate)\n");
    }
    else if (g_menu_state == CURRENT_STATUS_PROCESS_INPUT)
    {
        g_print("*** input dec process number\n");
    }
    else if (g_menu_state == CURRENT_STATUS_GET_OUTPUT)
    {
        g_print("*** input get output buffer number\n");
    }
    else
    {
        g_print("*** unknown status.\n");
        quit_program();
    }
    g_print(" >>> ");
}

gboolean timeout_menu_display(void* data)
{
    displaymenu();
    return FALSE;
}


static void interpret (char *cmd)
{
    switch (g_menu_state)
    {
        case CURRENT_STATUS_MAINMENU:
            {
                _interpret_main_menu(cmd);
            }
            break;
        case CURRENT_STATUS_FILENAME:
            {
                input_filepath(cmd);
                reset_menu_state();
            }
            break;
        case CURRENT_STATUS_SET_CODEC:
            {
                static int codecid = 0;
                static int flag = 0;
                static int cnt = 0;

                int tmp;
                char **ptr = NULL;
                switch (cnt)
                {
                    case 0:
                        tmp = atoi(cmd);

                        if(tmp > 100)
                        {
                            tmp = strtol(cmd, ptr, 16);
                            codecid = 0x2000 + ((tmp & 0xFF) << 4);
                            use_video = 1;
                        }
                        else
                        {
                            codecid = 0x1000 + (tmp<<4);
                        }
                        cnt++;
                        break;
                    case 1:
                        flag = atoi(cmd);
                        if (GET_IS_ENCODER(flag))
                            use_encoder = 1;
                        else if (GET_IS_DECODER(flag))
                            use_encoder = 0;
                        _mediacodec_set_codec(codecid, flag);
                        reset_menu_state();
                        codecid = 0;
                        flag = 0;
                        cnt = 0;
                        break;
                    default:
                        break;
                }
            }
            break;
        case CURRENT_STATUS_SET_VDEC_INFO:
            {
                static int cnt = 0;
                switch (cnt)
                {
                    case 0:
                        width = atoi(cmd);
                        cnt++;
                        break;
                    case 1:
                        height = atoi(cmd);
                        _mediacodec_set_vdec_info(width, height);
                        reset_menu_state();
                        cnt = 0;
                        break;
                    default:
                        break;
                }
            }break;
        case CURRENT_STATUS_SET_VENC_INFO:
            {
                static int cnt = 0;
                switch (cnt) {
                    case 0:
                        width = atoi(cmd);
                        cnt++;
                        break;
                    case 1:
                        height = atoi(cmd);
                        cnt++;
                        break;
                    case 2:
                        fps = atol(cmd);
                        cnt++;
                        break;
                    case 3:
                        target_bits = atoi(cmd);
                        g_print("width = %d, height = %d, fps = %f, target_bits = %d\n", width, height, fps, target_bits);
                        _mediacodec_set_venc_info(width, height, fps, target_bits);
                        reset_menu_state();
                        cnt = 0;
                        break;
                    default:
                        break;
                }
            }
            break;
        case CURRENT_STATUS_SET_ADEC_INFO:
            {
                static int cnt = 0;
                switch (cnt)
                {
                    case 0:
                        samplerate = atoi(cmd);
                        cnt++;
                        break;
                    case 1:
                        channel = atoi(cmd);
                        cnt++;
                        break;
                    case 2:
                        bit = atoi(cmd);
                        _mediacodec_set_adec_info(samplerate, channel,bit);
                        reset_menu_state();
                        cnt = 0;
                        break;
                    default:
                        break;
                }
            }break;
        case CURRENT_STATUS_SET_AENC_INFO:
            {
                static int cnt = 0;
                switch (cnt)
                {
                    case 0:
                        samplerate = atoi(cmd);
                        cnt++;
                        break;
                    case 1:
                        channel = atoi(cmd);
                        cnt++;
                        break;
                    case 2:
                        bit = atoi(cmd);
                        cnt++;
                        break;
                    case 3:
                        bitrate = atoi(cmd);
                        _mediacodec_set_aenc_info(samplerate, channel,bit,bitrate);
                        reset_menu_state();
                        cnt = 0;
                        break;
                    default:
                        break;
                }
            }break;
        case CURRENT_STATUS_PROCESS_INPUT:
            {
                static int num = 0;
                num = atoi(cmd);
                _mediacodec_process_input_n(num);
                reset_menu_state();
            }
            break;
        case CURRENT_STATUS_GET_OUTPUT:
            {
                static int num = 0;
                num = atoi(cmd);
                _mediacodec_get_output_n(num);
                reset_menu_state();
            }
            break;
         default:
            break;
    }

    g_timeout_add(100, timeout_menu_display, 0);
}

static void display_sub_basic()
{
    g_print("\n");
    g_print("=========================================================================================\n");
    g_print("                                    media codec test\n");
    g_print("-----------------------------------------------------------------------------------------\n");
    g_print("a. Create \t");
    g_print("sc. Set codec \t\t");
    g_print("vd. Set vdec info w, h\t");
    g_print("ve. Set venc info \n");
    g_print("ad. Set adec info s, c, b\t");
    g_print("ae. Set aenc info \n");
    g_print("pr. Prepare \t");
    g_print("pi. Process input \t");
    g_print("o. Get output \t\t");
    g_print("rb. Reset output buffer \n");
    g_print("pa. Process all frames \n");
    g_print("un. Unprepare \t");
    g_print("dt. Destroy \t\t");
    g_print("q. quite test suite \t");
    g_print("\n");
    g_print("=========================================================================================\n");
}

gboolean input (GIOChannel *channel)
{
    gchar buf[MAX_STRING_LEN];
    gsize read;
    GError *error = NULL;

    g_io_channel_read_chars(channel, buf, MAX_STRING_LEN, &read, &error);
    buf[read] = '\0';
    g_strstrip(buf);
    interpret (buf);

    return TRUE;
}

int main(int argc, char *argv[])
{
    GIOChannel *stdin_channel;
    stdin_channel = g_io_channel_unix_new(0);
    g_io_channel_set_flags (stdin_channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch (stdin_channel, G_IO_IN, (GIOFunc)input, NULL);

    displaymenu();

    ops.data = NULL;

    return appcore_efl_main(PACKAGE, &argc, &argv, &ops);

}
