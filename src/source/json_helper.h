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

#ifndef _JSON_HELPER_H_
#define _JSON_HELPER_H_

#include <stdint.h>
#include <stdbool.h>
#include "parson.h"

char * json_object_dotget_serialize_to_string( const JSON_Object *pRootObject, const char * pName, bool bRemoveQuotes );

uint64_t json_object_dotget_uint64( const JSON_Object *pRootObject, const char * pName, int base );

#endif