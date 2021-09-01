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

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvs/mkv_parser.h"
#include "kvs/nalu.h"
#include "kvs/port.h"
#include "kvs/restapi.h"

#include "sample_config.h"

#if ENABLE_IOT_CREDENTIAL
#    include "kvs/iot_credential_provider.h"
#endif /* ENABLE_IOT_CREDENTIAL */

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#ifdef KVS_USE_POOL_ALLOCATOR
#include "kvs/pool_allocator.h"
static char pMemPool[POOL_ALLOCATOR_SIZE];
#endif

typedef struct Kvs
{
#if ENABLE_IOT_CREDENTIAL
    IotCredentialRequest_t xIotCredentialReq;
#endif /* ENABLE_IOT_CREDENTIAL */

    KvsServiceParameter_t xServicePara;
    KvsDescribeStreamParameter_t xDescPara;
    KvsCreateStreamParameter_t xCreatePara;
    KvsGetDataEndpointParameter_t xGetDataEpPara;
    KvsPutMediaParameter_t xPutMediaPara;

    PutMediaHandle xPutMediaHandle;

    char *pcFilename;
    FILE *fp;
    long int xOffset;
    long int xFileSize;

    bool bIsFileUploaded;
} Kvs_t;

static void sleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static int kvsInitialize(Kvs_t *pKvs, char *pcMkvFileName)
{
    int res = ERRNO_NONE;
    char *pcStreamName = KVS_STREAM_NAME;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pKvs, 0, sizeof(Kvs_t));

        if ((pKvs->fp = fopen(pcMkvFileName, "rb")) == NULL)
        {
            printf("Failed to open file:%s\r\n", pcMkvFileName);
            res = ERRNO_FAIL;
        }
        else if (fseek(pKvs->fp, 0L, SEEK_END) != 0 || ((pKvs->xFileSize = ftell(pKvs->fp)) < 0) || fseek(pKvs->fp, 0L, SEEK_SET) != 0)
        {
            printf("Failed to calculate file size\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
            pKvs->xServicePara.pcHost = AWS_KVS_HOST;
            pKvs->xServicePara.pcRegion = AWS_KVS_REGION;
            pKvs->xServicePara.pcService = AWS_KVS_SERVICE;
            pKvs->xServicePara.pcAccessKey = AWS_ACCESS_KEY;
            pKvs->xServicePara.pcSecretKey = AWS_SECRET_KEY;

            pKvs->xDescPara.pcStreamName = pcStreamName;

            pKvs->xCreatePara.pcStreamName = pcStreamName;
            pKvs->xCreatePara.uDataRetentionInHours = 2;

            pKvs->xGetDataEpPara.pcStreamName = pcStreamName;

            pKvs->xPutMediaPara.pcStreamName = pcStreamName;
            pKvs->xPutMediaPara.xTimecodeType = TIMECODE_TYPE_ABSOLUTE;

#if ENABLE_IOT_CREDENTIAL
            pKvs->xIotCredentialReq.pCredentialHost = CREDENTIALS_HOST;
            pKvs->xIotCredentialReq.pRoleAlias = ROLE_ALIAS;
            pKvs->xIotCredentialReq.pThingName = THING_NAME;
            pKvs->xIotCredentialReq.pRootCA = ROOT_CA;
            pKvs->xIotCredentialReq.pCertificate = CERTIFICATE;
            pKvs->xIotCredentialReq.pPrivateKey = PRIVATE_KEY;
#endif

            pKvs->xOffset = 0;
            pKvs->bIsFileUploaded = false;
        }

        if (res != ERRNO_NONE)
        {
            if (pKvs->fp != NULL)
            {
                fclose(pKvs->fp);
                pKvs->fp = NULL;
            }
        }
    }

    return res;
}

static void kvsTerminate(Kvs_t *pKvs)
{
    if (pKvs != NULL)
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
            free(pKvs->xServicePara.pcPutMediaEndpoint);
            pKvs->xServicePara.pcPutMediaEndpoint = NULL;
        }
        if (pKvs->fp != NULL)
        {
            fclose(pKvs->fp);
            pKvs->fp = NULL;
        }
    }
}

static int setupDataEndpoint(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
        }
        else
        {
            printf("Try to describe stream\r\n");
            if (Kvs_describeStream(&(pKvs->xServicePara), &(pKvs->xDescPara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
            {
                printf("Failed to describe stream\r\n");
                printf("Try to create stream\r\n");
                if (Kvs_createStream(&(pKvs->xServicePara), &(pKvs->xCreatePara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to create stream\r\n");
                    res = ERRNO_FAIL;
                }
            }

            if (res == ERRNO_NONE)
            {
                if (Kvs_getDataEndpoint(&(pKvs->xServicePara), &(pKvs->xGetDataEpPara), &uHttpStatusCode, &(pKvs->xServicePara.pcPutMediaEndpoint)) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to get data endpoint\r\n");
                    res = ERRNO_FAIL;
                }
            }
        }
    }

    if (res == ERRNO_NONE)
    {
        printf("PUT MEDIA endpoint: %s\r\n", pKvs->xServicePara.pcPutMediaEndpoint);
    }

    return res;
}

static int putMedia(Kvs_t *pKvs)
{
    int res = 0;
    unsigned int uHttpStatusCode = 0;
    char *pBuf = NULL;
    size_t uBufSize = DEFAULT_BUFSIZE;
    size_t uBytesToSend = 0;

    printf("Try to put media\r\n");
    if (pKvs == NULL)
    {
        printf("Invalid argument: pKvs\r\n");
        res = ERRNO_FAIL;
    }
    else if ((pBuf = (char *)malloc(uBufSize)) == NULL)
    {
        printf("OOM: pBuf\r\n");
    }
    else if (Kvs_putMediaStart(&(pKvs->xServicePara), &(pKvs->xPutMediaPara), &uHttpStatusCode, &(pKvs->xPutMediaHandle)) != 0 || uHttpStatusCode != 200)
    {
        printf("Failed to setup PUT MEDIA\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        while (1)
        {
            uBytesToSend = fread(pBuf, 1, uBufSize, pKvs->fp);
            if (uBytesToSend == 0)
            {
                pKvs->bIsFileUploaded = true;
                break;
            }
            if (Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, (uint8_t *)pBuf, uBytesToSend) != ERRNO_NONE)
            {
                printf("Failed to Kvs_putMediaUpdateRaw\r\n");
                res = ERRNO_FAIL;
                break;
            }
            if (Kvs_putMediaDoWork(pKvs->xPutMediaHandle) != ERRNO_NONE)
            {
                break;
            }
        }
    }

    printf("Leaving put media\r\n");
    Kvs_putMediaFinish(pKvs->xPutMediaHandle);
    pKvs->xPutMediaHandle = NULL;
    if (pBuf != NULL)
    {
        free(pBuf);
    }

    return res;
}

void Kvs_run(Kvs_t *pKvs, char *pcMkvFileName)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

#if ENABLE_IOT_CREDENTIAL
    IotCredentialToken_t *pToken = NULL;
#endif /* ENABLE_IOT_CREDENTIAL */

    if (kvsInitialize(pKvs, pcMkvFileName) != 0)
    {
        printf("Failed to initialize KVS\r\n");
    }
    else
    {
        while (1)
        {
#if ENABLE_IOT_CREDENTIAL
            Iot_credentialTerminate(pToken);
            if ((pToken = Iot_getCredential(&(pKvs->xIotCredentialReq))) == NULL)
            {
                printf("Failed to get Iot credential\r\n");
                break;
            }
            else
            {
                pKvs->xServicePara.pcAccessKey = pToken->pAccessKeyId;
                pKvs->xServicePara.pcSecretKey = pToken->pSecretAccessKey;
                pKvs->xServicePara.pcToken = pToken->pSessionToken;
            }
#endif

            if (setupDataEndpoint(pKvs) != ERRNO_NONE)
            {
                printf("Failed to get PUT MEDIA endpoint");
            }
            else if (putMedia(pKvs) != ERRNO_NONE)
            {
                printf("End of PUT MEDIA\r\n");
                break;
            }

            if (pKvs->bIsFileUploaded)
            {
                break;
            }
            sleepInMs(100); /* Wait for retry */
        }
    }

#if ENABLE_IOT_CREDENTIAL
    Iot_credentialTerminate(pToken);
#endif /* ENABLE_IOT_CREDENTIAL */

    kvsTerminate(pKvs);
}

void printUsage(char *pcProgramName)
{
    printf("Usage: %s -i MkvFile\r\n", pcProgramName);
    printf("\r\n");
}

int main(int argc, char *argv[])
{
    int res = 0;
    const char *optstring = "i:h";
    int c;
    Kvs_t xKvs = {0};

    char *pcMkvFilename = NULL;

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorInit((void *)pMemPool, sizeof(pMemPool));
#endif

    while ((c = getopt(argc, argv, optstring)) != -1)
    {
        switch (c)
        {
            case 'i':
                pcMkvFilename = optarg;
                break;
            case 'h':
                printUsage(argv[0]);
                break;
            default:
                printf("Unknown option\r\n");
        }
    }

    if (pcMkvFilename == NULL)
    {
        printUsage(argv[0]);
        res = -1;
    }
    else
    {
        platformInit();

        Kvs_run(&xKvs, pcMkvFilename);
    }

    printf("\r\n");

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorDeinit();
#endif

    return res;
}