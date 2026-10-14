/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <LLCRYPTO_CIPHER_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "sni_extras.h"
#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_key.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/**
 * Cipher context, one per cipher instance of the caller.
 */
typedef struct {
	psa_key_id_t key_id;
	llcrypto_key_close_with_psa key_close; // releases key_id
	psa_algorithm_t algo;
	uint8_t is_decrypting; // direction, fixed at init
	int32_t iv_length;
	uint8_t *iv;
	int32_t aad_length;
	uint8_t *aad; // AEAD additional data, accumulated by update_AAD
	uint32_t tag_length; // AEAD tag length in bytes
	psa_cipher_operation_t operation; // multi-part cipher operation (CBC/ECB/CTR)
	psa_aead_operation_t aead_operation; // multi-part AEAD operation (GCM)
	bool operation_active; // an operation is started and not yet finished/aborted
	int32_t buffered_length; // input bytes PSA holds back (fed but not output yet), e.g. a partial block
} LLCRYPTO_CIPHER_ctx;

/**
 * Cipher crypt function type. Only the crypt implementation varies between transformations
 * (multi-part PSA cipher vs one-shot AEAD); the direction comes from the context, and init/close
 * are shared by every transformation, so they are called directly rather than dispatched per-row.
 * do_final tells whether this chunk is the last one of the message.
 */
typedef int32_t (*LLCRYPTO_CIPHER_crypt)(LLCRYPTO_CIPHER_ctx *cipher_ctx, const uint8_t *buffer, size_t buffer_length,
                                         uint8_t *output, size_t *output_length, uint8_t do_final);

typedef struct {
	const char *name; // the name of the transformation
	psa_algorithm_t algo;
	LLCRYPTO_CIPHER_crypt crypt;
	LLCRYPTO_CIPHER_transformation_desc description;
} LLCRYPTO_CIPHER_transformation;

// AES block size, fixed by the algorithm (FIPS 197) and identical for every mode of operation.
#define AES_BLOCK_BITS    (128u)

// AEAD (CCM/GCM) authentication-tag length bounds, in bytes: the min and max of the valid tag sizes
// documented at LLCRYPTO_CIPHER_IMPL_init. Signed to match the int32_t tag_length compared against them.
#define AEAD_TAG_MIN_BYTES    (4)
#define AEAD_TAG_MAX_BYTES    (16)

#if defined(PSA_WANT_ALG_CBC_NO_PADDING)
#define AES_CBC_BLOCK_BITS    AES_BLOCK_BITS
#define AES_CBC_BLOCK_BYTES   (AES_CBC_BLOCK_BITS / LLCRYPTO_BITS_PER_BYTE)
#endif
#if defined(PSA_WANT_KEY_TYPE_DES)
// DES/3DES block size, fixed by the algorithm (64 bits) and identical for single-/triple-DES and every
// mode of operation.
#define DES_BLOCK_BITS    (64u)
#define DES_BLOCK_BYTES   (DES_BLOCK_BITS / LLCRYPTO_BITS_PER_BYTE)
#endif
#if defined(PSA_WANT_ALG_ECB_NO_PADDING)
#define AES_ECB_BLOCK_BITS    AES_BLOCK_BITS
#define AES_ECB_BLOCK_BYTES   (AES_ECB_BLOCK_BITS / LLCRYPTO_BITS_PER_BYTE)
#endif
#if defined(PSA_WANT_ALG_CTR)
#define AES_CTR_BLOCK_BITS    AES_BLOCK_BITS
#define AES_CTR_BLOCK_BYTES   (AES_CTR_BLOCK_BITS / LLCRYPTO_BITS_PER_BYTE) // block_size must match IV length (16 bytes
                                                                            // for AES)
#define AES_CTR_UNIT_BYTES    (1u) // CTR is a stream cipher mode, byte-granular
#endif
#if (defined(PSA_WANT_ALG_GCM) || defined(_NOT_IMPLEMENTED_PSA_WANT_ALG_CCM))
#define AEAD_BLOCK_BYTES   (1u) // GCM/CCM is byte-granular
#endif //PSA_WANT_ALG_GCM || _NOT_IMPLEMENTED_PSA_WANT_ALG_CCM

/**
 * Return the PSA algorithm actually used by an AEAD context.
 */
static psa_algorithm_t llcrypto_psa_aead_effective_algo(psa_algorithm_t algo, uint32_t tag_length) {
	psa_algorithm_t effective = algo;
	if (PSA_AEAD_TAG_MAX_SIZE != tag_length) {
		/* cppcheck-suppress misra-c2012-12.1 ; precedence is inside vendored macro PSA_ALG_AEAD_WITH_SHORTENED_TAG */
		effective = PSA_ALG_AEAD_WITH_SHORTENED_TAG(algo, tag_length);
	}
	return effective;
}

static int32_t llcrypto_psa_cipher_init(LLCRYPTO_CIPHER_ctx *cipher_ctx, uint8_t is_decrypting,
                                        struct llcrypto_key *key) {
	VEEPORT_LOGGER_LOG_DBG("%d", is_decrypting);
	int32_t return_code;
	psa_key_usage_t usage = 0;
	psa_algorithm_t key_algo = cipher_ctx->algo;

	if (1u == is_decrypting) {
		usage = PSA_KEY_USAGE_DECRYPT;
	} else {
		usage = PSA_KEY_USAGE_ENCRYPT;
	}

	/* For AEAD, the key must be imported with the shortened-tag algorithm so the policy matches the
	 * actual operation algorithm. */
	if (PSA_ALG_IS_AEAD(cipher_ctx->algo)) {
		key_algo = llcrypto_psa_aead_effective_algo(cipher_ctx->algo, cipher_ctx->tag_length);
	}

	return_code = llcrypto_key_get_psa_id(key, usage, key_algo, &cipher_ctx->key_id, &cipher_ctx->key_close);

	return return_code;
}

/**
 * Ends the multi-part PSA operation of the context (cipher or AEAD), if any, and drops the bytes
 * PSA still holds. psa_cipher_abort / psa_aead_abort are safe on an inactive operation per PSA
 * Crypto API §10.3.3 / §10.4.13, so this may be called whatever the current state is.
 */
static void llcrypto_psa_cipher_stop(LLCRYPTO_CIPHER_ctx *cipher_ctx) {
	psa_status_t status = psa_cipher_abort(&cipher_ctx->operation);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_cipher_abort failed! (Error: %d)", status);
	}
	status = psa_aead_abort(&cipher_ctx->aead_operation);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_aead_abort failed! (Error: %d)", status);
	}
	cipher_ctx->operation_active = false;
	cipher_ctx->buffered_length = 0;
}

#if (defined(PSA_WANT_ALG_CBC_NO_PADDING) || defined(PSA_WANT_ALG_ECB_NO_PADDING) || defined(PSA_WANT_ALG_CTR))
/**
 * Starts the multi-part PSA cipher operation of the context in its direction, with the current IV.
 */
static int32_t llcrypto_psa_cipher_start(LLCRYPTO_CIPHER_ctx *cipher_ctx) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t return_code = LLCRYPTO_SUCCESS;
	psa_status_t status;

	if (1u == cipher_ctx->is_decrypting) {
		status = psa_cipher_decrypt_setup(&cipher_ctx->operation, cipher_ctx->key_id, cipher_ctx->algo);
	} else {
		status = psa_cipher_encrypt_setup(&cipher_ctx->operation, cipher_ctx->key_id, cipher_ctx->algo);
	}
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_cipher_setup failed! (Error: %d)", status);
		return_code = llcrypto_sni_error_code(status);
	}

	/* Set the IV (used by CBC and CTR; not used by ECB) */
	if ((LLCRYPTO_SUCCESS == return_code) && (PSA_ALG_ECB_NO_PADDING != cipher_ctx->algo)) {
		status = psa_cipher_set_iv(&cipher_ctx->operation, cipher_ctx->iv, (size_t)cipher_ctx->iv_length);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_cipher_set_iv failed! (Error: %d)", status);
			return_code = llcrypto_sni_error_code(status);
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		cipher_ctx->operation_active = true;
	} else {
		llcrypto_psa_cipher_stop(cipher_ctx);
	}

	return return_code;
}

static int32_t llcrypto_psa_cipher_crypt(LLCRYPTO_CIPHER_ctx *cipher_ctx, const uint8_t *buffer, size_t buffer_length,
                                         uint8_t *output, size_t *output_length, uint8_t do_final) {
	VEEPORT_LOGGER_LOG_DBG("do_final %d", do_final);
	int32_t return_code = LLCRYPTO_SUCCESS;
	const size_t output_buffer_size = *output_length;
	const bool first_chunk = !cipher_ctx->operation_active; // an OOM failure is only retryable on the first chunk
	size_t output_len = 0;
	size_t total_len = 0;
	psa_status_t status;

	/* First chunk of a message: start the operation. It stays open across update calls. */
	if (first_chunk) {
		return_code = llcrypto_psa_cipher_start(cipher_ctx);
	}

	/* Feed this chunk */
	if (LLCRYPTO_SUCCESS == return_code) {
		status = psa_cipher_update(&cipher_ctx->operation, buffer, buffer_length, output, output_buffer_size,
		                           &output_len);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_cipher_update failed! (Error: %d)", status);
			return_code = first_chunk ? llcrypto_sni_error_code(status) : LLCRYPTO_ERROR;
		} else {
			total_len = output_len;
			/* PSA consumes the whole input but may output less (a partial block is held back) or more
			 * (a block held back earlier is released): the difference is the change in held-back bytes.
			 * Both operands come from int32_t-bounded array lengths, so it fits. */
			cipher_ctx->buffered_length += (int32_t)buffer_length - (int32_t)output_len;
			VEEPORT_LOGGER_LOG_DBG("psa_cipher_update success (size = %d, buffered = %d)", (int32_t)total_len,
			                       cipher_ctx->buffered_length);
		}
	}

	/* Last chunk: finalize the operation */
	if ((LLCRYPTO_SUCCESS == return_code) && (1u == do_final)) {
		status = psa_cipher_finish(&cipher_ctx->operation, &output[total_len], (output_buffer_size - total_len),
		                           &output_len);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_cipher_finish failed! (Error: %d)", status);
			return_code = first_chunk ? llcrypto_sni_error_code(status) : LLCRYPTO_ERROR;
		} else {
			total_len += output_len;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		*output_length = total_len;
	}

	/* The operation ends with the message or a failed operation cannot be resumed (PSA leaves it in
	 * an error state): release it so the next call starts a fresh one from the stored IV. */
	if ((LLCRYPTO_SUCCESS != return_code) || (1u == do_final)) {
		llcrypto_psa_cipher_stop(cipher_ctx);
	}

	return return_code;
}

#endif //PSA_WANT_ALG_CBC_NO_PADDING || PSA_WANT_ALG_ECB_NO_PADDING || PSA_WANT_ALG_CTR

#if (defined(PSA_WANT_ALG_GCM) || defined(_NOT_IMPLEMENTED_PSA_WANT_ALG_CCM))
/**
 * Starts the multi-part PSA AEAD operation of the context: direction, nonce and the whole AAD
 * (the caller must provide the whole AAD before the first chunk).
 * CCM would additionally need psa_aead_set_lengths() before the nonce, i.e. the total plaintext
 * length up front, which streaming cannot provide; only GCM is registered.
 */
static int32_t llcrypto_psa_aead_start(LLCRYPTO_CIPHER_ctx *cipher_ctx, psa_algorithm_t algo) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t return_code = LLCRYPTO_SUCCESS;
	psa_status_t status;

	if (1u == cipher_ctx->is_decrypting) {
		status = psa_aead_decrypt_setup(&cipher_ctx->aead_operation, cipher_ctx->key_id, algo);
	} else {
		status = psa_aead_encrypt_setup(&cipher_ctx->aead_operation, cipher_ctx->key_id, algo);
	}
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_aead_setup failed! (Error: %d)", status);
		return_code = llcrypto_sni_error_code(status);
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		status = psa_aead_set_nonce(&cipher_ctx->aead_operation, cipher_ctx->iv, (size_t)cipher_ctx->iv_length);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_aead_set_nonce failed! (Error: %d)", status);
			return_code = llcrypto_sni_error_code(status);
		}
	}

	if ((LLCRYPTO_SUCCESS == return_code) && (cipher_ctx->aad_length > 0)) {
		status = psa_aead_update_ad(&cipher_ctx->aead_operation, cipher_ctx->aad, (size_t)cipher_ctx->aad_length);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_aead_update_ad failed! (Error: %d)", status);
			return_code = llcrypto_sni_error_code(status);
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		cipher_ctx->operation_active = true;
	} else {
		llcrypto_psa_cipher_stop(cipher_ctx);
	}

	return return_code;
}

static int32_t llcrypto_psa_aead_cipher_crypt(LLCRYPTO_CIPHER_ctx *cipher_ctx, const uint8_t *buffer,
                                              size_t buffer_length, uint8_t *output, size_t *output_length,
                                              uint8_t do_final) {
	VEEPORT_LOGGER_LOG_DBG("do_final %d", do_final);
	int32_t return_code = LLCRYPTO_SUCCESS;
	const size_t output_buffer_size = *output_length;
	const size_t tag_length = (size_t)cipher_ctx->tag_length;
	const bool first_chunk = !cipher_ctx->operation_active; // an OOM failure is only retryable on the first chunk
	size_t input_length = buffer_length;
	size_t output_len = 0;
	size_t total_len = 0;
	psa_status_t status;

	/* First chunk of a message: start the operation. It stays open across update calls. */
	if (first_chunk) {
		const psa_algorithm_t algo = llcrypto_psa_aead_effective_algo(cipher_ctx->algo, cipher_ctx->tag_length);
		return_code = llcrypto_psa_aead_start(cipher_ctx, algo);
	}

	/* The input stream carries the tag inline after the ciphertext while PSA handles it out-of-band:
	 * on the final decryption chunk, split it off the input before feeding. */
	if ((LLCRYPTO_SUCCESS == return_code) && (1u == cipher_ctx->is_decrypting) && (1u == do_final)) {
		if (buffer_length < tag_length) {
			VEEPORT_LOGGER_LOG_DBG("final chunk shorter than the tag (%d<%d)",
			                       (int32_t)buffer_length, (int32_t)tag_length);
			return_code = LLCRYPTO_ERROR;
		} else {
			input_length = buffer_length - tag_length;
		}
	}

	/* Feed this chunk. The output goes straight to the caller — on decryption it is NOT authenticated
	 * until the tag is verified below. The caller sizes the output exactly, which is below
	 * the pessimistic PSA_AEAD_UPDATE_OUTPUT_SIZE bound, so the capacity is left to psa_aead_update. */
	if (LLCRYPTO_SUCCESS == return_code) {
		status = psa_aead_update(&cipher_ctx->aead_operation, buffer, input_length, output, output_buffer_size,
		                         &output_len);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_aead_update failed! (Error: %d)", status);
			return_code = first_chunk ? llcrypto_sni_error_code(status) : LLCRYPTO_ERROR;
		} else {
			total_len = output_len;
			cipher_ctx->buffered_length += (int32_t)input_length - (int32_t)output_len;
		}
	}

	/* Last chunk: finalize the operation. Decryption verifies the tag split off above (PSA releases
	 * any block it still holds first); encryption appends the tag PSA produces right after the data. */
	if ((LLCRYPTO_SUCCESS == return_code) && (1u == do_final)) {
		if (1u == cipher_ctx->is_decrypting) {
			status = psa_aead_verify(&cipher_ctx->aead_operation, &output[total_len],
			                         output_buffer_size - total_len, &output_len, &buffer[input_length], tag_length);
			if (PSA_ERROR_INVALID_SIGNATURE == status) {
				VEEPORT_LOGGER_LOG_DBG("authentication failed");
				return_code = LLCRYPTO_ERROR_AEAD_AUTH;
			} else if (PSA_SUCCESS != status) {
				VEEPORT_LOGGER_LOG_DBG("psa_aead_verify failed! (Error: %d)", status);
				return_code = first_chunk ? llcrypto_sni_error_code(status) : LLCRYPTO_ERROR;
			} else {
				total_len += output_len;
			}
		} else if ((output_buffer_size - total_len) < tag_length) {
			const size_t remaining = output_buffer_size - total_len;
			VEEPORT_LOGGER_LOG_DBG("output buffer too small for the tag (%u<%u)",
			                       (unsigned int)remaining, (unsigned int)tag_length);
			return_code = LLCRYPTO_ERROR;
		} else {
			/* psa_aead_finish takes separate ciphertext and tag outputs. GCM holds no ciphertext back,
			 * so the trailing ciphertext lands in a scratch buffer and must stay empty. */
			uint8_t trailer[PSA_AEAD_FINISH_OUTPUT_MAX_SIZE];
			size_t tag_len = 0;
			status = psa_aead_finish(&cipher_ctx->aead_operation, trailer, sizeof(trailer), &output_len,
			                         &output[total_len], tag_length, &tag_len);
			if (PSA_SUCCESS != status) {
				VEEPORT_LOGGER_LOG_DBG("psa_aead_finish failed! (Error: %d)", status);
				return_code = first_chunk ? llcrypto_sni_error_code(status) : LLCRYPTO_ERROR;
			} else if (output_len > 0u) {
				VEEPORT_LOGGER_LOG_DBG("unexpected %d trailing ciphertext bytes",
				                       (int32_t)output_len);
				return_code = LLCRYPTO_ERROR;
			} else {
				total_len += tag_len;
			}
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		*output_length = total_len;
	}

	/* The operation ends with the message, and a failed operation cannot be resumed (PSA leaves it
	 * in an error state): release it so the next call starts a fresh one from the stored IV. */
	if ((LLCRYPTO_SUCCESS != return_code) || (1u == do_final)) {
		llcrypto_psa_cipher_stop(cipher_ctx);
	}

	return return_code;
}

#endif //PSA_WANT_ALG_GCM || _NOT_IMPLEMENTED_PSA_WANT_ALG_CCM

/**
 * Shared body of the encrypt / decrypt natives: checks the direction the context was set up for and
 * the array bounds, runs the transformation and converts the outcome for the caller. On failure a
 * NativeException is thrown — with the OOM error code when memory ran out.
 */
static int32_t llcrypto_cipher_crypt(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                     int32_t buffer_offset, int32_t buffer_length, uint8_t *output,
                                     int32_t output_offset, uint8_t do_final, uint8_t is_decrypting,
                                     const char *failure_message) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	int32_t buffer_array_length = microej_sni_get_array_length(buffer);
	int32_t output_array_length = microej_sni_get_array_length(output);
	const LLCRYPTO_CIPHER_transformation *transformation = (const LLCRYPTO_CIPHER_transformation *)transformation_id;
	LLCRYPTO_CIPHER_ctx *cipher_ctx = (LLCRYPTO_CIPHER_ctx *)native_id;

	if (cipher_ctx->is_decrypting != is_decrypting) {
		/* the PSA operation direction is fixed at setup */
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_WRONG_DIRECTION);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else if (!llcrypto_bounds_ok(buffer_array_length, buffer_offset, buffer_length) ||
	           !llcrypto_offset_ok(output_array_length, output_offset)) {
		VEEPORT_LOGGER_LOG_DBG("invalid offsets/lengths: buffer_offset %d buffer_length %d (array %d), "
		                       "output_offset %d (array %d)", buffer_offset, buffer_length,
		                       buffer_array_length, output_offset, output_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		/* output_array_length >= output_offset >= 0 guaranteed by the guard. Operands can be casted to size_t */
		size_t output_bytes = (size_t)output_array_length - (size_t)output_offset;
		int32_t rc = transformation->crypt(cipher_ctx, &buffer[buffer_offset], (size_t)buffer_length,
		                                   &output[output_offset], &output_bytes, do_final);
		switch (rc) {
		case LLCRYPTO_SUCCESS:
		{
			LLCRYPTO_ASSERT(output_bytes <= (size_t)INT32_MAX);
			return_code = (int32_t)output_bytes;
			break;
		}
		case LLCRYPTO_ERROR_AEAD_AUTH:
		{
			/* not an error for the caller: the return value reports the authentication failure */
			return_code = LLCRYPTO_ERROR_AEAD_AUTH;
			break;
		}
		case LLCRYPTO_ERROR_OOM:
		{
			VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
			return_code = LLCRYPTO_ERROR_OOM;
			break;
		}
		default:
		{
			VEEPORT_LOGGER_LOG_DBG("failed (rc %d)", rc);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "%s", failure_message);
			return_code = LLCRYPTO_ERROR;
			break;
		}
		}
	}
	return return_code;
}

static void llcrypto_cipher_close(void *ctx) {
	VEEPORT_LOGGER_LOG_DBG("native_id %p", ctx);
	LLCRYPTO_CIPHER_ctx *cipher_ctx = ctx;

	if (NULL != cipher_ctx) {
		llcrypto_psa_cipher_stop(cipher_ctx);
		free(cipher_ctx->iv);
		free(cipher_ctx->aad);
		if (NULL != cipher_ctx->key_close) {
			cipher_ctx->key_close(cipher_ctx->key_id);
		}
		free(cipher_ctx);
	}
}

int32_t LLCRYPTO_CIPHER_IMPL_get_transformation_description(uint8_t *transformation_name,
                                                            LLCRYPTO_CIPHER_transformation_desc *transformation_desc) {
	static const LLCRYPTO_CIPHER_transformation available_transformations[] = {
#if defined(PSA_WANT_ALG_CBC_NO_PADDING)
		{
			.name = "AES/CBC/NoPadding",
			.algo = PSA_ALG_CBC_NO_PADDING,
			.crypt = llcrypto_psa_cipher_crypt,
			.description = {
				.block_size = AES_CBC_BLOCK_BYTES,
				.unit_bytes = AES_CBC_BLOCK_BYTES,
				.cipher_mode = CBC_MODE,
			}
		},
#endif //PSA_WANT_ALG_CBC_NO_PADDING
#if defined(PSA_WANT_ALG_CBC_NO_PADDING) && defined(PSA_WANT_KEY_TYPE_DES)
		{
			.name = "DESede/CBC/NoPadding",
			.algo = PSA_ALG_CBC_NO_PADDING,
			.crypt = llcrypto_psa_cipher_crypt,
			.description = {
				.block_size = DES_BLOCK_BYTES,
				.unit_bytes = DES_BLOCK_BYTES,
				.cipher_mode = CBC_MODE,
			}
		},
#endif //PSA_WANT_ALG_CBC_NO_PADDING && PSA_WANT_KEY_TYPE_DES
#if defined(PSA_WANT_ALG_ECB_NO_PADDING)
		{
			.name = "AES/ECB/NoPadding",
			.algo = PSA_ALG_ECB_NO_PADDING,
			.crypt = llcrypto_psa_cipher_crypt,
			.description = {
				.block_size = AES_ECB_BLOCK_BYTES,
				.unit_bytes = AES_ECB_BLOCK_BYTES,
				.cipher_mode = ECB_MODE,
			}
		},
#endif //PSA_WANT_ALG_ECB_NO_PADDING
#if defined(PSA_WANT_ALG_GCM)
		{
			.name = "AES/GCM/NoPadding",
			.algo = PSA_ALG_GCM,
			.crypt = llcrypto_psa_aead_cipher_crypt,
			.description = {
				.block_size = AEAD_BLOCK_BYTES,
				.unit_bytes = AEAD_BLOCK_BYTES,
				.cipher_mode = GCM_MODE,
			}
		},
#endif //PSA_WANT_ALG_GCM
#if defined(PSA_WANT_ALG_CTR)
		{
			.name = "AES/CTR/NoPadding",
			.algo = PSA_ALG_CTR,
			.crypt = llcrypto_psa_cipher_crypt,
			.description = {
				.block_size = AES_CTR_BLOCK_BYTES,
				.unit_bytes = AES_CTR_UNIT_BYTES,
				.cipher_mode = CTR_MODE,
			}
		},
#endif //PSA_WANT_ALG_CTR
/* AES/CCM is disabled: not implemented on the Java side yet. The C entry is kept but gated on a
 * never-defined sentinel so it is always compiled out (the other modes gate on their real
 * PSA_WANT_ALG_* flag). It will be enabled — gating on PSA_WANT_ALG_CCM — once a newer crypto
 * library version adds CCM support. */
#if defined(_NOT_IMPLEMENTED_PSA_WANT_ALG_CCM)
		{
			.name = "AES/CCM/NoPadding",
			.algo = PSA_ALG_CCM,
			.crypt = llcrypto_psa_aead_cipher_crypt,
			.description = {
				.block_size = AEAD_BLOCK_BYTES,
				.unit_bytes = AEAD_BLOCK_BYTES,
				.cipher_mode = CCM_MODE,
			}
		}
#endif //_NOT_IMPLEMENTED_PSA_WANT_ALG_CCM
	};
	VEEPORT_LOGGER_LOG_DBG("transformation_name %s", transformation_name);
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_ERROR;
	/* Reject a transformation_desc array too small to hold the description before the memcpy writes into it. */
	if (microej_sni_get_array_length(transformation_desc) < (int32_t)sizeof(LLCRYPTO_CIPHER_transformation_desc)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		int32_t nb_transformations = sizeof(available_transformations) / sizeof(LLCRYPTO_CIPHER_transformation);
		const LLCRYPTO_CIPHER_transformation *transformation = &available_transformations[0];

		while (--nb_transformations >= 0) {
			if (strcmp((const char *)transformation_name, transformation->name) == 0) {
				(void)memcpy(transformation_desc, &(transformation->description),
				             sizeof(LLCRYPTO_CIPHER_transformation_desc));
				break;
			}
			transformation++;
		}

		if (0 <= nb_transformations) {
			return_code = (intptr_t)transformation;
		}
	}
	LLCRYPTO_PROFILE_END();
	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
}

int32_t LLCRYPTO_CIPHER_IMPL_get_buffered_length(int32_t nativeTransformationId, int32_t nativeId) {
	LLCRYPTO_UNUSED_PARAM(nativeTransformationId);
	const LLCRYPTO_CIPHER_ctx *cipher_ctx = (const LLCRYPTO_CIPHER_ctx *)nativeId;
	return cipher_ctx->buffered_length;
}

void LLCRYPTO_CIPHER_IMPL_get_IV(int32_t transformation_id, int32_t native_id, uint8_t *iv, int32_t iv_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	LLCRYPTO_PROFILE_START();
	const LLCRYPTO_CIPHER_ctx *cipher_ctx = (const LLCRYPTO_CIPHER_ctx *)native_id;
	/* Defense-in-depth: bound iv_length against the caller's iv array (not just the stored IV length)
	 * before memcpy writes into it, to reject an undersized destination array. */
	if ((iv_length < 0) || (iv_length > cipher_ctx->iv_length) || (iv_length > microej_sni_get_array_length(iv))) {
		VEEPORT_LOGGER_LOG_DBG("invalid iv_length %d (stored %d)", iv_length,
		                       cipher_ctx->iv_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		(void)memcpy(iv, cipher_ctx->iv, iv_length);
	}
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_CIPHER_IMPL_set_IV(int32_t transformation_id, int32_t native_id, uint8_t *iv, int32_t iv_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_CIPHER_ctx *cipher_ctx = (LLCRYPTO_CIPHER_ctx *)native_id;
	/* Defense-in-depth: bound iv_length against the caller's iv array (not just the stored IV length)
	 * before memcpy reads from it, to reject an undersized source array. */
	if ((iv_length < 0) || (iv_length > cipher_ctx->iv_length) || (iv_length > microej_sni_get_array_length(iv))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		(void)memcpy(cipher_ctx->iv, iv, iv_length);
		/* A new IV starts a new message: drop any operation in progress and held-back input so the
		 * next crypt call restarts from this IV (the caller resets the IV after every final call). */
		llcrypto_psa_cipher_stop(cipher_ctx);
	}
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_CIPHER_IMPL_get_IV_length(int32_t transformation_id, int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	const LLCRYPTO_CIPHER_ctx *cipher_ctx = (const LLCRYPTO_CIPHER_ctx *)native_id;
	return cipher_ctx->iv_length;
}

void LLCRYPTO_CIPHER_IMPL_update_AAD(int32_t transformation_id, int32_t native_id, uint8_t *aad, int32_t offset,
                                     int32_t aad_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_CIPHER_ctx *cipher_ctx = (LLCRYPTO_CIPHER_ctx *)native_id;
	int32_t aad_array_length = microej_sni_get_array_length(aad);
	if (!llcrypto_bounds_ok(aad_array_length, offset, aad_length) ||
	    (aad_length > (INT32_MAX - cipher_ctx->aad_length))) {
		VEEPORT_LOGGER_LOG_DBG("invalid AAD offset %d / length %d (array %d, buffered %d)", offset,
		                       aad_length, aad_array_length, cipher_ctx->aad_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
	} else {
		int32_t new_total = cipher_ctx->aad_length + aad_length;
		uint8_t *new_aad = realloc(cipher_ctx->aad, new_total);
		if (NULL != new_aad) {
			(void)memcpy(&new_aad[cipher_ctx->aad_length], &aad[offset], aad_length);
			cipher_ctx->aad = new_aad;
			cipher_ctx->aad_length = new_total;
		} else {
			VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
		}
	}
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_CIPHER_IMPL_init(int32_t transformation_id, uint8_t is_decrypting, int32_t secret_key_id, uint8_t *iv,
                                  int32_t iv_length, int32_t tag_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_SUCCESS;
	const LLCRYPTO_CIPHER_transformation *transformation = (const LLCRYPTO_CIPHER_transformation *)transformation_id;
	LLCRYPTO_CIPHER_ctx *cipher_ctx = NULL;
	struct llcrypto_key *key = (struct llcrypto_key *)secret_key_id;

	/* tag_length is meaningful only when the algorithm supports AEAD (CCM, GCM).
	 * Valid tag_length for:
	 *   * CCM: 4, 6, 8, 10, 12, 14, and 16 bytes
	 *   * GCM: 4, 8, 12, 13, 14, 15, and 16 bytes */
	if (PSA_ALG_IS_AEAD(transformation->algo)
	    && ((tag_length < AEAD_TAG_MIN_BYTES) || (tag_length > AEAD_TAG_MAX_BYTES))) {
		VEEPORT_LOGGER_LOG_DBG("invalid tag_length %d", tag_length);
		return_code = LLCRYPTO_ERROR_INVALID_PARAMETER;
	}

	if ((PSA_ALG_ECB_NO_PADDING != transformation->algo) &&
	    ((iv_length <= 0) || (iv_length > microej_sni_get_array_length(iv)))) {
		VEEPORT_LOGGER_LOG_DBG("invalid iv length %d", iv_length);
		return_code = LLCRYPTO_ERROR_INVALID_PARAMETER;
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		cipher_ctx = calloc(1, sizeof(LLCRYPTO_CIPHER_ctx));
		if (NULL == cipher_ctx) {
			VEEPORT_LOGGER_LOG_DBG("failed to allocate cipher_ctx");
			return_code = LLCRYPTO_ERROR_OOM;
		} else {
			/* close() aborts these on every failure path below, so they must hold the PSA initializer first */
			cipher_ctx->operation = psa_cipher_operation_init();
			cipher_ctx->aead_operation = psa_aead_operation_init();
		}
	}

	/* Set the IV (used by CBC and CTR; not used by ECB) */
	if ((LLCRYPTO_SUCCESS == return_code) && (PSA_ALG_ECB_NO_PADDING != transformation->algo)) {
		cipher_ctx->iv = calloc(1, iv_length);
		if (NULL == cipher_ctx->iv) {
			VEEPORT_LOGGER_LOG_DBG("failed to allocate iv");
			return_code = LLCRYPTO_ERROR_OOM;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		if (PSA_ALG_ECB_NO_PADDING != transformation->algo) {
			cipher_ctx->iv_length = iv_length;
			(void)memcpy(cipher_ctx->iv, iv, iv_length);
		} else {
			cipher_ctx->iv_length = LLCRYPTO_ERROR;
		}
		cipher_ctx->tag_length = (uint32_t)tag_length;
		cipher_ctx->algo = transformation->algo;
		cipher_ctx->is_decrypting = is_decrypting;

		return_code = llcrypto_psa_cipher_init(cipher_ctx, is_decrypting, key);
		if (LLCRYPTO_ERROR_OOM == return_code) {
			/* No free PSA key slot: returned, not thrown, so that the Crypto library collects the
			 * unreachable ciphers, which frees their slots, and retries. */
			VEEPORT_LOGGER_LOG_DBG("init failed: no PSA key slot or memory left");
		} else if (LLCRYPTO_SUCCESS != return_code) {
			VEEPORT_LOGGER_LOG_DBG("init failed: %d", return_code);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INIT_FAILED);
			return_code = LLCRYPTO_ERROR;
		} else {
			/* key imported */
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		if (SNI_OK != SNI_registerResource(cipher_ctx, llcrypto_cipher_close, NULL)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (intptr_t)(cipher_ctx);
	} else {
		/* Single teardown for every failure path: close() is NULL-safe and releases iv, aad,
		 * the PSA key and the context itself. The SNI resource was either never reached or its
		 * registration failed, so there is nothing to unregister here. */
		llcrypto_cipher_close(cipher_ctx);
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_CIPHER_IMPL_decrypt(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                     int32_t buffer_offset,
                                     int32_t buffer_length, uint8_t *output, int32_t output_offset, uint8_t do_final) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = llcrypto_cipher_crypt(transformation_id, native_id, buffer, buffer_offset, buffer_length,
	                                            output, output_offset, do_final, 1u, "decrypt failed");
	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_CIPHER_IMPL_encrypt(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                     int32_t buffer_offset,
                                     int32_t buffer_length, uint8_t *output, int32_t output_offset, uint8_t do_final) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = llcrypto_cipher_crypt(transformation_id, native_id, buffer, buffer_offset, buffer_length,
	                                            output, output_offset, do_final, 0u, "encrypt failed");
	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_CIPHER_IMPL_close(int32_t transformation_id, int32_t native_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id); /* every transformation shares the same close function */
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_CIPHER_ctx *cipher_ctx = (LLCRYPTO_CIPHER_ctx *)native_id;

	llcrypto_unregister_and_close(cipher_ctx, llcrypto_cipher_close);
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_CIPHER_IMPL_get_close_id(int32_t transformation_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id); /* every transformation shares the same close function */
	VEEPORT_LOGGER_LOG_DBG("");
	// cppcheck-suppress [misra-c2012-11.1, misra-c2012-11.6] // Abstract data type for SNI usage
	return (intptr_t)llcrypto_cipher_close;
}
