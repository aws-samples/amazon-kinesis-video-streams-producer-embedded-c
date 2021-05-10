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

#ifndef KVS_STREAM_H
#define KVS_STREAM_H

#include "kvs/mkv_generator.h"

typedef struct DataFrameIn
{
    MkvClusterType_t xClusterType;
    char *pData;
    size_t uDataLen;
    uint64_t uTimestampMs;
    bool bIsKeyFrame;
    TrackType_t xTrackType;
    void *pUserData;
} DataFrameIn_t;

typedef struct DataFrame *DataFrameHandle;

typedef struct Stream *StreamHandle;

/**
 * @brief Create a stream
 *
 * @param[in] pVideoTrackInfo The video track info
 * @param[in] pAudioTrackInfo The audio track info if any
 * @return The stream handle on success, NULL otherwise
 */
StreamHandle Kvs_streamCreate(VideoTrackInfo_t *pVideoTrackInfo, AudioTrackInfo_t *pAudioTrackInfo);

/**
 * @brief Terminate a stream handle
 *
 * @param[in] xStreamHandle The stream handle
 */
void Kvs_streamTermintate(StreamHandle xStreamHandle);

/**
 * @brief Get MKV EBML and segment header from a stream
 *
 * @param[in] xStreamHandle The stream handle
 * @param[out] ppMkvHeader The MKV EBML and segment header
 * @param[out] puMkvHeaderLen The length of MKV header
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_streamGetMkvEbmlSegHdr(StreamHandle xStreamHandle, uint8_t **ppMkvHeader, size_t *puMkvHeaderLen);

/**
 * @brief Add a data Frame to a stream
 *
 * Data members in DataFrameIn_t are set by application, then it will be added into stream with needed information
 * and return DataFrameHandle which wrapped these information.
 *
 * @param xStreamHandle[in] The stream handle
 * @param pxDataFrameIn[in] The data frame that is set by application
 * @return The data frame handle on success, NULL otherwise
 */
DataFrameHandle Kvs_streamAddDataFrame(StreamHandle xStreamHandle, DataFrameIn_t *pxDataFrameIn);

/**
 * @brief Pop a data frame from a stream
 *
 * @param xStreamHandle[in] The stream handle
 * @return data frame handle if data frame is available, NULL otherwise
 */
DataFrameHandle Kvs_streamPop(StreamHandle xStreamHandle);

/**
 * @brief Peek a data frame from a stream without pop it out
 *
 * @param xStreamHandle[in] The stream handle
 * @return data frame handle if data frame is available, NULL otherwise
 */
DataFrameHandle Kvs_streamPeek(StreamHandle xStreamHandle);

/**
 * @brief Check if there is any data available in the stream
 * 
 * @param xStreamHandle[in] The stream handle
 * @return true if there no data available, false otherwise
 */
bool Kvs_streamIsEmpty(StreamHandle xStreamHandle);

/**
 * @brief Check if a specific track type of data frame available in the stream
 *
 * Check if a specific track type of data frame available in the stream.  If the media has both video and audio 
 * track, it's necessary to check if both track type of data frame in the stream, otherwise a newly added data 
 * frame may have earlier timestamp than a data frame that has been sent.  The descending data frame would 
 * corrupt MKV data.
 *
 * @param xStreamHandle[in] The stream handle
 * @param xTrackType[in] The specific track type
 * @return true if the specific track type available, false otherwise
 */
bool Kvs_streamAvailOnTrack(StreamHandle xStreamHandle, TrackType_t xTrackType);

/**
 * @brief Get The total memory used in a stream
 *
 * Memory total = size of stream handle + MKV EBML & segment len + total size of data frame handle and data
 *
 * @param xStreamHandle[in] The stream handle
 * @param puMemTotal[out] The total memory used
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_streamMemStatTotal(StreamHandle xStreamHandle, size_t *puMemTotal);

/**
 * @brief Get MKV header and data from a data frame
 *
 * @param xDataFrameHandle[in] The data frame handle
 * @param ppMkvHeader[out] The MKV header
 * @param puMkvHeaderLen[out] THe MKV header length
 * @param ppData[out] The data pointer
 * @param puDataLen[out] The data length
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_dataFrameGetContent(DataFrameHandle xDataFrameHandle, uint8_t **ppMkvHeader, size_t *puMkvHeaderLen, uint8_t **ppData, size_t *puDataLen);

/**
 * @brief Terminate a data frame handle
 *
 * @param xDataFrameHandle[in] The data frame handle
 */
void Kvs_dataFrameTerminate(DataFrameHandle xDataFrameHandle);

#endif /* KVS_STREAM_H */