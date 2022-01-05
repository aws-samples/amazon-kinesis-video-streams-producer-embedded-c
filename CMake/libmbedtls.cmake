set(MBEDTLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/mbedtls)

set(MBEDTLS_SRC_CRYPTO
    ${MBEDTLS_DIR}/library/aes.c
    ${MBEDTLS_DIR}/library/aesni.c
    ${MBEDTLS_DIR}/library/arc4.c
    ${MBEDTLS_DIR}/library/aria.c
    ${MBEDTLS_DIR}/library/asn1parse.c
    ${MBEDTLS_DIR}/library/asn1write.c
    ${MBEDTLS_DIR}/library/base64.c
    ${MBEDTLS_DIR}/library/bignum.c
    ${MBEDTLS_DIR}/library/blowfish.c
    ${MBEDTLS_DIR}/library/camellia.c
    ${MBEDTLS_DIR}/library/ccm.c
    ${MBEDTLS_DIR}/library/chacha20.c
    ${MBEDTLS_DIR}/library/chachapoly.c
    ${MBEDTLS_DIR}/library/cipher.c
    ${MBEDTLS_DIR}/library/cipher_wrap.c
    ${MBEDTLS_DIR}/library/cmac.c
    ${MBEDTLS_DIR}/library/ctr_drbg.c
    ${MBEDTLS_DIR}/library/des.c
    ${MBEDTLS_DIR}/library/dhm.c
    ${MBEDTLS_DIR}/library/ecdh.c
    ${MBEDTLS_DIR}/library/ecdsa.c
    ${MBEDTLS_DIR}/library/ecjpake.c
    ${MBEDTLS_DIR}/library/ecp.c
    ${MBEDTLS_DIR}/library/ecp_curves.c
    ${MBEDTLS_DIR}/library/entropy.c
    ${MBEDTLS_DIR}/library/entropy_poll.c
    ${MBEDTLS_DIR}/library/error.c
    ${MBEDTLS_DIR}/library/gcm.c
    ${MBEDTLS_DIR}/library/havege.c
    ${MBEDTLS_DIR}/library/hkdf.c
    ${MBEDTLS_DIR}/library/hmac_drbg.c
    ${MBEDTLS_DIR}/library/md.c
    ${MBEDTLS_DIR}/library/md2.c
    ${MBEDTLS_DIR}/library/md4.c
    ${MBEDTLS_DIR}/library/md5.c
    ${MBEDTLS_DIR}/library/memory_buffer_alloc.c
    ${MBEDTLS_DIR}/library/nist_kw.c
    ${MBEDTLS_DIR}/library/oid.c
    ${MBEDTLS_DIR}/library/padlock.c
    ${MBEDTLS_DIR}/library/pem.c
    ${MBEDTLS_DIR}/library/pk.c
    ${MBEDTLS_DIR}/library/pk_wrap.c
    ${MBEDTLS_DIR}/library/pkcs12.c
    ${MBEDTLS_DIR}/library/pkcs5.c
    ${MBEDTLS_DIR}/library/pkparse.c
    ${MBEDTLS_DIR}/library/pkwrite.c
    ${MBEDTLS_DIR}/library/platform.c
    ${MBEDTLS_DIR}/library/platform_util.c
    ${MBEDTLS_DIR}/library/poly1305.c
    ${MBEDTLS_DIR}/library/psa_crypto.c
    ${MBEDTLS_DIR}/library/psa_crypto_se.c
    ${MBEDTLS_DIR}/library/psa_crypto_slot_management.c
    ${MBEDTLS_DIR}/library/psa_crypto_storage.c
    ${MBEDTLS_DIR}/library/psa_its_file.c
    ${MBEDTLS_DIR}/library/ripemd160.c
    ${MBEDTLS_DIR}/library/rsa.c
    ${MBEDTLS_DIR}/library/rsa_internal.c
    ${MBEDTLS_DIR}/library/sha1.c
    ${MBEDTLS_DIR}/library/sha256.c
    ${MBEDTLS_DIR}/library/sha512.c
    ${MBEDTLS_DIR}/library/threading.c
    ${MBEDTLS_DIR}/library/timing.c
    ${MBEDTLS_DIR}/library/version.c
    ${MBEDTLS_DIR}/library/version_features.c
    ${MBEDTLS_DIR}/library/xtea.c
)

set(MBEDTLS_SRC_X509
    ${MBEDTLS_DIR}/library/certs.c
    ${MBEDTLS_DIR}/library/pkcs11.c
    ${MBEDTLS_DIR}/library/x509.c
    ${MBEDTLS_DIR}/library/x509_create.c
    ${MBEDTLS_DIR}/library/x509_crl.c
    ${MBEDTLS_DIR}/library/x509_crt.c
    ${MBEDTLS_DIR}/library/x509_csr.c
    ${MBEDTLS_DIR}/library/x509write_crt.c
    ${MBEDTLS_DIR}/library/x509write_csr.c
)

set(MBEDTLS_SRC_TLS
    ${MBEDTLS_DIR}/library/debug.c
    ${MBEDTLS_DIR}/library/net_sockets.c
    ${MBEDTLS_DIR}/library/ssl_cache.c
    ${MBEDTLS_DIR}/library/ssl_ciphersuites.c
    ${MBEDTLS_DIR}/library/ssl_cli.c
    ${MBEDTLS_DIR}/library/ssl_cookie.c
    ${MBEDTLS_DIR}/library/ssl_msg.c
    ${MBEDTLS_DIR}/library/ssl_srv.c
    ${MBEDTLS_DIR}/library/ssl_ticket.c
    ${MBEDTLS_DIR}/library/ssl_tls.c
)

set(MBEDTLS_INC
    ${MBEDTLS_DIR}/include
)

# setup mbedcrypto static library
add_library(mbedcrypto STATIC ${MBEDTLS_SRC_CRYPTO})
target_include_directories(mbedcrypto PUBLIC ${MBEDTLS_INC})

# setup mbedx509 static library
add_library(mbedx509 STATIC ${MBEDTLS_SRC_X509})
target_include_directories(mbedx509 PUBLIC ${MBEDTLS_INC})

# setup mbedtls static library
add_library(mbedtls STATIC ${MBEDTLS_SRC_TLS})
target_include_directories(mbedtls PUBLIC ${MBEDTLS_INC})

include(GNUInstallDirs)

install(TARGETS mbedcrypto
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS mbedx509
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)

install(TARGETS mbedtls
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)
