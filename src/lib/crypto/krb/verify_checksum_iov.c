/*
 * lib/crypto/verify_checksum_iov.c
 *
 * Copyright 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "cksumtypes.h"
#include "aead.h"

krb5_error_code KRB5_CALLCONV
krb5_k_verify_checksum_iov(krb5_context context,
			   krb5_cksumtype checksum_type,
			   krb5_key key,
			   krb5_keyusage usage,
			   const krb5_crypto_iov *data,
			   size_t num_data,
			   krb5_boolean *valid)
{
    unsigned int i;
    const struct krb5_cksumtypes *ctp;
    size_t cksumlen;
    krb5_error_code ret;
    krb5_data computed;
    krb5_crypto_iov *checksum;

    for (i = 0; i < krb5_cksumtypes_length; i++) {
	if (krb5_cksumtypes_list[i].ctype == checksum_type)
	    break;
    }
    if (i == krb5_cksumtypes_length)
	return KRB5_BAD_ENCTYPE;
    ctp = &krb5_cksumtypes_list[i];

    checksum = krb5int_c_locate_iov((krb5_crypto_iov *)data, num_data,
				    KRB5_CRYPTO_TYPE_CHECKSUM);
    if (checksum == NULL)
	return(KRB5_BAD_MSIZE);

    /* If there's actually a verify function, call it. */
    if (ctp->keyhash && ctp->keyhash->verify_iov) {
	return (*ctp->keyhash->verify_iov)(key, usage, 0, data, num_data,
					   &checksum->data, valid);
    }

    /* Otherwise, make the checksum again, and compare. */
    if (ctp->keyhash != NULL)
	computed.length = ctp->keyhash->hashsize;
    else
	computed.length = ctp->hash->hashsize;

    if (ctp->trunc_size != 0)
	cksumlen = ctp->trunc_size;
    else
	cksumlen = computed.length;

    if (checksum->data.length != cksumlen)
	return KRB5_BAD_MSIZE;

    computed.data = malloc(computed.length);
    if (computed.data == NULL)
	return ENOMEM;

    ret = krb5int_c_make_checksum_iov(&krb5_cksumtypes_list[i], key, usage,
				      data, num_data, &computed);
    if (ret) {
	free(computed.data);
	return ret;
    }

    *valid = (computed.length == cksumlen) &&
	     (memcmp(computed.data, checksum->data.data, cksumlen) == 0);

    free(computed.data);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_verify_checksum_iov(krb5_context context,
			   krb5_cksumtype checksum_type,
			   const krb5_keyblock *keyblock,
			   krb5_keyusage usage,
			   const krb5_crypto_iov *data,
			   size_t num_data,
			   krb5_boolean *valid)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
	return ret;
    ret = krb5_k_verify_checksum_iov(context, checksum_type, key, usage, data,
				     num_data, valid);
    krb5_k_free_key(context, key);
    return ret;
}
