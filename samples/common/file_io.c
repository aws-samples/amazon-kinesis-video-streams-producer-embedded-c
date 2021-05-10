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
#include <stdio.h>

#include "file_io.h"

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

int getFileSize(char * pcFilename, size_t *puFileSize)
{
    int res = ERRNO_NONE;
    FILE *fp = NULL;
    long xFileSize = 0;

    if (pcFilename == NULL || puFileSize == NULL)
    {
        printf("Invalid filename\r\n");
        res = ERRNO_FAIL;
    }
    else if ((fp = fopen(pcFilename, "rb")) == NULL)
    {
        printf("Failed to open file: %s\r\n", pcFilename);
        res = ERRNO_FAIL;
    }
    else if (fseek(fp, 0L, SEEK_END) != 0 || 
             ((xFileSize = ftell(fp)) < 0) ||
             fseek(fp, 0L, SEEK_SET) != 0)
    {
        printf("Failed to calculate file size\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        *puFileSize = xFileSize;
    }

    if (fp != NULL)
    {
        fclose(fp);
    }

    return res;
}

int readFile(char *pcFilename, char *pBuf, size_t uBufSize, size_t *puBytesRead)
{
    int res = ERRNO_NONE;
    FILE *fp = NULL;
    long xFileSize = 0;
    size_t uBytesRead = 0;

    if (pcFilename == NULL || pBuf == NULL || uBufSize == 0 || puBytesRead == NULL)
    {
        printf("Invalid filename\r\n");
        res = ERRNO_FAIL;
    }
    else if ((fp = fopen(pcFilename, "rb")) == NULL)
    {
        printf("Failed to open file: %s\r\n", pcFilename);
        res = ERRNO_FAIL;
    }
    else if (fseek(fp, 0L, SEEK_END) != 0 || 
             ((xFileSize = ftell(fp)) < 0) ||
             fseek(fp, 0L, SEEK_SET) != 0)
    {
        printf("Failed to calculate file size\r\n");
        res = ERRNO_FAIL;
    }
    else if (uBufSize < xFileSize)
    {
        printf("Buffer not enough\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        uBytesRead = fread(pBuf, 1, xFileSize, fp);
        *puBytesRead = uBytesRead;
    }

    fclose(fp);

    return res;
}