# KVS Error Code

In the KVS library, a zero value error code is treated as a success, and a negative integer value error code is treated as a failure. The error code might come from other modules or libraries like RESTful HTTP status code, MbedTLS error code, PutMedia error code, etc. Since all error codes from these modules or libraries are less than the maximum value of 16 bits integer, we keep the 0th-15th bit error code and use the 16th-19th bit as the module type.

```text
+-------+-------------+-------------------------------------------------+
|  Bits | Field Name  |                   Description                   |
+=======+=============+=================================================+
| 19:16 | Module type | It indicates which module gave the error code   |
+-------+-------------+-------------------------------------------------+
| 15:0  | Error code  | The error code in the module                    |
+-------+-------------+-------------------------------------------------+
```

When you get an error code, you can use macro `KVS_GET_ERROR_MODULE_TYPE` to get the module type and use macro `KVS_GET_ERROR_MODULE_CODE` to get the error code in this module. The following code snippet tells how to use it.

```c
int res = some_kvs_api();
printf("KVS error:-%X\n", -res);
// If the error is MBEDTLS_ERR_NET_RECV_FAILED(-0x004C), then the print result is
//      KVS error: -3004C

if (KVS_GET_ERROR_MODULE_TYPE(res) == KVS_MODULE_MBEDTLS)
{
    printf("Error from MbedTLS: -%X\n", -KVS_GET_ERROR_MODULE_CODE(res));
    //      Error from MbedTLS: -0x004C
}
// ......
```
