/*
 * Copyright 2004 Red Hat, Inc.
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

#ifndef pam_krb5_minikafs_h
#define pam_krb5_minikafs_h

#include "options.h"
#include "stash.h"

/* Determine if AFS is running. */
int minikafs_has_afs(void);

/* Determine in which cell a given file resides.  Returns 0 on success. */
int minikafs_cell_of_file(const char *file, char *cell, size_t length);

/* Determine in which realm a cell exists.  We do this by obtaining the address
 * of the fileserver which holds /afs/cellname (assuming that the root.cell
 * volume from the cell is mounted there), converting the address to a host
 * name, and then asking libkrb5 to tell us to which realm the host belongs. */
int minikafs_realm_of_cell(struct _pam_krb5_options *options,
			   const char *cell, char *realm, size_t length);

/* Create a new PAG. */
int minikafs_setpag(void);

/* Clear our tokens. */
int minikafs_unlog(void);

/* Try to get tokens for the named cell using every available mechanism. */
int minikafs_log(krb5_context ctx, krb5_ccache ccache,
		 struct _pam_krb5_options *options,
		 const char *cell, uid_t uid, int try_v5_2b_first);

#endif
