#include "../../config.h"

#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif

#ifdef HAVE_SECURITY_PAM_MODULES_H
#include <security/pam_modules.h>
#endif

#include KRB5_H

#include "../../src/init.h"
#include "../../src/log.h"
#include "../../src/minikafs.h"
#include "../../src/v5.h"
#include "../../src/xstr.h"

static void
check_weak(krb5_context ctx)
{
	const unsigned char weak[][8] = {
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
	};
	const unsigned char notweak[][8] = {
		{0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02},
		{0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02},
	};
	unsigned int i;

	for (i = 0; i < sizeof(weak) / sizeof(weak[0]); i++) {
		if (!minikafs_key_is_weak(weak[i])) {
			fprintf(stderr, "expected key %d to be weak\n", i);
			exit(1);
		}
	}
	for (i = 0; i < sizeof(notweak) / sizeof(notweak[0]); i++) {
		if (minikafs_key_is_weak(notweak[i])) {
			fprintf(stderr, "expected key %d to not be weak\n", i);
			exit(1);
		}
	}
}

static void
check_r2k(krb5_context ctx)
{
	const krb5_enctype yes[] = {
		ENCTYPE_AES128_CTS_HMAC_SHA1_96,
		ENCTYPE_AES256_CTS_HMAC_SHA1_96,
	};
	const krb5_enctype no[] = {
		ENCTYPE_DES3_CBC_SHA1,
	};
	unsigned int i;

	for (i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
		if (!minikafs_r2k_is_identity(ctx, yes[i])) {
			fprintf(stderr, "expected enctype %d to be identity\n", i);
			exit(1);
		}
	}
	for (i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
		if (minikafs_r2k_is_identity(ctx, no[i])) {
			fprintf(stderr, "expected enctype %d to not be identity\n", i);
			exit(1);
		}
	}
}

static void
check_hmac(krb5_context ctx)
{
	unsigned char key1[] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
	unsigned char text1[] = "Hi There";
	unsigned char result1[] = {0x92, 0x94, 0x72, 0x7a, 0x36, 0x38, 0xbb, 0x1c, 0x13, 0xf4, 0x8e, 0xf8, 0x15, 0x8b, 0xfc, 0x9d};
	unsigned char key2[] = "Jefe";
	unsigned char text2[] = "what do ya want for nothing?";
	unsigned char result2[] = {0x75, 0x0c, 0x78, 0x3e, 0x6a, 0xb0, 0xb5, 0x03, 0xea, 0xa8, 0x6e, 0x31, 0x0a, 0x5d, 0xb7, 0x38};
	unsigned char key3[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
	unsigned char text3[] = {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
				 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
				 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
				 0xdd, 0xdd};
	unsigned char result3[] = {0x56, 0xbe, 0x34, 0x52, 0x1d, 0x14, 0x4c, 0x88, 0xdb, 0xb8, 0xc7, 0x33, 0xf0, 0xe8, 0xb3, 0xf6};
	unsigned char key4[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
				0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};
	unsigned char text4[] = {0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
				 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
				 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
				 0xcd, 0xcd};
	unsigned char result4[] = {0x69, 0x7e, 0xaf, 0x0a, 0xca, 0x3a, 0x3a, 0xea, 0x3a, 0x75, 0x16, 0x47, 0x46, 0xff, 0xaa, 0x79};
	unsigned char key5[] = {0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c};
	unsigned char text5[] = "Test With Truncation";
	unsigned char result5[] = {0x56, 0x46, 0x1e, 0xf2, 0x34, 0x2e, 0xdc, 0x00, 0xf9, 0xba, 0xb9, 0x95, 0x69, 0x0e, 0xfd, 0x4c};
	unsigned char key6[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
	unsigned char text6[] = "Test Using Larger Than Block-Size Key - Hash Key First";
	unsigned char result6[] = {0x6b, 0x1a, 0xb7, 0xfe, 0x4b, 0xd7, 0xbf, 0x8f, 0x0b, 0x62, 0xe6, 0xce, 0x61, 0xb9, 0xd0, 0xcd};
	unsigned char key7[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
				0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
	unsigned char text7[] = "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data";
	unsigned char result7[] = {0x6f, 0x63, 0x0f, 0xad, 0x67, 0xcd, 0xa0, 0xee, 0x1f, 0xb1, 0xf5, 0x62, 0xdb, 0x3a, 0xa5, 0x3e};
	struct {
		unsigned char *key;
		size_t key_size;
		unsigned char *text;
		size_t text_size;
		unsigned char *result;
	} vectors[] = {
		{key1, 16, text1, 8, result1},
		{key2, 4, text2, 28, result2},
		{key3, 16, text3, 50, result3},
		{key4, 25, text4, 50, result4},
		{key5, 16, text5, 20, result5},
		{key6, 80, text6, 54, result6},
		{key7, 80, text7, 73, result7},
	};
	krb5_data in, out;
	unsigned int i;

	for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
		memset(&in, 0, sizeof(in));
		in.data = (char *) vectors[i].text;
		in.length = vectors[i].text_size;
		memset(&out, 0, sizeof(out));
		if (minikafs_hmac_md5(ctx, vectors[i].key, vectors[i].key_size,
				      &in, &out) != 0) {
			fprintf(stderr, "error computing hmac %d\n", i);
			free(out.data);
			exit(1);
		}
		if (memcmp(out.data, vectors[i].result, out.length) != 0) {
			fprintf(stderr, "hmac %d calculated wrong\n", i);
			free(out.data);
			exit(1);
		}
		free(out.data);
	}
}

static void
check_k2r(krb5_context ctx)
{
	krb5_enctype etypes[] = {
		ENCTYPE_DES3_CBC_SHA1,
	};
	size_t kbytes, klength;
	krb5_data bytes, recovered;
	krb5_keyblock key;
	unsigned int i, j;

	for (i = 0; i < sizeof(etypes) / sizeof(etypes[0]); i++) {
		for (j = 0; j < 100; j++) {
			if (krb5_c_keylengths(ctx, etypes[i], &kbytes, &klength) != 0) {
				fprintf(stderr, "error reading key length for %d\n", etypes[i]);
				exit(1);
			}
			if ((kbytes != 21) || (klength != 24)) {
				fprintf(stderr, "keylength for %d were unexpected\n", etypes[i]);
				exit(1);
			}
			memset(&bytes, 0, sizeof(bytes));
			bytes.data = malloc(kbytes);
			bytes.length = kbytes;
			if (krb5_c_random_make_octets(ctx, &bytes) != 0) {
				fprintf(stderr, "error generating random bytes for %d\n", etypes[i]);
				exit(1);
			}
			memset(&key, 0, sizeof(key));
			key.contents = malloc(klength);
			key.length = klength;
			key.enctype = etypes[i];
			if (krb5_c_random_to_key(ctx, etypes[i], &bytes, &key) != 0) {
				fprintf(stderr, "error deriving random key for %d\n", etypes[i]);
				exit(1);
			}
			memset(&recovered, 0, sizeof(recovered));
			recovered.data = malloc(kbytes);
			recovered.length = kbytes;
			minikafs_des3_k2r(key.contents, (unsigned char *) recovered.data);
			if ((recovered.length != bytes.length) ||
			    (memcmp(recovered.data, bytes.data, bytes.length) != 0)) {
				fprintf(stderr, "error recovering random bytes for %d\n", etypes[i]);
				exit(1);
			}
			free(bytes.data);
			free(key.contents);
			free(recovered.data);
		}
	}
}

int
main(int argc, char **argv)
{
	krb5_context ctx;
	krb5_error_code ret;

	ctx = NULL;
	ret = krb5_init_context(&ctx);
	if (ret != 0) {
		printf("Error initializing Kerberos.\n");
		return ret;
	}
	check_weak(ctx);
	check_r2k(ctx);
	check_hmac(ctx);
	check_k2r(ctx);
	krb5_free_context(ctx);
	printf("OK\n");
	return 0;
}
