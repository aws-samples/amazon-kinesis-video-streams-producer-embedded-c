#ifdef __cplusplus
extern "C" {
#include "net/http_parser_adapter.h"
}
#endif

#include <gtest/gtest.h>

TEST(HttpParser_parseHttpResponse, invalid_parameter)
{
    const char *pHttp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 90\r\n\r\n{\"StreamInfo\": {\"Status\": \"ACTIVE\",\"StreamARN\": \"xxxxxxxx\",\"StreamName\": \"my-kvs-stream\"}}";
    size_t uHttpLen = 161;
    const char *pExpectedBodyLoc = pHttp + 71;
    size_t uExpectedBodyLen = 90;
    unsigned int uStatusCode = 0;
    const char *pBodyLoc = NULL;
    size_t uBodyLen = 0;

    EXPECT_NE(0, HttpParser_parseHttpResponse(NULL, uHttpLen, &uStatusCode, &pBodyLoc, &uBodyLen));
    EXPECT_NE(0, HttpParser_parseHttpResponse(pHttp, 0, &uStatusCode, &pBodyLoc, &uBodyLen));
}

TEST(HttpParser_parseHttpResponse, valid_parameter)
{
    const char *pHttp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 90\r\n\r\n{\"StreamInfo\": {\"Status\": \"ACTIVE\",\"StreamARN\": \"xxxxxxxx\",\"StreamName\": \"my-kvs-stream\"}}";
    size_t uHttpLen = 161;
    const char *pExpectedBodyLoc = pHttp + 71;
    size_t uExpectedBodyLen = 90;
    unsigned int uStatusCode = 0;
    const char *pBodyLoc = NULL;
    size_t uBodyLen = 0;

    EXPECT_EQ(0, HttpParser_parseHttpResponse(pHttp, uHttpLen, &uStatusCode, &pBodyLoc, &uBodyLen));
    EXPECT_EQ(200, uStatusCode);
    EXPECT_EQ(uExpectedBodyLen, uBodyLen);
    EXPECT_EQ(pExpectedBodyLoc, pBodyLoc);
}

TEST(HttpParser_parseHttpResponse, valid_parameter_empty_result)
{
    const char *pHttp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 90\r\n\r\n{\"StreamInfo\": {\"Status\": \"ACTIVE\",\"StreamARN\": \"xxxxxxxx\",\"StreamName\": \"my-kvs-stream\"}}";
    size_t uHttpLen = 161;
    const char *pExpectedBodyLoc = pHttp + 71;
    size_t uExpectedBodyLen = 90;
    unsigned int uStatusCode = 0;
    const char *pBodyLoc = NULL;
    size_t uBodyLen = 0;

    EXPECT_EQ(0, HttpParser_parseHttpResponse(pHttp, uHttpLen, NULL, &pBodyLoc, &uBodyLen));
    EXPECT_EQ(0, HttpParser_parseHttpResponse(pHttp, uHttpLen, &uStatusCode, NULL, &uBodyLen));
    EXPECT_EQ(200, uStatusCode);
    EXPECT_EQ(0, HttpParser_parseHttpResponse(pHttp, uHttpLen, &uStatusCode, &pBodyLoc, NULL));
    EXPECT_EQ(200, uStatusCode);
}

TEST(HttpParser_parseHttpResponse, valid_parameter_without_body)
{
    const char *pHttp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
    size_t uHttpLen = 51;
    unsigned int uStatusCode = 0;
    const char *pBodyLoc = NULL;
    size_t uBodyLen = 0;

    EXPECT_EQ(0, HttpParser_parseHttpResponse(pHttp, uHttpLen, &uStatusCode, &pBodyLoc, &uBodyLen));
    EXPECT_EQ(200, uStatusCode);
    EXPECT_EQ(0, uBodyLen);
    EXPECT_EQ(NULL, pBodyLoc);
}