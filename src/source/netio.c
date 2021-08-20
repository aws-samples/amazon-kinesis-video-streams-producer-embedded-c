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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/time.h>

/* Thirdparty headers */
#include "azure_c_shared_utility/xlogging.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net.h"
#include "mbedtls/net_sockets.h"

/* Public headers */
#include "kvs/allocator.h"
#include "kvs/errors.h"

/* Internal headers */
#include "netio.h"

typedef struct NetIo
{
    /* Basic ssl connection parameters */
    mbedtls_net_context xFd;
    mbedtls_ssl_context xSsl;
    mbedtls_ssl_config xConf;
    mbedtls_ctr_drbg_context xCtrDrbg;
    mbedtls_entropy_context xEntropy;

    /* Variables for IoT credential provider. It's optional feature so we declare them as pointers. */
    mbedtls_x509_crt *pRootCA;
    mbedtls_x509_crt *pCert;
    mbedtls_pk_context *pPrivKey;
} NetIo_t;

static int prvCreateX509Cert(NetIo_t *pxNet)
{
    int xRes = KVS_ERRNO_NONE;

    if (pxNet == NULL || (pxNet->pRootCA = (mbedtls_x509_crt *)KVS_MALLOC(sizeof(mbedtls_x509_crt))) == NULL ||
        (pxNet->pCert = (mbedtls_x509_crt *)KVS_MALLOC(sizeof(mbedtls_x509_crt))) == NULL ||
        (pxNet->pPrivKey = (mbedtls_pk_context *)KVS_MALLOC(sizeof(mbedtls_pk_context))) == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        mbedtls_x509_crt_init(pxNet->pRootCA);
        mbedtls_x509_crt_init(pxNet->pCert);
        mbedtls_pk_init(pxNet->pPrivKey);
    }

    return xRes;
}

static int prvInitConfig(NetIo_t *pxNet, const char *pcRootCA, const char *pcCert, const char *pcPrivKey)
{
    int xRes = KVS_ERRNO_NONE;

    if (pxNet == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        mbedtls_ssl_set_bio(&(pxNet->xSsl), &(pxNet->xFd), mbedtls_net_send, mbedtls_net_recv, NULL);

        if (mbedtls_ssl_config_defaults(&(pxNet->xConf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
        {
            LogError("Failed to config ssl");
            xRes = KVS_ERRNO_FAIL;
        }
        else
        {
            mbedtls_ssl_conf_rng(&(pxNet->xConf), mbedtls_ctr_drbg_random, &(pxNet->xCtrDrbg));
            if (pcRootCA != NULL && pcCert != NULL && pcPrivKey != NULL)
            {
                if (mbedtls_x509_crt_parse(pxNet->pRootCA, (void *)pcRootCA, strlen(pcRootCA) + 1) != 0 ||
                    mbedtls_x509_crt_parse(pxNet->pCert, (void *)pcCert, strlen(pcCert) + 1) != 0 ||
                    mbedtls_pk_parse_key(pxNet->pPrivKey, (void *)pcPrivKey, strlen(pcPrivKey) + 1, NULL, 0) != 0)
                {
                    LogError("Failed to parse x509");
                    xRes = KVS_ERRNO_FAIL;
                }
                else
                {
                    mbedtls_ssl_conf_authmode(&(pxNet->xConf), MBEDTLS_SSL_VERIFY_REQUIRED);
                    mbedtls_ssl_conf_ca_chain(&(pxNet->xConf), pxNet->pRootCA, NULL);

                    if (mbedtls_ssl_conf_own_cert(&(pxNet->xConf), pxNet->pCert, pxNet->pPrivKey) != 0)
                    {
                        LogError("Failed to conf own cert");
                        xRes = KVS_ERRNO_FAIL;
                    }
                }
            }
            else
            {
                mbedtls_ssl_conf_authmode(&(pxNet->xConf), MBEDTLS_SSL_VERIFY_OPTIONAL);
            }
        }
    }

    if (xRes == KVS_ERRNO_NONE)
    {
        if (mbedtls_ssl_setup(&(pxNet->xSsl), &(pxNet->xConf)) != 0)
        {
            LogError("Failed to setup ssl");
            xRes = KVS_ERRNO_FAIL;
        }
    }

    return xRes;
}

static int prvConnect(NetIo_t *pxNet, const char *pcHost, const char *pcPort, const char *pcRootCA, const char *pcCert, const char *pcPrivKey)
{
    int xRes = KVS_ERRNO_NONE;
    int ret = 0;

    if (pxNet == NULL || pcHost == NULL || pcPort == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pcRootCA != NULL && pcCert != NULL && pcPrivKey != NULL) && prvCreateX509Cert(pxNet) != KVS_ERRNO_NONE)
    {
        LogError("Failed to init x509");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((ret = mbedtls_net_connect(&(pxNet->xFd), pcHost, pcPort, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        LogError("Failed to connect to %s:%s", pcHost, pcPort);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (prvInitConfig(pxNet, pcRootCA, pcCert, pcPrivKey) != KVS_ERRNO_NONE)
    {
        LogError("Failed to config ssl");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((ret = mbedtls_ssl_handshake(&(pxNet->xSsl))) != 0)
    {
        LogError("ssl handshake err (%d)", ret);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        /* nop */
    }

    return xRes;
}

NetIoHandle NetIo_Create(void)
{
    NetIo_t *pxNet = NULL;

    if ((pxNet = (NetIo_t *)KVS_MALLOC(sizeof(NetIo_t))) != NULL)
    {
        memset(pxNet, 0, sizeof(NetIo_t));

        mbedtls_net_init(&(pxNet->xFd));
        mbedtls_ssl_init(&(pxNet->xSsl));
        mbedtls_ssl_config_init(&(pxNet->xConf));
        mbedtls_ctr_drbg_init(&(pxNet->xCtrDrbg));
        mbedtls_entropy_init(&(pxNet->xEntropy));

        if (mbedtls_ctr_drbg_seed(&(pxNet->xCtrDrbg), mbedtls_entropy_func, &(pxNet->xEntropy), NULL, 0) != 0)
        {
            NetIo_Terminate(pxNet);
            pxNet = NULL;
        }
    }

    return pxNet;
}

void NetIo_Terminate(NetIoHandle xNetIoHandle)
{
    NetIo_t *pxNet = (NetIo_t *)xNetIoHandle;

    if (pxNet != NULL)
    {
        mbedtls_ctr_drbg_free(&(pxNet->xCtrDrbg));
        mbedtls_entropy_free(&(pxNet->xEntropy));
        mbedtls_net_free(&(pxNet->xFd));
        mbedtls_ssl_free(&(pxNet->xSsl));
        mbedtls_ssl_config_free(&(pxNet->xConf));

        if (pxNet->pRootCA != NULL)
        {
            mbedtls_x509_crt_free(pxNet->pRootCA);
            KVS_FREE(pxNet->pRootCA);
            pxNet->pRootCA = NULL;
        }

        if (pxNet->pCert != NULL)
        {
            mbedtls_x509_crt_free(pxNet->pCert);
            KVS_FREE(pxNet->pCert);
            pxNet->pCert = NULL;
        }

        if (pxNet->pPrivKey != NULL)
        {
            mbedtls_pk_free(pxNet->pPrivKey);
            KVS_FREE(pxNet->pPrivKey);
            pxNet->pPrivKey = NULL;
        }
        KVS_FREE(pxNet);
    }
}

int NetIo_Connect(NetIoHandle xNetIoHandle, const char *pcHost, const char *pcPort)
{
    return prvConnect(xNetIoHandle, pcHost, pcPort, NULL, NULL, NULL);
}

int NetIo_ConnectWithX509(NetIoHandle xNetIoHandle, const char *pcHost, const char *pcPort, const char *pcRootCA, const char *pcCert, const char *pcPrivKey)
{
    return prvConnect(xNetIoHandle, pcHost, pcPort, pcRootCA, pcCert, pcPrivKey);
}

void NetIo_Disconnect(NetIoHandle xNetIoHandle)
{
    NetIo_t *pxNet = (NetIo_t *)xNetIoHandle;

    if (pxNet != NULL)
    {
        mbedtls_ssl_close_notify(&(pxNet->xSsl));
    }
}

int NetIo_Send(NetIoHandle xNetIoHandle, const unsigned char *pBuffer, size_t uBytesToSend)
{
    int n = 0;
    int xRes = KVS_ERRNO_NONE;
    NetIo_t *pxNet = (NetIo_t *)xNetIoHandle;
    size_t uBytesRemaining = uBytesToSend;
    char *pIndex = (char *)pBuffer;

    if (pxNet == NULL || pBuffer == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        do
        {
            n = mbedtls_ssl_write(&(pxNet->xSsl), (const unsigned char *)pIndex, uBytesRemaining);
            if (n < 0 || n > uBytesRemaining)
            {
                LogError("SSL send error %d", n);
                xRes = KVS_ERRNO_FAIL;
                break;
            }
            uBytesRemaining -= n;
            pIndex += n;
        } while (uBytesRemaining > 0);
    }

    return xRes;
}

int NetIo_Recv(NetIoHandle xNetIoHandle, unsigned char *pBuffer, size_t uBufferSize, size_t *puBytesReceived)
{
    int n;
    int xRes = KVS_ERRNO_NONE;
    NetIo_t *pxNet = (NetIo_t *)xNetIoHandle;

    if (pxNet == NULL || pBuffer == NULL || puBytesReceived == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        n = mbedtls_ssl_read(&(pxNet->xSsl), pBuffer, uBufferSize);
        if (n < 0 || n > uBufferSize)
        {
            LogError("SSL recv error %d", n);
            xRes = KVS_ERRNO_FAIL;
        }
        else
        {
            *puBytesReceived = n;
        }
    }

    return xRes;
}

bool NetIo_isDataAvailable(NetIoHandle xNetIoHandle)
{
    int xRes = KVS_ERRNO_NONE;
    NetIo_t *pxNet = (NetIo_t *)xNetIoHandle;
    bool bDataAvailable = false;
    struct timeval tv = {0};
    fd_set read_fds = {0};
    int fd = 0;

    if (pxNet == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        fd = pxNet->xFd.fd;
        if (fd >= 0)
        {
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            if (select(fd + 1, &read_fds, NULL, NULL, &tv) >= 0)
            {
                if (FD_ISSET(fd, &read_fds))
                {
                    bDataAvailable = true;
                }
            }
        }
    }

    return bDataAvailable;
}