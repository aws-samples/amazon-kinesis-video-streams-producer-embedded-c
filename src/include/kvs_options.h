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

#ifndef _KVS_OPTIONS_H_
#define _KVS_OPTIONS_H_

/* The retry limits when KVS try to connect to server */
#define MAX_CONNECTION_RETRY                ( 3 )

/* The retry interval before next connection retry */
#define CONNECTION_RETRY_INTERVAL_IN_MS     ( 1000 )

/* The size of AWS Signer V4 buffer size */
#define AWS_SIGNER_V4_BUFFER_SIZE           ( 2048 )

/* The size of HTTP send buffer */
#define MAX_HTTP_SEND_BUFFER_LEN            ( 2048 )

/* The size of HTTP receive buffer */
#define MAX_HTTP_RECV_BUFFER_LEN            ( 2048 )

/* The sleep interval if there is no action */
#define PUT_MEDIA_IDLE_INTERVAL_IN_MS       ( 1 )

#endif // #ifndef _KVS_OPTIONS_H_