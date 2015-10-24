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
 *
 */
#include <stdio.h>
#include <dlog.h>
#include <media_codec_bitstream.h>

void mc_init_bits(mc_bitstream_t *stream, unsigned char *data, int size)
{
    stream->data = data;
    stream->numBytes = size;
    stream->bitcnt = 32;
    stream->bytePos = 0;
    stream->dataBitPos = 0;
    stream->buffer = 0;
}

short mc_show_bits(mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData)
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;
    unsigned int i;

    if (nbits > (32 - bitcnt))  {
        dataBytePos = dataBitPos >> 3;
        bitcnt = dataBitPos & 7;
        if (dataBytePos > stream->numBytes - 4) {
            stream->buffer = 0;
            for (i = 0; i < stream->numBytes - dataBytePos; i++) {
                stream->buffer |= stream->data[dataBytePos + i];
                stream->buffer <<= 8;
            }
            stream->buffer <<= 8 * (3 - i);
        } else {
            bits = &stream->data[dataBytePos];
            stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        stream->bitcnt = bitcnt;
    }
    bitcnt += nbits;

    *pulOutData = (stream->buffer >> (32 - bitcnt)) & mask[(unsigned short)nbits];

    return 0;
}

short mc_read_bits(mc_bitstream_t *stream, unsigned char nbits, unsigned int *pulOutData)
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;

    if ((dataBitPos + nbits) > (stream->numBytes << 3)) {
        *pulOutData = 0;
        return -1;
    }

    if (nbits > (32 - bitcnt)) {
        dataBytePos = dataBitPos >> 3;
        bitcnt = dataBitPos & 7;
        bits = &stream->data[dataBytePos];
        stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }


    stream->dataBitPos += nbits;
    stream->bitcnt = (unsigned char)(bitcnt + nbits);


    *pulOutData = (stream->buffer >> (32 - stream->bitcnt)) & mask[(unsigned short)nbits];

    return 0;
}

short mc_byte_align(mc_bitstream_t *stream)
{
    unsigned char *bits;
    unsigned int dataBitPos = stream->dataBitPos;
    unsigned int bitcnt = stream->bitcnt;
    unsigned int dataBytePos;
    unsigned int leftBits;

    leftBits = 8 - (dataBitPos & 0x7);
    if (leftBits == 8) {
        if ((dataBitPos + 8) > (unsigned int)(stream->numBytes << 3))
            return (-1);
        dataBitPos += 8;
        bitcnt += 8;
    } else {
        dataBytePos = dataBitPos >> 3;
        dataBitPos += leftBits;
        bitcnt += leftBits;
    }

    if (bitcnt > 32) {
        dataBytePos = dataBitPos >> 3;
        bits = &stream->data[dataBytePos];
        stream->buffer = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }


    stream->dataBitPos = dataBitPos;
    stream->bitcnt = bitcnt;

    return 0;
}
unsigned int __mc_bytestream_to_nal(unsigned char *data, int size)
{
    unsigned char val, zero_count;
    unsigned char *pNal = data;
    int index = 0;

    zero_count = 0;

    val = pNal[index++];
    while (!val) {
        zero_count++;
        val = pNal[index++];
    }

    zero_count = 0;

    while (1) {
        if (index >= size)
            return (index - 1);

        val = pNal[index++];

        if (!val)
            zero_count++;
        else {
            if ((zero_count >= 2) && (val == 1))
                break;
            else {
                zero_count = 0;
            }
        }
    }

    if (zero_count > 3)
        zero_count = 3;

    return (index - zero_count - 1);
}

int __mc_decode_sps(mc_bitstream_t *pstream, int *width, int *height)
{
    int ret = MC_ERROR_NONE;
    unsigned int tmp = 0;

    int profile_idc = 0;

    mc_read_bits(pstream, 8, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 1, &tmp);
    mc_read_bits(pstream, 5, &tmp);
    mc_read_bits(pstream, 8, &tmp);

    profile_idc = tmp;

    if (profile_idc > 51)
        ret = MC_INVALID_IN_BUF;

    /*TODO parse width, height, etc...*/

    return ret;
}

int _mc_check_h264_bytestream(unsigned char *nal, int byte_length, bool port, bool *codec_config, bool *sync_flag, bool *slice)
{
    int ret = MC_ERROR_NONE;
    int stacked_length = 0;
    int nal_length = 0;
    unsigned int syntax = 0;
    unsigned int state = 0;
    int count = 0;
    int nal_unit_type = 0;

    mc_bitstream_t pstream;

    nal_unit_type = nal[2] == 1 ? (nal[3] & 0x1F) : (nal[4] & 0x1F);

    if (nal_unit_type == 0x7 || nal_unit_type == 0x8 || nal_unit_type == 0x9) {

        while (1) {
            nal_length = __mc_bytestream_to_nal(nal + stacked_length, byte_length - stacked_length);

            mc_init_bits(&pstream, nal + stacked_length, byte_length - stacked_length);
            mc_read_bits(&pstream, 32, &syntax);
            mc_read_bits(&pstream, 8, &syntax);

            switch (syntax & 0x1F) {
                case NAL_SEQUENCE_PARAMETER_SET:
                    LOGD("nal_unit_type : SPS");
                    if ((ret = __mc_decode_sps(&pstream, NULL, NULL)) != MC_ERROR_NONE)
                        return ret;
                    state |= MC_EXIST_SPS;
                    break;
                case NAL_PICTURE_PARAMETER_SET:
                    LOGD("nal_unit_type : PPS");
                    state |= MC_EXIST_PPS;
                    break;
                case NAL_SLICE_IDR:
                    LOGD("nal_unit_type : IDR");
                    state |= MC_EXIST_IDR;
                    break;
                default:
                    state |= MC_EXIST_SLICE;
                    LOGD("nal_unit_type : %x", syntax & 0x1F);
                    break;
            }

            LOGD("stacked_length : %d, nal_length : %d, byte_length : %d", stacked_length, nal_length, byte_length);

            stacked_length += nal_length;
            count++;

            if ((stacked_length >= byte_length) || count > 5)
                break;
        }
    } else if (nal_unit_type == 0x5) {
        state |= MC_EXIST_IDR;
        LOGD("nal_unit_type is IDR");
    } else if (nal_unit_type == 0x01 || nal_unit_type == 0x02 || nal_unit_type == 0x03 || nal_unit_type == 0x04) {
        state |= MC_EXIST_SLICE;
        LOGD("nal_unit_type : %x", nal_unit_type);
    } else {
        LOGD("Non VCL");
    }

    LOGD("for debug state :%d, %d", state, MC_VALID_FIRST_SLICE);

    /* input port */
    if (!port && !CHECK_VALID_PACKET(state, MC_VALID_FIRST_SLICE))
        return MC_INVALID_IN_BUF;

    /* output port */
    if (port) {
        *codec_config = CHECK_VALID_PACKET(state, MC_VALID_HEADER) ? 1 : 0;
        *sync_flag = CHECK_VALID_PACKET(state, MC_EXIST_IDR) ? 1 : 0;
        *slice = CHECK_VALID_PACKET(state, MC_EXIST_SLICE) ? 1 : 0;
    }

    return ret;
}

int _mc_check_valid_h263_frame(unsigned char *p, int size)
{
    unsigned char *end = p + size - 3;
    int count = 0;

    do {
        /* Found the start of the frame, now try to find the end */
        if ((p[0] == 0x00) && (p[1] == 0x00) && ((p[2]&0xFC) == 0x80))
            count++;
        p++;
    } while (count == 1 && p < end);

    if (count != 1)
        return MC_INVALID_IN_BUF; /* frame boundary violated */

    return MC_ERROR_NONE;
}

bool _mc_is_voss(unsigned char *buf, int size, int *codec_size)
{
    unsigned char *p = buf;
    unsigned char *end = p + size - 3;
    if (size < 4)
        return false;

    if (!((p[0] == 0x00) && (p[1] == 0x00) && (p[2] == 0x01) && (p[3] == 0xB0)))
        return false;

    if (codec_size) {
        for (; p < end ; p++) {
            if ((p[0] == 0x00) && (p[1] == 0x00) && (p[2] == 0x01) && (p[3] == 0xB6)) {
                *codec_size = p-buf;
                break;
            }
        }
    }

    return true;
}

bool _mc_is_ivop(unsigned char *p, int size, int pos)
{
    if (size < (pos + 5))
        return false;

    p = p + pos;

    if ((p[0] == 0x00) && (p[1] == 0x00) && (p[2] == 0x01) && (p[3] == 0xB6)) {
        /* VOP_CODING_TYPE (binary)  Coding method
        // 00  intra-coded (I)
        // 01  predictive-coded (P)
        // 10  bidirectionally-predictive-coded (B)
        // 11  sprite (S)
        */
        if ((p[4] & 0xC0) == 0x0)   /*I-VOP */
           return true;
    }
    return false;
}

bool _mc_is_vop(unsigned char *p, int size, int pos)
{
    if (size < (pos + 4))
        return false;

    p = p + pos;

    if ((p[0] == 0x00) && (p[1] == 0x00) && (p[2] == 0x01) && (p[3] == 0xB6))
        return true;

    return false;
}

int _mc_check_mpeg4_out_bytestream(unsigned char *buf, int buf_length, bool* need_codec_data, bool *need_sync_flag)
{
    int codec_data_size = 0;
    g_return_val_if_fail(need_codec_data != NULL, MC_PARAM_ERROR);
    g_return_val_if_fail(need_sync_flag != NULL, MC_PARAM_ERROR);

    *need_codec_data = FALSE;
    *need_sync_flag = FALSE;

    if (_mc_is_voss(buf, buf_length, &codec_data_size))
         *need_codec_data = TRUE;
    if (_mc_is_ivop(buf, buf_length, codec_data_size))
        *need_sync_flag = TRUE;

    return codec_data_size;
}

bool _mc_check_h263_out_bytestream(unsigned char *p, int buf_length, bool* need_sync_flag)
{
    g_return_val_if_fail(need_sync_flag != NULL, MC_PARAM_ERROR);

    *need_sync_flag = FALSE;

    /* PSC not present */
    if ((p[0] != 0x00) || (p[1] != 0x00) || ((p[2]&0xFC) != 0x80)) {
        return false;
    }

    /* PTYPE Field, Bit 9: Picture Coding Type, "0" INTRA (I-picture), "1" INTER (P-picture) */
    if (!(p[4] & 0x2)) {
        *need_sync_flag = TRUE;
    }

    return TRUE;
}

