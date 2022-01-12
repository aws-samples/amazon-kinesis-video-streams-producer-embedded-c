/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Internal headers */
#include "os/allocator.h"

typedef struct BitStream
{
    unsigned char *pBuf;
    int xCurrentBit;
} BitStream_t;

static unsigned int uReadBit(BitStream_t *pBitStream)
{
    int nIndex = pBitStream->xCurrentBit / 8;
    int nOffset = pBitStream->xCurrentBit % 8 + 1;
    pBitStream->xCurrentBit++;
    return (pBitStream->pBuf[nIndex] >> (8 - nOffset)) & 0x01;
}

static unsigned int uReadBits(BitStream_t *pBitStream, int n)
{
    int r = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        r |= (uReadBit(pBitStream) << (n - i - 1));
    }
    return r;
}

static unsigned int uReadExponentialGolombCode(BitStream_t *pBitStream)
{
    int r = 0;
    int i = 0;
    while ((uReadBit(pBitStream) == 0) && (i < 32))
    {
        i++;
    }
    r = uReadBits(pBitStream, i);
    r += (1 << i) - 1;
    return r;
}

static unsigned int uReadSE(BitStream_t *pBitStream)
{
    int r = uReadExponentialGolombCode(pBitStream);
    if (r & 0x01)
    {
        r = (r + 1) / 2;
    }
    else
    {
        r = -(r / 2);
    }
    return r;
}

void getH264VideoResolution(char *pSps, size_t uSpsLen, uint16_t *puWidth, uint16_t *puHeight)
{
    BitStream_t xBitStream = {.pBuf = (unsigned char *)pSps, .xCurrentBit = 0};
    int frame_crop_left_offset = 0;
    int frame_crop_right_offset = 0;
    int frame_crop_top_offset = 0;
    int frame_crop_bottom_offset = 0;
    int crop_unit_x = 0;
    int crop_unit_y = 0;
    int chroma_format_idc = 1;
    int profile_idc = uReadBits(&xBitStream, 8);
    int constraint_set0_flag = uReadBit(&xBitStream);
    int constraint_set1_flag = uReadBit(&xBitStream);
    int constraint_set2_flag = uReadBit(&xBitStream);
    int constraint_set3_flag = uReadBit(&xBitStream);
    int constraint_set4_flag = uReadBit(&xBitStream);
    int constraint_set5_flag = uReadBit(&xBitStream);
    int reserved_zero_2bits = uReadBits(&xBitStream, 2);
    int level_idc = uReadBits(&xBitStream, 8);
    int seq_parameter_set_id = uReadExponentialGolombCode(&xBitStream);

    int xWidth = 0;
    int xHeight = 0;

    /* Please refer to https://www.itu.int/rec/T-REC-H.264/ Section 7.4.2.1.1 Sequence parameter set data semantics */
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118 ||
        profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 || profile_idc == 135)
    {
        chroma_format_idc = uReadExponentialGolombCode(&xBitStream);
        if (chroma_format_idc == 3)
        {
            int residual_colour_transform_flag = uReadBit(&xBitStream);
        }

        int bit_depth_luma_minus8 = uReadExponentialGolombCode(&xBitStream);
        int bit_depth_chroma_minus8 = uReadExponentialGolombCode(&xBitStream);
        int qpprime_y_zero_transform_bypass_flag = uReadBit(&xBitStream);

        int seq_scaling_matrix_present_flag = uReadBit(&xBitStream);
        if (seq_scaling_matrix_present_flag)
        {
            int i = 0;
            for (i = 0; i < 8; i++)
            {
                int seq_scaling_list_present_flag = uReadBit(&xBitStream);
                if (seq_scaling_list_present_flag)
                {
                    int sizeOfScalingList = (i < 6) ? 16 : 64;
                    int lastScale = 8;
                    int nextScale = 8;
                    int j = 0;
                    for (j = 0; j < sizeOfScalingList; j++)
                    {
                        if (nextScale != 0)
                        {
                            int delta_scale = uReadSE(&xBitStream);
                            nextScale = (lastScale + delta_scale + 256) % 256;
                        }
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }

    int log2_max_frame_num_minus4 = uReadExponentialGolombCode(&xBitStream);
    int pic_order_cnt_type = uReadExponentialGolombCode(&xBitStream);
    if (pic_order_cnt_type == 0)
    {
        int log2_max_pic_order_cnt_lsb_minus4 = uReadExponentialGolombCode(&xBitStream);
    }
    else if (pic_order_cnt_type == 1)
    {
        int delta_pic_order_always_zero_flag = uReadBit(&xBitStream);
        int offset_for_non_ref_pic = uReadSE(&xBitStream);
        int offset_for_top_to_bottom_field = uReadSE(&xBitStream);
        int num_ref_frames_in_pic_order_cnt_cycle = uReadExponentialGolombCode(&xBitStream);
        int i;
        for (i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            uReadSE(&xBitStream);
        }
    }
    int max_num_ref_frames = uReadExponentialGolombCode(&xBitStream);
    int gaps_in_frame_num_value_allowed_flag = uReadBit(&xBitStream);
    int pic_width_in_mbs_minus1 = uReadExponentialGolombCode(&xBitStream);
    int pic_height_in_map_units_minus1 = uReadExponentialGolombCode(&xBitStream);
    int frame_mbs_only_flag = uReadBit(&xBitStream);
    if (!frame_mbs_only_flag)
    {
        int mb_adaptive_frame_field_flag = uReadBit(&xBitStream);
    }
    int direct_8x8_inference_flag = uReadBit(&xBitStream);
    int frame_cropping_flag = uReadBit(&xBitStream);
    if (frame_cropping_flag)
    {
        frame_crop_left_offset = uReadExponentialGolombCode(&xBitStream);
        frame_crop_right_offset = uReadExponentialGolombCode(&xBitStream);
        frame_crop_top_offset = uReadExponentialGolombCode(&xBitStream);
        frame_crop_bottom_offset = uReadExponentialGolombCode(&xBitStream);
        if (0 == chroma_format_idc)
        {
            crop_unit_x = 1;
            crop_unit_y = 2 - frame_mbs_only_flag;
        }
        else if (1 == chroma_format_idc)
        {
            crop_unit_x = 2;
            crop_unit_y = 2 * (2 - frame_mbs_only_flag);
        }
        else if (2 == chroma_format_idc)
        {
            crop_unit_x = 2;
            crop_unit_y = 2 - frame_mbs_only_flag;
        }
        else
        {
            crop_unit_x = 1;
            crop_unit_y = 2 - frame_mbs_only_flag;
        }
    }

    xWidth = ((pic_width_in_mbs_minus1 + 1) * 16) - crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
    xHeight = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16) - crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset);

    *puWidth = (uint16_t)xWidth;
    *puHeight = (uint16_t)xHeight;
}