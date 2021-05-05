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

#ifndef _AWS_IOT_CREDENTIAL_PROVIDER_H_
#define _AWS_IOT_CREDENTIAL_PROVIDER_H_

typedef struct
{
    char * pCredentialHost;
    char * pRoleAlias;
    char * pThingName;
    char * pRootCA;
    char * pCertificate;
    char * pPrivateKey;
} IotCredentialRequest_t;

typedef struct
{
    char * pAccessKeyId;
    char * pSecretAccessKey;
    char * pSessionToken;
} IotCredentialToken_t;

int32_t Iot_getCredential( IotCredentialRequest_t * pReq,
                           IotCredentialToken_t * pToken );

void Iot_terminateCredential( IotCredentialToken_t * pToken );

#endif // #ifndef _AWS_IOT_CREDENTIAL_PROVIDER_H_