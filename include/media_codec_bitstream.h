/*
* Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TIZEN_MEDIA_CODEC_BITSTREAM_H__
#define __TIZEN_MEDIA_CODEC_BITSTREAM_H__

#include <tizen.h>
#include <media_codec_port.h>

typedef struct _mc_bitstream_t mc_bitstream_t;

typedef enum
{
    NAL_SLICE_NO_PARTITIONING = 1,
    NAL_SLICE_PART_A,
    NAL_SLICE_PART_B,
    NAL_SLICE_PART_C,
    NAL_SLICE_IDR,
    NAL_SEI,
    NAL_SEQUENCE_PARAMETER_SET,
    NAL_PICTURE_PARAMETER_SET,
    NAL_PICTURE_DELIMITER,
    NAL_END_OF_SEQUENCE,
    NAL_END_OF_STREAM,
    NAL_FILLER_DATA,
    NAL_PREFIX_SVC = 14
} mc_nal_unit_type_e;

enum
{
    MC_EXIST_SPS    = 1 << 0,
    MC_EXIST_PPS    = 1 << 1,
    MC_EXIST_IDR    = 1 << 2,
    MC_EXIST_SLICE  = 1 << 3,

    MC_VALID_HEADER = ( MC_EXIST_SPS | MC_EXIST_PPS ),
    MC_VALID_FIRST_SLICE = ( MC_EXIST_SPS | MC_EXIST_PPS | MC_EXIST_IDR )
};

#define CHECK_VALID_PACKET(state, expected_state) \
  ((state & (expected_state)) == (expected_state))

static const unsigned int mask[33] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};

struct _mc_bitstream_t
{
    unsigned char *data;
    unsigned int numBytes;
    unsigned int bytePos;
    unsigned int buffer;
    unsigned int dataBitPos;
    unsigned int bitcnt;
};


#ifdef __cplusplus
extern "C" {
#endif

#define MC_READ16B(x) ((((const unsigned char*)(x))[0] << 8) | ((const unsigned char*)(x))[1])

#define MC_READ32B(x) ((((const unsigned char*)(x))[0] << 24) | \
                      (((const unsigned char*)(x))[1] << 16) | \
                      (((const unsigned char*)(x))[2] <<  8) | \
                      ((const unsigned char*)(x))[3])

void mc_init_bits(mc_bitstream_t *stream, unsigned char *data, int size);
short mc_show_bits(mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData);
short mc_read_bits( mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData );
short mc_byte_align( mc_bitstream_t *stream );

bool _mc_is_voss(unsigned char *buf, int size, int *codec_size);
bool _mc_is_ivop(unsigned char *p, int size, int pos);
bool _mc_is_vop(unsigned char *p, int size, int pos);


int __mc_decode_sps(mc_bitstream_t *pstream, int *width, int *height);
unsigned int __mc_bytestream_to_nal( unsigned char* data, int size );
int _mc_check_h264_bytestream ( unsigned char *nal, int byte_length, bool port, bool *codec_config, bool *sync_flag, bool *slice);
int _mc_check_valid_h263_frame(unsigned char *p, int size);
bool _mc_check_h263_out_bytestream(unsigned char *p, int buf_length, bool* need_sync_flag);
int _mc_check_mpeg4_out_bytestream(unsigned char *buf, int buf_length, bool* need_codec_data, bool *need_sync_flag);


#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_BITSTREAM_H__ */
