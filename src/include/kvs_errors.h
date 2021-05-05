/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef _KVS_ERRORS_H_
#define _KVS_ERRORS_H_

#define KVS_STATUS_SUCCEEDED                            0

#define KVS_STATUS_GENERIC_BASE                         ( 100 )
#define KVS_STATUS_INVALID_ARG                          -( KVS_STATUS_GENERIC_BASE + 0 )
#define KVS_STATUS_NOT_ENOUGH_MEMORY                    -( KVS_STATUS_GENERIC_BASE + 1 )
#define KVS_STATUS_BUFFER_SIZE_NOT_ENOUGH               -( KVS_STATUS_GENERIC_BASE + 2 )
#define KVS_STATUS_BUFFER_OVERFLOW                      -( KVS_STATUS_GENERIC_BASE + 3 )
#define KVS_STATUS_FAIL_TO_CALCULATE_HASH               -( KVS_STATUS_GENERIC_BASE + 4 )

#define KVS_STATUS_NETWORK_BASE                         ( 200 )
#define KVS_STATUS_FAIL_TO_INIT_NETWORK                 -( KVS_STATUS_NETWORK_BASE + 0 )
#define KVS_STATUS_NETWORK_CONNECT_ERROR                -( KVS_STATUS_NETWORK_BASE + 1 )
#define KVS_STATUS_NETWORK_CONFIG_ERROR                 -( KVS_STATUS_NETWORK_BASE + 2 )
#define KVS_STATUS_NETWORK_SETUP_ERROR                  -( KVS_STATUS_NETWORK_BASE + 3 )
#define KVS_STATUS_NETWORK_SSL_HANDSHAKE_ERROR          -( KVS_STATUS_NETWORK_BASE + 4 )
#define KVS_STATUS_NETWORK_SEND_ERROR                   -( KVS_STATUS_NETWORK_BASE + 5 )
#define KVS_STATUS_NETWORK_RECV_ERROR                   -( KVS_STATUS_NETWORK_BASE + 6 )
#define KVS_STATUS_NETWORK_SOCKET_NOT_CONNECTED         -( KVS_STATUS_NETWORK_BASE + 7 )
#define KVS_STATUS_NETWORK_NO_AVAILABLE_RECV_DATA       -( KVS_STATUS_NETWORK_BASE + 8 )
#define KVS_STATUS_NETWORK_SELECT_ERROR                 -( KVS_STATUS_NETWORK_BASE + 9 )

#define KVS_STATUS_REST_BASE                            ( 300 )
#define KVS_STATUS_REST_UNKNOWN_ERROR                   -( KVS_STATUS_REST_BASE + 0 )
#define KVS_STATUS_REST_EXCEPTION_ERROR                 -( KVS_STATUS_REST_BASE + 1 )
#define KVS_STATUS_REST_NOT_AUTHORIZED_ERROR            -( KVS_STATUS_REST_BASE + 2 )
#define KVS_STATUS_REST_RES_NOT_FOUND_ERROR             -( KVS_STATUS_REST_BASE + 3 )

#define KVS_STATUS_MKV_BASE                             ( 400 )
#define KVS_STATUS_MKV_INVALID_ANNEXB_CONTENT           -( KVS_STATUS_MKV_BASE + 0 )
#define KVS_STATUS_MKV_FAIL_TO_PARSE_SPS_N_PPS          -( KVS_STATUS_MKV_BASE + 1 )
#define KVS_STATUS_MKV_FAIL_TO_GENERATE_CODEC_PRIVATE_DATA  -( KVS_STATUS_MKV_BASE + 2 )

#define KVS_STATUS_JSON_BASE                            ( 500 )
#define KVS_STATUS_JSON_PARSE_ERROR                     -( KVS_STATUS_JSON_BASE + 0 )

#define KVS_STATUS_IOT_CERTIFICATE_BASE                 ( 600 )
#define KVS_STATUS_IOT_CERTIFICATE_ERROR                -( KVS_STATUS_IOT_CERTIFICATE_BASE + 0 )

#define KVS_STATUS_FRAGMENT_BASE                        ( 700 )
#define KVS_STATUS_FRAGMENT_INVALID_FORMAT              -( KVS_STATUS_FRAGMENT_BASE + 0 )
#define KVS_STATUS_FRAGMENT_ERROR_ACK                   -( KVS_STATUS_FRAGMENT_BASE + 1 )

#define KVS_STATUS_PUT_MEDIA_BASE                       ( 800 )
#define KVS_STATUS_PUT_MEDIA_UNKNOWN_ERROR                              -( KVS_STATUS_PUT_MEDIA_BASE + 0  )
#define KVS_STATUS_PUT_MEDIA_STREAM_READ_ERROR                          -( KVS_STATUS_PUT_MEDIA_BASE + 1  )
#define KVS_STATUS_PUT_MEDIA_MAX_FRAGMENT_SIZE_REACHED                  -( KVS_STATUS_PUT_MEDIA_BASE + 2  )
#define KVS_STATUS_PUT_MEDIA_MAX_FRAGMENT_DURATION_REACHED              -( KVS_STATUS_PUT_MEDIA_BASE + 3  )
#define KVS_STATUS_PUT_MEDIA_MAX_CONNECTION_DURATION_REACHED            -( KVS_STATUS_PUT_MEDIA_BASE + 4  )
#define KVS_STATUS_PUT_MEDIA_FRAGMENT_TIMECODE_LESSER_THAN_PREVIOUS     -( KVS_STATUS_PUT_MEDIA_BASE + 5  )
#define KVS_STATUS_PUT_MEDIA_MORE_THAN_ALLOWED_TRACKS_FOUND             -( KVS_STATUS_PUT_MEDIA_BASE + 6  )
#define KVS_STATUS_PUT_MEDIA_INVALID_MKV_DATA                           -( KVS_STATUS_PUT_MEDIA_BASE + 7  )
#define KVS_STATUS_PUT_MEDIA_INVALID_PRODUCER_TIMESTAMP                 -( KVS_STATUS_PUT_MEDIA_BASE + 8  )
#define KVS_STATUS_PUT_MEDIA_STREAM_NOT_ACTIVE                          -( KVS_STATUS_PUT_MEDIA_BASE + 9  )
#define KVS_STATUS_PUT_MEDIA_FRAGMENT_METADATA_LIMIT_REACHED            -( KVS_STATUS_PUT_MEDIA_BASE + 10 )
#define KVS_STATUS_PUT_MEDIA_TRACK_NUMBER_MISMATCH                      -( KVS_STATUS_PUT_MEDIA_BASE + 11 )
#define KVS_STATUS_PUT_MEDIA_FRAMES_MISSING_FOR_TRACK                   -( KVS_STATUS_PUT_MEDIA_BASE + 12 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_ACCESS_DENIED                      -( KVS_STATUS_PUT_MEDIA_BASE + 13 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_DISABLED                           -( KVS_STATUS_PUT_MEDIA_BASE + 14 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_VALIDATION_ERROR                   -( KVS_STATUS_PUT_MEDIA_BASE + 15 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_UNAVAILABLE                        -( KVS_STATUS_PUT_MEDIA_BASE + 16 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_INVALID_USAGE                      -( KVS_STATUS_PUT_MEDIA_BASE + 17 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_INVALID_STATE                      -( KVS_STATUS_PUT_MEDIA_BASE + 18 )
#define KVS_STATUS_PUT_MEDIA_KMS_KEY_NOT_FOUND                          -( KVS_STATUS_PUT_MEDIA_BASE + 19 )
#define KVS_STATUS_PUT_MEDIA_INTERNAL_ERROR                             -( KVS_STATUS_PUT_MEDIA_BASE + 20 )
#define KVS_STATUS_PUT_MEDIA_ARCHIVAL_ERROR                             -( KVS_STATUS_PUT_MEDIA_BASE + 21 )

#endif // #ifndef _KVS_ERRORS_H_