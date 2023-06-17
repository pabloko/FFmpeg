/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * EVC decoder/parser shared code
 */

#ifndef AVCODEC_EVC_PARSE_H
#define AVCODEC_EVC_PARSE_H

#include <stdint.h>

#include "libavutil/intreadwrite.h"
#include "libavutil/log.h"
#include "libavutil/rational.h"
#include "evc.h"
#include "evc_ps.h"

// The sturcture reflects Slice Header RBSP(raw byte sequence payload) layout
// @see ISO_IEC_23094-1 section 7.3.2.6
//
// The following descriptors specify the parsing process of each element
// u(n)  - unsigned integer using n bits
// ue(v) - unsigned integer 0-th order Exp_Golomb-coded syntax element with the left bit first
// u(n)  - unsigned integer using n bits.
//         When n is "v" in the syntax table, the number of bits varies in a manner dependent on the value of other syntax elements.
typedef struct EVCParserSliceHeader {
    int slice_pic_parameter_set_id;                                     // ue(v)
    int single_tile_in_slice_flag;                                      // u(1)
    int first_tile_id;                                                  // u(v)
    int arbitrary_slice_flag;                                           // u(1)
    int last_tile_id;                                                   // u(v)
    int num_remaining_tiles_in_slice_minus1;                            // ue(v)
    int delta_tile_id_minus1[EVC_MAX_TILE_ROWS * EVC_MAX_TILE_COLUMNS]; // ue(v)

    int slice_type;                                                     // ue(v)
    int no_output_of_prior_pics_flag;                                   // u(1)
    int mmvd_group_enable_flag;                                         // u(1)
    int slice_alf_enabled_flag;                                         // u(1)

    int slice_alf_luma_aps_id;                                          // u(5)
    int slice_alf_map_flag;                                             // u(1)
    int slice_alf_chroma_idc;                                           // u(2)
    int slice_alf_chroma_aps_id;                                        // u(5)
    int slice_alf_chroma_map_flag;                                      // u(1)
    int slice_alf_chroma2_aps_id;                                       // u(5)
    int slice_alf_chroma2_map_flag;                                     // u(1)
    int slice_pic_order_cnt_lsb;                                        // u(v)

    // @note
    // Currently the structure does not reflect the entire Slice Header RBSP layout.
    // It contains only the fields that are necessary to read from the NAL unit all the values
    // necessary for the correct initialization of the AVCodecContext structure.

    // @note
    // If necessary, add the missing fields to the structure to reflect
    // the contents of the entire NAL unit of the SPS type

} EVCParserSliceHeader;

// picture order count of the current picture
typedef struct EVCParserPoc {
    int PicOrderCntVal;     // current picture order count value
    int prevPicOrderCntVal; // the picture order count of the previous Tid0 picture
    int DocOffset;          // the decoding order count of the previous picture
} EVCParserPoc;

typedef struct EVCParserContext {
    EVCParamSets ps;
    EVCParserPoc poc;

    int nuh_temporal_id;            // the value of TemporalId (shall be the same for all VCL NAL units of an Access Unit)
    int nalu_type;                  // the current NALU type

    // Dimensions of the decoded video intended for presentation.
    int width;
    int height;

    // Dimensions of the coded video.
    int coded_width;
    int coded_height;

    // The format of the coded data, corresponds to enum AVPixelFormat
    int format;

    // AV_PICTURE_TYPE_I, EVC_SLICE_TYPE_P, AV_PICTURE_TYPE_B
    int pict_type;

    // Set by parser to 1 for key frames and 0 for non-key frames
    int key_frame;

    // Picture number incremented in presentation or output order.
    // This corresponds to EVCEVCParserPoc::PicOrderCntVal
    int output_picture_number;

    // profile
    // 0: FF_PROFILE_EVC_BASELINE
    // 1: FF_PROFILE_EVC_MAIN
    int profile;

    // Framerate value in the compressed bitstream
    AVRational framerate;

    // Number of pictures in a group of pictures
    int gop_size;

    // Number of frames the decoded output will be delayed relative to the encoded input
    int delay;

    int parsed_extradata;

} EVCParserContext;

static inline int evc_get_nalu_type(const uint8_t *bits, int bits_size, void *logctx)
{
    int unit_type_plus1 = 0;

    if (bits_size >= EVC_NALU_HEADER_SIZE) {
        unsigned char *p = (unsigned char *)bits;
        // forbidden_zero_bit
        if ((p[0] & 0x80) != 0) {
            av_log(logctx, AV_LOG_ERROR, "Invalid NAL unit header\n");
            return -1;
        }

        // nal_unit_type
        unit_type_plus1 = (p[0] >> 1) & 0x3F;
    }

    return unit_type_plus1 - 1;
}

static inline uint32_t evc_read_nal_unit_length(const uint8_t *bits, int bits_size, void *logctx)
{
    uint32_t nalu_len = 0;

    if (bits_size < EVC_NALU_LENGTH_PREFIX_SIZE) {
        av_log(logctx, AV_LOG_ERROR, "Can't read NAL unit length\n");
        return 0;
    }

    nalu_len = AV_RB32(bits);

    return nalu_len;
}

// nuh_temporal_id specifies a temporal identifier for the NAL unit
int ff_evc_get_temporal_id(const uint8_t *bits, int bits_size, void *logctx);

int ff_evc_parse_nal_unit(EVCParserContext *ctx, const uint8_t *buf, int buf_size, void *logctx);

#endif /* AVCODEC_EVC_PARSE_H */