/*
 * Copyright 2003,2004,2005 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of the
 * GNU Lesser General Public License, in which case the provisions of the
 * LGPL are required INSTEAD OF the above restrictions.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../config.h"

#ifdef HAVE_SECURITY_PAM_MODULES_H
#define PAM_SM_ACCT_MGMT
#include <security/pam_modules.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <krb5.h>
#ifdef USE_KRB4
#include KRB4_DES_H
#include KRB4_KRB_H
#ifdef KRB4_KRB_ERR_H
#include KRB4_KRB_ERR_H
#endif
#endif

#include "init.h"
#include "log.h"
#include "options.h"
#include "prompter.h"
#include "stash.h"
#include "userinfo.h"
#include "v5.h"
#include "v4.h"

#ident "$Id$"

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		 int argc, PAM_KRB5_MAYBE_CONST char **argv)
{
	PAM_KRB5_MAYBE_CONST char *user;
	krb5_context ctx;
	struct _pam_krb5_options *options;
	struct _pam_krb5_user_info *userinfo;
	struct _pam_krb5_stash *stash;
	int i, retval;
	char *wrong_password = "pam_krb5 validity check";

	/* Initialize Kerberos. */
	if (_pam_krb5_init_ctx(&ctx, argc, argv) != 0) {
		warn("error initializing Kerberos");
		return PAM_SERVICE_ERR;
	}

	/* Get the user's name. */
	i = pam_get_user(pamh, &user, NULL);
	if (i != PAM_SUCCESS) {
		warn("could not identify user name");
		krb5_free_context(ctx);
		return i;
	}

	/* Read our options. */
	options = _pam_krb5_options_init(pamh, argc, argv, ctx);
	if (options == NULL) {
		warn("error parsing options (shouldn't happen)");
		krb5_free_context(ctx);
		return PAM_SERVICE_ERR;
	}

	/* Get information about the user and the user's principal name. */
	userinfo = _pam_krb5_user_info_init(ctx, user, options->realm,
					    options->user_check,
					    options->n_mappings,
					    options->mappings);
	if (userinfo == NULL) {
		warn("error getting information about '%s'", user);
		_pam_krb5_options_free(pamh, ctx, options);
		krb5_free_context(ctx);
		return PAM_USER_UNKNOWN;
	}

	/* Check the minimum UID argument. */
	if ((options->minimum_uid != (uid_t) -1) &&
	    (userinfo->uid < options->minimum_uid)) {
		if (options->debug) {
			debug("ignoring '%s' -- uid below minimum = %lu", user,
			      (unsigned long) options->minimum_uid);
		}
		_pam_krb5_user_info_free(ctx, userinfo);
		_pam_krb5_options_free(pamh, ctx, options);
		krb5_free_context(ctx);
		return PAM_IGNORE;
	}

	/* Get the stash for this user. */
	stash = _pam_krb5_stash_get(pamh, userinfo, options);
	if (stash == NULL) {
		_pam_krb5_user_info_free(ctx, userinfo);
		_pam_krb5_options_free(pamh, ctx, options);
		krb5_free_context(ctx);
		return PAM_SERVICE_ERR;
	}

	/* If we haven't previously attempted to authenticate this user, make
	 * a quick check to screen out unknown users. */
	if (stash->v5attempted == 0) {
		/* We didn't participate in authentication, so stand back. */
		if (options->debug) {
			debug("user '%s' was not authenticated by " PACKAGE
			      ", returning \"user unknown\"", user);
		}
		retval = PAM_USER_UNKNOWN;
	} else {
		/* Check what happened when we asked for initial credentials. */
		switch (stash->v5result) {
		case 0:
			if (options->debug) {
				debug("account management succeeds for '%s'",
				      user);
			}
			retval = PAM_SUCCESS;
			break;
		case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
		case KRB5KDC_ERR_NAME_EXP:
			if (options->ignore_unknown_principals) {
				notice("account checks fail for '%s': "
				       "user is unknown or account expired "
				       "(ignoring)", user);
				retval = PAM_IGNORE;
			} else {
				notice("account checks fail for '%s': "
				       "user is unknown or account expired",
				       user);
				retval = PAM_USER_UNKNOWN;
			}
			break;
		case KRB5KDC_ERR_KEY_EXP:
			notice("account checks fail for '%s': "
			       "password has expired", user);
			retval = PAM_NEW_AUTHTOK_REQD;
			break;
		case EAGAIN:
		case KRB5_REALM_CANT_RESOLVE:
			notice("account checks fail for '%s': "
			       "can't resolve KDC addresses", user);
			return PAM_AUTHINFO_UNAVAIL;
			break;
		case KRB5_KDC_UNREACH:
			notice("account checks fail for '%s': "
			       "KDCs are unreachable", user);
			return PAM_AUTHINFO_UNAVAIL;
			break;
		default:
			notice("account checks fail for '%s': "
			       "unknown reason %d (%s)", user, stash->v5result,
			       v5_error_message(stash->v5result));
			retval = PAM_SERVICE_ERR;
			break;
		}
	}

	/* If we got this far, check the target user's .k5login file. */
	if ((retval == PAM_SUCCESS) && options->user_check) {
		if (krb5_kuserok(ctx, userinfo->principal_name, user) == 0) {
			notice("account checks fail for '%s': user disallowed "
			       "by .k5login file for '%s'",
			       userinfo->unparsed_name, user);
			retval = PAM_PERM_DENIED;
		}
	}

	/* Clean up. */
	if (options->debug) {
		debug("pam_acct_mgmt returning %d (%s)", retval,
		      pam_strerror(pamh, retval));
	}
	_pam_krb5_options_free(pamh, ctx, options);
	_pam_krb5_user_info_free(ctx, userinfo);
	krb5_free_context(ctx);

	return retval;
}
