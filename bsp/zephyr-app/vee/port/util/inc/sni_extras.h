/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef SNI_EXTRAS_H
#define SNI_EXTRAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sni.h>

/*
 * Read the length of an SNI Java array.
 *
 * SNI_getArrayLength (from sni.h) reads the length word stored just before the
 * array data, so it casts the array pointer to a different object type
 * (MISRA 11.3) and offsets it (18.4). Both are inherent to the MicroEJ SNI ABI
 * and cannot be avoided at the call site. Wrapping the macro in this one
 * function isolates the deviation to a single place for the whole VEE Port,
 * instead of repeating it at every call site across every module.
 */
static inline int32_t microej_sni_get_array_length(void *array) {
	/* cppcheck-suppress [misra-c2012-11.3, misra-c2012-11.5, misra-c2012-18.4] : the SNI array-length macro
	 * reads the length word before the array data; the void*-to-object cast (11.5), the object-to-object cast
	 * (11.3) and the offset (18.4) are all inherent to the MicroEJ SNI ABI */
	return SNI_getArrayLength(array);
}

#ifdef __cplusplus
}
#endif

#endif /* SNI_EXTRAS_H */
