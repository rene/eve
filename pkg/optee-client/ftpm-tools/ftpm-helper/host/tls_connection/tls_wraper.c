// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2016-2017, Linaro Limited
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/x509.h>
#include <mbedtls/ssl.h>
#include "tls_wraper.h"

typedef struct {
    mbedtls_ssl_context *ssl;
    mbedtls_net_context *net;
} io_ctx_t;

typedef struct {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
} rng_context_t;

const char *pers = "secedge_activation_app";
static rng_context_t g_rng;
static io_ctx_t io_ctx;
static mbedtls_ssl_config conf;
static mbedtls_x509_crt cacert;
static mbedtls_x509_crt clicert;
static mbedtls_pk_context pkey;

static void rng_init(rng_context_t *rng)
{
    mbedtls_ctr_drbg_init(&rng->drbg);
    mbedtls_entropy_init(&rng->entropy);
}

static int rng_seed(rng_context_t *rng, const char *pers)
{
    int rc;

    int (*f_entropy)(void *, unsigned char *, size_t) = mbedtls_entropy_func;

    rc = mbedtls_ctr_drbg_seed(&rng->drbg,
                                    f_entropy, &rng->entropy,
                                    (const unsigned char *) pers,
                                    strlen(pers));

    if (rc != 0) {
        fprintf(stderr, "ERR: mbedtls_ctr_drbg_seed returned -0x%x\n", (unsigned int) -rc);
        return rc;
    }

    return 0;
}

static void rng_free(rng_context_t *rng)
{
    mbedtls_ctr_drbg_free(&rng->drbg);
    mbedtls_entropy_free(&rng->entropy);
}

static int rng_get(void *p_rng, unsigned char *output, size_t output_len)
{
    rng_context_t *rng = p_rng;

    return mbedtls_ctr_drbg_random(&rng->drbg, output, output_len);
}

static int send_cb(void *ctx, unsigned char const *buf, size_t len)
{
    io_ctx_t *io_ctx = (io_ctx_t *) ctx;

    return mbedtls_net_send(io_ctx->net, buf, len);
}

static int recv_cb(void *ctx, unsigned char *buf, size_t len)
{
    io_ctx_t *io_ctx = (io_ctx_t *) ctx;
    int rc;

	rc = mbedtls_net_recv(io_ctx->net, buf, len);
    if (rc < 0) {
		fprintf(stderr, "ERR: Read error\n");
    }
    return rc;
}

static int recv_timeout_cb(void *ctx, unsigned char *buf, size_t len,
                    uint32_t timeout)
{
    io_ctx_t *io_ctx = (io_ctx_t *) ctx;
    int rc;

    rc = mbedtls_net_recv_timeout(io_ctx->net, buf, len, timeout);
    if (rc < 0) {
        fprintf(stderr, "ERR: Read error\n");
    }
    return rc;
}


int tls_init(tls_client_config *tls_client, mbedtls_ssl_context *ssl, mbedtls_net_context *server_fd)
{

	int rc = 0;
	psa_status_t status;

	psa_crypto_init();	
//mbedtls_debug_set_threshold(4);
    mbedtls_net_init(server_fd);
    mbedtls_ssl_init(ssl);
    rng_init(&g_rng);
    mbedtls_ssl_config_init(&conf);
	mbedtls_x509_crt_init(&cacert);
    mbedtls_x509_crt_init(&clicert);
    mbedtls_pk_init(&pkey);

    rc = rng_seed(&g_rng, pers);
    if (rc != 0) {
        goto exit;
    }

	status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        fprintf(stderr, "ERR: Failed to initialize PSA Crypto implementation: %d\n", (int) status);
        rc = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        goto exit;
    }

	rc = mbedtls_x509_crt_parse(&cacert, tls_client->ca_buff, tls_client->ca_len);
	if (rc < 0) {
		fprintf(stderr, "ERR: Failed to parse CA: %x\n", rc);
        goto exit;
    }
	rc = mbedtls_x509_crt_parse(&clicert, tls_client->crt_buff, tls_client->crt_len);
	if (rc < 0) {
		fprintf(stderr, "ERR: Failed to parse client cert: %x\n", rc);
        goto exit;
    }

	rc = mbedtls_pk_parse_key(&pkey, (const unsigned char *) tls_client->key_buff, tls_client->key_len, NULL, 0, rng_get, &g_rng);
	if (rc < 0) {
		fprintf(stderr, "ERR: Failed to parse client key: %x\n", rc);
        goto exit;
    }

	if ((rc = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        fprintf(stderr, "ERR: Failed to create ssl config: %x\n", rc);
        goto exit;
    }

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_rng(&conf, rng_get, &g_rng);

	if (tls_client->read_timeout) {
		mbedtls_ssl_conf_read_timeout(&conf, tls_client->read_timeout);
	} else {
		mbedtls_ssl_conf_read_timeout(&conf, 0);
	}

	mbedtls_ssl_conf_tls13_key_exchange_modes(&conf, MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_ALL);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);

	if ((rc = mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey)) != 0) {
		fprintf(stderr, "ERR: Failed to set own certificate chain and private key: %x\n", rc);
		goto exit;
	}

	mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
	mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);

	if ((rc = mbedtls_ssl_setup(ssl, &conf)) != 0) {
        fprintf(stderr, "ERR: Failed to set up an SSL context: %x\n", rc);
        goto exit;
    }

	if ((rc = mbedtls_ssl_set_hostname(ssl, tls_client->server_name)) != 0) {
        fprintf(stderr, "ERR: Failed to set the hostname: %x\n", rc);
        goto exit;
    }

exit:
	return rc;
}

int tls_connect(tls_client_config *tls_client, mbedtls_ssl_context *ssl, mbedtls_net_context *server_fd)
{

	int rc = 0;
	uint32_t flags;
    char buf[1024];

    io_ctx.ssl = ssl;
    io_ctx.net = server_fd;
    mbedtls_ssl_set_bio(ssl, &io_ctx, send_cb, recv_cb, recv_timeout_cb);

	if ((rc = mbedtls_net_connect(server_fd, tls_client->server_name, tls_client->server_port, MBEDTLS_NET_PROTO_TCP)) != 0) {
		fprintf(stderr, "ERR: Failed to initiate a connection with %s:%s retured %x\n", tls_client->server_name, tls_client->server_port, rc);
		goto exit;
	}

	rc = mbedtls_net_set_block(server_fd);
	if (rc != 0) {
        fprintf(stderr, "ERR: net_set_(non)block() returned -0x%x\n", (unsigned int) -rc);
        goto exit;
    }
	
	while ((rc = mbedtls_ssl_handshake(ssl)) != 0) {
		if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE && rc != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
			mbedtls_strerror(rc, buf, sizeof(buf));
            fprintf(stderr, "ERR: Failed to perform the SSL handshake %x: %s \n", rc, buf);
			if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				fprintf(stderr, "ERR: Unable to verify the server's certificate. Either it is invalid\n");
			}
			goto exit;
		}
    }

	/*
     * Verify the server certificate
     */
    if ((flags = mbedtls_ssl_get_verify_result(ssl)) != 0) {
        char vrfy_buf[512];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        fprintf(stderr, "ERR: Certificate verification failed: %s\n", vrfy_buf);
		rc = flags;
    }

exit:
	return rc;
}

/*
 * [in]		memref[0]	ssl
 * [in]		memref[1].buffer	buffer 
 * [in]		memref[1].size	len 
 */
int tls_send(mbedtls_ssl_context *ssl, uint8_t *data, size_t datalen)
{

	int rc = 0;
	size_t written = 0;
	int frags = 0;

	do {
		while ((rc = mbedtls_ssl_write(ssl, data + written, datalen - written)) < 0) {
			if (rc != MBEDTLS_ERR_SSL_WANT_READ &&
				rc != MBEDTLS_ERR_SSL_WANT_WRITE &&
				rc != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
				fprintf(stderr, "ERR: mbedtls_ssl_write error  %x\n", (unsigned int) -rc);
				goto exit;
			}
		}
		frags++;
		written += rc;
	} while (written < datalen);

exit:
	return (rc < 0) ? rc : written;

}

int tls_read(mbedtls_ssl_context *ssl, uint8_t **data, size_t *datalen)
{

	int rc = 0;
	unsigned char buf[MAX_REQUEST_SIZE];
	char err[1024];
	int len = 0;
	uint8_t *ptr = NULL;
	int data_offset = 0;

	*datalen = 0;
	*data = calloc(0, 1);
	do {
		len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		rc = mbedtls_ssl_read(ssl, buf, len);
		if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}

		if (rc <= 0) {
			switch (rc) {
				case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
#ifdef CFG_SEQ_FTPM_TEST
					printf("DEBUG: Server send close notify\n");
#endif
					rc = MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
					goto exit;
				case MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET:
					continue;
				default:
					mbedtls_strerror(rc, err, sizeof(err));
#ifdef CFG_SEQ_FTPM_TEST
					printf("DEBUG: mbedtls_ssl_read returned %s\n", err);
#endif
					goto exit;
			}
		}

		len = rc;
		buf[len] = '\0';
		/* End of message should be detected according to the syntax of the
			* application protocol (eg HTTP), just use a dummy test here. */
		if (rc > 0) {
			*datalen += len;
			ptr = realloc(*data, *datalen + 1);
			if (!ptr) {
				fprintf(stderr, "ERR: Failed to realloc.\n");
				rc = -ENOMEM;
				goto exit;
			}
			*data = ptr;
			memcpy(*data + data_offset, buf, len);
			if (buf[len-1] == '\n') {
				rc = 0;
				break;
			}
			data_offset += len;
		}
	} while (1);

	(*data)[*datalen] = '\0';

exit:
	if (rc && rc != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		if (*data) {
			free(*data);
			*data = NULL;
			*datalen = 0;
		}
	}
	return rc;
}

/*
 * [in]		memref[0]	ssl
 */
int tls_close(mbedtls_ssl_context *ssl)
{
	int rc = 0;
	do {
		rc = mbedtls_ssl_close_notify(ssl);
	} while (rc == MBEDTLS_ERR_SSL_WANT_WRITE);

    rng_free(&g_rng);
	mbedtls_psa_crypto_free();
	mbedtls_ssl_config_free(&conf);
	mbedtls_x509_crt_free(&clicert);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_pk_free(&pkey);
	
	return 0;
}
