/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <string.h>
#include <psa/crypto.h>
#include <LLCRYPTO_EXTERNAL_KEYSTORE_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>
#include <zephyr/sys/util.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
#include <cracen_psa_kmu.h>
#endif

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * The external keystore is backed by the Nordic CRACEN Key Management Unit (KMU), accessed through the
 * Oberon/CRACEN PSA driver (cracen_psa_kmu.h, PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT). These are NOT part
 * of the generic Zephyr / Mbed TLS 4.1 (TF-PSA-Crypto) stack, so the implementation is guarded with
 * CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU:
 *   - defined (e.g. nRF Connect SDK): the full KMU-backed implementation.
 *   - otherwise (generic Zephyr): the natives report the operation as not implemented, without any
 *     dependency on a CRACEN-only symbol.
 */

#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)

/* Key context from KMU */
typedef struct {
	const jbyte *alias;
	LLCRYPTO_entry_type type;
	psa_key_id_t psa_id;
} LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx;

/*
 * https://docs.nordicsemi.com/bundle/ncs-3.1.1/page/nrf/app_dev/device_guides/nrf54l/cryptography.html#ug-nrf54l-crypto-kmu-slots
 *
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------
 | Reserved KMU slots | nRF Connect usage | Description
 | (range inclusive)  |                   |
 | ---------------------------------------------------------------------------------------------------------------------------------------------------------------
 | 180-182            | Reserved          | --
 | 183-185            | IKG seed          | 384-bit random seed to generate keys using the CRACEN IKG.
 | 186                | Provisioning slot | Reserved slot for internal KMU usage. This slot is used to validate that
 |                                          provisioning of the KMU slots is completed.
 | 187-225            | Reserved          | --
 | 226-227            | UROT_PUBKEY_0     | Revokable firmware image key for upgradable bootloader, generation 0.
 |                                          ED25519 public key.
 | 228-229            | UROT_PUBKEY_1     | Revokable firmware image key for upgradable bootloader, generation 1.
 |                                          ED25519 public key.
 | 230-231            | UROT_PUBKEY_2     | Revokable firmware image key for upgradable bootloader, generation 2.
 |                                          ED25519 public key.
 | 232-241            | Reserved          | --
 | 242-243            | BL_PUBKEY_0       | Revokable firmware image key for immutable bootloader, generation 0.
 |                                          ED25519 public key.
 | 244-245            | BL_PUBKEY_1       | Revokable firmware image key for immutable bootloader, generation 1.
 |                                          ED25519 public key.
 | 246-247            | BL_PUBKEY_2       | Revokable firmware image key for immutable bootloader, generation 2.
 |                                          ED25519 public key.
 | 248-249            | Reserved          | Random bytes which invalidate the protected RAM content after an operation.
 | 250-255            | Reserved          | --
 |
 | Add key in non reserved key slot with following key type:
 | https://docs.nordicsemi.com/bundle/ncs-3.1.1/page/nrf/app_dev/device_guides/nrf54l/cryptography.html#supported_key_types
 |
 */
/* App signature verification key */
#define KEY_SAPP_SIGN_VERIF_KMU_SLOT (1U)   // ECC P-256 key => 4 KMU slots
/* Test keys */
#define KEY_AES_128_GCM_KMU_SLOT     (5U)   // GCM-AES 128 key => 1 KMU slot
#define KEY_AES_256_GCM_KMU_SLOT     (6U)   // GCM-AES 256 key => 2 KMU slots
/* DLMS/COSEM Keys */
#define KEK_KMU_SLOT                 (8U)   // GCM-AES 256 key => 2 KMU slots
#define GUEK_KMU_SLOT                (10U)  // GCM-AES 256 key => 2 KMU slots
#define GBEK_KMU_SLOT                (12U)  // GCM-AES 256 key => 2 KMU slots
#define GAK_KMU_SLOT                 (14U)  // GCM-AES 256 key => 2 KMU slots

/* ABLA global key table initialization */
static const LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx g_keystore_ctx[] = {
	/* App signature verification key */
	{ (const jbyte *)"KEY_SAPP_SIGN_VERIF",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, KEY_SAPP_SIGN_VERIF_KMU_SLOT) },
	/* Test keys */
	{ (const jbyte *)"KEY_AES_128_GCM",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, KEY_AES_128_GCM_KMU_SLOT) },
	{ (const jbyte *)"KEY_AES_256_GCM",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, KEY_AES_256_GCM_KMU_SLOT) },
	/* DLMS/COSEM Keys */
	{ (const jbyte *)"KEK",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, KEK_KMU_SLOT) },
	{ (const jbyte *)"GUEK",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, GUEK_KMU_SLOT) },
	{ (const jbyte *)"GBEK",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, GBEK_KMU_SLOT) },
	{ (const jbyte *)"GAK",
	  LLCRYPTO_ENTRY_TYPE_KEY,
	  PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, GAK_KMU_SLOT) }
};

/**
 * Search for a registered (case sensitive) alias
 *
 * @retval LLCRYPTO_ERROR if no key is registered with this alias
 */
static jint llcrypto_keystore_key_info_from_alias(const jbyte *alias, LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx *key_info) {
	jint return_code = LLCRYPTO_ERROR;

	VEEPORT_LOGGER_LOG_DBG("alias %s",
	                       ((NULL != alias) ? (const char *)alias : "none"));

	if (NULL != alias) {
		for (jint index = 0; index < (jint)ARRAY_SIZE(g_keystore_ctx); index++) {
			jboolean alias_is_present = (NULL != g_keystore_ctx[index].alias) &&
			                            (strcmp((const char *)g_keystore_ctx[index].alias, (const char *)alias) == 0);
			if (alias_is_present) {
				if (NULL != key_info) {
					VEEPORT_LOGGER_LOG_DBG("alias %s found with PSA ID %u",
					                       (const char *)alias, g_keystore_ctx[index].psa_id);
					(void)memcpy(key_info, &g_keystore_ctx[index], sizeof(LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx));
				}
				return_code = LLCRYPTO_SUCCESS;
				break;
			}
		}
	}

	return return_code;
}

#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */

// -----------------------------------------------------------------------------
// Key Store Common
// -----------------------------------------------------------------------------

void LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_login(jchar *password) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	VEEPORT_LOGGER_LOG_DBG("password %s", (NULL != password) ? "***" : "none");
	LLCRYPTO_UNUSED_PARAM(password); // Not used by KMU in NRF54L15-DK
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(password);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_size(void) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint return_code = ARRAY_SIZE(g_keystore_ctx);
	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getAlias(jint index, jbyte *alias) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint return_code = LLCRYPTO_ERROR;
	VEEPORT_LOGGER_LOG_DBG("index %d, alias %s", index,
	                       ((NULL != alias) ? (char *)alias : "none"));

	jint alias_len = microej_sni_get_array_length(alias);
	if (alias_len <= 0) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
	} else if ((index >= 0) && (index < (jint)ARRAY_SIZE(g_keystore_ctx))) {
		(void)strncpy((char *)alias, (const char *)g_keystore_ctx[index].alias, alias_len);
		if ('\0' == alias[alias_len - 1]) {
			return_code = LLCRYPTO_SUCCESS;
		} else {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG("Invalid Keystore index");
	}

	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(index);
	LLCRYPTO_UNUSED_PARAM(alias);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jboolean LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_containsAlias(jbyte *alias) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	VEEPORT_LOGGER_LOG_DBG("alias %s", ((NULL != alias) ? (char *)alias : "none"));
	jboolean alias_is_present = false;

	LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx key_info;
	key_info.psa_id = PSA_KEY_ID_NULL;
	jint ret = llcrypto_keystore_key_info_from_alias(alias, &key_info);
	if (LLCRYPTO_SUCCESS == ret) {
		/* Test if KMU slot contains a key */
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		psa_status_t status = psa_get_key_attributes(key_info.psa_id, &attributes);
		psa_reset_key_attributes(&attributes);
		if (PSA_SUCCESS == status) {
			alias_is_present = true;
		}
	}

	VEEPORT_LOGGER_LOG_DBG("return_code %s", ((alias_is_present) ? "true" : "false"));
	return alias_is_present;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(alias);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_deleteEntry(jbyte *alias) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	VEEPORT_LOGGER_LOG_DBG("alias %s", ((NULL != alias) ? (char *)alias : "none"));
	jint return_code = LLCRYPTO_SUCCESS;

	LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx key_info;
	key_info.psa_id = PSA_KEY_ID_NULL;
	jint ret = llcrypto_keystore_key_info_from_alias(alias, &key_info);
	if (LLCRYPTO_SUCCESS == ret) {
		if (PSA_SUCCESS != psa_destroy_key(key_info.psa_id)) {
			return_code = LLCRYPTO_ERROR;
		}
	}

	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(alias);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getEntryType(jbyte *alias) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint entry_type = LLCRYPTO_ERROR;

	LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx key_info;
	jint ret = llcrypto_keystore_key_info_from_alias(alias, &key_info);
	if (LLCRYPTO_SUCCESS == ret) {
		entry_type = key_info.type;
	}

	return entry_type;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(alias);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

// -----------------------------------------------------------------------------
// Key Store Key
// -----------------------------------------------------------------------------

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getKeyId(jbyte *alias, jchar *password) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	LLCRYPTO_UNUSED_PARAM(password);
	jint return_code = LLCRYPTO_ERROR;
	VEEPORT_LOGGER_LOG_DBG("alias %s password %s",
	                       ((NULL != alias) ? (char *)alias : "none"),
	                       (NULL != password) ? "***" : "none");

	LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx key_info;
	key_info.psa_id = PSA_KEY_ID_NULL;
	jint ret = llcrypto_keystore_key_info_from_alias(alias, &key_info);
	if (LLCRYPTO_SUCCESS == ret) {
		// Probe the key slot to confirm it is provisioned and readable before handing the
		// handle to Java. Only the status is needed; the attributes themselves are not consumed here.
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		psa_status_t status = psa_get_key_attributes(key_info.psa_id, &attributes);
		psa_reset_key_attributes(&attributes);
		if (PSA_SUCCESS == status) {
			struct llcrypto_key *key = llcrypto_key_from_psa_id(key_info.psa_id);
			if (NULL != key) {
				return_code = (jint)(uintptr_t)key;
			} else {
				/* Key-object allocation failed (heap exhaustion): keep return_code = LLCRYPTO_ERROR so the
				 * Java caller maps it to null, rather than handing back the NULL handle 0. */
				VEEPORT_LOGGER_LOG_DBG("llcrypto_key_from_psa_id() allocation failed");
			}
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_get_key_attributes() failed %d", status);
		}
	}
	VEEPORT_LOGGER_LOG_DBG("psa_id %u", key_info.psa_id);
	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(alias);
	LLCRYPTO_UNUSED_PARAM(password);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getKeyType(jint key_id) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint return_code;
	struct llcrypto_key *key = (struct llcrypto_key *)(uintptr_t)key_id;
	VEEPORT_LOGGER_LOG_DBG("key_id %d", key_id);

	if (NULL == key) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_KEY_HANDLE);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		/* llcrypto_key_get_type() checks the key backend before reading key->psa_id: only PSA-backed
		 * keys are supported, any other backend fails with LLCRYPTO_ERROR. */
		return_code = (jint)llcrypto_key_get_type(key);
		if (LLCRYPTO_ERROR == return_code) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR,
			                                               "Unsupported key backend or key attributes not readable");
			return_code = SNI_IGNORED_RETURNED_VALUE;
		}
	}

	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(key_id);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

void LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getKeyAlgorithm(jint key_id, jbyte *algorithm) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	struct llcrypto_key *key = (struct llcrypto_key *)(uintptr_t)key_id;
	VEEPORT_LOGGER_LOG_DBG("key_id %d", key_id);

	jint algo_len = microej_sni_get_array_length(algorithm);
	if (NULL == key) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_KEY_HANDLE);
	} else if (algo_len <= 0) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
	} else if (LLCRYPTO_SUCCESS == llcrypto_key_get_algorithm_name(key, (char *)algorithm, (size_t)algo_len)) {
		/* llcrypto_key_get_algorithm_name() checks the key backend before reading key->psa_id (only
		 * PSA-backed keys are supported) and writes a null-terminated name, truncated to the buffer
		 * size if needed. */
		VEEPORT_LOGGER_LOG_DBG("algorithm %s", algorithm);
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Unsupported key backend or psa_key_type_t");
	}
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(key_id);
	LLCRYPTO_UNUSED_PARAM(algorithm);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_openKeyEntryWithData(jbyte *alias, jbyte *key, jint chain_len) {
	LLCRYPTO_UNUSED_PARAM(alias);
	LLCRYPTO_UNUSED_PARAM(key);
	LLCRYPTO_UNUSED_PARAM(chain_len);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_openKeyEntryWithNativeId(jbyte *alias, jint key_id, jchar *password,
                                                              jint chain_len) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint return_code = LLCRYPTO_ERROR;
	psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
	struct llcrypto_key *key = (struct llcrypto_key *)(uintptr_t)key_id;
	VEEPORT_LOGGER_LOG_DBG("alias %s chain_len %d",
	                       ((NULL != alias) ? (const char *)alias : "none"), (int)chain_len);
	LLCRYPTO_UNUSED_PARAM(password); // Not used by KMU in NRF54L15-DK

	if (NULL == key) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_KEY_HANDLE);
	} else if (0 == chain_len) {
		LLCRYPTO_EXTERNAL_KEYSTORE_key_ctx key_info;
		key_info.psa_id = PSA_KEY_ID_NULL;
		jint ret = llcrypto_keystore_key_info_from_alias(alias, &key_info);
		if (LLCRYPTO_SUCCESS == ret) {
			/**
			 * Remove previous key from KMU if present
			 */
			psa_status_t status = psa_destroy_key(key_info.psa_id);
			if (PSA_ERROR_INVALID_HANDLE == status) {
				VEEPORT_LOGGER_LOG_DBG("No key found in KMU with alias %s",
				                       (const char *)alias);
			} else if (PSA_SUCCESS != status) {
				VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR,
				                                               "Failed to delete previous key from KMU");
				goto end;
			}

			/**
			 * At this point we cannot know the usage of the key (encrypt/decrypt?).
			 * So we will just set the EXPORT usage
			 */
			psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_EXPORT);
			/**
			 * psa_import_key requires an algorithm to be specified. We set PSA_ALG_GCM arbitrarily,
			 * but the key is going to be derived/copied/exported as needed before getting used.
			 */
			psa_set_key_algorithm(&attrs, PSA_ALG_GCM);
			psa_set_key_lifetime(&attrs,
			                     PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT,
			                                                                    PSA_KEY_LOCATION_CRACEN_KMU));
			psa_set_key_id(&attrs, key_info.psa_id);

			switch (key->backend) {
			case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
			{
				VEEPORT_LOGGER_LOG_DBG("Source key is secret key");

				psa_set_key_type(&attrs, key->secret_key.type);
				psa_set_key_bits(&attrs, key->secret_key.length * 8);

				/**
				 * Copy key into KMU
				 */
				psa_key_handle_t key_handle;
				status = psa_import_key(&attrs, key->secret_key.buffer, key->secret_key.length, &key_handle);

				(void)key_handle; /* persistent KMU key: id already known via psa_set_key_id */

				if (PSA_SUCCESS != status) {
					VEEPORT_LOGGER_LOG_DBG("psa_import_key() error with status %d",
					                       status);
					VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_KMU_COPY_FAILED);
					goto end;
				}
				break;
			}
			case LLCRYPTO_KEY_BACKEND_PSA:
			{
				VEEPORT_LOGGER_LOG_DBG("Source key is PSA key ");
				/**
				 * Copy key into KMU
				 */
				status = psa_copy_key(key->psa_id, &attrs, &key_info.psa_id);
				if (PSA_SUCCESS != status) {
					VEEPORT_LOGGER_LOG_DBG("psa_copy_key() error with status %d",
					                       status);
					VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_KMU_COPY_FAILED);
					goto end;
				}
				break;
			}
			default:
			{
				VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Invalid key type");
				goto end;
			}
			}

			struct llcrypto_key *new_key = llcrypto_key_from_psa_id(key_info.psa_id);
			if (NULL != new_key) {
				return_code = (jint)(uintptr_t)new_key;
			} else {
				/* Key-object allocation failed (heap exhaustion): keep return_code = LLCRYPTO_ERROR rather
				 * than handing back the NULL handle 0. */
				VEEPORT_LOGGER_LOG_DBG("llcrypto_key_from_psa_id() allocation failed");
			}
		}
	} else {
		// A certificate chain can be added in the KeyStore in the same time when chain_len is not equal to 0
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);
	}

end:
	psa_reset_key_attributes(&attrs);
	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(alias);
	LLCRYPTO_UNUSED_PARAM(key_id);
	LLCRYPTO_UNUSED_PARAM(password);
	LLCRYPTO_UNUSED_PARAM(chain_len);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_closeKeyEntry(jint handle) {
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	jint return_code = LLCRYPTO_SUCCESS;
	struct llcrypto_key *key = (struct llcrypto_key *)(uintptr_t)handle;

	VEEPORT_LOGGER_LOG_DBG("handle=%p", (void *)(intptr_t)handle);

	if (NULL == key) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_KEY_HANDLE);
		return_code = LLCRYPTO_ERROR;
	} else {
		/* The handle was registered with SNI by llcrypto_key_from_psa_id() when the entry was opened.
		 * Managed Code closes every handle it opened, so release it here: without this the key object
		 * stays in the SNI resource table forever, since no Java object references it. */
		llcrypto_unregister_and_close(key, (SNI_closeFunction)llcrypto_key_free);
	}

	return return_code;
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(handle);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_CRACEN_KMU);
	return SNI_IGNORED_RETURNED_VALUE;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
}

// -----------------------------------------------------------------------------
// Key Store Certificate
// -----------------------------------------------------------------------------

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateHandle(jbyte *alias) {
	LLCRYPTO_UNUSED_PARAM(alias);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateSize(jint handle) {
	LLCRYPTO_UNUSED_PARAM(handle);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

void LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateData(jint handle, jbyte *buffer) {
	LLCRYPTO_UNUSED_PARAM(handle);
	LLCRYPTO_UNUSED_PARAM(buffer);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_setCertificateEntry(jbyte *alias, jbyte *certificate) {
	LLCRYPTO_UNUSED_PARAM(alias);
	LLCRYPTO_UNUSED_PARAM(certificate);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_insertCertificateInChain(jint handle, jint index, jbyte *certificate) {
	LLCRYPTO_UNUSED_PARAM(handle);
	LLCRYPTO_UNUSED_PARAM(index);
	LLCRYPTO_UNUSED_PARAM(certificate);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

// -----------------------------------------------------------------------------
// Key Store Certificate Chain
// -----------------------------------------------------------------------------

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateChainHandle(jbyte *alias) {
	LLCRYPTO_UNUSED_PARAM(alias);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateChainLength(jint handle) {
	LLCRYPTO_UNUSED_PARAM(handle);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_getCertificateFromChain(jint handle, jint index) {
	LLCRYPTO_UNUSED_PARAM(handle);
	LLCRYPTO_UNUSED_PARAM(index);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}

jint LLCRYPTO_EXTERNAL_KEYSTORE_IMPL_closeCertificateChainHandle(jint handle) {
	LLCRYPTO_UNUSED_PARAM(handle);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);

	return SNI_IGNORED_RETURNED_VALUE;
}
