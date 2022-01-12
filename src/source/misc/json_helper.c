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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Internal headers */
#include "misc/json_helper.h"
#include "os/allocator.h"

char *json_object_dotget_serialize_to_string(const JSON_Object *pxRootObject, const char *pcName, bool bRemoveQuotes)
{
    char *pcRes = NULL;
    char *pcVal = NULL;
    size_t uValLen = 0;
    JSON_Value *pxJsonValue = NULL;

    if (pxRootObject != NULL)
    {
        if ((pxJsonValue = json_object_dotget_value(pxRootObject, pcName)) != NULL)
        {
            if ((pcVal = json_serialize_to_string(pxJsonValue)) != NULL)
            {
                uValLen = strlen(pcVal);
                if (bRemoveQuotes)
                {
                    if (uValLen > 2 && (pcRes = kvsMalloc(uValLen - 1)) != NULL)
                    {
                        memcpy(pcRes, pcVal + 1, uValLen - 2);
                        pcRes[uValLen - 2] = '\0';
                    }
                    json_free_serialized_string(pcVal);
                }
                else
                {
                    if (uValLen > 0 && (pcRes = kvsMalloc(uValLen + 1)) != NULL)
                    {
                        memcpy(pcRes, pcVal, uValLen);
                        pcRes[uValLen] = '\0';
                    }
                    json_free_serialized_string(pcVal);
                }
            }
        }
    }

    return pcRes;
}

uint64_t json_object_dotget_uint64(const JSON_Object *pxRootObject, const char *pcName, int xBase)
{
    uint64_t uVal = 0;
    char *pcValue = NULL;
    JSON_Value *pxJsonValue = NULL;

    if (pxRootObject != NULL)
    {
        pxJsonValue = json_object_dotget_value(pxRootObject, pcName);
        if (pxJsonValue != NULL)
        {
            pcValue = json_serialize_to_string(pxJsonValue);
            if (pcValue != NULL)
            {
                /* We have a valid string value here. */
                uVal = strtoull(pcValue, NULL, xBase);
                json_free_serialized_string(pcValue);
            }
        }
    }

    return uVal;
}