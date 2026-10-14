/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_CONFIGURATION_H
#define LLCRYPTO_CONFIGURATION_H

/* Include VEE Port User configuration file. */
#if defined __has_include
#if __has_include("veeport_configuration.h")
#include "veeport_configuration.h"
#endif // __has_include("veeport_configuration.h")
#else
#include "veeport_configuration.h"
#endif // defined __has_include

#include "veeport_logger.h"
#include "veeport_errors.h"

/**
 * @brief Log module shared by every source file of the Crypto Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_CRYPTO_LOG_LEVEL.
 */
#define LLCRYPTO_LOG_MODULE                          microej_crypto, VEEPORT_LOGGER_MODULE_LEVEL(CRYPTO)

#ifdef __cplusplus
extern "C" {
#endif

/* Suppress unused-parameter warnings */
#define LLCRYPTO_UNUSED_PARAM(x)                     ((void)(x))

/*
 * MbedTLS does not define a success constant. Define one here to avoid magic
 * numbers.
 */
#define LLCRYPTO_MBEDTLS_SUCCESS                     (0)

/*
 * Mbed TLS version compatibility. The port is written for Mbed TLS 4 (TF-PSA-Crypto); the few names it uses
 * that Mbed TLS 3.6 spells differently are provided here.
 */
#include <mbedtls/build_info.h>
#if MBEDTLS_VERSION_MAJOR == 3
#include <mbedtls/pk.h>
#include <mbedtls/psa_util.h>
/* Gate of the ECDSA DER <-> raw signature converters. */
#if defined(MBEDTLS_PSA_UTIL_HAVE_ECDSA)
#define PSA_HAVE_ALG_SOME_ECDSA
#endif
/* The X.509 signature algorithm identifiers are mbedtls_pk_type_t values in Mbed TLS 3. */
#define MBEDTLS_PK_SIGALG_RSA_PKCS1V15               MBEDTLS_PK_RSA
#define MBEDTLS_PK_SIGALG_ECDSA                      MBEDTLS_PK_ECDSA
#endif /* MBEDTLS_VERSION_MAJOR == 3 */

/* Enable local asserts by redirecting to the BSP's assert macro */
#if defined(CONFIG_ASSERT)
#include <zephyr/sys/__assert.h>
#define LLCRYPTO_ASSERT(x)                           __ASSERT_NO_MSG(x)
#else
/* CONFIG_ASSERT not set: expression is evaluated (no unused-variable warning) but not checked */
#define LLCRYPTO_ASSERT(x)                           ((void)(x))
#endif

/*
 * Compile-time assertion valid under C99. The port builds -std=c99, where the C11
 * _Static_assert keyword exists only as a GCC extension; unknown to a strict C99
 * parser, it trips MISRA 17.3 (implicit function declaration). This expands instead
 * to a typedef whose array size is negative when `cond` is false, so a violated
 * invariant fails compilation. `msg` documents the invariant at the call site.
 * Use at file scope: a block-scope typedef would trip -Wunused-local-typedefs.
 */
/* cppcheck-suppress misra-c2012-20.10 ; ## is needed to build a per-__LINE__ unique typedef name for the C99 static
 * assertion; there is no token-paste-free way to generate a distinct identifier per use. */
#define LLCRYPTO_STATIC_ASSERT_CONCAT_(a, b)         a ## b
#define LLCRYPTO_STATIC_ASSERT_CONCAT(a, b)          LLCRYPTO_STATIC_ASSERT_CONCAT_(a, b)
#define LLCRYPTO_STATIC_ASSERT(cond, msg) \
		typedef char LLCRYPTO_STATIC_ASSERT_CONCAT(llcrypto_static_assert_line_, __LINE__)[(cond) ? 1 : -1]

/*
 * Number of bits in a byte, used for byte-to-bit size conversions.
 */
#define LLCRYPTO_BITS_PER_BYTE                       (8U)

/*
 * Upper bound, in bytes, of an exported asymmetric key:
 *  - a private key as a PKCS#8 PrivateKeyInfo DER structure;
 *  - a public key as an X.509 SubjectPublicKeyInfo DER structure.
 * Sized for the largest supported key (RSA-2048) and reused as the scratch buffer the ASN.1 writer
 * fills from its end before the encoded key is moved to the front.
 */
#define LLCRYPTO_PRIVATE_KEY_MAX_PKCS8_LEN           (3072)
#define LLCRYPTO_PUBLIC_KEY_MAX_DER_LEN              (3072)

/**
 * RFC 3394 (classic AES Key Wrap — no padding)
 * We can estimate the size of the unwrapped key, by removing 8 bytes.
 * That extra 8 bytes is the integrity register (A) defined by the RFC.
 * This is useful if later the application wants to get the key size (ex: getEncoded())
 */
#define LLCRYPTO_RFC3394_INTEGRITY_REGISTER_BYTES    (8U)

/*
 * Shared native exception messages.
 *
 * Every message thrown from more than one call site is defined here so the wording
 * stays byte-identical everywhere: the linker stores merged identical string literals
 * only once in ROM, so any drift in wording costs a full extra copy. Messages do not
 * repeat the throwing function name — the Java stack trace of the NativeException
 * already identifies the native method, and the debug traces carry the function name
 * through Zephyr's own CONFIG_LOG_FUNC_NAME_PREFIX_DBG prefix.
 */
#define LLCRYPTO_MSG_NOT_IMPLEMENTED                 "Not implemented"
#define LLCRYPTO_MSG_REQUIRES_CRACEN_KMU             "Requires CRACEN KMU"
#define LLCRYPTO_MSG_REQUIRES_OBERON_PSA             "Requires Oberon PSA driver"
#define LLCRYPTO_MSG_INVALID_LENGTH                  "Invalid length"
#define LLCRYPTO_MSG_INVALID_OFFSET_LENGTH           "Invalid buffer offset or length"
#define LLCRYPTO_MSG_BUFFER_TOO_SMALL                "Buffer too small"
#define LLCRYPTO_MSG_EXPORT_FAILED                   "Export failed"
#define LLCRYPTO_MSG_SNI_REGISTER_FAILED             "Can't register SNI native resource"
#define LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED         "Algorithm not supported"
#define LLCRYPTO_MSG_INVALID_KEY_HANDLE              "Invalid key handle"
#define LLCRYPTO_MSG_KMU_COPY_FAILED                 "Key copy in KMU failed"
#define LLCRYPTO_MSG_GET_PSA_KEY_FAILED              "Failed to get PSA key"
#define LLCRYPTO_MSG_GET_PSA_PRIVATE_KEY_FAILED      "Failed to get PSA key for private key"
#define LLCRYPTO_MSG_KEY_GENERATION_FAILED           "Key generation failed"
#define LLCRYPTO_MSG_PARSE_FAILED                    "failed to parse"
#define LLCRYPTO_MSG_INIT_FAILED                     "init failed"
#define LLCRYPTO_MSG_UPDATE_FAILED                   "update failed"
#define LLCRYPTO_MSG_WRONG_DIRECTION                 "Cipher direction mismatch"

/* Set to 1 to enable profiling across all LLCRYPTO modules */
#ifndef LLCRYPTO_PROFILE
#define LLCRYPTO_PROFILE                             (0)
#endif

#if LLCRYPTO_PROFILE

#include <inttypes.h>
#include "microej_time.h"

#define LLCRYPTO_PROFILE_START()       int64_t _profile_start_time = microej_time_get_application_time()
#define LLCRYPTO_PROFILE_END()                                                                     \
		do {                                                                                       \
			int64_t _profile_duration = microej_time_get_application_time() - _profile_start_time; \
			VEEPORT_LOGGER_LOG_INF("profile %" PRId64 "ms", _profile_duration);                    \
		} while (0)

#else

#define LLCRYPTO_PROFILE_START()       ((void)0)
#define LLCRYPTO_PROFILE_END()         ((void)0)

#endif /*LLCRYPTO_PROFILE*/

#ifdef __cplusplus
}
#endif

#endif /* LLCRYPTO_CONFIGURATION_H */
