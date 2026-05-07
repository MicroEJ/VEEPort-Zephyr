# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.


include_guard()
message("microej/security component is included.")

# required to include "pk_wrap.h" with MBEDTLS 3.0
target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/../modules/crypto/mbedtls/library
)

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_CIPHER_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_DIGEST_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_KEY_AGREEMENT_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_KEY_FACTORY_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_KEY_PAIR_GENERATOR_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_MAC_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_PRIVATE_KEY_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_PUBLIC_KEY_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_RANDOM_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_RSA_CIPHER_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_SECRET_KEY_FACTORY_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_SECRET_KEY_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_SIG_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLSEC_X509_CERT_impl.c
)
