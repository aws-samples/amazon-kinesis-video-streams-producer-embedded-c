#ifdef __cplusplus
extern "C" {
#include "kvs/nalu.h"
}
#endif

#include <gtest/gtest.h>

TEST(isKeyFrame, invalid_parameter)
{
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    /* Test invalid pointer */
    EXPECT_FALSE(isKeyFrame(NULL, uFrameLen));

    /* Test invalid length */
    EXPECT_FALSE(isKeyFrame(pFrame, 0));
}

TEST(NALU_getNaluType, annexb_3bytes_header)
{
    uint8_t uNaluNRI = 0x01 << 5;
    uint8_t uNaluType = NALU_TYPE_IFRAME;
    uint8_t uNaluVal = uNaluNRI | uNaluType;
    uint8_t pFrame[] = {0x00, 0x00, 0x01, uNaluVal, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    EXPECT_EQ(uNaluType, NALU_getNaluType(pFrame, uFrameLen));
}

TEST(NALU_getNaluType, annexb_4bytes_header)
{
    uint8_t uNaluNRI = 0x01 << 5;
    uint8_t uNaluType = NALU_TYPE_IFRAME;
    uint8_t uNaluVal = uNaluNRI | uNaluType;
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, uNaluVal, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    EXPECT_EQ(uNaluType, NALU_getNaluType(pFrame, uFrameLen));
}

TEST(NALU_getNaluType, avcc_header)
{
    uint8_t uNaluNRI = 0x01 << 5;
    uint8_t uNaluType = NALU_TYPE_IFRAME;
    uint8_t uNaluVal = uNaluNRI | uNaluType;
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x02, uNaluVal, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    EXPECT_EQ(uNaluType, NALU_getNaluType(pFrame, uFrameLen));
}

TEST(NALU_getNaluType, invalid_parameter)
{
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    /* Test invalid pointer */
    EXPECT_EQ(NALU_TYPE_UNKNOWN, NALU_getNaluType(NULL, uFrameLen));

    /* Test invalid length */
    EXPECT_EQ(NALU_TYPE_UNKNOWN, NALU_getNaluType(pFrame, 0));
}

TEST(NALU_getNaluFromAvccNalus, valid_nalus)
{
    int res = 0;
    uint8_t pSps[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x19, 
        /* SPS */
        0x67, 0x64, 0x00, 0x0A, 0xAC, 0x72, 0x84, 0x44,
        0x26, 0x84, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x00, 0x03, 0x00, 0xCA, 0x3C, 0x48, 0x96, 0x11,
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint8_t pPps[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x07,
        /* PPS */
        0x68, 0xE8, 0x43, 0x8F, 0x13, 0x21, 0x30
    };
    size_t uPpsLen = sizeof(pPps) / sizeof(pPps[0]);
    uint8_t pIframe[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x02,
        /* I-frame */
        0x65, 0xFF
    };
    size_t uIframeLen = sizeof(pIframe) / sizeof(pIframe[0]);

    uint8_t pAvccBuf[uSpsLen + uPpsLen + uIframeLen];
    size_t uAvccLen = sizeof(pAvccBuf) / sizeof(pAvccBuf[0]);
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    memcpy(pAvccBuf, pSps, uSpsLen);
    memcpy(pAvccBuf + uSpsLen, pPps, uPpsLen);
    memcpy(pAvccBuf + uSpsLen + uPpsLen, pIframe, uIframeLen);

    /* Test if it can find SPS NALU at the beginning of NALUs. */
    res = NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_SPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uSpsLen == uNaluLen + 4); /* 4 for header */

    /* Test if it can find PPS NALU at the middle of NALUs. */
    res = NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_PPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + uSpsLen + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uPpsLen == uNaluLen + 4); /* 4 for header */

    /* Test if it can find I-frame NALU at the end of NALUs. */
    res = NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_IFRAME, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + uSpsLen + uPpsLen + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uIframeLen == uNaluLen + 4); /* 4 for header */
}

TEST(NALU_getNaluFromAvccNalus, invalid_parameter)
{
    uint8_t uNaluNRI = 0x01 << 5;
    uint8_t uNaluType = NALU_TYPE_IFRAME;
    uint8_t uNaluVal = uNaluNRI | uNaluType;
    uint8_t pAvccBuf[] = {0x00, 0x00, 0x00, 0x02, uNaluVal, 0xFF};
    size_t uAvccLen = sizeof(pAvccBuf) / sizeof(pAvccBuf[0]);
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(NULL, uAvccLen, uNaluType, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAvccBuf, 0, uNaluType, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_UNKNOWN, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, 0xFF, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, uNaluType, NULL, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, uNaluType, &pNalu, NULL));
}

TEST(NALU_getNaluFromAnnexBNalus, valid_nalus_with_3bytes_header)
{
    int res = 0;
    uint8_t pSps[] = {
        /* NALU header */
        0x00, 0x00, 0x01, 
        /* SPS */
        0x67, 0x64, 0x00, 0x0A, 0xAC, 0x72, 0x84, 0x44,
        0x26, 0x84, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x00, 0x03, 0x00, 0xCA, 0x3C, 0x48, 0x96, 0x11,
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint8_t pPps[] = {
        /* NALU header */
        0x00, 0x00, 0x01,
        /* PPS */
        0x68, 0xE8, 0x43, 0x8F, 0x13, 0x21, 0x30
    };
    size_t uPpsLen = sizeof(pPps) / sizeof(pPps[0]);
    uint8_t pIframe[] = {
        /* NALU header */
        0x00, 0x00, 0x01,
        /* I-frame */
        0x65, 0xFF
    };
    size_t uIframeLen = sizeof(pIframe) / sizeof(pIframe[0]);

    uint8_t pAvccBuf[uSpsLen + uPpsLen + uIframeLen];
    size_t uAvccLen = sizeof(pAvccBuf) / sizeof(pAvccBuf[0]);
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    memcpy(pAvccBuf, pSps, uSpsLen);
    memcpy(pAvccBuf + uSpsLen, pPps, uPpsLen);
    memcpy(pAvccBuf + uSpsLen + uPpsLen, pIframe, uIframeLen);

    /* Test if it can find SPS NALU at the beginning of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAvccBuf, uAvccLen, NALU_TYPE_SPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + 3 == pNalu); /* 3 for header */
    EXPECT_TRUE(uSpsLen == uNaluLen + 3); /* 3 for header */

    /* Test if it can find PPS NALU at the middle of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAvccBuf, uAvccLen, NALU_TYPE_PPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + uSpsLen + 3 == pNalu); /* 3 for header */
    EXPECT_TRUE(uPpsLen == uNaluLen + 3); /* 3 for header */

    /* Test if it can find I-frame NALU at the end of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAvccBuf, uAvccLen, NALU_TYPE_IFRAME, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAvccBuf + uSpsLen + uPpsLen + 3 == pNalu); /* 3 for header */
    EXPECT_TRUE(uIframeLen == uNaluLen + 3); /* 3 for header */
}

TEST(NALU_getNaluFromAnnexBNalus, valid_nalus_with_4bytes_header)
{
    int res = 0;
    uint8_t pSps[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x01, 
        /* SPS */
        0x67, 0x64, 0x00, 0x0A, 0xAC, 0x72, 0x84, 0x44,
        0x26, 0x84, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x00, 0x03, 0x00, 0xCA, 0x3C, 0x48, 0x96, 0x11,
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint8_t pPps[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x01,
        /* PPS */
        0x68, 0xE8, 0x43, 0x8F, 0x13, 0x21, 0x30
    };
    size_t uPpsLen = sizeof(pPps) / sizeof(pPps[0]);
    uint8_t pIframe[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x01,
        /* I-frame */
        0x65, 0xFF
    };
    size_t uIframeLen = sizeof(pIframe) / sizeof(pIframe[0]);

    uint8_t pAnnexbBuf[uSpsLen + uPpsLen + uIframeLen];
    size_t uAnnexbLen = sizeof(pAnnexbBuf) / sizeof(pAnnexbBuf[0]);
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    memcpy(pAnnexbBuf, pSps, uSpsLen);
    memcpy(pAnnexbBuf + uSpsLen, pPps, uPpsLen);
    memcpy(pAnnexbBuf + uSpsLen + uPpsLen, pIframe, uIframeLen);

    /* Test if it can find SPS NALU at the beginning of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAnnexbBuf, uAnnexbLen, NALU_TYPE_SPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAnnexbBuf + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uSpsLen == uNaluLen + 4); /* 4 for header */

    /* Test if it can find PPS NALU at the middle of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAnnexbBuf, uAnnexbLen, NALU_TYPE_PPS, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAnnexbBuf + uSpsLen + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uPpsLen == uNaluLen + 4); /* 4 for header */

    /* Test if it can find I-frame NALU at the end of NALUs. */
    res = NALU_getNaluFromAnnexBNalus(pAnnexbBuf, uAnnexbLen, NALU_TYPE_IFRAME, &pNalu, &uNaluLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(pAnnexbBuf + uSpsLen + uPpsLen + 4 == pNalu); /* 4 for header */
    EXPECT_TRUE(uIframeLen == uNaluLen + 4); /* 4 for header */
}

TEST(NALU_getNaluFromAnnexBNalus, invalid_parameter)
{
    uint8_t uNaluNRI = 0x01 << 5;
    uint8_t uNaluType = NALU_TYPE_IFRAME;
    uint8_t uNaluVal = uNaluNRI | uNaluType;
    uint8_t pAnnexbBuf[] = {0x00, 0x00, 0x00, 0x01, uNaluVal, 0xFF};
    size_t uAnnexbLen = sizeof(pAnnexbBuf) / sizeof(pAnnexbBuf[0]);
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(NULL, uAnnexbLen, uNaluType, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAnnexbBuf, 0, uNaluType, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAnnexbBuf, uAnnexbLen, NALU_TYPE_UNKNOWN, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAnnexbBuf, uAnnexbLen, 0xFF, &pNalu, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAnnexbBuf, uAnnexbLen, uNaluType, NULL, &uNaluLen));

    EXPECT_NE(0, NALU_getNaluFromAvccNalus(pAnnexbBuf, uAnnexbLen, uNaluType, &pNalu, NULL));
}

TEST(NALU_isAnnexBFrame, valid_annexb_frames)
{
    {
        /* Valid 3 bytes header */
        uint8_t pFrame[] = {0x00, 0x00, 0x01, 0x65, 0xFF};
        size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

        EXPECT_TRUE(NALU_isAnnexBFrame(pFrame, uFrameLen));
    }

    {
        /* Valid 4 bytes header */
        uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
        size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

        EXPECT_TRUE(NALU_isAnnexBFrame(pFrame, uFrameLen));
    }
}

TEST(NALU_isAnnexBFrame, invalid_annexb_frames)
{
    {
        /* Invalid 3 bytes header */
        uint8_t pFrame[] = {0x00, 0x00, 0x02, 0x65, 0xFF};
        size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

        EXPECT_FALSE(NALU_isAnnexBFrame(pFrame, uFrameLen));
    }

    {
        /* Invalid 4 bytes header */
        uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x02, 0x65, 0xFF};
        size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

        EXPECT_FALSE(NALU_isAnnexBFrame(pFrame, uFrameLen));
    }
}

TEST(NALU_isAnnexBFrame, invalid_parameter)
{
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
    size_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);

    /* Test invalid pointer */
    EXPECT_FALSE(NALU_isAnnexBFrame(NULL, uFrameLen));

    /* Test invalid length */
    EXPECT_FALSE(NALU_isAnnexBFrame(pFrame, 0));
}

TEST(NALU_convertAnnexBToAvccInPlace, annexb_4_bytes_header)
{
    int res = 0;
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
    uint32_t uFrameSize = sizeof(pFrame) / sizeof(pFrame[0]);
    uint32_t uFrameLen = uFrameSize;
    uint32_t uAvccLen = 0;

    res = NALU_convertAnnexBToAvccInPlace(pFrame, uFrameLen, uFrameSize, &uAvccLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(uFrameLen == uAvccLen);
    EXPECT_TRUE(pFrame[3] == 0x02); // The AVCC length should be 2

}

TEST(NALU_convertAnnexBToAvccInPlace, annexb_3_bytes_header)
{
    int res = 0;
    uint8_t pFrame[] = {0x00, 0x00, 0x01, 0x65, 0xFF, 0x00};
    uint32_t uFrameSize = sizeof(pFrame) / sizeof(pFrame[0]);
    uint32_t uFrameLen = uFrameSize - 1;
    uint32_t uAvccLen = 0;

    res = NALU_convertAnnexBToAvccInPlace(pFrame, uFrameLen, uFrameSize, &uAvccLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(uFrameLen + 1 == uAvccLen);
    EXPECT_TRUE(pFrame[3] == 0x02); // The AVCC length should be 2
}

TEST(NALU_convertAnnexBToAvccInPlace, more_than_one_annexb)
{
    int res = 0;
    uint8_t pSps[] = {
        /* NALU header */
        0x00, 0x00, 0x01, 
        /* SPS */
        0x67, 0x64, 0x00, 0x0A, 0xAC, 0x72, 0x84, 0x44,
        0x26, 0x84, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x00, 0x03, 0x00, 0xCA, 0x3C, 0x48, 0x96, 0x11,
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint8_t pPps[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x01,
        /* PPS */
        0x68, 0xE8, 0x43, 0x8F, 0x13, 0x21, 0x30
    };
    size_t uPpsLen = sizeof(pPps) / sizeof(pPps[0]);
    uint8_t pIframe[] = {
        /* NALU header */
        0x00, 0x00, 0x00, 0x01,
        /* I-frame */
        0x65, 0xFF
    };
    size_t uIframeLen = sizeof(pIframe) / sizeof(pIframe[0]);

    size_t extra_bufsize = 4;
    uint8_t pAnnexbBuf[uSpsLen + uPpsLen + uIframeLen + extra_bufsize];
    uint32_t uAnnexbSize = sizeof(pAnnexbBuf) / sizeof(pAnnexbBuf[0]);
    uint32_t uAnnexbLen = uAnnexbSize - extra_bufsize;
    uint32_t uAvccLen = 0;

    memcpy(pAnnexbBuf, pSps, uSpsLen);
    memcpy(pAnnexbBuf + uSpsLen, pPps, uPpsLen);
    memcpy(pAnnexbBuf + uSpsLen + uPpsLen, pIframe, uIframeLen);

    res = NALU_convertAnnexBToAvccInPlace(pAnnexbBuf, uAnnexbLen, uAnnexbSize, &uAvccLen);
    EXPECT_EQ(0, res);
    EXPECT_TRUE(uAnnexbLen + 1 == uAvccLen);
}

TEST(NALU_convertAnnexBToAvccInPlace, invalid_parameter)
{
    uint8_t pFrame[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0xFF};
    uint32_t uFrameLen = sizeof(pFrame) / sizeof(pFrame[0]);
    uint32_t uAvccLen = 0;

    /* Test invalid pointer */
    EXPECT_NE(0, NALU_convertAnnexBToAvccInPlace(NULL, uFrameLen, uFrameLen, &uAvccLen));

    /* Test invalid length */
    EXPECT_NE(0, NALU_convertAnnexBToAvccInPlace(pFrame, 0, uFrameLen, &uAvccLen));

    /* Test invalid size */
    EXPECT_NE(0, NALU_convertAnnexBToAvccInPlace(pFrame, uFrameLen, 0, &uAvccLen));

    /* Test invalid pointer for returned AVCC length */
    EXPECT_NE(0, NALU_convertAnnexBToAvccInPlace(pFrame, uFrameLen, uFrameLen, NULL));
}

TEST(NALU_getH264VideoResolutionFromSps, valid_sps)
{
    int res = 0;
    uint8_t pSps[] = {
        /* SPS */
        0x67, 0x42, 0x80, 0x1e, 0xda, 0x02, 0x80, 0xf6, 
        0x94, 0x82, 0x83, 0x03, 0x03, 0x68, 0x50, 0x9a, 
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint16_t uWidth = 0;
    uint16_t uHeight = 0;

    res = NALU_getH264VideoResolutionFromSps(pSps, uSpsLen, &uWidth, &uHeight);
    EXPECT_EQ(0, res);
    EXPECT_EQ(640, uWidth);
    EXPECT_EQ(480, uHeight);
}

TEST(NALU_getH264VideoResolutionFromSps, invalid_parameter)
{
    uint8_t pSps[] = {
        /* SPS */
        0x67, 0x64, 0x00, 0x0A, 0xAC, 0x72, 0x84, 0x44,
        0x26, 0x84, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
        0x00, 0x03, 0x00, 0xCA, 0x3C, 0x48, 0x96, 0x11,
        0x80
    };
    size_t uSpsLen = sizeof(pSps) / sizeof(pSps[0]);
    uint16_t uWidth = 0;
    uint16_t uHeight = 0;

    EXPECT_NE(0, NALU_getH264VideoResolutionFromSps(NULL, uSpsLen, &uWidth, &uHeight));

    EXPECT_NE(0, NALU_getH264VideoResolutionFromSps(pSps, 0, &uWidth, &uHeight));

    EXPECT_NE(0, NALU_getH264VideoResolutionFromSps(pSps, uSpsLen, NULL, &uHeight));

    EXPECT_NE(0, NALU_getH264VideoResolutionFromSps(pSps, uSpsLen, &uWidth, NULL));
}