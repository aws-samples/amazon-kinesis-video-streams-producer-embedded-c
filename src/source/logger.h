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

#include <stdio.h>

#ifdef ENABLE_LOG_ERROR
#define LOG_ERROR(...) do { printf("ERROR:"); printf(__VA_ARGS__); } while (0)
#else
#define LOG_ERROR(...)
#endif

#ifdef ENABLE_LOG_WARN
#define LOG_WARN(...) do { printf("WARN:"); printf(__VA_ARGS__); } while (0)
#else
#define LOG_WARN(...)
#endif

#ifdef ENABLE_LOG_INFO
#define LOG_INFO(...) do { printf(__VA_ARGS__); } while (0)
#else
#define LOG_INFO(...)
#endif

#ifdef ENABLE_LOG_DEBUG
#define LOG_DEBUG(...) do { printf("DEBUG:"); printf(__VA_ARGS__); } while (0)
#else
#define LOG_DEBUG(...)
#endif