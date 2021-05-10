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

#ifndef AWS_SIGNER_V4_H
#define AWS_SIGNER_V4_H

typedef struct AwsSigV4 *AwsSigV4Handle;

/**
 * @brief Create AWS Signature V4 handle
 *
 * @param[in] pcHttpMethod HTTP method. (Ex. GET, PUT, or POST)
 * @param[in] pcUri The relative path of the URI
 * @param[in] pcQuery The query string. (Exclude the question character)
 * @return The handle of AWS Signature V4 handle on success. On error, NULL is returned.
 */
AwsSigV4Handle AwsSigV4_Create(char *pcHttpMethod, char *pcUri, char *pcQuery);

/**
 * @brief Terminate AWS Signature V4 handle
 *
 * @param[in] xSigV4Handle The handle of AWS Signature V4 handle
 */
void AwsSigV4_Terminate(AwsSigV4Handle xSigV4Handle);

/**
 * @brief Add a HTTP header into canonical request.
 *
 * Add a HTTP header into canonical request. Please note that the header should be in lower-case and should be added
 * in sorted alphabet order.
 *
 * @param[in] xSigV4Handle The AWS Signature V4 handle
 * @param[in] pcHeader The HTTP header name in lower case
 * @param[in] pcValue The HTTP header value
 * @return 0 on success, non-zero value otherwise
 */
int AwsSigV4_AddCanonicalHeader(AwsSigV4Handle xSigV4Handle, const char *pcHeader, const char *pcValue);

/**
 * @brief Add HTTP body into canonical request.
 *
 * Add HTTP body into canonical request. It's the last part of the canonical request. Please note this function is
 * required even there is no HTTP body because an empty body still has an encoded hex result which is required in
 * canonical request.
 *
 * @param[in] xSigV4Handle The AWS Signature V4 handle
 * @param[in] pBody The HTTP body
 * @param[in] uBodyLen The HTTP body length
 * @return 0 on success, non-zero value otherwise
 */
int AwsSigV4_AddCanonicalBody(AwsSigV4Handle xSigV4Handle, const char *pBody, size_t uBodyLen);

/**
 * @brief Sign the canonical request with key and other information
 *
 * @param xSigV4Handle The AWS Signature V4 handle
 * @param pcAccessKey The AWS IAM access key
 * @param pcSecretKey The AWS IAM secret key
 * @param pcRegion AWS region
 * @param pcService AWS service
 * @param pcXAmzDate Date with AWS x-amz-date format
 * @return 0 on success, non-zero value otherwise
 */
int AwsSigV4_Sign(AwsSigV4Handle xSigV4Handle, char *pcAccessKey, char *pcSecretKey, char *pcRegion, char *pcService, const char *pcXAmzDate);

/**
 * @brief After sign, get the Authorization as the value of HTTP header "authorization"
 *
 * @param xSigV4Handle The AWS Signature V4 handle
 * @return The value of HTTP header "authorization"
 */
const char *AwsSigV4_GetAuthorization(AwsSigV4Handle xSigV4Handle);

#endif /* AWS_SIGNER_V4_H */