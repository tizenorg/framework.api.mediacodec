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

/*
 * 0  (disable)  :  used in  (*.aac_lc, adts)
 * 1  (enable)   : used in  (*.m4a, mp4), Need "codec_data"
 * TODO : AAC_CODECDATA_SIZE is temporal size (16 byte) for codec_data
 * TODO : WMA_CODECDATA_SIZE is temporal size (64 byte) for codec_data
 */
#if 1
    #define AAC_CODECDATA_SIZE    16
    #define WMA_CODECDATA_SIZE   64     /*MAX sizeof(WMAUDIO3WAVEFORMAT)*/
    #define VORBIS_CODECDATA_SIZE    4096
    #define FLAC_CODECDATA_SIZE    4096 /* MAX sizeof(2^24) */
#endif

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
int g_len,bMultipleFiles;
FILE *fp_src = NULL;
media_format_h input_fmt = NULL;
#if USE_INPUT_QUEUE
//media_packet_h *input_buf = NULL;
media_packet_h input_buf[MAX_INPUT_BUF_NUM];
#else
media_packet_h in_buf = NULL;
#endif
media_packet_h output_buf = NULL;
//async_queue_t *input_avaliable = NULL;
GQueue input_available;

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

unsigned char sps[100];
unsigned char pps[100];
unsigned char tmp_buf[1000000];
static int sps_len, pps_len;

media_format_mimetype_e mimetype;

int use_video = 0;
int use_encoder = 0;
int frame_count = 0;

#if DUMP_OUTBUF
int g_OutFileCtr;
FILE *fp_out = NULL;
#endif
//static gpointer _feed_pa(gpointer data);
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
    int nal_unit_type;
    int init;

    zero_count = 0;
    if (feof(fd))
        return -1;

    result = fread(buffer, 1, read_size, fd);

    if(result != read_size)
    {
        //exit(1);
        return -1;
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
            break;
        }
        val = buffer[0];
    }
    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 0;
    nal[nal_length++] = 1;
    zero_count = 0;
    init = 1;
    while(1)
    {
        if (feof(fd))
            return nal_length;

        result = fread(buffer, 1, read_size, fd);
        if(result != read_size)
        {
            break;
        }
        val = buffer[0];

        if(init) {
            nal_unit_type = val & 0xf;
            init = 0;
        }
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

    if (nal_unit_type == 0x7)
    {
        sps_len = nal_length;
        memcpy(sps, nal, nal_length);
        return 0;
    }
    else if (nal_unit_type == 0x8)
    {
        pps_len = nal_length;
        memcpy(pps, nal, nal_length);
        return 0;
    }
    else if (nal_unit_type == 0x5)
    {
        memcpy(tmp_buf, nal, nal_length);
        memcpy(nal, sps, sps_len);
        memcpy(nal + sps_len, pps, pps_len);
        memcpy(nal + sps_len + pps_len, tmp_buf, nal_length);
        nal_length += sps_len + pps_len;
    }

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
        //exit(1);
        return -1;
    }

    memcpy(yuv, buffer, width*height*3/2);

    return width*height*3/2;
}

int bytestream2h263unit(FILE *fd, unsigned char* p)
{
    int len = 0;
    size_t result;
    int read_size = 1, state = 1, bStart = 0;
    unsigned char val;

    if (feof(fd))
        return -1;

    while(1)
    {
        if(fread(&val, 1, read_size, fd) != read_size)
            return -1;
        p[len] = val;
        len += read_size;

        switch(state)
        {
        case 1:
            if(val==0x00)
                state++;
        break;
        case 2:
            if(val==0x00)
                state++;
            else
                state = 1;
        break;
        case 3:
            state = 1;
            if((val&0xFC)==0x80)
            {
                if(bStart)
                {
                    // Got next PSC
                    fseek(fd, -3, SEEK_CUR);
                    return len - 3;
                }
                else
                {
                    bStart = 1;
                    // Flush header
                    len = 3;
                    p[0] = p[1] = 0;
                    p[2] = 0x80;
                }
            }
        break;
        }
    }
}

int bytestream2mpeg4unit(FILE *fd, unsigned char* p,int* is_voss)
{
    int len = 0;
    size_t result;
    int read_size = 1, state = 1, bType=0;
    unsigned char val;

    if (feof(fd))
        return -1;

    while(1)
    {
        if(fread(&val, 1, read_size, fd) != read_size)
            return -1;
        p[len] = val;
        len += read_size;

        switch(state)
        {
        case 1:
            if(val==0x00)
                state++;
        break;
        case 2:
            if(val==0x00)
                state++;
            else
                state = 1;
        break;
        case 3:
            if(val==0x01)
                state++;
            else
                state = 1;
        break;
        case 4:
            state = 1;
            if(val==0xB0 || val==0xB6)
            {
                if(bType == 0xB6)
                {
                    // Got next VOSS/IVOP
                    fseek(fd, -4, SEEK_CUR);
                    return len - 4;
                }
                if(!bType)
                {
                    // Flush header
                    len = 4;
                    p[0] = p[1] = 0x00;
                    p[2] = 0x01;
                    p[3] = val;
                    if(is_voss && val==0xB0)
                        *is_voss = 1;
                }
                bType = val;
            }
        break;
        }
    }
}


/**
  * Extract Input data for MP3 decoder
  * (MPEG-1/2/2.5 layer 3)
  * As-Is  : Extractor code support only mp3 file exclude ID3tag, So mp3 file should start mp3 sync format. (0xffe0)
  * To-Be : Will support mp3 file include ID3tag (v1,v2)
  **/

static const guint mp3types_bitrates[2][3][16] = {
{
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,}
  },
{
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,}
  },
};

static const guint mp3types_freqs[3][3] = { {44100, 48000, 32000},
									 {22050, 24000, 16000},
									 {11025, 12000, 8000}
};

unsigned int extract_input_mp3dec(FILE *fd, unsigned char* mp3data)
{
    int readsize;
    size_t result;
    unsigned char buffer[1000000];
    guint header;
    guint padding, bitrate, lsf, layer, mpg25;
    guint hdr_bitrate, sf;

    /*
      * It is not support skip logicif ID3 Tag  and Xing Header.
      * So It MUST start as mp3 valid frame sequence.
      * Testsuit that are not guaranteed to be available on functionality of all General DEMUXER/PARSER.
     */

    if (feof(fd))
        return 0;

    result = fread(buffer, 1, 4, fd);   //mp3 header
    if(result != 4) {
        g_print ("[ERROR] fread size is %d\n", result);
        return -1;
    }

    header = GST_READ_UINT32_BE (buffer);
#if 1   //normal extract code
    if (header == 0) {
        g_print ("[ERROR] read header size is 0\n");
        return -1;
    }

    /* if it's not a valid sync */
    if ((header & 0xffe00000) != 0xffe00000) {
        g_print ("[ERROR] invalid sync\n");
        return -1;
    }

    /* if it's an invalid MPEG version */
    if (((header >> 19) & 3) == 0x1) {
        g_print ("[ERROR] invalid MPEG version: %d\n", (header >> 19) & 3);
        return -1;
    } else {
        if (header & (1 << 20)) {
            lsf = (header & (1 << 19)) ? 0 : 1;
            mpg25 = 0;
        } else {
            lsf = 1;
            mpg25 = 1;
        }
    }

    /* if it's an invalid layer */
    if (!((header >> 17) & 3)) {
        g_print("[ERROR] invalid layer: %d\n", (header >> 17) & 3);
        return -1;
    } else {
        layer = 4 - ((header >> 17) & 0x3);
    }

    /* if it's an invalid bitrate */
    if (((header >> 12) & 0xf) == 0xf) {
        g_print ("[ERROR] invalid bitrate: %d\n", (header >> 12) & 0xf);
        return -1;
    } else {
        bitrate = (header >> 12) & 0xF;
        hdr_bitrate = mp3types_bitrates[lsf][layer - 1][bitrate] * 1000;
        /* The caller has ensured we have a valid header, so bitrate can't be zero here. */
        if(hdr_bitrate == 0)
            return -1;
    }

    /* if it's an invalid samplerate */
    if (((header >> 10) & 0x3) == 0x3) {
        g_print ("[ERROR] invalid samplerate: %d\n", (header >> 10) & 0x3);
        return -4;
    } else {
        sf = (header >> 10) & 0x3;
        sf = mp3types_freqs[lsf + mpg25][sf];
    }

    padding = (header >> 9) & 0x1;

    switch (layer) {
    case 1:
        readsize = 4 * ((hdr_bitrate * 12) / sf + padding);
        break;
    case 2:
        readsize = (hdr_bitrate * 144) / sf + padding;
        break;
    default:
    case 3:
        readsize = (hdr_bitrate * 144) / (sf << lsf) + padding;
        break;
    }
#else   //simple extract code - hard coding test code for supporting only 'test.mp3'
     readsize = 1044 + ((header >> 9) & 0x1);     //only simple test => (1044 + padding)
#endif

    if (readsize > 0) {
        result = fread(buffer+4, 1, (readsize - 4), fd);
        memcpy(mp3data, buffer,readsize);
    } else {
        readsize = 0;
        g_print("[FAIL] Not found mp3 frame sync.....\n");
    }

    return readsize;
}


/**
  * Extract Input data for AAC decoder
  * (case of (LC profile) ADTS format)
  * codec_data : Don't need
  **/
unsigned int extract_input_aacdec(FILE *fd, unsigned char* aacdata)
{
    int readsize;
    size_t result;
    unsigned int header_size = ADTS_HEADER_SIZE;
    unsigned char buffer[1000000];

    if (feof(fd))
        return 0;

    result = fread(buffer, 1, header_size, fd);   //adts header
    if(result != header_size)
    {
        return -1;
    }

    if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
        readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
        result = fread(buffer + header_size, 1,(readsize - header_size), fd);
        memcpy(aacdata, buffer, readsize);
    } else {
        readsize = 0;
        g_print("[FAIL] Not found aac frame sync.....\n");
    }

    return readsize;
}

/**
  * Extract Input data for AAC decoder
  * (case of (AAC+/EAAC+ profile) RAW format)
  * codec_data : Need
  * profile : AAC_LC(2) AAC_HE(5), AAC_HE_PS (29)
  **/
unsigned int extract_input_aacdec_m4a_test(FILE *fd, unsigned char* aacdata)
{
    int readsize = 0;
    size_t result;
    unsigned int header_size = ADTS_HEADER_SIZE;
    unsigned char buffer[1000000];
    unsigned char codecdata[AAC_CODECDATA_SIZE] = {0,};

    /*
      * It is not support full parsing MP4 container box.
      * So It MUST start as RAW valid frame sequence.
      * Testsuit that are not guaranteed to be available on functionality of all General DEMUXER/PARSER.
     */

    if (feof(fd))
        return 0;

    if (frame_count == 0)
    {
        /*
          * CAUTION : Codec data is needed only once  in first time
          * Codec data is made(or extracted) by MP4 demuxer in 'esds' box.
          * So I use this data (byte) as hard coding for temporary our testing.
          */
#if 1
        /*
          * The codec_data data is according to AudioSpecificConfig,
          *  ISO/IEC 14496-3, 1.6.2.1
          *
          *  below example is test for using "test.aac" or "TestSample-AAC-LC.m4a"
          * case : M4A - LC profile
          * codec_data=(buffer)119056e5000000000000000000000000
          * savs aac decoder get codec_data. size: 16  (Tag size : 5 byte)
          *     - codec data: profile  : 2
          *     - codec data: samplrate: 48000
          *     - codec data: channels : 2
          */
        /* 2 bytes are mandatory */
        codecdata[0] = 0x11;         //ex) (5bit) 2 (LC) / (4bit) 3 (48khz)
        codecdata[1] = 0x90;         //ex) (4bit) 2 (2ch)
        /* othter bytes are (optional) epconfig information */
        codecdata[2] = 0x56;
        codecdata[3] = 0xE5;
        codecdata[4] = 0x00;
#else
        /*
          *  below example is test for using "TestSample-EAAC+.m4a"
          *
          * case : M4A - HE-AAC v1 and v2 profile
          * codec_data=(buffer)138856e5a54880000000000000000000
          * savs aac decoder get codec_data. size: 16  (Tag size : 7 byte)
          *     - codec data: profile  : 2
          *     - codec data: samplrate: 22050
          *     - codec data: channels : 1
          */
        /* 2 bytes are mandatory */
        codecdata[0] = 0x13;         //ex) (5bit) 2 (LC) / (4bit) 9 (22khz)
        codecdata[1] = 0x88;         //ex) (4bit) 1 (1ch)
        /* othter bytes are (optional) epconfig information */
        codecdata[2] = 0x56;
        codecdata[3] = 0xE5;
        codecdata[4] = 0xA5;
        codecdata[5] = 0x48;
        codecdata[6] = 0x80;
#endif

        memcpy(aacdata, codecdata, AAC_CODECDATA_SIZE);

        result = fread(buffer, 1, header_size, fd);   //adts header
        if(result != header_size)
        {
            return -1;
        }

        if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
            readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
            readsize = readsize -header_size;
            result = fread(buffer, 1, readsize, fd);    //Make only RAW data, so exclude header 7 bytes
            memcpy(aacdata+AAC_CODECDATA_SIZE, buffer, readsize);
        }

        g_print( "[example] Insert 'codec_data' in 1st frame buffer size (%d)\n", readsize+AAC_CODECDATA_SIZE);
        return (readsize + AAC_CODECDATA_SIZE);           //return combination of (codec_data + raw_data)
    }

    result = fread(buffer, 1, header_size, fd);   //adts header
    if(result != header_size)
    {
        exit(1);
    }

    if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
        readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
        readsize = readsize -header_size;
        result = fread(buffer, 1, readsize, fd);    //Make only RAW data, so exclude header 7 bytes
        memcpy(aacdata, buffer, readsize);
    } else {
        readsize = 0;
        g_print("[FAIL] Not found aac frame sync.....\n");
    }

    return readsize;            //return only raw_data
}

/**
  * Extract Input data for AMR-NB/WB decoder
  *  - AMR-NB  : mime type ("audio/AMR")          /   8Khz / 1 ch / 16 bits
  *  - AMR-WB : mime type ("audio/AMR-WB")  / 16Khz / 1 ch / 16 bits
  **/
static const char AMR_header [] = "#!AMR\n";
static const char AMRWB_header [] = "#!AMR-WB\n";
#define AMR_NB_MIME_HDR_SIZE          6
#define AMR_WB_MIME_HDR_SIZE          9
static const int block_size_nb[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
static const int block_size_wb[16] = { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, -1, -1, -1, -1, 0, 0 };
int *blocksize_tbl;

unsigned int extract_input_amrdec(FILE *fd, unsigned char* amrdata)
{
    int readsize = 0;
    size_t result;
    unsigned int mime_size = AMR_NB_MIME_HDR_SIZE;
    unsigned char buffer[1000000];
    unsigned int fsize, mode;

    if (feof(fd))
        return 0;

    if (frame_count == 0)
    {
        /* Check if the given data contains an AMR mime header. */
        result = fread(buffer, 1, mime_size, fd);   //amr-nb header
        if(result != mime_size)
            return -1;

        if ( !memcmp (buffer, AMR_header, AMR_NB_MIME_HDR_SIZE))
        {
            blocksize_tbl = (int *)block_size_nb;
            g_print("[----AMR-NB mime header detected----]\n");
        }
        else
        {
            result = fread(buffer + mime_size, 1, 3, fd);   //need more (3) bytes for checking amr-wb mime header
            if (!memcmp (buffer, AMRWB_header, AMR_WB_MIME_HDR_SIZE))
            {
                mime_size = AMR_WB_MIME_HDR_SIZE;
                blocksize_tbl = (int *)block_size_wb;
                g_print("[----AMR-WB mime header detected----]\n");
            }
            else
            {
                g_print("[ERROR] AMR-NB/WB don't detected..\n");
                return 0;
            }
        }
    }

    result = fread(buffer, 1, 1, fd);                   /* mode byte check */
    if(result != 1)
        return -1;
    if ((buffer[0] & 0x83) == 0)
    {
        mode = (buffer[0] >> 3) & 0x0F;            /* Yep. Retrieve the frame size */
        fsize = blocksize_tbl[mode];

        result = fread(buffer+1, 1, fsize, fd);     /* +1 for the previous mode byte */
        if(result != fsize)
            return -1;
        memcpy(amrdata, buffer, fsize);
        readsize = fsize + 1;
    } else {
        readsize = 0;
        g_print("[FAIL] Not found amr frame sync.....\n");
    }

    return readsize;
}

/**
  * Extract Input data for WMA decoder
  * codec_data : Need
  * format : WMAV1, WMAV2, WMAPRO, WMALSL
  **/
  #if 1
unsigned int extract_input_wmadec_test(FILE *fd, unsigned char* wmadata)
{
    int readsize = 0;
    size_t result = 0;
    unsigned char buffer[1000000];
    unsigned char codecdata[WMA_CODECDATA_SIZE] = {0,};

    /*
      * It is not support full parsing ASF container.
      * So It MUST start as each Payload sequence.
      * Testsuit that are not guaranteed to be available on functionality of all General DEMUXER/PARSER.
     */
    if (feof(fd))
        return 0;

    if (frame_count == 0)
    {
        /*  FIXME :  temp test ONLY using case of 'test_t1.wma' that Generates a file for testing purposes
          * This file is consist of
          *    - Only ES : each payload data (total = (16EA * 1024) bytes)
          * Stream Format is (Codec_data(64 bytes) + ES) + ES + ES + ES ..
        */
        codecdata[0] = 0x00;
        codecdata[1] = 0x00;
        codecdata[2] = 0x00;
        codecdata[3] = 0x00;
        codecdata[4] = 0x01;
        codecdata[5] = 0x00;
        codecdata[6] = 0x00;
        codecdata[7] = 0x00;
        codecdata[8] = 0x00;
        codecdata[9] = 0x00;

        memcpy(wmadata, codecdata, WMA_CODECDATA_SIZE);

        readsize = 1024;    /* payload size */
        result = fread(buffer, 1, readsize, fd);
        if(result != readsize)
            return -1;
        memcpy(wmadata+WMA_CODECDATA_SIZE, buffer, readsize);

        g_print( "[Info] Insert 'stream header (%d)' and 1st readsize (%d) in 1st frame buffer size (%d)\n",
                                        WMA_CODECDATA_SIZE, readsize, readsize+WMA_CODECDATA_SIZE);
        return ( WMA_CODECDATA_SIZE + readsize);           //return combination of (codec_data + raw_data)

    }

    readsize = 1024;    /* payload size */
    result = fread(buffer, 1, readsize, fd);
    if(result != readsize)
        return -1;
    memcpy(wmadata, buffer, readsize);

    return readsize;            //return only raw_data
}
#else
unsigned int extract_input_wmadec_test_1(FILE *fd, unsigned char* wmadata)
{
    int readsize = 0;
    size_t result = 0;
    unsigned char buffer[1000000];
    unsigned char codecdata[WMA_CODECDATA_SIZE] = {0,};

    if (feof(fd))
        return 0;

    if (frame_count == 0)
    {
        /*  FIXME :  temp test ONLY using case of 'test_t1_2.wma'
          * This file is consist of
          *    - Codec_data : 'Stream Type Specific(28 bytes)' in Stream Properties Object(ObjectID : B7DC0791-A9B7-11CF-8EE6-00C00C205365)
          *       ->  18 bytes (Stream Type Specific data) + 10 bytes (Extra data)
          *       ->  18 bytes is used for Caps
          *       ->  10 bytes is used for real codec_data
          *    - ES : each payload data (total = (16EA * 1024) bytes)
          *
          * Stream Format is (Codec_data(28 bytes) + ES) + ES + ES + ES ..
        */
        readsize = 28;    /* Stream Type Specific size */
        result = fread(codecdata, 1, readsize, fd);
        memcpy(wmadata, codecdata, WMA_CODECDATA_SIZE);

        readsize = 1024;    /* 1st payload size */
        result = fread(buffer, 1, readsize, fd);
        if(result != readsize)
            exit(1);
        memcpy(wmadata+WMA_CODECDATA_SIZE, buffer, readsize);

        g_print( "[example] Insert 'codec_data' in 1st frame buffer size (%d)\n", readsize+WMA_CODECDATA_SIZE);
        return ( WMA_CODECDATA_SIZE + readsize);           //return combination of (codec_data + raw_data)

    }

    readsize = 1024;    /* payload size */
    result = fread(buffer, 1, readsize, fd);
    memcpy(wmadata, buffer, readsize);

    return readsize;            //return only raw_data
}
#endif

/**
  * Extract Input data for VORBIS decoder
  * codec_data : Need as stream header pattern
  * format : audio/x-vorbis
  **/
static const int Key0_t1_segment_size_tbl[12] = { 1, 1, 1, 47, 45, 99, 73, 74, 91,146, 1 };     /* Only Used for key0_t1.ogg */
static const int Key0_t2_segment_size_tbl[15] = {30, 45, 3776, 1, 1, 1, 47, 45, 99, 73, 74, 91,146, 1 };     /* Only Used for key0_t1.ogg */
int *psize_tbl;
unsigned int extract_input_vorbisdec_test(FILE *fd, unsigned char* vorbisdata)
{
    int readsize = 0;
    size_t result = 0;
    unsigned char buffer[1000000];
    unsigned char codecdata[VORBIS_CODECDATA_SIZE] = {0,};

    /*
      * It is not support full parsing OGG container.
      * So It MUST start as each Segment sequence.
      * Testsuit that are not guaranteed to be available on functionality of all General DEMUXER/PARSER.
     */
    if (feof(fd)) {
        g_print("%s - [WARN][EOF] at frame_count (%4d)\n",__func__, frame_count);
        return 0;
    }

    if (frame_count == 0)
    {
        /*  FIXME :  temp test ONLY using case of 'key0_t1.ogg' that Generates a file for testing purposes
          * This file is consist of
          *    - Codec_data : 'vorbis header(3851 bytes)' in evey packet starts with a packet type byte(0x01, 0x03,0x05) and the 6 byte indentification string "vorbis"
          *       ->      30 bytes is used for Identifcation Header
          *       ->      45 bytes is used for Comment Header
          *       ->  3776 bytes is used for Setup Header
          *    - Only ES : each Segment data (total = 579 bytes for 10 frames)
          * Stream Format is (Codec_data(4096 bytes) + ES) + ES + ES + ES ..
        */

        readsize = 3851;    /* stream header size */
        result = fread(codecdata, sizeof(char), readsize, fd);
        if(result != readsize) {
            g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize, frame_count);
            return -1;
        }
        memcpy(vorbisdata, codecdata, VORBIS_CODECDATA_SIZE);

#if 0
        psize_tbl =  (int *)Key0_t1_segment_size_tbl;
#else
        fseek(fd, 0, SEEK_SET);
        psize_tbl =  (int *)Key0_t2_segment_size_tbl;
#endif

        readsize = psize_tbl[frame_count];    /* 1st segment size */
        result = fread(buffer, 1, readsize, fd);
        if(result != readsize) {
            g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize, frame_count);
            return -1;
        }
        memcpy(vorbisdata+VORBIS_CODECDATA_SIZE, buffer, readsize);

        g_print( "[Info] Insert 'stream header (%d)' and 1st readsize (%d) in 1st frame buffer size (%d)\n",
                                        VORBIS_CODECDATA_SIZE, readsize, readsize+VORBIS_CODECDATA_SIZE);
        return (VORBIS_CODECDATA_SIZE + readsize);           //return combination of (codec_data + raw_data)

    }

    readsize = psize_tbl[frame_count];     /* next segment size */
    result = fread(buffer, 1, readsize, fd);
    if(result != readsize) {
        g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize, frame_count);
        return -1;
    }
    memcpy(vorbisdata, buffer, readsize);

    return readsize;            //return only raw_data
}

/**
  * Extract Input data for FLAC decoder
  * codec_data : Need as stream header pattern
  * format : audio/x-flac
  **/
static const int key0_segment_size_tbl[3+3] = { 4, 38, 44, 1940, 774 };     /* Only Used for key0.flac */
static const int test_segment_size_tbl[3+11] = { 4, 38, 151, 6314, 5769, 6294, 5780, 6597, 9718, 8691, 8180, 7443, 7953, };     /* Only Used for test.flac */
int *flac_psize_tbl;
unsigned int extract_input_flacdec_test(FILE *fd, unsigned char* flacdata)
{
    int readsize = 0;
    size_t result = 0;
    unsigned char buffer[1000000];
    unsigned char codecdata[FLAC_CODECDATA_SIZE] = {0,};
    int codecdata_size = 0;
    guint readsize1 = 0;
    guint readsize2 = 0;

    /*
      * It is not support full parsing FLAC container.
      * So It MUST start as each valid flac stream sequence.
      * Testsuit that are not guaranteed to be available on functionality of all General FLAC PARSER.
     */
    if (feof(fd)) {
        g_print("%s - [WARN][EOF] at frame_count (%4d)\n",__func__, frame_count);
        return 0;
    }

    if (frame_count == 0)
    {
        /*  FIXME :  temp test ONLY using case of 'key0.flac' that original file for testing purposes
          * This file is consist of
          *    - Stream:  METADATA_BLOCK (INFO + COMMRNT) + FRAME (two frame (1940 + 774) bytes)
          * Stream Format is (Codec_data(51 bytes) + ES) + ES..
        */

        /* Pre-append : add marker is madatory */
        codecdata[0] = 0x7f;
        codecdata[1] = 0x46;    /* F */
        codecdata[2] = 0x4c;    /* L*/
        codecdata[3] = 0x41;    /* A */
        codecdata[4] = 0x43;    /* C */
        codecdata[5] = 0x01;    /* mapping version major */
        codecdata[6] = 0x00;    /* mapping version minor */
        codecdata[7] = 0x00;
        codecdata[8] = 0x02;

        codecdata_size = 9;
        memcpy(flacdata, codecdata, codecdata_size);

        /* case1 : using fixed method of codec data size */
        /* For 1st stream header */
        readsize1 = 42;       /* 'fLaC' +  STREAMINFO (38) */
        result = fread(codecdata + codecdata_size, 1, readsize1, fd);
        if(result != readsize1) {
            g_print("%s - [ERROR] fread fail : readsize1 (%4d) at  frame_count (%4d) \n",__func__, readsize1, frame_count);
            return -1;
        }

        /* For check valid marker */
        if ((codecdata[9] == 0x66) && (codecdata[10] == 0x4c)  && (codecdata[11] == 0x61)  && (codecdata[12] == 0x43))
        {
            codecdata_size += readsize1;
        } else {
            g_print ("[ERROR] invalid maker\n");
            return 0;
        }

        /* For 2nd stream header */
        readsize2 = 4;
        result = fread(codecdata + codecdata_size, 1, readsize2, fd);
        if(result != readsize2) {
            g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize2, frame_count);
            return -1;
        }
        if ((codecdata[51] & 0x7f) == 4)
        {
            codecdata_size += readsize2;
            /* For size of 2nd stream header  */
            readsize2 = ((codecdata[52] << 16) | (codecdata[53] << 8) | (codecdata[54]));
            result = fread(codecdata + codecdata_size, 1, readsize2, fd);
            codecdata_size += readsize2;
            if(result != readsize2) {
                g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize2, frame_count);
                return -1;
            }
            memcpy(flacdata, codecdata, FLAC_CODECDATA_SIZE);
        } else {
            g_print ("[ERROR] invalid type(4) of vorbis comment \n");
            return 0;
        }

        /* codec plugins don't used streamheader, So It want to fead direct file sequence */
        fseek(fd, 0, SEEK_SET);
        flac_psize_tbl =  (int *)key0_segment_size_tbl;

        readsize = flac_psize_tbl[frame_count];    /* 1st segment size */
        result = fread(buffer, 1, readsize, fd);
        if(result != readsize) {
            g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize, frame_count);
            return -1;
        }
        memcpy(flacdata + FLAC_CODECDATA_SIZE, buffer, readsize);

        g_print( "[Info] Insert 'stream header (%d)' and 1st readsize (%d) in 1st frame buffer size (%d)\n",
                                        FLAC_CODECDATA_SIZE, readsize, readsize+FLAC_CODECDATA_SIZE);
        return ( FLAC_CODECDATA_SIZE + readsize);           //return combination of (codec_data + raw_data)
    }

    readsize = flac_psize_tbl[frame_count];     /* next segment size */
    result = fread(buffer, 1, readsize, fd);
    if(result != readsize) {
        g_print("%s - [ERROR] fread fail : readsize (%4d) at  frame_count (%4d) \n",__func__, readsize, frame_count);
        return -1;
    }
    memcpy(flacdata, buffer, readsize);

    return readsize;            //return only raw_data
}

/**
  * Extract Input data for AAC encoder
  **/
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
        return -1;
    }

    memcpy(rawdata, buffer,readsize);

    return readsize;
}

/**
 *  Add ADTS header at the beginning of each and every AAC packet.
 *  This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *  Note the packetLen must count in the ADTS header itself.
 **/
void add_adts_header_for_aacenc(unsigned char *buffer, int packetLen) {
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


/**
  * Extract Input data for AMR-NB/WB encoder
  * But, WB encoder is not support because we don't have WB decoder for reference.
  **/
#define AMRNB_PCM_INPUT_SIZE      320
#define AMRWB_PCM_INPUT_SIZE      640
#define AMRNB_ENC_TEST  1               /* 1: AMR-NB , 0: AMR-WB*/
int write_amr_header = 1;                   /* write  magic number for AMR Header at one time */
unsigned int extract_input_amrenc(FILE *fd, unsigned char* rawdata, int is_amr_nb)
{
    int readsize;
    size_t result;
    unsigned char buffer[1000000];

    if (feof(fd))
        return 0;

    if (is_amr_nb)
        readsize =  AMRNB_PCM_INPUT_SIZE;
    else
        readsize =  AMRWB_PCM_INPUT_SIZE;

    result = fread(buffer, 1, readsize, fd);
    if(result != readsize)
    {
        return -1;
    }

    memcpy(rawdata, buffer,readsize);

    if (frame_count == 0)
    {
        g_print("amr encoder input size (%d) - NB(320) / WB(640)\n", readsize);
    }

    return readsize;
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
    g_OutFileCtr = 0;
#endif

    strncpy (g_uri, filename, len);
    g_len = len;

    bMultipleFiles = 0;
    if(g_uri[g_len-1] == '/')
        bMultipleFiles = 1;

    return;
}

void _allocate_buf(void)
{
#if USE_INPUT_QUEUE
    int i = 0;

    // !!!! remove dependency on internal headers.
    //input_avaliable = mc_async_queue_new();
    //input_buf = (media_packet_h *)malloc(sizeof(media_packet_h)*MAX_INPUT_BUF_NUM);

    for (i = 0; i < MAX_INPUT_BUF_NUM; i++)
    {
        media_packet_create_alloc(input_fmt, NULL, NULL, &input_buf[i]);
        g_print("input queue buf = %p\n", input_buf[i]);
        //mc_async_queue_push(input_avaliable, input_buf[i]);
        g_queue_push_tail(&input_available, input_buf[i]);
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

        //mc_async_queue_free(input_avaliable);

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
        //free(input_buf);
        //input_buf = NULL;
    }
    g_queue_clear(&input_available);
    return;
}
#endif

static void _mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data)
{
    if (pkt != NULL)
    {
#if USE_INPUT_QUEUE
        media_packet_unset_flags(pkt, MEDIA_PACKET_CODEC_CONFIG);
        //mc_async_queue_push(input_avaliable, pkt);
        g_queue_push_tail(&input_available, pkt);
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

static bool _mcdiacodec_supported_cb(mediacodec_codec_type_e type, void *user_data)
{
    if(type != -1)
      g_printf("type : %x\n", type);
    return true;
}

void _mediacodec_destroy(void)
{
    int i = 0;
    g_print("mediacodec_destroy\n");

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
    g_print("codecid = 0x%x, flag = %d\n", codecid, flag);
    if (g_media_codec[0] != NULL)
    {
        mediacodec_set_codec(g_media_codec[0], (mediacodec_codec_type_e)codecid, flag);
        encoder = GET_IS_ENCODER(flag) ? 1 : 0;
        if (use_video)
        {
            if (encoder)
            {
                //input_fmt->mimetype |= MEDIA_FORMAT_RAW;
                mimetype |= MEDIA_FORMAT_NV12;
                mimetype |= MEDIA_FORMAT_RAW;
                //mimetype |= MEDIA_FORMAT_I420;
            }
            else
            {
                //input_fmt->mimetype |= MEDIA_FORMAT_H264_SP;
                if(codecid == MEDIACODEC_MPEG4)
                    mimetype |= MEDIA_FORMAT_MPEG4_SP;
                else if(codecid == MEDIACODEC_H263)
                    mimetype |= MEDIA_FORMAT_H263;
                else
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
                if (codecid == MEDIACODEC_AAC)                            /* same as MEDIACODEC_AAC_LC */
                    mimetype |= MEDIA_FORMAT_AAC;                   /* MPEG-2/4 : (*.aac, adts), need adts header */
                else if (codecid == MEDIACODEC_AAC_HE)
                    mimetype |= MEDIA_FORMAT_AAC_HE;             /* MPEG-4 : (*.m4a, mp4) */
                else if (codecid == MEDIACODEC_AAC_HE_PS)
                    mimetype |= MEDIA_FORMAT_AAC_HE_PS;      /* MPEG-4 : (*.m4a, mp4) */
                else if (codecid == MEDIACODEC_MP3)
                    mimetype |= MEDIA_FORMAT_MP3;
                else if (codecid == MEDIACODEC_AMR_NB)
                    mimetype |= MEDIA_FORMAT_AMR_NB;
                else if (codecid == MEDIACODEC_AMR_WB)
                    mimetype |= MEDIA_FORMAT_AMR_WB;
                else if (codecid == MEDIACODEC_VORBIS)
                    mimetype |= MEDIA_FORMAT_VORBIS;
                else if (codecid == MEDIACODEC_FLAC)
                    mimetype |= MEDIA_FORMAT_FLAC;
                else if (codecid == MEDIACODEC_WMAV1)
                    mimetype |= MEDIA_FORMAT_WMAV1;
                else if (codecid == MEDIACODEC_WMAV2)
                    mimetype |= MEDIA_FORMAT_WMAV2;
                else if (codecid == MEDIACODEC_WMAPRO)
                    mimetype |= MEDIA_FORMAT_WMAPRO;
                else if (codecid == MEDIACODEC_WMALSL)
                    mimetype |= MEDIA_FORMAT_WMALSL;
            }
            mimetype |= MEDIA_FORMAT_AUDIO;
            g_print("[audio test] mimetype (0x%x)\n", mimetype);
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

            mediacodec_foreach_supported_codec(g_media_codec[i], _mcdiacodec_supported_cb, g_media_codec[i]);

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

int extract_input_per_frame (FILE *fp, unsigned char *rawdata)
{
    int readsize=0;
    size_t result = 0;
    while (!feof(fp))
        readsize += fread(rawdata+readsize, 1, 1, fp);
    return readsize;
}
int _mediacodec_process_input(void)
{
//    g_print("_mediacodec_process_input (frame_count :%d)\n", frame_count);
    int buf_size = 0;
#if USE_INPUT_QUEUE
    media_packet_h in_buf = NULL;
#endif
    void *data = NULL;
    int ret = 0, codec_config=0;
    mediacodec_s * handle = NULL;
    mc_handle_t* mc_handle = NULL;
    int err = MEDIACODEC_ERROR_NONE;

    if (g_media_codec[0] == NULL)
    {
        g_print("mediacodec handle is not created\n");
        return MEDIACODEC_ERROR_INVALID_PARAMETER;
    }
    else
    {
        handle = (mediacodec_s *) g_media_codec[0];
        mc_handle = (mc_handle_t*) handle->mc_handle;
    }

    if (fp_src == NULL)
    {
        if (bMultipleFiles)
        {
            if(g_uri[g_len-1] != '/')
                g_uri[g_len++] = '/';
            sprintf(g_uri+g_len, "%05d", frame_count);
        }
        fp_src = fopen(g_uri, "r");
        if (fp_src == NULL)
        {
            g_print("%s file open failed\n", g_uri);
            return MEDIACODEC_ERROR_INVALID_PARAMETER;
        }
    }
#if USE_INPUT_QUEUE
    //in_buf = mc_async_queue_pop(input_avaliable);
    in_buf = g_queue_pop_head(&input_available);
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
                /*
                  * Video Encoder
                  */
                if (bMultipleFiles)
                {
                    buf_size = extract_input_per_frame(fp_src, data);
                    fclose(fp_src);
                    sprintf(g_uri+g_len, "%05d", frame_count+1);
                    fp_src = fopen(g_uri, "r");
                    if (fp_src == NULL)
                    {
                        media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
                        err = MEDIACODEC_ERROR_INVALID_INBUFFER;
                    }
                }
                else
                {
                    buf_size = bytestream2yuv420(fp_src, data);
                }
                media_packet_set_pts (in_buf, pts);
                g_print("input pts = %llu\n", pts);
                if (fps != 0)
                {
                    pts += (GST_SECOND / fps);
                }
            }
            else
            {
                /*
                  * Aduio Encoder - AAC /AMR-NB
                  */
                if (mc_handle->codec_id == MEDIACODEC_AAC_LC)
                {
                    buf_size = extract_input_aacenc(fp_src, data);
                    media_packet_set_pts (in_buf, pts);
                    g_print("input pts = %llu\n", pts);
                    if (samplerate != 0)
                    {
                        pts += ((GST_SECOND / samplerate) * samplebyte);
                    }
                }
                else if(mc_handle->codec_id == MEDIACODEC_AMR_NB)
                {
                    buf_size = extract_input_amrenc(fp_src, data, 1);
                    media_packet_set_pts (in_buf, pts);
                    g_print("input pts = %llu\n", pts);
                    pts += (GST_SECOND / 50);    /* (20ms/frame) AMR_FRAMES_PER_SECOND = 50 */
                }
                else
                {
                    g_print(" [Input process] Not Suppor Audio Encodert!!!!! - mimetype (0x%x) codecid (0x%x)\n", mimetype, mc_handle->codec_id);
                }
            }
        }
        else
        {
            if (use_video)
            {
                /*
                  * Video Decoder
                  */
                //else if(frame_count == 150)
                //    ret = media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
                switch(mimetype)
                {
                    case MEDIA_FORMAT_H264_SP:
                    case MEDIA_FORMAT_H264_MP:
                    case MEDIA_FORMAT_H264_HP:
                    {
                        buf_size = bytestream2nalunit(fp_src, data);

                        if(frame_count == 2)
                            ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                    }
                    break;

                    case MEDIA_FORMAT_MPEG4_SP:
                    case MEDIA_FORMAT_MPEG4_ASP:
                        codec_config = 0;
                        if(bMultipleFiles)
                        {
                            buf_size = extract_input_per_frame(fp_src, data);
                            /* check for voss for setting codec_data flag 0x000001B0 */
                            if(*(int*)data==0xb0010000)
                                codec_config = 1;
                            fclose(fp_src);
                            sprintf(g_uri+g_len, "%05d", frame_count+1);
                            fp_src = fopen(g_uri, "r");

                            if (fp_src == NULL)
                            {
                                media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
                                err = MEDIACODEC_ERROR_INVALID_INBUFFER;
                            }
                        }
                        else
                        {
                             buf_size = bytestream2mpeg4unit(fp_src,data,&codec_config);
                        }
                        if(codec_config)
                        {
                            ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                            g_print("%s MPEG4 frame Set Codec Config\n",__func__);
                        }
                    break;

                    case MEDIA_FORMAT_H263:
                    case MEDIA_FORMAT_H263P:
                        if(bMultipleFiles)
                        {
                            buf_size = extract_input_per_frame(fp_src, data);
                            fclose(fp_src);
                            sprintf(g_uri+g_len, "%05d", frame_count+1);
                            fp_src = fopen(g_uri, "r");
                            if (fp_src == NULL)
                            {
                                media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
                                err = MEDIACODEC_ERROR_INVALID_INBUFFER;
                            }
                        }
                        else
                        {
                             buf_size = bytestream2h263unit(fp_src,data);
                        }
                    break;

                    default:
                     g_print("packet feeding not support for this codec type\n",__func__);
                    break;
                }
//mc_hex_dump("nal",data, 16);
            }
            else
            {
                    /*
                      * Audio Decoder - MP3
                      *                          - AAC_LC (adts)
                      *                          - AAC_LC (raw) / AAC_HE (v1) / AAC_HE_PS (v2)
                      *                          - AMR-NB / AMR-WB
                      */
                if (mimetype == MEDIA_FORMAT_AAC_LC)
                {
#if 1       /* ORG */
                    /* It is available case of  MEDIA_FORMAT_AAC_LC (ADTS) */
                    buf_size = extract_input_aacdec(fp_src, data);
#else   /* temp TEST (aac-lc raw) using codec data */
                    /* It is also available case of  MEDIA_FORMAT_AAC_LC (RAW) */
                    if(frame_count[i] == 0) {
                        g_print("%s -[LEO] Enable 'AAC_LC_RAW' for *.mp4 or *.m4a case\n",__func__);
                        ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                        //To-Do for inserting codec_data
                    }
                    buf_size = extract_input_aacdec_m4a_test(fp_src, data, i);
#endif
                }
                else if (mimetype == MEDIA_FORMAT_AAC_HE || mimetype == MEDIA_FORMAT_AAC_HE_PS)
                {
                    if(frame_count == 0) {
                        g_print("%s -Enable 'HE_AAC_V12_ENABLE' for *.mp4 or *.m4a case\n",__func__);
                        ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                        //To-Do for inserting codec_data
                    }
                    buf_size = extract_input_aacdec_m4a_test(fp_src, data);
                }
                else if (mimetype == MEDIA_FORMAT_MP3)
                {
                     buf_size = extract_input_mp3dec(fp_src, data);
                }
                else if (mimetype == MEDIA_FORMAT_AMR_NB || mimetype == MEDIA_FORMAT_AMR_WB)
                {
                     buf_size = extract_input_amrdec(fp_src, data);
                }
                else if (mimetype == MEDIA_FORMAT_WMAV1  || mimetype == MEDIA_FORMAT_WMAV2  ||
                         mimetype == MEDIA_FORMAT_WMAPRO || mimetype == MEDIA_FORMAT_WMALSL)
                {
                    if (frame_count == 0) {
                        g_print("%s -Enable 'MEDIA_PACKET_CODEC_CONFIG' for *.wma case\n",__func__);
                        ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                        //Need 'To-Do' for inserting codec_data
                    }
                    buf_size = extract_input_wmadec_test(fp_src, data);
                }
                else if (mimetype == MEDIA_FORMAT_VORBIS)
                {
                    if (frame_count == 0) {
                        g_print("%s -Enable 'MEDIA_PACKET_CODEC_CONFIG' for *.ogg case\n",__func__);
                        ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                        //Need 'To-Do' for inserting streamheader
                    }
                    buf_size = extract_input_vorbisdec_test(fp_src, data);
                }
                else if (mimetype == MEDIA_FORMAT_FLAC)
                {
                    if (frame_count == 0) {
                        g_print("%s -Enable 'MEDIA_PACKET_CODEC_CONFIG' for *.flac case\n",__func__);
                        ret = media_packet_set_flags(in_buf, MEDIA_PACKET_CODEC_CONFIG);
                        //Need 'To-Do' for inserting streamheader
                    }
                    buf_size = extract_input_flacdec_test(fp_src, data);
                }
                else
                {
                    g_print(" [Input process] Not Suppor Audio Decodert!!!!! - mimetype (0x%x) codecid (0x%x)\n", mimetype, mc_handle->codec_id);
                }
            }
        }

        if (buf_size >= 0)
        {
            if (use_video && buf_size == 4)
            {
                media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
                media_packet_set_buffer_size(in_buf, 4);
                mediacodec_process_input (g_media_codec[0], in_buf, 0);
                g_printf("eos packet is sent\n");

                return MEDIACODEC_ERROR_INVALID_INBUFFER;
            }
            media_packet_set_buffer_size(in_buf, buf_size);
            g_print("%s - input_buf size = %4d  (0x%x) at %4d frame, %p\n",__func__, buf_size, buf_size, frame_count, in_buf);

            ret = mediacodec_process_input (g_media_codec[0], in_buf, 0);
            if (use_video && buf_size == -1)
            {
                g_print("%s - END : input_buf size = %d  frame_count : %d\n",__func__, buf_size,  frame_count);
                return MEDIACODEC_ERROR_INVALID_INBUFFER;
            }
        }
        else
        {
            g_print("%s - [WARN] Check to input buf_size = %4d  at %4d frame, %p\n",__func__, buf_size, frame_count, in_buf);
            return MEDIACODEC_ERROR_INVALID_INBUFFER;
        }

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
    mediacodec_s * handle = NULL;
    mc_handle_t* mc_handle = NULL;

    g_print("_mediacodec_get_output\n");
    if (g_media_codec[0] == NULL)
    {
        g_print("mediacodec handle is not created\n");
        return MEDIACODEC_ERROR_INVALID_PARAMETER;
    }
    else
    {
        handle = (mediacodec_s *) g_media_codec[0];
        mc_handle = (mc_handle_t*) handle->mc_handle;
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
            /*
              * Prepend Header For Aduio Encoder of AAC(adts) and AMR-NB
              */
            if (mc_handle->codec_id == MEDIACODEC_AAC_LC)
            {
                if (buf_size > 0)
                {
                    /* This is used only AAC encoder case for adding each ADTS frame header */
                    add_adts_header_for_aacenc(buf_adts, (buf_size+ADTS_HEADER_SIZE));
                    fwrite(&buf_adts[0], 1, ADTS_HEADER_SIZE, fp_out);
                }
            }
            else if (mc_handle->codec_id == MEDIACODEC_AMR_NB)
            {
                if ((buf_size > 0) && (write_amr_header == 1))
                {
                    /* This is used only AMR encoder case for adding AMR masic header in only first frame */
                    g_print("%s - AMR_header write in first frame\n",__func__);
                    fwrite(&AMR_header[0], 1, sizeof(AMR_header)   - 1, fp_out);         /* AMR-NB magic number */
                    write_amr_header = 0;
                }
            }
        }

        if (data != NULL)
        {
            fwrite(data, 1, buf_size, fp_out);

            // if g_OutFileCtr is set to 1 by developer, dump the output frame by frame in specified output folder
            if(g_OutFileCtr)
            {
                FILE* fp;
                char str[200];
                sprintf(str,"/media/frames/%05d", g_OutFileCtr-1);
                fp = fopen(str,"wb");
                if(fp)
                {
                    g_OutFileCtr++;
                    fwrite(data, 1, buf_size, fp);
                    fclose(fp);
                }
                else
                {
                    g_OutFileCtr = 0; // Avoid trying to open file next time
                }
            }
        }
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
static gpointer vdec_task(gpointer data)
{
    int i;
    //TODO extract packets from MediaDemuxer
    /*
        for(i=0; demuxer->get_track_count; i++)
        {
        }
    */
    return NULL;
}

void _mediacodec_process_all(void)
{
    g_print("_mediacodec_process_all\n");
    int ret = MEDIACODEC_ERROR_NONE;
    GThread *vdec_thread, *venc_thread, *adec_thread, *aenc_thread;

    while(1)
    {
        ret = _mediacodec_process_input();

        if(ret != MEDIACODEC_ERROR_NONE) {
            g_print ("_mediacodec_process_input ret = %d\n", ret);
            break;
        }
    }

/*
    if(use_encoder)
    {
        if(use_video)
        {
        }
        else
        {
        }
    }
    else
    {
        if(use_video)
        {
            vdec_thread = g_thread_create(vdec_task, NULL, TRUE, NULL);

            g_thread_join(vdec_thread);
        }
        else
        {
        }

    }
*/
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
#if 1  //NEW
        g_print("*** Codec id : Select Codec ID Number  (e.g. AAC_LC = 96)\n");
        g_print("               L16    =  16 (0x10)\n");
        g_print("               ALAW   =  32 (0x20)\n");
        g_print("               ULAW   =  48 (0x30)\n");
        g_print("               AMR_NB =  64 (0x40)\n");
        g_print("               AMR_WB =  65 (0x41)\n");
        g_print("               G729   =  80 (0x50)\n");
        g_print("               AAC_LC =  96 (0x60)\n");
        g_print("               AAC_HE =  97 (0x61)\n");
        g_print("               AAC_PS =  98 (0x62)\n");
        g_print("               MP3    = 112 (0x70)\n");
        g_print("               VORBIS = 128 (0x80)\n");
        g_print("               FLAC   = 144 (0x90)\n");
        g_print("               WMAV1  = 160 (0xA0)\n");
        g_print("               WMAV2  = 161 (0xA1)\n");
        g_print("               WMAPRO = 162 (0xA2)\n");
        g_print("               WMALSL = 163 (0xA3)\n");
        g_print("               -------------------\n");
        g_print("               H261   = 101\n");
        g_print("               H263   = 102\n");
        g_print("               H264   = 103\n");
        g_print("               MJPEG  = 104\n");
        g_print("               MPEG1  = 105\n");
        g_print("               MPEG2  = 106\n");
        g_print("               MPEG4  = 107\n");
        g_print("               -------------------\n");
        g_print("*** Flags : Select Combination Number (e.g. DEOCDER + TYPE_SW = 10)\n");
        g_print("               CODEC : ENCODER =  1       DECODER =  2\n");
        g_print("               TYPE  : HW      =  4       SW      =  8\n");
        g_print("               TYPE  : OMX     = 16       GEN     = 32\n");
        g_print("*** input codec id, flags.\n");
#else
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
#endif
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
        g_print("*** input audio decode configure.(samplerate, channel, bit (e.g. 48000,  2, 16))\n");
    }
    else if (g_menu_state == CURRENT_STATUS_SET_AENC_INFO)
    {
        g_print("*** input audio encode configure.(samplerate, channel, bit, bitrate (e.g. 48000,  2, 16, 128000))\n");
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

#if 1  //NEW - ToDo : TEST for case of MP3 (112 = 0x70), VORBIS(128), FLAC(144), WMA(160~163)
                        if(tmp > 100 &&
                           (tmp != 112) &&
                           (tmp != 128) &&
                           (tmp != 144) &&
                           (tmp != 160) && (tmp != 161) && (tmp != 162) && (tmp != 163))     //Temporary
#else
                        if(tmp > 100)       //orginal
#endif
                        {
                            tmp = strtol(cmd, ptr, 16);
                            codecid = 0x2000 + ((tmp & 0xFF) << 4);
                            use_video = 1;
                        }
                        else
                        {
#if 1  //NEW
                            codecid = 0x1000 + tmp;
#else
                            codecid = 0x1000 + (tmp<<4);
#endif
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
    g_print("a. Create \t\t");
    g_print("sc. Set codec \n");
    g_print("vd. Set vdec info \t");
    g_print("ve. Set venc info \n");
    g_print("ad. Set adec info \t");
    g_print("ae. Set aenc info \n");
    g_print("pr. Prepare \t\t");
    g_print("pi. Process input \n");
    g_print("o. Get output \t\t");
    g_print("rb. Reset output buffer \n");
    g_print("pa. Process all frames \n");
    g_print("un. Unprepare \t\t");
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
