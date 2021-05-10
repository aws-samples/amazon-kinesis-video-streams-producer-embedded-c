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

#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <inttypes.h>
#include <stdbool.h>
#include "parson.h"

/**
 * @brief Get string value of a JSON value
 *
 * @param pxRootObject[in] The root JSON object, which created from parson library.
 * @param pcName[in] The name of the property
 * @param bRemoveQuotes[in] Remove quotes if it's true
 * @return The value of the property on success, or NULL on error.
 */
char *json_object_dotget_serialize_to_string(const JSON_Object *pxRootObject, const char *pcName, bool bRemoveQuotes);

/**
 * @brief Get UINT64 value of a JSON value
 * 
 * @param 
 */
uint64_t json_object_dotget_uint64(const JSON_Object *pxRootObject, const char *pcName, int xBase);

#endif