/* Third party headers */
#include "llhttp.h"

/* Public headers */
#include "kvs/errors.h"

/* Internal headers */
#include "net/http_parser_adapter.h"

typedef struct
{
    llhttp_settings_t xSettings;
    const char *pBodyLoc;
    size_t uBodyLen;
} llhttp_settings_ex_t;

static int prvHandleHttpOnBodyComplete(llhttp_t *pHttpParser, const char *at, size_t length)
{
    llhttp_settings_ex_t *pxSettings = (llhttp_settings_ex_t *)(pHttpParser->settings);
    pxSettings->pBodyLoc = at;
    pxSettings->uBodyLen = length;
    return 0;
}

static enum llhttp_errno prvParseHttpResponse(const char *pBuf, size_t uLen, unsigned int *puStatusCode, const char **ppBodyLoc, size_t *puBodyLen)
{
    llhttp_t xHttpParser = {0};
    llhttp_settings_ex_t xSettings = {0};
    enum llhttp_errno xHttpErrno = HPE_OK;

    llhttp_settings_init(&(xSettings.xSettings));
    xSettings.xSettings.on_body = prvHandleHttpOnBodyComplete;
    llhttp_init(&xHttpParser, HTTP_RESPONSE, (llhttp_settings_t *)&xSettings);

    xHttpErrno = llhttp_execute(&xHttpParser, pBuf, (size_t)uLen);
    if (xHttpErrno == HPE_OK)
    {
        if (puStatusCode != NULL)
        {
            *puStatusCode = xHttpParser.status_code;
        }
        if (ppBodyLoc != NULL && puBodyLen != NULL)
        {
            *ppBodyLoc = xSettings.pBodyLoc;
            *puBodyLen = xSettings.uBodyLen;
        }
    }

    return xHttpErrno;
}

int HttpParser_parseHttpResponse(const char *pBuf, size_t uLen, unsigned int *puStatusCode, const char **ppBodyLoc, size_t *puBodyLen)
{
    int res = KVS_ERRNO_NONE;

    if (pBuf == NULL || uLen == 0)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((prvParseHttpResponse(pBuf, uLen, puStatusCode, ppBodyLoc, puBodyLen) != HPE_OK))
    {
        res = KVS_ERROR_HTTP_PARSE_EXECUTE_FAIL;
    }

    return res;
}