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

#ifndef __TIZEN_MEDIA_CODEC_H__
#define __TIZEN_MEDIA_CODEC_H__

#include <tizen.h>
#include <stdint.h>
#include <media_packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @file media_codec.h
* @brief This file contains the capi media codec API.
*/

/**
* @addtogroup CAPI_MEDIA_CODEC_MODULE
* @{
*/

/**
 * @brief Media Codec type handle.
 * @since_tizen 2.3
 */
typedef struct mediacodec_s *mediacodec_h;

/**
 * @brief Enumeration of  media codec support  type
 * @since_tizen 2.3
 * @remarks If this codec is to be used as an encoder or decoder, the codec flag must be set to #MEDIACODEC_ENCODER or
 *          #MEDIACODEC_DECODER. If user doesn't set optional flag, default flags will be set to #MEDIACODEC_SUPPORT_TYPE_SW.
 */
typedef enum
{
    MEDIACODEC_ENCODER          = 0x1,      /**< This flag is for using the encoder */
    MEDIACODEC_DECODER          = 0x2,      /**< This flag is for using the decoder */
    MEDIACODEC_SUPPORT_TYPE_HW  = 0x4,      /**< This is an optional flag for using the h/w codec */
    MEDIACODEC_SUPPORT_TYPE_SW  = 0x8,      /**< This is an optional flag for using the s/w codec */
} mediacodec_support_type_e;

/**
 * @brief Enumerations of  media codec type
 * @since_tizen 2.3
 */
typedef enum
{
    MEDIACODEC_NONE         = 0x0,      /**< NONE*/
    MEDIACODEC_L16          = 0x1010,   /**< L16*/
    MEDIACODEC_ALAW         = 0x1020,   /**< ALAW*/
    MEDIACODEC_ULAW         = 0x1030,   /**< ULAW*/
    MEDIACODEC_AMR          = 0x1040,   /**< MEDIACDEC_AMR indicates AMR-NB (Since 2.4)*/
    MEDIACODEC_AMR_NB       = 0x1040,   /**< AMR-NB (Since 2.4)*/
    MEDIACODEC_AMR_WB       = 0x1041,   /**< AMR-WB (Since 2.4)*/
    MEDIACODEC_G729         = 0x1050,   /**< G729*/
    MEDIACODEC_AAC          = 0x1060,   /**< MEDIACDEC_AAC indicates AAC-LC (Since 2.4)*/
    MEDIACODEC_AAC_LC       = 0x1060,   /**< AAC-LC (Since 2.4)*/
    MEDIACODEC_AAC_HE       = 0x1061,   /**< HE-AAC (Since 2.4)*/
    MEDIACODEC_AAC_HE_PS    = 0x1062,   /**< HE-AAC-PS (Since 2.4)*/
    MEDIACODEC_MP3          = 0x1070,   /**< MP3*/
    MEDIACODEC_VORBIS       = 0x1080,   /**< VORBIS (Since 2.4)*/
    MEDIACODEC_FLAC         = 0x1090,   /**< FLAC (Since 2.4)*/
    MEDIACODEC_WMAV1        = 0x10A0,   /**< WMA version 1 (Since 2.4)*/
    MEDIACODEC_WMAV2        = 0x10A1,   /**< WMA version 2  (Since 2.4)*/
    MEDIACODEC_WMAPRO       = 0x10A2,   /**< WMA Professional (Since 2.4)*/
    MEDIACODEC_WMALSL       = 0x10A3,   /**< WMA Lossless (Since 2.4)*/

    MEDIACODEC_H261         = 0x2010,   /**< H.261*/
    MEDIACODEC_H263         = 0x2020,   /**< H.263*/
    MEDIACODEC_H264         = 0x2030,   /**< H.264*/
    MEDIACODEC_MJPEG        = 0x2040,   /**< MJPEG*/
    MEDIACODEC_MPEG1        = 0x2050,   /**< MPEG1*/
    MEDIACODEC_MPEG2        = 0x2060,   /**< MPEG2*/
    MEDIACODEC_MPEG4        = 0x2070,   /**< MPEG4*/
    MEDIACODEC_HEVC         = 0x2080,   /**< HEVC (Since 2.4)*/
    MEDIACODEC_VP8          = 0x2090,   /**< VP8 (Since 2.4)*/
    MEDIACODEC_VP9          = 0x20A0,   /**< VP9 (Since 2.4)*/
    MEDIACODEC_VC1          = 0x20B0,   /**< VC1 (Since 2.4)*/
} mediacodec_codec_type_e;

/**
 * @brief Enumeration of  media codec error
 * @since_tizen 2.3
 */
typedef enum
{
    MEDIACODEC_ERROR_NONE                       = TIZEN_ERROR_NONE,                     /**< Successful */
    MEDIACODEC_ERROR_OUT_OF_MEMORY              = TIZEN_ERROR_OUT_OF_MEMORY,            /**< Out of memory */
    MEDIACODEC_ERROR_INVALID_PARAMETER          = TIZEN_ERROR_INVALID_PARAMETER,        /**< Invalid parameter */
    MEDIACODEC_ERROR_INVALID_OPERATION          = TIZEN_ERROR_INVALID_OPERATION,        /**< Invalid operation */
    MEDIACODEC_ERROR_NOT_SUPPORTED_ON_DEVICE    = TIZEN_ERROR_NOT_SUPPORTED,            /**< Not supported */
    MEDIACODEC_ERROR_PERMISSION_DENIED          = TIZEN_ERROR_PERMISSION_DENIED,        /**< Permission denied */
    MEDIACODEC_ERROR_INVALID_STATE              = TIZEN_ERROR_MEDIACODEC | 0x01,        /**< Invalid state */
    MEDIACODEC_ERROR_INVALID_INBUFFER           = TIZEN_ERROR_MEDIACODEC | 0x02,        /**< Invalid input buffer */
    MEDIACODEC_ERROR_INVALID_OUTBUFFER          = TIZEN_ERROR_MEDIACODEC | 0x03,        /**< Invalid output buffer */
    MEDIACODEC_ERROR_INTERNAL                   = TIZEN_ERROR_MEDIACODEC | 0x04,        /**< Internal error */
    MEDIACODEC_ERROR_NOT_INITIALIZED            = TIZEN_ERROR_MEDIACODEC | 0x05,        /**< Not initialized mediacodec */
    MEDIACODEC_ERROR_INVALID_STREAM             = TIZEN_ERROR_MEDIACODEC | 0x06,        /**< Invalid stream */
    MEDIACODEC_ERROR_CODEC_NOT_FOUND            = TIZEN_ERROR_MEDIACODEC | 0x07,        /**< Not supported format */
    MEDIACODEC_ERROR_DECODE                     = TIZEN_ERROR_MEDIACODEC | 0x08,        /**< Error while decoding data */
    MEDIACODEC_ERROR_NO_FREE_SPACE              = TIZEN_ERROR_MEDIACODEC | 0x09,        /**< Out of storage */
    MEDIACODEC_ERROR_STREAM_NOT_FOUND           = TIZEN_ERROR_MEDIACODEC | 0x0a,        /**< Cannot find stream */
    MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT       = TIZEN_ERROR_MEDIACODEC | 0x0b,        /**< Not supported format */
    MEDIACODEC_ERROR_BUFFER_NOT_AVAILABLE       = TIZEN_ERROR_MEDIACODEC | 0x0c,        /**< Not available buffer */
    MEDIACODEC_ERROR_OVERFLOW_INBUFFER          = TIZEN_ERROR_MEDIACODEC | 0x0d,        /**< Overflow input buffer (Since 2.4)*/
    MEDIACODEC_ERROR_RESOURCE_OVERLOADED        = TIZEN_ERROR_MEDIACODEC | 0x0e,        /**< Exceed the instance limits (Since 2.4)*/
} mediacodec_error_e;

/**
 * @brief Enumeration of buffer status
 * @since_tizen 2.4
 */
typedef enum
{
    MEDIACODEC_NEED_DATA,
    MEDIACODEC_ENOUGH_DATA
} mediacodec_status_e;

/**
 * @brief Called when the input buffer(pkt) used up.
 * @since_tizen 2.3
 * @details It will be invoked when mediacodec has used input buffer.
 * @param[in] pkt  The media packet handle
 * @param[in] user_data  The user data passed from the callback registration function
 * @pre It will be invoked when input buffer process completed if you register this callback using mediacodec_set_input_buffer_used_cb().
 * @see mediacodec_set_input_buffer_used_cb()
 * @see mediacodec_unset_input_buffer_used_cb()
 */
typedef void (*mediacodec_input_buffer_used_cb)(media_packet_h pkt, void *user_data);

/**
 * @brief Called when the output buffer is available.
 * @since_tizen 2.3
 * @details It will be invoked when mediacodec has output buffer.
 * @param[in] pkt  The media packet handle
 * @param[in] user_data  The user data passed from the callback registration function
 * @pre It will be invoked when mediacodec process completed(had output buffer) if you register this callback using mediacodec_set_fill_buffer_cb().
 * @see mediacodec_set_output_buffer_available_cb()
 * @see mediacodec_unset_output_buffer_available_cb()
 */
typedef void (*mediacodec_output_buffer_available_cb)(media_packet_h pkt, void *user_data);

/**
 * @brief Called when the error has occured
 * @since_tizen 2.3
 * @details It will be invoked when the error has occured.
 * @param[in] error_code  The error code
 * @param[in] user_data  The user data passed from the callback registration function
 * @pre It will be invoked when the error has occured if you register this callback using mediacodec_set_error_cb().
 * @see mediacodec_set_error_cb()
 * @see mediacodec_unset_error_cb()
 */
typedef void (*mediacodec_error_cb)(mediacodec_error_e error, void *user_data);

/**
 * @brief Called when there is no data to decode/encode
 * @since_tizen 2.3
 * @details It will be invoked when the end-of-stream is reached.
 * @param[in] user_data  The user data passed from the callback registration function
 * @pre It will be invoked when the eos event generate if you register this callback using mediacodec_set_eos_cb().
 * @see mediacodec_set_eos_cb()
 * @see mediacodec_unset_eos_cb()
 */
typedef void (*mediacodec_eos_cb)(void *user_data);

/**
 * @brief Called when the mediacodec needs more data or has enough data.
 * @since_tizen 2.4
 * @details It is recommended that the application stops calling mediacodec_process_input() when MEDIACODEC_ENOUGH_DATA
 * is invoked.
 * @param[in] user_data  The user data passed from the callback registration function
 * @see mediacodec_set_buffer_status_cb()
 * @see mediacodec_unset_buffer_status_cb()
 */
typedef void (*mediacodec_buffer_status_cb)(mediacodec_status_e status, void *user_data);

/**
 * @brief Called once for each supported codec types.
 * @since_tizen 2.4
 * @param[in] codec_type  The codec type
 * @param[in] user_data   The user data passed from the foreach function
 * @return  @c true to continue with the next iteration of the loop, @c false to break out of the loop.
 * @pre mediacodec_foreach_supported_codec() will invoke this callback.
 * @see mediacodec_foreach_supported_codec()
 */
typedef bool (*mediacodec_supported_codec_cb)(mediacodec_codec_type_e codec_type, void *user_data);

/**
 * @brief Creates a mediacodec handle for decoding/encoding
 * @since_tizen 2.3
 * @remarks you must release @a mediacodec using mediacodec_destroy().\n
 *          Although you can create multiple mediacodec handles at the same time,
 *          the mediacodec cannot guarantee proper operation because of limited resources, like
 *          audio or display device.
 *
 * @param[out]  mediacodec  A new handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 */
int mediacodec_create(mediacodec_h *mediacodec);

/**
 * @brief Destroys the mediacodec handle and releases all its resources.
 * @since_tizen 2.3
 * @remarks To completely shutdown the mediacodec operation, call this function with a valid player handle from any
 *          mediacodec
 *
 * @param[in]  mediacodec  The handle to mediacodec to be destroyed.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 @endcode
 */
int mediacodec_destroy(mediacodec_h mediacodec);

/**
 * @brief Sets the codec type and decoder/encoder.
 * @since_tizen 2.3
 * @remarks If this codec is to be used as a decoder, pass the #MEDIACODEC_DECODER flag.
 *          If this codec is to be used as an encoder, pass the #MEDIACODEC_ENCODER flag.
 *          By default, It is used software default setting. If user want software setting, pass the
 *          #MEDIACODEC_SUPPORT_TYPE_SW flags.
 * @param[in] mediacodec  The handle of mediacodec
 * @param[in] codec_type  The identifier of the codec type of the decoder/encoder
 * @param[in] flags  The encoding/decoding scheme.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 * @retval #MEDIACODEC_ERROR_CODEC_NOT_FOUND Codec not found
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_destroy(mediacodec);
 @endcode
 */
int mediacodec_set_codec(mediacodec_h mediacodec, mediacodec_codec_type_e codec_type, mediacodec_support_type_e flags);

/**
 * @brief Sets the default info for the video decoder
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] width  The width for video decoding.
 * @param[in] height  The height for video decoding.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 @endcode
 */
int mediacodec_set_vdec_info(mediacodec_h mediacodec, int width, int height);

/**
 * @brief Sets the default info for the video encoder
 * @since_tizen 2.3
 * @remarks The frame rate is the speed of recording and the speed of playback.
 *          If user wants the default setting for ratecontrol, set @a target_bits to @c 0.
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] width  The width for video encoding.
 * @param[in] height  The height for video encoding.
 * @param[in] fps  The frame rate in frames per second.
 * @param[in] target_bits The target bitrates in bits per second.(a unit of kbit)
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_set_venc_info(mediacodec, 640, 480, 30, 1000);
 @endcode
 */
int mediacodec_set_venc_info(mediacodec_h mediacodec, int width, int height, int fps, int target_bits);

/**
 * @brief Sets the default info for the audio decoder
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] samplerate  The samplerate for audio decoding.
 * @param[in] channel  The channels for audio decoding.
  * @param[in] bit  The bits resolution for audio decoding.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_AAC, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW);
 mediacodec_set_adec_info(mediacodec, 44100, 2, 16);
 @endcode
 */
int mediacodec_set_adec_info(mediacodec_h mediacodec, int samplerate, int channel, int bit);

/**
 * @brief Sets the default info for the audio encdoer
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] samplerate  The samplerate for audio encoding.
 * @param[in] channel  The channels for audio encoding.
 * @param[in] bit  The bits resolution for audio encoding.
 * @param[in] bitrate  The bitrate for audio encoding.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_AAC, MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW);
 mediacodec_set_aenc_info(mediacodec, 44100, 2, 16, 128);
 @endcode
 */
int mediacodec_set_aenc_info(mediacodec_h mediacodec, int samplerate, int channel, int bit, int bitrate);

/**
 * @brief Prepares @a mediacodec for encoding/decoding.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 * @pre The mediacodec should call mediacodec_set_codec()and mediacodec_set_vdec_info()/mediacodec_set_venc_info() before calling mediacodec_prepare()
 *      If the decoder is set by mediacodec_set_codec(), mediacodec_set_vdec_info() should be called. If the encoder is set by
 *      mediacodec_set_codec(), mediacodec_set_venc_info() should be called.
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_prepare(mediacodec);
 @endcode
 */
int mediacodec_prepare(mediacodec_h mediacodec);

/**
 * @brief Unprepares @a mediacodec for encoding/decoding.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_prepare(mediacodec);
 mediacodec_unprepare(mediacodec);
 @endcode
 */
int mediacodec_unprepare(mediacodec_h mediacodec);

/**
 * @brief Decodes/Encodes a packet. The function passed undecoded/unencoded packet to the input queue and decode/encode a
 *          frame sequentially.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] inbuf  The current input format for the decoder/encoder
 * @param[in] timeOutUs  The timeout in microseconds. \n
 *                       The input buffer wait up to "timeOutUs" microseconds.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 * @retval #MEDIACODEC_ERROR_OVERFLOW_INBUFFER Overflow inputbuffer
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;
 media_packet_h pkt;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_prepare(mediacodec);
 mediacodec_process_input(pkt);
 @endcode
 */
int mediacodec_process_input (mediacodec_h mediacodec, media_packet_h inbuf, uint64_t timeOutUs);

/**
 * @brief Gets the decoded or encoded packet from the output queue.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[out] outbuf  The current output of the decoder/encoder. this function passed decoded/encoded frame to output
 *                    queue.
 * @param[in] timeOutUs  The timeout in microseconds. \n
 *                       The input buffer wait up to "timeOutUs" microseconds.
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #MEDIACODEC_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #MEDIACODEC_ERROR_INVALID_OPERATION Invalid operation
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;
 media_packet_h pkt;
 media_packet_h out_pkt;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_prepare(mediacodec);
 mediacodec_process_input(pkt);
 mediacodec_get_output(mediacodec, &out_pkt, 1000);
 @endcode
 */
int mediacodec_get_output (mediacodec_h mediacodec, media_packet_h *outbuf, uint64_t timeOutUs);

/**
 * @brief Flushes both input and output buffers.
 * @since_tizen 2.4
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;
 media_packet_h pkt;
 media_packet_h out_pkt;

 mediacodec_create(&mediacodec);
 mediacodec_set_codec(mediacodec, MEDIACODEC_H264, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW);
 mediacodec_prepare(mediacodec);
 mediacodec_process_input(pkt);
 mediacodec_flush_buffers(mediacodec);
 @endcode
 */
int mediacodec_flush_buffers (mediacodec_h mediacodec);

/**
 * @brief set empty buffer callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to register
 * @param[in] user_data  The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @pre	mediacodec_set_input_buffer_used_cb should be called before mediacodec_preare().
 * @post mediacodec_input_buffer_used_cb will be invoked.
 * @see mediacodec_set_input_buffer_used_cb()
 * @see mediacodec_unset_input_buffer_used_cb()
 */
int mediacodec_set_input_buffer_used_cb(mediacodec_h mediacodec, mediacodec_input_buffer_used_cb callback, void* user_data);

/**
 * @brief unset input buffer used callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @see mediacodec_set_input_buffer_used_cb()
 */
int mediacodec_unset_input_buffer_used_cb(mediacodec_h mediacodec);

/**
 * @brief set output buffer available callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to register
 * @param[in] user_data  The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @pre	mediacodec_set_output_buffer_available_cb should be called before mediacodec_preare().
 * @post mediacodec_output_buffer_available_cb will be invoked.
 * @see mediacodec_set_output_buffer_available_cb()
 * @see mediacodec_unset_output_buffer_available_cb()
 */
int mediacodec_set_output_buffer_available_cb(mediacodec_h mediacodec, mediacodec_output_buffer_available_cb callback, void* user_data);

/**
 * @brief unset output buffer available callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @see mediacodec_set_output_buffer_available_cb()
 */
int mediacodec_unset_output_buffer_available_cb(mediacodec_h mediacodec);

/**
 * @brief set error callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to register
 * @param[in] user_data  The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @pre	mediacodec_set_error_cb should be called before mediacodec_preare().
 * @post mediacodec_error_cb will be invoked.
 * @see mediacodec_set_error_cb()
 * @see mediacodec_unset_error_cb()
 */
int mediacodec_set_error_cb(mediacodec_h mediacodec, mediacodec_error_cb callback, void* user_data);

/**
 * @brief unset error callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @see mediacodec_set_error_cb()
 */
int mediacodec_unset_error_cb(mediacodec_h mediacodec);

/**
 * @brief set eos callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to register
 * @param[in] user_data  The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @pre	mediacodec_set_eos_cb should be called before mediacodec_preare().
 * @post mediacodec_eos_cb will be invoked.
 * @see mediacodec_set_eos_cb()
 * @see mediacodec_unset_eos_cb()
 */
int mediacodec_set_eos_cb(mediacodec_h mediacodec, mediacodec_eos_cb callback, void* user_data);

/**
 * @brief unset eos callback the media codec for process, asynchronously.
 * @since_tizen 2.3
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @see mediacodec_set_event_handler_cb()
 */
int mediacodec_unset_eos_cb(mediacodec_h mediacodec);

/**
 * @brief Registers a callback function to be invoked when the mediacodec needs more data or has enough data.
 * @since_tizen 2.4
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to register
 * @param[in] user_data  The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @pre	mediacodec_set_buffer_status_cb should be called before mediacodec_preare().
 * @post mediacodec_buffer_status_cb will be invoked.
 * @see mediacodec_set_buffer_status_cb()
 * @see mediacodec_unset_buffer_status_cb()
 */
int mediacodec_set_buffer_status_cb(mediacodec_h mediacodec, mediacodec_buffer_status_cb callback, void* user_data);

/**
 * @brief Unregisters the callback function.
 * @since_tizen 2.4
 * @param[in] mediacodec  The handle to mediacodec
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 */
int mediacodec_unset_buffer_status_cb(mediacodec_h mediacodec);

/**
 * @brief Retrieves all supported codecs by invoking callback function once for each supported codecs.
 * @since_tizen 2.4
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in] callback  The callback function to invoke
 * @param[in] user_data The user data to be passed to the callback function
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 * @see mediacodec_foreach_supported_codec()
 */
int mediacodec_foreach_supported_codec(mediacodec_h mediacodec, mediacodec_supported_codec_cb callback, void *user_data);

/**
 * @brief Verifies whether encoding can be performed with codec_type or not.
 * @since_tizen 2.4
 * @param[in] mediacodec  The handle to mediacodec
 * @param[in]  codec_type  The identifier of the codec type of the encoder.
 * @param[in]  encoder  Whether the encoder or decoder : (@c true = encoder, @c false = decoder).
 * @param[out]  support_type  (@c MEDIACODEC_SUPPORT_TYPE_HW = mediacodec can be performed with hardware codec, @c MEDIACODEC_SUPPORT_TYPE_SW = mediacodec can be performed with software codec)
 * @return @c 0 on success, otherwise a negative error value
 * @retval #MEDIACODEC_ERROR_NONE Successful
 * @retval #MEDIACODEC_ERROR_INVALID_PARAMETER Invalid parameter
 @code
 #include <media_codec.h>
 mediacodec_h mediacodec;
 mediacodec_support_type_e type;
 mediacodec_codec_type_e codec_type = MEDIACODEC_H264;

 mediacodec_create(&mediacodec);
 mediacodec_get_supported_type(mediacodec, codec_type, 1, &type);

 if(type == MEDIACODEC_SUPPORT_TYPE_HW)
     // only h/w supported
 else if (type == MEDIACODEC_SUPPORT_TYPE_SW)
     // only s/w supported
 else if (type == (MEDIACODEC_SUPPORT_TYPE_HW|MEDIACODEC_SUPPORT_TYPE_SW)
     // both supported

 mediacodec_set_codec(mediacodec, codec_type, MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_HW);
 @endcode
 */
int mediacodec_get_supported_type(mediacodec_h mediacodec, mediacodec_codec_type_e codec_type, bool encoder, int *support_type);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_H__ */

