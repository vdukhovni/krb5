/*
 * COPYRIGHT (C) 2006,2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "pkinit.h"

#define FAKECERT

krb5_error_code pkinit_init_req_opts(pkinit_req_opts **reqopts)
{
    krb5_error_code retval = ENOMEM;
    pkinit_req_opts *opts = NULL;

    *reqopts = NULL;
    opts = (pkinit_req_opts *) calloc(1, sizeof(pkinit_req_opts));
    if (opts == NULL)
	return retval;

    opts->require_eku = 1;
    opts->require_san = 1;
    opts->allow_upn = 0;
    opts->dh_or_rsa = DH_PROTOCOL;
    opts->require_crl_checking = 0;
    opts->require_hostname_match = 0;
    opts->dh_size = 1024;
    opts->win2k_target = 0;
    opts->win2k_require_cksum = 0;

    *reqopts = opts;

    return 0;
}

void pkinit_fini_req_opts(pkinit_req_opts *opts)
{
    if (opts != NULL)
	free(opts);
    return;
}

krb5_error_code pkinit_init_plg_opts(pkinit_plg_opts **plgopts)
{
    krb5_error_code retval = ENOMEM;
    pkinit_plg_opts *opts = NULL;

    *plgopts = NULL;
    opts = (pkinit_plg_opts *) calloc(1, sizeof(pkinit_plg_opts));
    if (opts == NULL)
	return retval;

    opts->require_eku = 1;
    opts->require_san = 1;
    opts->dh_or_rsa = DH_PROTOCOL;
    opts->allow_upn = 0;
    opts->require_crl_checking = 0;

    opts->princ_in_cert = 0;
    opts->dh_min_bits = 0;
    opts->allow_proxy_certs = 0;

    *plgopts = opts;

    return 0;
}

void pkinit_fini_plg_opts(pkinit_plg_opts *opts)
{
    if (opts != NULL)
	free(opts);
    return;
}

krb5_error_code pkinit_init_identity_opts(pkinit_identity_opts **idopts)
{
    pkinit_identity_opts *opts = NULL;

    *idopts = NULL;
    opts = (pkinit_identity_opts *) calloc(1, sizeof(pkinit_identity_opts));
    if (opts == NULL)
	return ENOMEM;

    opts->identity = NULL;
    opts->anchors = NULL;
    opts->intermediates = NULL;
    opts->crls = NULL;
    opts->ocsp = NULL;
    opts->dn_mapping_file = NULL;	/* XXX should be in "kdc opts" ??? */

    *idopts = opts;

    return 0;
}

static void
free_list(char **list)
{
    int i;

    if (list == NULL)
	return;

    for (i = 0; list[i] != NULL; i++)
	free(list[i]);
     free(list);
}

static krb5_error_code
copy_list(char ***dst, char **src)
{
    int i;
    char **newlist;

    if (dst == NULL)
	return EINVAL;
    *dst = NULL;

    if (src == NULL)
	return 0;

    for (i = 0; src[i] != NULL; i++);

    newlist = calloc(1, (i + 1) * sizeof(*newlist));
    if (newlist == NULL)
	return ENOMEM;

    for (i = 0; src[i] != NULL; i++) {
	newlist[i] = strdup(src[i]);
	if (newlist[i] == NULL)
	    goto cleanup;
    }
    newlist[i] = NULL;
    *dst = newlist;
    return 0;
cleanup:
    free_list(newlist);
    return ENOMEM;
}

krb5_error_code
pkinit_dup_identity_opts(pkinit_identity_opts *src_opts,
			 pkinit_identity_opts **dest_opts)
{
    pkinit_identity_opts *newopts;
    krb5_error_code retval;

    *dest_opts = NULL;
    retval = pkinit_init_identity_opts(&newopts);
    if (retval)
	return retval;

    retval = ENOMEM;

    if (src_opts->identity != NULL) {
	newopts->identity = strdup(src_opts->identity);
	if (newopts->identity == NULL)
	    goto cleanup;
    }

    retval = copy_list(&newopts->anchors, src_opts->anchors);
    if (retval)
	goto cleanup;

    retval = copy_list(&newopts->intermediates,src_opts->intermediates);
    if (retval)
	goto cleanup;

    retval = copy_list(&newopts->crls, src_opts->crls);
    if (retval)
	goto cleanup;

    if (src_opts->ocsp != NULL) {
	newopts->ocsp = strdup(src_opts->ocsp);
	if (newopts->ocsp == NULL)
	    goto cleanup;
    }

    *dest_opts = newopts;
    return 0;
cleanup:
    pkinit_fini_identity_opts(newopts);   
}

void
pkinit_fini_identity_opts(pkinit_identity_opts *idopts)
{
    int i;

    if (idopts == NULL)
	return;

    if (idopts->identity)
	free(idopts->identity);
    free_list(idopts->anchors);
    free_list(idopts->intermediates);
    free_list(idopts->crls);
	
    free(idopts);
}

krb5_error_code
pkinit_initialize_identity(krb5_context context,
			   pkinit_identity_opts *idopts,
			   pkinit_identity_crypto_context id_cryptoctx)
{
    krb5_error_code retval = EINVAL;
    int i;

    pkiDebug("pkinit_initialize_identity %p %p %p\n",
	     context, idopts, id_cryptoctx);
    if (idopts == NULL || id_cryptoctx == NULL)
	goto errout;

    if (idopts->identity == NULL)
	goto errout;

    retval = pkinit_process_identity_option(context,
					    PKINIT_ID_OPT_USER_IDENTITY,
					    idopts->identity, id_cryptoctx);
    if (retval)
	goto errout;

    for (i = 0; idopts->anchors != NULL && idopts->anchors[i] != NULL; i++) {
	retval = pkinit_process_identity_option(context,
						PKINIT_ID_OPT_ANCHOR_CAS,
						idopts->anchors[i],
						id_cryptoctx);
	if (retval)
	    goto errout;
    }
    for (i = 0; idopts->intermediates != NULL
		&& idopts->intermediates[i] != NULL; i++) {
	retval = pkinit_process_identity_option(context,
						PKINIT_ID_OPT_INTERMEDIATE_CAS,
						idopts->intermediates[i],
						id_cryptoctx);
	if (retval)
	    goto errout;
    }
    for (i = 0; idopts->crls != NULL && idopts->crls[i] != NULL; i++) {
	retval = pkinit_process_identity_option(context, PKINIT_ID_OPT_CRLS,
						idopts->crls[i], id_cryptoctx);
	if (retval)
	    goto errout;
    }
    retval = pkinit_process_identity_option(context, PKINIT_ID_OPT_OCSP,
					    idopts->ocsp, id_cryptoctx);
    if (retval)
	goto errout;
errout:
    return retval;
}

void free_krb5_pa_pk_as_req(krb5_pa_pk_as_req **in)
{
    if (*in == NULL) return;
    if ((*in)->signedAuthPack.data != NULL)
	free((*in)->signedAuthPack.data);
    if ((*in)->trustedCertifiers != NULL)
	free_krb5_external_principal_identifier(&(*in)->trustedCertifiers);
    if ((*in)->kdcPkId.data != NULL)
	free((*in)->kdcPkId.data);
    free(*in);
}

void free_krb5_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 **in)
{
    if (*in == NULL) return;
    if ((*in)->signedAuthPack.data != NULL)
	free((*in)->signedAuthPack.data);
    if ((*in)->kdcCert.data != NULL)
	free((*in)->kdcCert.data);
    if ((*in)->encryptionCert.data != NULL)
	free((*in)->encryptionCert.data);
    if ((*in)->trustedCertifiers != NULL)
	free_krb5_trusted_ca(&(*in)->trustedCertifiers);
    free(*in);
}

void free_krb5_reply_key_pack(krb5_reply_key_pack **in)
{
    if (*in == NULL) return;
    if ((*in)->replyKey.contents != NULL)
	free((*in)->replyKey.contents);
    if ((*in)->asChecksum.contents != NULL)
	free((*in)->asChecksum.contents);
    free(*in);
}

void free_krb5_reply_key_pack_draft9(krb5_reply_key_pack_draft9 **in)
{
    if (*in == NULL) return;
    if ((*in)->replyKey.contents != NULL)
	free((*in)->replyKey.contents);
    free(*in);
}

void free_krb5_auth_pack(krb5_auth_pack **in)
{
    if ((*in) == NULL) return;
    if ((*in)->clientPublicValue != NULL) {
    /* not freeing clientPublicValue->algorithm.algorithm.data because
     * client has that as static memory but server allocates it therefore
     * the server will free it outside of this function
     */
	if ((*in)->clientPublicValue->algorithm.parameters.data != NULL)
	    free((*in)->clientPublicValue->algorithm.parameters.data);
	if ((*in)->clientPublicValue->subjectPublicKey.data != NULL)
	    free((*in)->clientPublicValue->subjectPublicKey.data);
	free((*in)->clientPublicValue);
    }
    if ((*in)->pkAuthenticator.paChecksum.contents != NULL)
	free((*in)->pkAuthenticator.paChecksum.contents);
    free(*in);
}

void free_krb5_auth_pack_draft9(krb5_context context,
				krb5_auth_pack_draft9 **in)
{
    if ((*in) == NULL) return;
    krb5_free_principal(context, (*in)->pkAuthenticator.kdcName);
    free(*in);
}

void free_krb5_pa_pk_as_rep(krb5_pa_pk_as_rep **in)
{
    if (*in == NULL) return;
    switch ((*in)->choice) {
	case choice_pa_pk_as_rep_dhInfo:
	    if ((*in)->u.dh_Info.dhSignedData.data != NULL)
		free((*in)->u.dh_Info.dhSignedData.data);
	    break;
	case choice_pa_pk_as_rep_encKeyPack:
	    if ((*in)->u.encKeyPack.data != NULL)
		free((*in)->u.encKeyPack.data);
	    break;
	default:
	    break;
    }
    free(*in);
}

void free_krb5_pa_pk_as_rep_draft9(krb5_pa_pk_as_rep_draft9 **in)
{
    if (*in == NULL) return;
    if ((*in)->u.encKeyPack.data != NULL)
	free((*in)->u.encKeyPack.data);
    free(*in);
}

void free_krb5_external_principal_identifier(krb5_external_principal_identifier ***in)
{
    int i = 0;
    if (*in == NULL) return;
    while ((*in)[i] != NULL) {
	if ((*in)[i]->subjectName.data != NULL)
	    free((*in)[i]->subjectName.data);
	if ((*in)[i]->issuerAndSerialNumber.data != NULL)
	    free((*in)[i]->issuerAndSerialNumber.data);
	if ((*in)[i]->subjectKeyIdentifier.data != NULL)
	    free((*in)[i]->subjectKeyIdentifier.data);
	free((*in)[i]);
	i++;
    }
    free(*in);
}

void free_krb5_trusted_ca(krb5_trusted_ca ***in)
{
    int i = 0;
    if (*in == NULL) return;
    while ((*in)[i] != NULL) {
	switch((*in)[i]->choice) {
	    case choice_trusted_cas_principalName:
		break;
	    case choice_trusted_cas_caName:
		if ((*in)[i]->u.caName.data != NULL)
		    free((*in)[i]->u.caName.data);
		break;
	    case choice_trusted_cas_issuerAndSerial:
		if ((*in)[i]->u.issuerAndSerial.data != NULL)
		    free((*in)[i]->u.issuerAndSerial.data);
		break;
	}
	free((*in)[i]);
	i++;
    }
    free(*in);
}

void free_krb5_typed_data(krb5_typed_data ***in)
{
    int i = 0;
    if (*in == NULL) return;
    while ((*in)[i] != NULL) {
	if ((*in)[i]->data != NULL)
	    free((*in)[i]->data);
	free((*in)[i]);
	i++;
    }
    free(*in);
}

void free_krb5_algorithm_identifier(krb5_algorithm_identifier ***in)
{
    int i = 0;
    if (*in == NULL) return;
    while ((*in)[i] != NULL) {
	if ((*in)[i]->algorithm.data != NULL)
	    free((*in)[i]->algorithm.data);
	if ((*in)[i]->parameters.data != NULL)
	    free((*in)[i]->parameters.data);
	free((*in)[i]);
	i++;
    }
    free(*in);
}

void
free_krb5_subject_pk_info(krb5_subject_pk_info **in)
{
    if ((*in) == NULL) return;
    if ((*in)->algorithm.parameters.data != NULL)
	free((*in)->algorithm.parameters.data);
    if ((*in)->subjectPublicKey.data != NULL)
	free((*in)->subjectPublicKey.data);
    free(*in);
}

void
free_krb5_kdc_dh_key_info(krb5_kdc_dh_key_info **in)
{
    if (*in == NULL) return;
    if ((*in)->subjectPublicKey.data != NULL)
	free((*in)->subjectPublicKey.data);
    free(*in);
}

void init_krb5_pa_pk_as_req(krb5_pa_pk_as_req **in)
{
    (*in) = malloc(sizeof(krb5_pa_pk_as_req));
    if ((*in) == NULL) return;
    (*in)->signedAuthPack.data = NULL;
    (*in)->signedAuthPack.length = 0;
    (*in)->trustedCertifiers = NULL;
    (*in)->kdcPkId.data = NULL;
    (*in)->kdcPkId.length = 0;
}

void init_krb5_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 **in)
{
    (*in) = malloc(sizeof(krb5_pa_pk_as_req_draft9));
    if ((*in) == NULL) return;
    (*in)->signedAuthPack.data = NULL;
    (*in)->signedAuthPack.length = 0;
    (*in)->trustedCertifiers = NULL;
    (*in)->kdcCert.data = NULL;
    (*in)->kdcCert.length = 0;
    (*in)->encryptionCert.data = NULL;
    (*in)->encryptionCert.length = 0;
}

void init_krb5_reply_key_pack(krb5_reply_key_pack **in)
{
    (*in) = malloc(sizeof(krb5_reply_key_pack));
    if ((*in) == NULL) return;
    (*in)->replyKey.contents = NULL;
    (*in)->replyKey.length = 0;
    (*in)->asChecksum.contents = NULL;
    (*in)->asChecksum.length = 0;
}

void init_krb5_reply_key_pack_draft9(krb5_reply_key_pack_draft9 **in)
{
    (*in) = malloc(sizeof(krb5_reply_key_pack_draft9));
    if ((*in) == NULL) return;
    (*in)->replyKey.contents = NULL;
    (*in)->replyKey.length = 0;
}

void init_krb5_auth_pack(krb5_auth_pack **in)
{
    (*in) = malloc(sizeof(krb5_auth_pack));
    if ((*in) == NULL) return;
    (*in)->clientPublicValue = NULL;
    (*in)->supportedCMSTypes = NULL;
    (*in)->clientDHNonce.length = 0;
    (*in)->clientDHNonce.data = NULL;
    (*in)->pkAuthenticator.paChecksum.contents = NULL;
}

void init_krb5_auth_pack_draft9(krb5_auth_pack_draft9 **in)
{
    (*in) = malloc(sizeof(krb5_auth_pack_draft9));
    if ((*in) == NULL) return;
    (*in)->clientPublicValue = NULL;
}

void init_krb5_pa_pk_as_rep(krb5_pa_pk_as_rep **in)
{
    (*in) = malloc(sizeof(krb5_pa_pk_as_rep));
    if ((*in) == NULL) return;
    (*in)->u.dh_Info.serverDHNonce.length = 0;
    (*in)->u.dh_Info.serverDHNonce.data = NULL;
    (*in)->u.dh_Info.dhSignedData.length = 0;
    (*in)->u.dh_Info.dhSignedData.data = NULL;
    (*in)->u.encKeyPack.length = 0;
    (*in)->u.encKeyPack.data = NULL;
}

void init_krb5_pa_pk_as_rep_draft9(krb5_pa_pk_as_rep_draft9 **in)
{
    (*in) = malloc(sizeof(krb5_pa_pk_as_rep_draft9));
    if ((*in) == NULL) return;
    (*in)->u.dhSignedData.length = 0;
    (*in)->u.dhSignedData.data = NULL;
    (*in)->u.encKeyPack.length = 0;
    (*in)->u.encKeyPack.data = NULL;
}

void init_krb5_typed_data(krb5_typed_data **in)
{
    (*in) = malloc(sizeof(krb5_typed_data));
    if ((*in) == NULL) return;
    (*in)->type = 0;
    (*in)->length = 0;
    (*in)->data = NULL;
}

void
init_krb5_subject_pk_info(krb5_subject_pk_info **in)
{
    (*in) = malloc(sizeof(krb5_subject_pk_info));
    if ((*in) == NULL) return;
    (*in)->algorithm.parameters.data = NULL;
    (*in)->algorithm.parameters.length = 0;
    (*in)->subjectPublicKey.data = NULL;
    (*in)->subjectPublicKey.length = 0;
}

/* debugging functions */
void
print_buffer(unsigned char *buf, int len)
{
    int i = 0;
    if (len <= 0)
	return;

    for (i = 0; i < len; i++)
	pkiDebug("%02x ", buf[i]);
    pkiDebug("\n");
}

void
print_buffer_bin(unsigned char *buf, int len, char *filename)
{
    FILE *f = NULL;
    int i = 0;

    if (len <= 0 || filename == NULL)
	return;

    if ((f = fopen(filename, "w")) == NULL)
	return;

    for (i = 0; i < len; i++)
	fputc(buf[i], f);

    fclose(f);
}
