/*
 * Copyright (c) 2023-2024, NVIDIA Corporation & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <argp.h>
#include <ftpm_helper_ta.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <tee_client_api.h>
#include <unistd.h>

// Include cloud connet
#include <mbedtls/pem.h>
#include <mbedtls/base64.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "ftpm_helper_ta.h"
#include "tls_wraper.h"
#include "uuid4.h"
#include "json_parse.h"
#include "message.h"
#include "http_parser.h"
#include "base64_utils.h"

// Define for cloud connect
#define REQUEST_HEADER "%s https://%s%s HTTP/1.0\r\nx-trans-id: %s\r\ncookie: %ld\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n"
#define GET_CHALLENGE_PAGE "/api/v1/tpm/device/challenge"
#define ACTIVATE_DEVICE_PAGE "/api/v1/tpm/device/activate"

#define PEM_BEGIN_CERT "-----BEGIN CERTIFICATE-----\n"
#define PEM_END_CERT "-----END CERTIFICATE-----\n"
#define PEM_BEGIN_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n"
#define PEM_END_PUBLIC_KEY "-----END PUBLIC KEY-----\n"
#define PEM_BEGIN_PRIVATE_KEY_EC "-----BEGIN EC PRIVATE KEY-----\n"
#define PEM_END_PRIVATE_KEY_EC "-----END EC PRIVATE KEY-----\n"

#define ALLOC_SPRINTF(buf, fmt, args...) ({      \
	size_t buflen = 0;                           \
	buflen = snprintf(NULL, 0, fmt, ##args) + 1; \
	buf = alloca(buflen);                        \
	if (buf)                                     \
	{                                            \
		sprintf(buf, fmt, ##args);               \
	}                                            \
	buflen;                                      \
})

static char comms_base_url[FTPM_HELPER_TA_COMMS_BASE_URL_LENGTH];

static time_t current_time;
static char x_trans_id[37]; // 36 characters + null terminator

#define FTPM_HELPER_GET_RSA_EK_CERT		(1 << 0)
#define FTPM_HELPER_GET_EC_EK_CERT		(1 << 1)
#define FTPM_HELPER_GET_SILICON_ID_CERT		(1 << 2)
#define FTPM_HELPER_GET_FIRMWARE_ID_CERT	(1 << 3)
#define FTPM_HELPER_GET_RSA_EK_CSR		(1 << 4)
#define FTPM_HELPER_GET_EC_EK_CSR		(1 << 5)
#define FTPM_HELPER_QUERY_ECID			(1 << 6)
#define FTPM_HELPER_QUERY_SN			(1 << 7)
#define FTPM_HELPER_QUERY_PROV_MODE		(1 << 8)
#define FTPM_HELPER_QUERY_VERSION		(1 << 9)
#define FTPM_HELPER_SIGN_EK_CSR			(1 << 10)
#define FTPM_HELPER_RET_SIGNED_EK_CSR		(1 << 11)
#define FTPM_HELPER_INJECT_EPS			(1 << 12)

#define FTPM_HELPER_QUERY_ACTIVATION_STATE (1 << 13)
#define FTPM_HELPER_START_ACTIVATION (1 << 14)
#define FTPM_HELPER_SET_ACTIVATION_STATE (1 << 15)
#define FTPM_HELPER_UNSET_ACTIVATION_STATE (1 << 16)
#define FTPM_HELPER_QUERY_ECID_LABEL (1 << 17)
#define FTPM_HELPER_QUERY_SN_EKS (1 << 18)
#define FTPM_HELPER_QUERY_RSA_EK_CERT (1 << 19)
#define FTPM_HELPER_QUERY_EC_EK_CERT (1 << 20)
#define FTPM_HELPER_QUERY_EPS_SEED (1 << 21)
#define FTPM_HELPER_QUERY_COMMS_PRIV_KEY (1 << 22)
#define FTPM_HELPER_QUERY_DEV_CERT (1 << 23)
#define FTPM_HELPER_QUERY_CA_CERT (1 << 24)
#define FTPM_HELPER_QUERY_ACT_PUB_KEY (1 << 25)
#define FTPM_HELPER_GET_SHARED_SECRET (1 << 26)
#define FTPM_HELPER_INJECT_SN (1 << 27)
#define FTPM_HELPER_QUERY_LICENCE_TYPE (1 << 28)

#define INVALID_OPT_FOR_OFFLINE_PROV_MODE	(FTPM_HELPER_GET_SILICON_ID_CERT | \
						 FTPM_HELPER_GET_RSA_EK_CSR | \
						 FTPM_HELPER_GET_EC_EK_CSR | \
						 FTPM_HELPER_SIGN_EK_CSR | \
						 FTPM_HELPER_RET_SIGNED_EK_CSR)
#define INVALID_OPT_FOR_ONLINE_PROV_MODE	(FTPM_HELPER_GET_RSA_EK_CERT | \
						 FTPM_HELPER_GET_EC_EK_CERT)

static struct argp_option options[] = {
	{0, 'a', "OUTFILE", 0, "Output file of the fTPM RSA EK Certificate (Offline Provision Mode)."},
	{0, 'b', "OUTFILE", 0, "Output file of the fTPM EC EK Certificate (Offline Provision Mode)."},
	{0, 'c', "OUTFILE", 0, "Output file of the Silicon ID Certificate (Online Provision Mode)."},
	{0, 'd', "OUTFILE", 0, "Output file of the Firmware ID Certificate"},
	
	/*	
	{0, 'e', "OUTFILE", 0, "Output file of the fTPM RSA EK CSR (Online Provision Mode)."},
	{0, 'f', "OUTFILE", 0, "Output file of the fTPM EC EK CSR (Online Provision Mode)."},
	*/
	
	{0, 'g', NULL, 0, "Query the device ECID value"},
	{0, 'h', NULL, 0, "Query the device serial number."},

	/*
	{0, 'i', NULL, 0, "Query the provisioning mode."},
	*/
	
	{0, 'j', NULL, 0, "Query the version."},

	/*
	{0, 'k', "INFILE", 0, "Sign the EK CSR (Online Provision Mode)."},
	{0, 'l', "OUTFILE", 0, "Output of the signed EK CSR file (Online Provision Mode)."},
	{0, 'm', "EPS", 0, "Inject an EPS(starts with \"0x\", 64 bytes, big endian) into fTPM."},
	*/

	#ifdef CFG_SEQ_FTPM_TEST
	{0, 'A', "INFILE", 0, "Input file of the peer pub key"},
	{0, 'B', "OUTFILE", 0, "Output file of the shared secret"},
#ifdef CFG_JETSON_FTPM_HELPER_INJECT_SN
	{0, 'C', "SN", 0, "Inject an SN (starts with \"0x\", 10 bytes, big endian) into fTPM"},
#endif
#endif

	{0, 'S', NULL, 0, "Query the activation state"},
	{0, 'U', NULL, 0, "Start the activation"},
#ifdef CFG_SEQ_FTPM_TEST
 	{0, 'V', NULL, 0, "Set the activation"},
#endif
	{0, 'W', NULL, 0, "Unset the activation"},
	{0, 'J', NULL, 0, "Query the device ECID LABEL value"},
#ifdef CFG_SEQ_FTPM_TEST
	{0, 'K', NULL, 0, "Query RSA EK Cert in DER format"},
	{0, 'L', NULL, 0, "Query EC EK Cert in DER format"},
	{0, 'M', NULL, 0, "Query the device serial number from EKS image"},
	{0, 'N', NULL, 0, "Query EPS value, only for test"},
	{0, 'O', NULL, 0, "Query communication private key in PEM format"},
	{0, 'P', NULL, 0, "Query device cert in PEM format"},
	{0, 'Q', NULL, 0, "Query CA cert in PEM format"},
	{0, 'R', NULL, 0, "Query fTPM Activation public key"},
#endif
	{0, 'Z', NULL, 0, "Query the licence type"},

	{ 0 },
};

struct arguments {
	uint32_t ftpm_helper_options;
	char *out_rsa_ek_cert;
	char *out_ec_ek_cert;
	char *out_sid_cert;
	char *out_fw_id_cert;
	char *out_rsa_ek_csr;
	char *out_ec_ek_csr;
	char *in_sign_ek_csr;
	char *out_signed_ek_csr;
	char *inject_eps_value;
	
#ifdef CFG_SEQ_FTPM_TEST
	char *in_peer_pub_key;
	char *out_shared_secret;
#endif
};

typedef struct ftpm_helper_ca_ctx {
	TEEC_Context ctx;
	TEEC_Session sess;
	struct arguments *argus;
	FILE *fd_out_rsa_ek_cert;
	FILE *fd_out_ec_ek_cert;
	FILE *fd_out_sid_cert;
	FILE *fd_out_fw_id_cert;
	FILE *fd_out_rsa_ek_csr;
	FILE *fd_out_ec_ek_csr;
	FILE *fd_in_ek_csr;
	FILE *fd_out_signed_ek_csr;
	
#ifdef CFG_SEQ_FTPM_TEST
	FILE *fd_in_peer_pub_key;
	FILE *fd_out_shared_secret;
#endif
	
	uint32_t prov_mode;
	uint32_t ver_major;
	uint32_t ver_minor;
} ftpm_helper_ca_ctx_t;
static ftpm_helper_ca_ctx_t ca_sess;

/* Forward declarations */
static int get_file_size(FILE *fptr);

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *argus = state->input;

	switch (key) {
	case 'a':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_RSA_EK_CERT;
		if (arg)
			argus->out_rsa_ek_cert = arg;
		break;
	case 'b':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_EC_EK_CERT;
		if (arg)
			argus->out_ec_ek_cert = arg;
		break;
	case 'c':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_SILICON_ID_CERT;
		if (arg)
			argus->out_sid_cert = arg;
		break;
	case 'd':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_FIRMWARE_ID_CERT;		
		if (arg)
			argus->out_fw_id_cert = arg;
		break;
		
		/*
	case 'e':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_RSA_EK_CSR;
		if (arg)
			argus->out_rsa_ek_csr = arg;
		break;
	case 'f':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_EC_EK_CSR;
		if (arg)
			argus->out_ec_ek_csr = arg;
		break;
		*/
		
	case 'g':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_ECID;
		break;
	case 'h':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_SN;
		break;

		/*
	case 'i':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_PROV_MODE;
		break;
	case 'j':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_VERSION;
		break;
	case 'k':
		argus->ftpm_helper_options |= FTPM_HELPER_SIGN_EK_CSR;
		if (arg)
			argus->in_sign_ek_csr = arg;
		break;
	case 'l':
		argus->ftpm_helper_options |= FTPM_HELPER_RET_SIGNED_EK_CSR;
		if (arg)
			argus->out_signed_ek_csr = arg;
		break;
	case 'm':
		argus->ftpm_helper_options |= FTPM_HELPER_INJECT_EPS;
		if (arg)
			argus->inject_eps_value = arg;
		break;
		*/

#ifdef CFG_SEQ_FTPM_TEST
	case 'A':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_SHARED_SECRET;
		if (arg)
			 argus->in_peer_pub_key = arg;
		break;
	case 'B':
		argus->ftpm_helper_options |= FTPM_HELPER_GET_SHARED_SECRET;
		if (arg)
			argus->out_shared_secret = arg;
		break;
#ifdef CFG_JETSON_FTPM_HELPER_INJECT_SN
	case 'C':
		argus->ftpm_helper_options |= FTPM_HELPER_INJECT_SN;
		if (arg)
			argus->inject_sn_value = arg;
		break;
#endif
#endif

	case 'S':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_ACTIVATION_STATE;
		break;
	case 'U':
		argus->ftpm_helper_options |= FTPM_HELPER_START_ACTIVATION;
		break;
#ifdef CFG_SEQ_FTPM_TEST
	case 'V':
		argus->ftpm_helper_options |= FTPM_HELPER_SET_ACTIVATION_STATE;
		break;
#endif
	case 'W':
		argus->ftpm_helper_options |= FTPM_HELPER_UNSET_ACTIVATION_STATE;
		break;
	case 'J':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_ECID_LABEL;
		break;
	case 'K':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_RSA_EK_CERT;
		break;
	case 'L':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_EC_EK_CERT;
		break;
#ifdef CFG_SEQ_FTPM_TEST
	case 'M':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_SN_EKS;
		break;
	case 'N':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_EPS_SEED;
		break;
#endif
	case 'O':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_COMMS_PRIV_KEY;
		break;
	case 'P':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_DEV_CERT;
		break;
	case 'Q':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_CA_CERT;
		break;
	case 'R':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_ACT_PUB_KEY;
		break;
	case 'Z':
		argus->ftpm_helper_options |= FTPM_HELPER_QUERY_LICENCE_TYPE;
		break;
		
	case ARGP_KEY_END:
		if (argus->ftpm_helper_options == 0)
			argp_usage(state);
		break;
	case ARGP_KEY_ARG:
		if (state->argc <= 1)
			argp_usage(state);
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, NULL, 0, 0, 0, 0 };

static TEEC_Result prepare_tee_session(ftpm_helper_ca_ctx_t *ctx)
{
	TEEC_UUID uuid = FTPM_HELPER_TA_UUID;
	uint32_t origin;
	TEEC_Result rc;

	rc = TEEC_InitializeContext(NULL, &ctx->ctx);
	if (TEEC_SUCCESS != rc)
		goto tee_session_fail;

	rc = TEEC_OpenSession(&ctx->ctx, &ctx->sess, &uuid,
			      TEEC_LOGIN_PUBLIC, NULL, NULL, &origin);

	if (TEEC_SUCCESS != rc)
		TEEC_FinalizeContext(&ctx->ctx);

tee_session_fail:
	return rc;
}

static void terminate_tee_session(ftpm_helper_ca_ctx_t *ctx)
{
	struct arguments *argus = ctx->argus;

	TEEC_CloseSession(&ctx->sess);
	TEEC_FinalizeContext(&ctx->ctx);

	if (FTPM_HELPER_GET_RSA_EK_CERT & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_rsa_ek_cert)
			fclose(ctx->fd_out_rsa_ek_cert);
	}

	if (FTPM_HELPER_GET_EC_EK_CERT & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_ec_ek_cert)
			fclose(ctx->fd_out_ec_ek_cert);
	}

	if (FTPM_HELPER_GET_SILICON_ID_CERT & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_sid_cert)
			fclose(ctx->fd_out_sid_cert);
 	}

	if (FTPM_HELPER_GET_FIRMWARE_ID_CERT & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_fw_id_cert)
			fclose(ctx->fd_out_fw_id_cert);
	}

	if (FTPM_HELPER_GET_RSA_EK_CSR & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_rsa_ek_csr)
			fclose(ctx->fd_out_rsa_ek_csr);
	}

	if (FTPM_HELPER_GET_EC_EK_CSR & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_in_ek_csr)
			fclose(ctx->fd_in_ek_csr);
	}

	if (FTPM_HELPER_SIGN_EK_CSR & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_out_signed_ek_csr)
			fclose(ctx->fd_out_signed_ek_csr);
	}

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_GET_SHARED_SECRET & argus->ftpm_helper_options) {
		if (NULL != ctx->fd_in_peer_pub_key)
			fclose(ctx->fd_in_peer_pub_key);
		if (NULL != ctx->fd_out_shared_secret)
			fclose(ctx->fd_out_shared_secret);
	}
#endif
}

static void fail_handler(int i)
{
	terminate_tee_session(&ca_sess);

	exit(i);
}

static void ca_query_prov_mode(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
					 TEEC_VALUE_OUTPUT,
					 TEEC_VALUE_OUTPUT,
					 TEEC_NONE);
	cmd = FTPM_HELPER_TA_CMD_QUERY_PROV_MODE;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		return;
	}

	if (op.params[0].value.a == FTPM_PROV_MODE_OFFLINE ||
	    op.params[0].value.a == FTPM_PROV_MODE_ONLINE)
	    ca_sess.prov_mode = op.params[0].value.a;

	ca_sess.ver_major = op.params[1].value.a;
	ca_sess.ver_minor = op.params[2].value.a;
}

__attribute__((unused))
static void hexdecode(uint8_t *hexstr, size_t srclen, uint8_t *result, size_t *outlen)
{

	// if (srclen % 2 != 0) {
	//     goto out;
	// }

	printf("srclen=%zu\n", srclen);

	size_t destlen = srclen / 2;
	// result = calloc(destlen, 1);

	if (!result)
	{
		goto out;
	}

	printf("destlen=%zu\n", destlen);

	// remove first hex byte
	for (size_t i = 2, j = 0; i < srclen; i += 2, j++)
	{
		char hexByte[3] = {hexstr[i], hexstr[i + 1], '\0'};
		result[j] = (uint8_t)strtol(hexByte, NULL, 16);
	}

out:
	*outlen = result ? destlen : 0;
	printf("outlen=%zu\n", *outlen);
}

static int convert_der_to_pem(const char *header, const char *footer, uint8_t *der, size_t der_len, uint8_t **pem, size_t *pem_len)
{
	int rc = 0;
	size_t olen = 0;

	rc = mbedtls_pem_write_buffer(header, footer, der, der_len, NULL, 0, &olen);
	if (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
		fprintf(stderr, "%s: Unable to convert pem to der", __func__);
		goto out;
	}
	*pem = calloc(olen, 1);
	if (!*pem) {
		rc = -ENOMEM;
		fprintf(stderr, "%s: Not enough memory", __func__);
		goto out;
	}
	*pem_len = olen;
	rc = mbedtls_pem_write_buffer(header, footer, der, der_len, *pem, *pem_len, &olen);
	if (rc != 0) {
		fprintf(stderr, "%s: Unable to convert pem to der", __func__);
		goto out;
	}
#ifdef CFG_SEQ_FTPM_TEST
	printf("PEM format %s", *pem);
	printf("\n");
#endif

out:
	return rc;
}

#ifdef CFG_SEQ_FTPM_TEST
static void hex_dump(char *buf_name, uint8_t *buf, size_t len)
{
	int count = 0;

	if (!buf_name || !buf || len == 0)
		return;

	printf("%s: ", buf_name);

	for (int i = 0; i < len; i++) {
		count++;
		printf("%02x ", buf[i]);
		if (count % 32 == 0)
			printf("\n");
	}

	printf("\n");
}

static void ca_query_ecid(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint8_t *ecid_buf = NULL;
	uint32_t cmd, origin;
	int i;

	ecid_buf = calloc(1, FTPM_HELPER_TA_ECID_LENGTH);
	if (!ecid_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = ecid_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_ECID_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_ECID;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		goto out;
	}

	for (i = 0; i < FTPM_HELPER_TA_ECID_LENGTH; i++)
		fprintf(stdout, "%02x", ecid_buf[i]);
	fprintf(stdout, "\n");

out:
	free(ecid_buf);
}
#endif /* CFG_SEQ_FTPM_TEST */

static void ca_query_sn(uint8_t *sn_buf)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!sn_buf) {
		fprintf(stderr, "%s: NULL SN pointer.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = sn_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_SN_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_SN;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
	}
}

static TEEC_Result ca_query_ftpm_prop(uint32_t ta_cmd,
			       uint32_t buf_size,
			       FILE *out_fptr)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint8_t *buf = NULL;
	uint32_t cmd, origin;

	if (!out_fptr) {
		fprintf(stderr, "%s: invalid file ptr.\n", __func__);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	buf = calloc(1, buf_size);
	if (!buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = buf;
	op.params[0].tmpref.size = buf_size;
	cmd = ta_cmd;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		goto out;
	}

	/* Store the property. */
	if (op.params[0].tmpref.size > 0)
		fwrite(buf, op.params[0].tmpref.size, 1, out_fptr);

out:
	free(buf);

	return rc;
}

static void ca_sign_ek_csr(FILE *in_csr_fptr, FILE *out_csr_fptr)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint8_t *csr_buf = NULL;
	uint32_t csr_file_len;
	uint32_t cmd, origin;
	int f_rc;

	if (!in_csr_fptr || !out_csr_fptr) {
		fprintf(stderr, "%s: invalid file ptr.\n", __func__);
		return;
	}

	/* Check the csr file size. */
	csr_file_len = get_file_size(in_csr_fptr);
	if (csr_file_len > FTPM_EK_CSR_BUF_SIZE) {
		fprintf(stderr, "%s: invalid temp EK CSR file size.\n", __func__);
		return;
	}

	csr_buf = calloc(1, FTPM_EK_CSR_BUF_SIZE);
	if (!csr_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	f_rc = fread(csr_buf, 1, csr_file_len, in_csr_fptr);
	if (f_rc < 0) {
		fprintf(stderr, "%s: fail to read the CSR file.\n", __func__);
		goto out;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = csr_buf;
	op.params[0].tmpref.size = csr_file_len;
	cmd = FTPM_HELPER_TA_CMD_SIGN_EK_CSR;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		goto out;
	}

	/* Store the updated EK CSR. */
	if (op.params[0].tmpref.size > 0)
		fwrite(csr_buf, op.params[0].tmpref.size, 1, out_csr_fptr);

out:
	free(csr_buf);
}

#ifdef CFG_SEQ_FTPM_TEST
static TEEC_Result ca_inject_eps(uint8_t *eps, int eps_len)
{
	TEEC_Operation op;
	TEEC_Result rc = TEEC_SUCCESS;
	uint32_t cmd, origin;

	if (eps == NULL || eps_len != FTPM_HELPER_TA_EPS_BYTES)
		return TEEC_ERROR_BAD_PARAMETERS;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = eps;
	op.params[0].tmpref.size = eps_len;
	cmd = FTPM_HELPER_TA_CMD_INJECT_EPS;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS)
	{
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
				__func__, rc, origin);
		goto out;
	}

out:
	return rc;
}

#ifdef CFG_JETSON_FTPM_HELPER_INJECT_SN
static TEEC_Result ca_inject_sn(uint8_t *sn, int sn_len)
{
	TEEC_Operation op;
	TEEC_Result rc = TEEC_SUCCESS;
	uint32_t cmd, origin;

	if (sn == NULL || sn_len != FTPM_HELPER_TA_SN_LENGTH)
		return TEEC_ERROR_BAD_PARAMETERS;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
									 TEEC_NONE,
									 TEEC_NONE,
									 TEEC_NONE);
	op.params[0].tmpref.buffer = sn;
	op.params[0].tmpref.size = sn_len;
	cmd = FTPM_HELPER_TA_CMD_INJECT_SN;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS)
	{
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		goto out;
	}

out:
	return rc;
}
#endif
#endif

char *bin2hex(const unsigned char *bin, size_t len)
{
	char   *out;
	size_t  i;

	if (bin == NULL || len == 0)
		return NULL;

	out = malloc(len*2+1);
	for (i=0; i<len; i++) {
		out[i*2]   = "0123456789ABCDEF"[bin[i] >> 4];
		out[i*2+1] = "0123456789ABCDEF"[bin[i] & 0x0F];
	}
	out[len*2] = '\0';

	return out;
}

int hexchr2bin(const char hex, char *out)
{
	if (out == NULL)
		return 0;

	if (hex >= '0' && hex <= '9') {
		*out = hex - '0';
	} else if (hex >= 'A' && hex <= 'F') {
		*out = hex - 'A' + 10;
	} else if (hex >= 'a' && hex <= 'f') {
		*out = hex - 'a' + 10;
	} else {
		return 0;
	}

	return 1;
}

size_t hexs2bin(const char *hex, unsigned char **out)
{
	size_t len;
	char   b1;
	char   b2;
	size_t i;

	if (hex == NULL || *hex == '\0' || out == NULL)
		return 0;

	len = strlen(hex);
	if (len % 2 != 0)
		return 0;
	len /= 2;

	*out = malloc(len);
	memset(*out, 'A', len);
	for (i=0; i<len; i++) {
		if (!hexchr2bin(hex[i*2], &b1) || !hexchr2bin(hex[i*2+1], &b2))
			return 0;
		(*out)[i] = (b1 << 4) | b2;
	}
	return len;
}

static int hex_char_to_nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return TEEC_ERROR_BAD_PARAMETERS;
}

__attribute__((unused))
static TEEC_Result parse_eps_value(const char *input, uint8_t *eps, int eps_len)
{
	int i, j;
	int high, low;
	int eps_string_len = FTPM_HELPER_TA_EPS_BYTES * 2 + 2;

	if (input == NULL || eps == NULL || eps_len != FTPM_HELPER_TA_EPS_BYTES) {
		fprintf(stderr, "Invalid EPS parameters or length.\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (strlen(input) != eps_string_len) {
		fprintf(stderr, "Invalid length of the EPS string. The length must be %d\n", eps_string_len);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (input[0] != '0' || (input[1] != 'x' && input[1] != 'X')) {
		fprintf(stderr, "The EPS value must start with \'0x\'.\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	memset(eps, 0, eps_len);
	for (i = 2, j = 0; i < (eps_len * 2 + 2) && j < eps_len; i += 2, j++) {
		high = hex_char_to_nibble(input[i]);
		low = hex_char_to_nibble(input[i + 1]);

		if (high == TEEC_ERROR_BAD_PARAMETERS || low == TEEC_ERROR_BAD_PARAMETERS) {
			fprintf(stderr, "Invalid EPS hex string.\n");
			return TEEC_ERROR_BAD_PARAMETERS;
		}

		eps[j] = (high << 4) | low;
	}

	return TEEC_SUCCESS;
}

__attribute__((unused))
static TEEC_Result parse_sn_value(const char *input, uint8_t *sn, int sn_len)
{
	int i, j;
	int high, low;
	int sn_string_len = FTPM_HELPER_TA_SN_LENGTH * 2 + 2;

	if (input == NULL || sn == NULL || sn_len != FTPM_HELPER_TA_SN_LENGTH)
	{
		fprintf(stderr, "Invalid SN parameters or length.\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (strlen(input) != sn_string_len)
	{
		fprintf(stderr, "Invalid length of the SN string. The length must be %d\n", sn_string_len);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (input[0] != '0' || (input[1] != 'x' && input[1] != 'X'))
	{
		fprintf(stderr, "The SN value must start with \'0x\'.\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	memset(sn, 0, sn_len);
	for (i = 2, j = 0; i < (sn_len * 2 + 2) && j < sn_len; i += 2, j++)
	{
		high = hex_char_to_nibble(input[i]);
		low = hex_char_to_nibble(input[i + 1]);

		if (high == TEEC_ERROR_BAD_PARAMETERS || low == TEEC_ERROR_BAD_PARAMETERS)
		{
			fprintf(stderr, "Invalid SN hex string.\n");
			return TEEC_ERROR_BAD_PARAMETERS;
		}

		sn[j] = (high << 4) | low;
	}

	return TEEC_SUCCESS;
}

static void ca_push_challenge_code_buff(uint8_t *chge_buf, size_t chge_len,
                                        uint8_t *cloud_pub_key, size_t pub_len,
                                        uint8_t *nonce_buf)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!chge_buf || !cloud_pub_key || !nonce_buf) {
		fprintf(stderr, "%s: invalid ptr.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = chge_buf;
	op.params[0].tmpref.size = chge_len; // FTPM_CHGE_CODE_FROM_CLOUD_TOTAL_SIZE;
	op.params[1].tmpref.buffer = cloud_pub_key;
	op.params[1].tmpref.size = pub_len; // FTPM_CHGE_CODE_EPHEMERAL_PUB_KEY_SIZE;
	op.params[2].tmpref.buffer = nonce_buf;
	op.params[2].tmpref.size = FTPM_NONCE_CODE_SIZE;

	cmd = FTPM_HELPER_TA_CMD_DECODE_CHALLENGE_CLOUD_BASE64;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Nonce data", nonce_buf, FTPM_NONCE_CODE_SIZE);
#endif
out:
	return;
}

static int ca_query_activation_state(void)
{
	int result = -1;
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	cmd = FTPM_HELPER_TA_CMD_QUERY_ACTIVATION_STATE;

	// Send command to TA.
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
	} else {
		result = (int)op.params[0].value.a;
	}

	return result;
}

#ifdef CFG_SEQ_FTPM_TEST
static void ca_set_activation_state(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	cmd = FTPM_HELPER_TA_CMD_SET_ACTIVATION_STATE;

	// Send command to TA.
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
	} else if (!op.params[0].value.a) {
		fprintf(stderr, "%s: Error, fTPM not activated\n", __func__);
	}
}
#endif /* CFG_SEQ_FTPM_TEST */

static void ca_unset_activation_state(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	cmd = FTPM_HELPER_TA_CMD_UNSET_ACTIVATION_STATE;

	// Send command to TA.
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
	} else if (op.params[0].value.a) {
		fprintf(stderr, "%s: Error, fTPM still activated\n", __func__);
	}
}

static int build_get_challenge_url(uint8_t *ecid_label_buf,
                                   uint8_t *ek_cert_buf, size_t ek_cert_len,
                                   uint8_t **url, size_t *url_len)
{
	int rc = -1;
	char *body_req = NULL;
	struct challenge_req_msg challenge_req;
	char *header_req = NULL;
	size_t header_len;
#ifdef CFG_SEQ_FTPM_TEST
	size_t ecid_base64_len = 0;
#endif

	if (!ecid_label_buf || !ek_cert_buf) {
		fprintf(stderr, "%s: Null input pointer!\n", __func__);
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Memset challenge_req\n");
#endif
	memset(&challenge_req, 0, sizeof(struct challenge_req_msg));

	// ecid_label buffer
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("ECID_LABEL request", ecid_label_buf, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
#endif
	challenge_req.ecidlabel = bin2hex(ecid_label_buf, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("ECID_LABEL bas64", challenge_req.ecidlabel, ecid_base64_len);

	printf("ek cert len = %zu\n", ek_cert_len);
#endif
	challenge_req.ekcert = calloc(ek_cert_len, 1);
	memcpy(challenge_req.ekcert, ek_cert_buf, ek_cert_len);

	// Generate a random UUID for X-Trans-Id
	uuid4_generate(x_trans_id);
	current_time = time(NULL);

#ifdef CFG_SEQ_FTPM_TEST
	printf("Convert object to json\n");
#endif

	body_req = c_to_json((void *)&challenge_req, &challenge_req_msg_parser, true);

#ifdef CFG_SEQ_FTPM_TEST
	printf("body_req len = %zu\n", strlen(body_req));
	printf("body_req = %s\n", body_req);
#endif
	header_len = ALLOC_SPRINTF(header_req, REQUEST_HEADER, "GET", comms_base_url, GET_CHALLENGE_PAGE, x_trans_id, current_time, strlen(body_req));
	if (!header_req) {
		fprintf(stderr, "%s: Unable to create https request header\n", __func__);
		goto out;
	}

	*url = calloc(header_len + strlen(body_req), 1);
	if (!url) {
		fprintf(stderr, "%s: Unable to create URL\n", __func__);
		goto out;
	}

	*url_len = header_len - 1;
	memcpy(*url, header_req, header_len - 1);
	memcpy(*url + *url_len, body_req, strlen(body_req));
	*url_len += strlen(body_req);
	rc = 0;

out:
	if (body_req)
		free(body_req);
	free(challenge_req.ecidlabel);
	free(challenge_req.ekcert);
	return rc;
}

static int build_request_activate_url(uint8_t *ecid_label_buf,
                                      uint8_t *nonce_buf,
                                      uint8_t *mb2_sig_buf, size_t mb2_sig_buf_len,
                                      uint8_t *tos_sig_buf, size_t tos_sig_buf_len,
                                      uint8_t **url, size_t *url_len)
{
	int rc = -1;
	char *body_req = NULL;
	struct activate_req_msg activate_req;
	char *header_req = NULL;
	size_t header_len;
	size_t nonce_enc_len = 0;
	size_t mb2sig_enc_len = 0;
	size_t tossig_enc_len = 0;

	memset(&activate_req, 0, sizeof(struct activate_req_msg));

	// ecid_label buffer
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("ECID_LABEL request", ecid_label_buf, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
#endif
	activate_req.ecidlabel = bin2hex(ecid_label_buf, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
	activate_req.nonce = base64_encode(nonce_buf, FTPM_NONCE_CODE_SIZE, &nonce_enc_len);
	activate_req.eventlogmb2sig = base64_encode(mb2_sig_buf, mb2_sig_buf_len, &mb2sig_enc_len);
	activate_req.eventlogtossig = base64_encode(tos_sig_buf, tos_sig_buf_len, &tossig_enc_len);

	body_req = c_to_json((void *)&activate_req, &activate_req_msg_parser, true);

	header_len = ALLOC_SPRINTF(header_req, REQUEST_HEADER, "POST", comms_base_url, ACTIVATE_DEVICE_PAGE, x_trans_id, current_time, strlen(body_req));
	if (!header_req) {
		fprintf(stderr, "%s: Unable to create https request header\n", __func__);
		goto out;
	}

	*url = calloc(header_len + strlen(body_req), 1);
	if (!url) {
		fprintf(stderr, "%s: Unable to create URL\n", __func__);
		goto out;
	}

	*url_len = header_len - 1;
	memcpy(*url, header_req, header_len - 1);
	memcpy(*url + *url_len, body_req, strlen(body_req));
	*url_len += strlen(body_req);
	rc = 0;

out:
	free(body_req);
	free(activate_req.nonce);
	free(activate_req.ecidlabel);
	free(activate_req.eventlogmb2sig);
	free(activate_req.eventlogtossig);

	return rc;
}

static int ca_verify_activation_sig(uint8_t *sn_ecid_label_buf,
                                    uint8_t *act_pubkey_buf, size_t act_buf_len,
                                    uint8_t *signature, size_t signature_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_VALUE_OUTPUT);

	op.params[0].tmpref.buffer = sn_ecid_label_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_SN_ECID_LABEL_LENGTH;
	op.params[1].tmpref.buffer = act_pubkey_buf;
	op.params[1].tmpref.size = act_buf_len;
	op.params[2].tmpref.buffer = signature;
	op.params[2].tmpref.size = signature_len;
	op.params[3].value.a = 0;

	cmd = FTPM_HELPER_TA_CMD_VERIFY_ACTIVATION_SIGNATURE;

	// Send command to TA.
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return -1;
	}

	if (op.params[3].value.a != 1) {
		fprintf(stderr, "%s: Verify signature failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return -1;
	}

	return rc;
}

static void ca_query_ecid_label(uint8_t *ecid_buf)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!ecid_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = ecid_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_ECID_LABEL_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_ECID_LABEL;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
	 	fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}
}

#ifdef CFG_SEQ_FTPM_TEST
static void ca_query_rsa_ek_cert(uint8_t *cert_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!cert_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = cert_buf;
	op.params[0].tmpref.size = FTPM_EK_CERT_BUF_SIZE;
	cmd = FTPM_HELPER_TA_CMD_GET_RSA_EK_CERT;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	*out_len = op.params[0].tmpref.size;
	// Dump ek cert
	hex_dump("EK cert", cert_buf, *out_len);
}
#endif

static void ca_query_ec_ek_cert(uint8_t *cert_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!cert_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = cert_buf;
	op.params[0].tmpref.size = FTPM_EK_CERT_BUF_SIZE;
	cmd = FTPM_HELPER_TA_CMD_GET_EC_EK_CERT;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	*out_len = op.params[0].tmpref.size;
#ifdef CFG_SEQ_FTPM_TEST
	// Dump ek cert
	hex_dump("EK cert", cert_buf, *out_len);
#endif
}

#ifdef CFG_SEQ_FTPM_TEST
static void ca_query_sn_eks(uint8_t *sn_buf)
{
	// printf("ca_query_sn_eks \n");
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!sn_buf)
	{
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
									 TEEC_NONE,
									 TEEC_NONE,
									 TEEC_NONE);
	op.params[0].tmpref.buffer = sn_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_SN_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_SN_EKS;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS)
	{
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
				__func__, rc, origin);
		return;
	}

	for (int i = 0; i < FTPM_HELPER_TA_SN_LENGTH; i++)
		fprintf(stdout, "%02x", sn_buf[i]);
	// fprintf(stdout, "\n");
}

static void ca_query_eps(uint8_t *eps_buf)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!eps_buf) {
		fprintf(stderr, "%s: no output buffer provided.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = eps_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_EPS_BYTES;
	cmd = FTPM_HELPER_TA_CMD_QUERY_EPS;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	hex_dump("EPS seed", eps_buf, FTPM_HELPER_TA_EPS_BYTES);
	fprintf(stdout, "\n");
}
#endif /* CFG_SEQ_FTPM_TEST */

static void ca_query_comms_priv_key(uint8_t *priv_key_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!priv_key_buf) {
		fprintf(stderr, "%s: no output buffer provided.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = priv_key_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_COMMS_PRIV_KEY_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_COMMS_PRIV_KEY;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	*out_len = op.params[0].tmpref.size;
}

static void ca_query_device_cert(uint8_t *device_cert_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!device_cert_buf) {
		fprintf(stderr, "%s: no output buffer provided.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = device_cert_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_DEVICE_CERT_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_DEVICE_CERT;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
	}

	*out_len = op.params[0].tmpref.size;
}

static void ca_query_CA_cert(uint8_t *ca_cert_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!ca_cert_buf) {
		fprintf(stderr, "%s: no output buffer provided.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = ca_cert_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_CA_CERT_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_CA_CERT;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	*out_len = op.params[0].tmpref.size;
}

static void ca_query_base_url(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;
	size_t out_len = 0;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = comms_base_url;
	op.params[0].tmpref.size = FTPM_HELPER_TA_COMMS_BASE_URL_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_COMMS_BASE_URL;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		/* Use the dev cluster if the base URL is not available */
		fprintf(stderr, "%s: activating against the dev cluster\n",
		                __func__);
		strncpy(comms_base_url, "prov.dev.me.secedge.com", FTPM_HELPER_TA_COMMS_BASE_URL_LENGTH);

		return;
	}

	out_len = op.params[0].tmpref.size;
	memcpy(comms_base_url, op.params[0].tmpref.buffer, out_len);
	comms_base_url[out_len] = '\0';
}

static void ca_query_act_pubkey(uint8_t *act_pubkey_buf, size_t *out_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	if (!act_pubkey_buf) {
		fprintf(stderr, "%s: no output buffer provided.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = act_pubkey_buf;
	op.params[0].tmpref.size = FTPM_HELPER_TA_ACT_PUB_KEY_LENGTH;
	cmd = FTPM_HELPER_TA_CMD_QUERY_ACT_PUB_KEY;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
	}

	*out_len = op.params[0].tmpref.size;
}

static void ca_query_evt_log_sig_buf(uint32_t ta_cmd, uint8_t *out_sig_buf, size_t *buff_len)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = out_sig_buf;
	op.params[0].tmpref.size = *buff_len;
	cmd = ta_cmd;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		return;
	}

	*buff_len = op.params[0].tmpref.size;
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Event Log Sig", out_sig_buf, *buff_len);
#endif
}

static void ca_start_activation(void)
{
	uint8_t *priv_key_buf = NULL;
	uint8_t *dev_cert_buf = NULL;
	uint8_t *ca_cert_buf = NULL;
	uint8_t *act_pubkey_buf = NULL;
	uint8_t *ecid_label_buf = NULL;
	char *ecid_label_hex_buf = NULL;
	uint8_t *ek_cert_buf = NULL;
	uint8_t *ek_cert_buf_decode = NULL;
	uint8_t *ek_cert_buf_decode_pem = NULL;
	uint8_t *chge_buf = NULL;
	uint8_t *cloud_pub_key = NULL;
	uint8_t *nonce_buf = NULL;
	uint8_t *mb2_sig_buf = NULL;
	uint8_t *tos_sig_buf = NULL;
	uint8_t *sn_buf = NULL;
	uint8_t *sn_ecid_label_buf = NULL;
	// uint8_t *out_signature = NULL;
	uint8_t *sig_buffer = NULL;
#ifdef CFG_SEQ_FTPM_TEST
	size_t dhsecret_len = 0;
	u_int8_t *dhsecret_buf = NULL;
#endif

	int rc = 0;
	tls_client_config tls_client = { 0 };
	mbedtls_ssl_context ssl;
	mbedtls_net_context server_fd;
	struct challenge_resp_msg *challenge_resp_obj = NULL;
	struct activate_resp_msg *activate_resp_obj = NULL;
	struct http_request *resp_obj = NULL;

	uint8_t *url = NULL;
	size_t url_len;
	uint8_t *read_data = NULL;
	size_t read_len = 0;

	// Get the activation base URL
	ca_query_base_url();

#ifdef CFG_SEQ_FTPM_TEST
	printf("Get device private key\n");
#endif
	// Get device private key
	priv_key_buf = calloc(1, FTPM_HELPER_TA_COMMS_PRIV_KEY_LENGTH);
	if (!priv_key_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	size_t comms_priv_key_len = 0;
	ca_query_comms_priv_key(priv_key_buf, &comms_priv_key_len);
#ifdef CFG_SEQ_FTPM_TEST
	printf("PEM format %s", priv_key_buf);
	printf("\n");
#endif

#ifdef CFG_SEQ_FTPM_TEST
	printf("Get device cert\n");
#endif
	// Get device cert
	dev_cert_buf = calloc(1, FTPM_HELPER_TA_DEVICE_CERT_LENGTH);
	if (!dev_cert_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	size_t device_cert_len = 0;
	ca_query_device_cert(dev_cert_buf, &device_cert_len);
#ifdef CFG_SEQ_FTPM_TEST
	printf("PEM format %s", dev_cert_buf);
	printf("\n");
#endif

#ifdef CFG_SEQ_FTPM_TEST
	printf("Get CA cert\n");
#endif
	// Get CA cert
	ca_cert_buf = calloc(1, FTPM_HELPER_TA_CA_CERT_LENGTH);
	if (!ca_cert_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	size_t ca_cert_len = 0;
	ca_query_CA_cert(ca_cert_buf, &ca_cert_len);
#ifdef CFG_SEQ_FTPM_TEST
	printf("PEM format %s", ca_cert_buf);
	printf("\n");
#endif

	// Generate ECID_LABEL
#ifdef CFG_SEQ_FTPM_TEST
	printf("Generate ECID_LABEL from ECID\n");
#endif
	ecid_label_buf = calloc(1, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
	if (!ecid_label_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	ca_query_ecid_label(ecid_label_buf);

#ifdef CFG_SEQ_FTPM_TEST
	printf("Get EK cert\n");
#endif
	// Get EK cert
	ek_cert_buf = calloc(1, FTPM_EK_CERT_BUF_SIZE);
	if (!ek_cert_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	size_t ek_cert_len = 0;
	ca_query_ec_ek_cert(ek_cert_buf, &ek_cert_len);
	size_t ek_cert_pem_len = 0;
	convert_der_to_pem(PEM_BEGIN_CERT, PEM_END_CERT, ek_cert_buf, ek_cert_len, &ek_cert_buf_decode_pem, &ek_cert_pem_len);

	// Invoke TLS connection: START REQUEST CHALLENGE ============================
	// Open
	// Send
	// Receive
	// Close

	printf("===== INVOKE CLOUD: START REQUEST CHALLENGE ===== \n");
	uuid4_init();

#ifdef CFG_SEQ_FTPM_TEST
	printf("Set ca cert\n");
#endif
	tls_client.ca_len = ca_cert_len+1; // Include NULL-terminator
	tls_client.ca_buff = calloc(1, tls_client.ca_len);
	if (!tls_client.ca_buff) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	memcpy(tls_client.ca_buff, ca_cert_buf, tls_client.ca_len);

#ifdef CFG_SEQ_FTPM_TEST
	printf("Set device cert\n");
#endif
	tls_client.crt_len = device_cert_len+1; // Include NULL-terminator
	tls_client.crt_buff = calloc(1, tls_client.crt_len);
	if (!tls_client.crt_buff) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	memcpy(tls_client.crt_buff, dev_cert_buf, tls_client.crt_len);

#ifdef CFG_SEQ_FTPM_TEST
	printf("Set device private key\n");
#endif
	tls_client.key_len = comms_priv_key_len+1; //Include NULL-terminator
	tls_client.key_buff = calloc(1, tls_client.key_len);
	if (!tls_client.key_buff) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

	memcpy(tls_client.key_buff, priv_key_buf, tls_client.key_len);

	tls_client.server_name = comms_base_url;
	tls_client.server_port = "443";
	tls_client.read_timeout = 0;
#ifdef CFG_SEQ_FTPM_TEST
	printf("Set server name %s - server port %s\n", tls_client.server_name, tls_client.server_port);
#endif

	rc = tls_init(&tls_client, &ssl, &server_fd);
	if (rc) {
		fprintf(stderr, "%s: Unable to init TLS\n", __func__);
		goto out;
	}

	// Connect
	rc = tls_connect(&tls_client, &ssl, &server_fd);
	if (rc) {
		fprintf(stderr, "%s: Unable to connect TLS\n", __func__);
		goto out;
	}

	rc = build_get_challenge_url(ecid_label_buf, ek_cert_buf_decode_pem, ek_cert_pem_len, &url, &url_len);
	if (rc) {
		fprintf(stderr, "%s: Unable build get challenge request\n", __func__);
		goto close_tls;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Send %s\n", url);
	printf("\n");
#endif

	// Send
	rc = tls_send(&ssl, url, url_len);
	if (rc < (int)url_len) {
		fprintf(stderr, "%s: Unable to send request\n", __func__);
		goto close_tls;
	}

	// Receive
	rc = tls_read(&ssl, &read_data, &read_len);
	switch (rc) {
		case 0:
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			rc = tls_close(&ssl);
			if (rc) {
				fprintf(stderr, "%s: Unable to close TLS\n", __func__);
			}
			break;
		default:
			fprintf(stderr, "%s: Unable to recv, rc = 0x%08x\n", __func__, rc);
			goto close_tls;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Read from server: %zu bytes read\n\n%s", read_len, read_data);
	printf("\n");
#endif

	// Parse response
	if (read_data) {
		resp_obj = parse_request((const char *)read_data, read_len);
		if (!resp_obj) {
			fprintf(stderr, "%s: Failed to parse HTTP response\n", __func__);
			goto out;
		}

		rc = 0;
		if (strcmp(resp_obj->status_code, "200") != 0) {
			fprintf(stderr, "%s: Server return status code: %s - %s\n", __func__, resp_obj->status_code, resp_obj->status_text);
			rc = -1;
		}

		// Parse the body even if there is an error
		if (resp_obj->body) {
			challenge_resp_obj = (struct challenge_resp_msg *)json_to_c(resp_obj->body, strlen(resp_obj->body), &challenge_resp_msg_parser);
			if (!challenge_resp_obj) {
				rc = -2;
				fprintf(stderr, "%s: Failed to parse body HTTP response\n", __func__);
			} else if ((rc == -1) && (challenge_resp_obj->error)) {
				fprintf(stderr, "%s: Error: %s\n", __func__, challenge_resp_obj->error);
			}
		}

		if (rc) {
			goto out;
		}
	}

#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Challenge base64 response", (uint8_t*)challenge_resp_obj->challenge, FTPM_CHGE_CODE_FROM_CLOUD_TOTAL_SIZE);
	printf("\n");
#endif

	free(url);
	url_len = 0;
	url = NULL;
	free(read_data);
	read_data = NULL;
	read_len = 0;
	free_request(resp_obj);
	resp_obj = NULL;
	mbedtls_ssl_free(&ssl);
	mbedtls_net_free(&server_fd);

	printf("===== END REQUEST CHALLENGE =====\n");
	// END REQUEST CHALLENGE ========================================================

	size_t chge_len = 0;
	chge_buf = base64_decode(challenge_resp_obj->challenge, strlen(challenge_resp_obj->challenge), &chge_len);
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Challenge Buf", chge_buf, chge_len);
	printf("\n");
#endif

	size_t cloud_pub_key_len = 0;
	cloud_pub_key = base64_decode(challenge_resp_obj->cloudpubKey, strlen(challenge_resp_obj->cloudpubKey), &cloud_pub_key_len);
#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Cloud Pub key buf", cloud_pub_key, cloud_pub_key_len);
	printf("\n");
#endif

#ifdef CFG_SEQ_FTPM_TEST
	// This dh secrect sent from cloud to verify, only for test
	if (challenge_resp_obj->dhsecret) {
		dhsecret_buf = base64_decode(challenge_resp_obj->dhsecret, strlen(challenge_resp_obj->dhsecret), &dhsecret_len);
		hex_dump("dh secret key buf from cloud", dhsecret_buf, dhsecret_len);
		printf("\n");
	}
#endif

	nonce_buf = calloc(1, FTPM_NONCE_CODE_SIZE);
	if (!nonce_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Decode challenge to nonce\n");
#endif
	ca_push_challenge_code_buff(chge_buf, chge_len, cloud_pub_key, cloud_pub_key_len, nonce_buf);

	// ========================================================
	// START REQUEST ACTIVATE
	printf("===== START REQUEST ACTIVATE =====\n");

	size_t mb2_sig_buf_len = FTPM_EVT_LOG_SIG_BUF_SIZE;
	size_t tos_sig_buf_len = FTPM_EVT_LOG_SIG_BUF_SIZE;
	mb2_sig_buf = calloc(1, FTPM_EVT_LOG_SIG_BUF_SIZE);
	tos_sig_buf = calloc(1, FTPM_EVT_LOG_SIG_BUF_SIZE);
	if (!mb2_sig_buf || !tos_sig_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Get MB2 event log signature \n");
#endif
	ca_query_evt_log_sig_buf(FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_MB2, mb2_sig_buf, &mb2_sig_buf_len);
#ifdef CFG_SEQ_FTPM_TEST
	printf("\n");

	printf("Get TOS event log signature \n");
#endif
	ca_query_evt_log_sig_buf(FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_TOS, tos_sig_buf, &tos_sig_buf_len);
#ifdef CFG_SEQ_FTPM_TEST
	printf("\n");
#endif

	uuid4_init();

	rc = build_request_activate_url(ecid_label_buf, nonce_buf, mb2_sig_buf, mb2_sig_buf_len, tos_sig_buf, tos_sig_buf_len, &url, &url_len);
	if (rc) {
		fprintf(stderr, "%s: Unable to build activate request\n", __func__);
		goto out;
	}

	rc = tls_init(&tls_client, &ssl, &server_fd);
	if (rc) {
		fprintf(stderr, "%s: Unable to init TLS\n", __func__);
		goto out;
	}

	rc = tls_connect(&tls_client, &ssl, &server_fd);
	if (rc) {
		fprintf(stderr, "%s: Unable to connect TLS\n", __func__);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Send %s\n", url);
	printf("\n");
#endif

	// Send
	rc = tls_send(&ssl, url, url_len);
	if (rc < (int)url_len) {
		fprintf(stderr, "%s: Unable to send request\n", __func__);
		goto close_tls;
	}

	// Receive
	rc = tls_read(&ssl, &read_data, &read_len);
	switch (rc) {
		case 0:
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			rc = tls_close(&ssl);
			if (rc) {
				fprintf(stderr, "%s: Unable to close TLS\n", __func__);
			}
			break;
		default:
			fprintf(stderr, "%s: Unable to recv\n", __func__);
			goto close_tls;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Read from server: %zu bytes read\n\n%s", read_len, read_data);
	printf("\n");
#endif

	if (read_data) {
		resp_obj = parse_request((const char *)read_data, read_len);
		if (!resp_obj) {
			fprintf(stderr, "%s: Failed to parse HTTP response\n", __func__);
			goto out;
		}

		rc = 0;
		if (strcmp(resp_obj->status_code, "200") != 0) {
			fprintf(stderr, "%s: Server return status code: %s - %s\n", __func__, resp_obj->status_code, resp_obj->status_text);
			rc = -1;
		}

		// Parse the body even if there is an error
		if (resp_obj->body) {
			activate_resp_obj = (struct activate_resp_msg *)json_to_c(resp_obj->body, strlen(resp_obj->body), &activate_resp_msg_parser);
			if (!activate_resp_obj) {
				rc = -2;
				fprintf(stderr, "%s: Failed to parse body HTTP respone\n", __func__);
			} else if ((rc == -1) && (activate_resp_obj->error)) {
				fprintf(stderr, "%s: Error: %s\n", __func__, activate_resp_obj->error);
			}
		}

		if (rc) {
			goto out;
		}
	}

	free(url);
	url_len = 0;
	url = NULL;
	free(read_data);
	read_data = NULL;
	read_len = 0;
	free_request(resp_obj);
	resp_obj = NULL;

	printf("===== END REQUEST ACTIVATE =====\n");
	// END REQUEST ACTIVATE ============================

	size_t sig_len = 0;

	// Verify signature
	/* Base64 decode the signature in the response */
	sig_buffer = base64_decode(activate_resp_obj->signature, strlen(activate_resp_obj->signature), &sig_len);
	if (!sig_buffer) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	hex_dump("Signature", sig_buffer, sig_len);
#endif

	sn_buf = calloc(1, FTPM_HELPER_TA_SN_LENGTH);
	ca_query_sn(sn_buf);

	sn_ecid_label_buf = calloc(1, FTPM_HELPER_TA_SN_ECID_LABEL_LENGTH);
	memcpy(sn_ecid_label_buf, sn_buf, FTPM_HELPER_TA_SN_LENGTH);
	memcpy(sn_ecid_label_buf + FTPM_HELPER_TA_SN_LENGTH, ecid_label_buf, FTPM_HELPER_TA_ECID_LABEL_LENGTH);

	// Get activation public key
	act_pubkey_buf = calloc(1, FTPM_HELPER_TA_ACT_PUB_KEY_LENGTH);
	if (!act_pubkey_buf)
	{
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	size_t act_pubkey_len = 0;
	ca_query_act_pubkey(act_pubkey_buf, &act_pubkey_len);

	// Verify activation signature
#ifdef CFG_SEQ_FTPM_TEST
	printf("Verify activation signature received from cloud\n");
#endif
	rc = ca_verify_activation_sig(sn_ecid_label_buf, act_pubkey_buf, act_pubkey_len, sig_buffer, sig_len);
	if  (rc) {
		fprintf(stderr, "%s: Verify activation failed. \n", __func__);
		goto out;
	}

#ifdef CFG_SEQ_FTPM_TEST
	printf("Verify activation success. \n");
	printf("\n");
#endif

	int act_status = ca_query_activation_state();
	if (act_status == 1) {
		printf("fTPM activation state enabled successfully! \n");
	} else {
		printf("fTPM activation state enabled failed! \n");
	}

close_tls:
	rc = tls_close(&ssl);
	if (rc) {
		fprintf(stderr, "%s: Unable to close TLS\n", __func__);
	}

out:
	free(priv_key_buf);
	free(dev_cert_buf);
	free(ca_cert_buf);
	free(act_pubkey_buf);
	free(ecid_label_buf);
	free(ecid_label_hex_buf);
	free(ek_cert_buf);
	free(ek_cert_buf_decode);
	free(ek_cert_buf_decode_pem);
	free(chge_buf);
	free(cloud_pub_key);
#ifdef CFG_SEQ_FTPM_TEST
	free(dhsecret_buf);
#endif
	free(nonce_buf);
	free(mb2_sig_buf);
	free(sig_buffer);
	free(read_data);
	free(url);
	free_request(resp_obj);
	free(tls_client.ca_buff);
	free(tls_client.crt_buff);
	free(tls_client.key_buff);
	if (challenge_resp_obj)
		msg_obj_put(challenge_resp_obj);
	mbedtls_ssl_free(&ssl);
	mbedtls_net_free(&server_fd);

	return;
}

static int get_file_size(FILE *fptr)
{
	size_t file_len;

	fseek(fptr, 0, SEEK_END);
	file_len = ftell(fptr);
	rewind(fptr);

	return file_len;
}

#ifdef CFG_SEQ_FTPM_TEST

static void ca_get_shared_secret(FILE *in_peer_pubkey_fptr, FILE *out_shared_secret_fptr)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint8_t *peer_pubkey_buf = NULL;
	uint8_t *shared_secret_buf = NULL;
	uint32_t peer_pubkey_file_len;
	uint32_t shared_secret_len = 256;
	uint32_t cmd, origin;
	int f_rc;

	if (!in_peer_pubkey_fptr || !out_shared_secret_fptr) {
		fprintf(stderr, "%s: invalid file ptr.\n", __func__);
		return;
	}

	/* Check the peer pubkey file size. */
	peer_pubkey_file_len = get_file_size(in_peer_pubkey_fptr);
	if (65 < peer_pubkey_file_len) {
		fprintf(stderr, "%s: invalid peer pubkey file size.\n", __func__);
		return;
	}

	peer_pubkey_buf = calloc(1, peer_pubkey_file_len);
	if (!peer_pubkey_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	f_rc = fread(peer_pubkey_buf, 1, peer_pubkey_file_len, in_peer_pubkey_fptr);
	if (f_rc < 0) {
		fprintf(stderr, "%s: fail to read the peer_pubkey file.\n", __func__);
		goto out;
	}

	shared_secret_buf = calloc(1, shared_secret_len);
	if (!shared_secret_buf) {
		fprintf(stderr, "%s: out of memory.\n", __func__);
		return;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
	                                 TEEC_MEMREF_TEMP_OUTPUT,
	                                 TEEC_NONE,
	                                 TEEC_NONE);
	op.params[0].tmpref.buffer = peer_pubkey_buf;
	op.params[0].tmpref.size = peer_pubkey_file_len;
	op.params[1].tmpref.buffer = shared_secret_buf;
	op.params[1].tmpref.size = shared_secret_len;
	cmd = FTPM_HELPER_TA_CMD_GET_SHARED_SECRET;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
		                __func__, rc, origin);
		goto out;
	}

	/* Store the shared secret. */
	if (op.params[1].tmpref.size > 0)
		fwrite(shared_secret_buf, op.params[1].tmpref.size, 1, out_shared_secret_fptr);

out:
	free(peer_pubkey_buf);
	free(shared_secret_buf);
}
#endif /* CFG_SEQ_FTPM_TEST */


static void ca_query_licence_type(void)
{
	TEEC_Operation op;
	TEEC_Result rc;
	uint32_t cmd, origin;
	// int i;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);

	cmd = FTPM_HELPER_TA_CMD_QUERY_LICENCE_TYPE;

	/* Send command to TA. */
	rc = TEEC_InvokeCommand(&ca_sess.sess, cmd, &op, &origin);
	if (rc != TEEC_SUCCESS) {
		fprintf(stderr, "%s: TEEC_InvokeCommand failed 0x%x origin 0x%x\n",
			__func__, rc, origin);
		goto out;
	}

	fprintf(stdout,"0x%02x\n",op.params[0].value.a);

 out:
	return;
}


void handle_ftpm_helper_options(ftpm_helper_ca_ctx_t *ctx)
{
	struct arguments *argus = ctx->argus;
	TEEC_Result rc = TEEC_SUCCESS;
#ifdef CFG_SEQ_FTPM_TEST
	uint8_t eps[FTPM_HELPER_TA_EPS_BYTES];
#endif
	uint8_t sn[FTPM_HELPER_TA_SN_LENGTH];

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_GET_SHARED_SECRET & argus->ftpm_helper_options) {
		if (!argus->in_peer_pub_key || !argus->out_shared_secret) {
			fprintf(stderr, "Error: missing -A or -B option.\n");
			fail_handler(1);
		}

		ctx->fd_in_peer_pub_key = fopen(argus->in_peer_pub_key, "rb");
		ctx->fd_out_shared_secret = fopen(argus->out_shared_secret, "wb");

		ca_get_shared_secret(ctx->fd_in_peer_pub_key,
		                     ctx->fd_out_shared_secret);
	}
#endif

	if (FTPM_HELPER_GET_RSA_EK_CERT & argus->ftpm_helper_options) {
		if (!argus->out_rsa_ek_cert) {
			fprintf(stderr, "Error: missing -a option.\n");
			fail_handler(1);
		}

		ctx->fd_out_rsa_ek_cert = fopen(argus->out_rsa_ek_cert, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_RSA_EK_CERT,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_rsa_ek_cert);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_GET_EC_EK_CERT & argus->ftpm_helper_options) {
		if (!argus->out_ec_ek_cert) {
			fprintf(stderr, "Error: missing -b option.\n");
			fail_handler(1);
		}

		ctx->fd_out_ec_ek_cert = fopen(argus->out_ec_ek_cert, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_EC_EK_CERT,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_ec_ek_cert);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_GET_SILICON_ID_CERT & argus->ftpm_helper_options) {
		if (!argus->out_sid_cert) {
			fprintf(stderr, "Error: missing -c option.\n");
			fail_handler(1);
		}

		ctx->fd_out_sid_cert = fopen(argus->out_sid_cert, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_SID_CERT,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_sid_cert);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_GET_FIRMWARE_ID_CERT & argus->ftpm_helper_options) {
		if (!argus->out_fw_id_cert) {
			fprintf(stderr, "Error: missing -d option.\n");
			fail_handler(1);
		}

		ctx->fd_out_fw_id_cert = fopen(argus->out_fw_id_cert, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_FW_ID_CERT,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_fw_id_cert);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_GET_RSA_EK_CSR & argus->ftpm_helper_options) {
		if (!argus->out_rsa_ek_csr) {
			fprintf(stderr, "Error: missing -e option.\n");
			fail_handler(1);
		}

		ctx->fd_out_rsa_ek_csr = fopen(argus->out_rsa_ek_csr, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_RSA_EK_CSR,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_rsa_ek_csr);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_GET_EC_EK_CSR & argus->ftpm_helper_options) {
		if (!argus->out_ec_ek_csr) {
			fprintf(stderr, "Error: missing -f option.\n");
			fail_handler(1);
		}

		ctx->fd_out_ec_ek_csr = fopen(argus->out_ec_ek_csr, "wb");
		rc = ca_query_ftpm_prop(FTPM_HELPER_TA_CMD_GET_EC_EK_CSR,
					FTPM_EK_CERT_BUF_SIZE,
					ctx->fd_out_ec_ek_csr);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

	if (FTPM_HELPER_SIGN_EK_CSR & argus->ftpm_helper_options) {
		if (!argus->in_sign_ek_csr || !argus->out_signed_ek_csr) {
			fprintf(stderr, "Error: missing -k or -l option.\n");
			fail_handler(1);
		}

		ctx->fd_in_ek_csr = fopen(argus->in_sign_ek_csr, "rb");
		ctx->fd_out_signed_ek_csr = fopen(argus->out_signed_ek_csr, "wb");
		ca_sign_ek_csr(ctx->fd_in_ek_csr,
				 ctx->fd_out_signed_ek_csr);
	}

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_QUERY_ECID & argus->ftpm_helper_options)
		ca_query_ecid();
#endif

	if (FTPM_HELPER_QUERY_SN & argus->ftpm_helper_options) {
		ca_query_sn(sn);

		for (int i = 0; i < FTPM_HELPER_TA_SN_LENGTH; i++)
			fprintf(stdout, "%02x", sn[i]);
		fprintf(stdout, "\n");
	}

	if (FTPM_HELPER_QUERY_PROV_MODE & argus->ftpm_helper_options) {
		if (ctx->prov_mode == FTPM_PROV_MODE_OFFLINE)
			fprintf(stdout, "offline\n");
		else if (ctx->prov_mode == FTPM_PROV_MODE_ONLINE)
			fprintf(stdout, "online\n");
		else
			fprintf(stdout, "unknown\n");
	}

	if (FTPM_HELPER_QUERY_VERSION & argus->ftpm_helper_options)
		fprintf(stdout, "%d.%d\n", ctx->ver_major, ctx->ver_minor);

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_INJECT_EPS & argus->ftpm_helper_options)
	{
		if (!argus->inject_eps_value)
		{
			fprintf(stderr, "Error: missing -g option.\n");
			fail_handler(1);
		}

		rc = parse_eps_value(argus->inject_eps_value, eps, FTPM_HELPER_TA_EPS_BYTES);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);

		rc = ca_inject_eps(eps, FTPM_HELPER_TA_EPS_BYTES);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}

#ifdef CFG_JETSON_FTPM_HELPER_INJECT_SN
	if (FTPM_HELPER_INJECT_SN & argus->ftpm_helper_options)
	{
		if (!argus->inject_sn_value)
		{
			fprintf(stderr, "Error: missing -C option.\n");
			fail_handler(1);
		}

		rc = parse_sn_value(argus->inject_sn_value, sn, FTPM_HELPER_TA_SN_LENGTH);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);

		rc = ca_inject_sn(sn, FTPM_HELPER_TA_SN_LENGTH);
		if (rc != TEEC_SUCCESS)
			fail_handler(1);
	}
#endif
#endif

	if (FTPM_HELPER_QUERY_ACTIVATION_STATE & argus->ftpm_helper_options)
		fprintf(stdout, "%d\n", ca_query_activation_state());

	if (FTPM_HELPER_START_ACTIVATION & argus->ftpm_helper_options)
		ca_start_activation();

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_SET_ACTIVATION_STATE & argus->ftpm_helper_options)
		ca_set_activation_state();
#endif

	if (FTPM_HELPER_UNSET_ACTIVATION_STATE & argus->ftpm_helper_options)
		ca_unset_activation_state();

	if (FTPM_HELPER_QUERY_ECID_LABEL & argus->ftpm_helper_options) {
		uint8_t *ecid_buf = calloc(1, FTPM_HELPER_TA_ECID_LABEL_LENGTH);
		if (!ecid_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		ca_query_ecid_label(ecid_buf);

		// Print out...
		for (int i = 0; i < FTPM_HELPER_TA_ECID_LABEL_LENGTH; i++)
			fprintf(stdout, "%02x", ecid_buf[i]);
		fprintf(stdout, "\n");

		free(ecid_buf);
	}

#ifdef CFG_SEQ_FTPM_TEST
	if (FTPM_HELPER_QUERY_RSA_EK_CERT & argus->ftpm_helper_options) {
		uint8_t *cert_buf = calloc(1, FTPM_EK_CERT_BUF_SIZE);
		if (!cert_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_rsa_ek_cert(cert_buf, &eks_len);
		// Dump ek cert
		hex_dump("EK cert", cert_buf, eks_len);
		free(cert_buf);
	}

	if (FTPM_HELPER_QUERY_EC_EK_CERT & argus->ftpm_helper_options) {
		uint8_t *cert_buf = calloc(1, FTPM_EK_CERT_BUF_SIZE);
		if (!cert_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_ec_ek_cert(cert_buf, &eks_len);
		// Dump ek cert
		hex_dump("EK cert", cert_buf, eks_len);
		free(cert_buf);
	}

	if (FTPM_HELPER_QUERY_SN_EKS & argus->ftpm_helper_options)
	{
		uint8_t *sn_buf = calloc(1, FTPM_HELPER_TA_SN_LENGTH);
		if (!sn_buf)
		{
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		ca_query_sn_eks(sn_buf);

		free(sn_buf);
	}

	if (FTPM_HELPER_QUERY_EPS_SEED & argus->ftpm_helper_options) {
		ca_query_eps(eps);
	}

	if (FTPM_HELPER_QUERY_COMMS_PRIV_KEY & argus->ftpm_helper_options) {
		uint8_t *priv_key_buf = calloc(1, FTPM_HELPER_TA_COMMS_PRIV_KEY_LENGTH);
		if (!priv_key_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_comms_priv_key(priv_key_buf, &eks_len);

		// Print out...
		printf("Comms private key %s", priv_key_buf);

		free(priv_key_buf);
	}

	if (FTPM_HELPER_QUERY_DEV_CERT & argus->ftpm_helper_options) {
		uint8_t *dev_cert_buf = calloc(1, FTPM_HELPER_TA_DEVICE_CERT_LENGTH);
		if (!dev_cert_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_device_cert(dev_cert_buf, &eks_len);
		printf("Device cert %s", dev_cert_buf);

		free(dev_cert_buf);
	}

	if (FTPM_HELPER_QUERY_CA_CERT & argus->ftpm_helper_options) {
		uint8_t *ca_cert_buf = calloc(1, FTPM_HELPER_TA_CA_CERT_LENGTH);
		if (!ca_cert_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_CA_cert(ca_cert_buf, &eks_len);
		printf("CA cert %s", ca_cert_buf);

		free(ca_cert_buf);
	}

	if (FTPM_HELPER_QUERY_ACT_PUB_KEY & argus->ftpm_helper_options) {
		uint8_t *act_pubkey_buf = calloc(1, FTPM_HELPER_TA_ACT_PUB_KEY_LENGTH);
		if (!act_pubkey_buf) {
			fprintf(stderr, "%s: out of memory.\n", __func__);
			return;
		}

		size_t eks_len = 0;
		ca_query_act_pubkey(act_pubkey_buf, &eks_len);
		hex_dump("Activation pub key", act_pubkey_buf, eks_len);

		free(act_pubkey_buf);
	}
#endif /* CFG_SEQ_FTPM_TEST */

	if (FTPM_HELPER_QUERY_LICENCE_TYPE & argus->ftpm_helper_options) {
		ca_query_licence_type();
	}

}

int main(int argc, char *argv[])
{
	struct arguments argus;

	/* Initialize the arguments */
	memset(&argus, 0, sizeof(struct arguments));

	/* Handle the break signal */
	signal(SIGINT, fail_handler);

	/* Handle the input parameters */
	argp_parse(&argp, argc, argv, 0, 0, &argus);
	ca_sess.argus = &argus;
	if (prepare_tee_session(&ca_sess))
		goto err_out;

	ca_sess.prov_mode = FTPM_PROV_MODE_UNKNOWN;
	ca_query_prov_mode();

	/* Check the device provision mode */
	if (ca_sess.prov_mode == FTPM_PROV_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid provision mode!\n");
		goto err_out;
	}

	/* Check the command inputs */
	if (ca_sess.prov_mode == FTPM_PROV_MODE_OFFLINE &&
	    argus.ftpm_helper_options & INVALID_OPT_FOR_OFFLINE_PROV_MODE) {
		fprintf(stderr, "Invalid command for offline provision mode!\n");
		argp_help(&argp, stderr, 0, NULL);
		goto err_out;
	}

	if (ca_sess.prov_mode == FTPM_PROV_MODE_ONLINE &&
	    argus.ftpm_helper_options & INVALID_OPT_FOR_ONLINE_PROV_MODE) {
		fprintf(stderr, "Invalid command for online provision mode!\n");
		argp_help(&argp, stderr, 0, NULL);
		goto err_out;
	}

	handle_ftpm_helper_options(&ca_sess);

err_out:
	terminate_tee_session(&ca_sess);

	return 0;
}
