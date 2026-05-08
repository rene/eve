/*
 * Copyright (c) 2023-2024, NVIDIA Corporation & AFFILIATES. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __FTPM_HELPER_TA_H__
#define __FTPM_HELPER_TA_H__

/*
 * Each trusted app UUID should have a unique UUID that is
 * generated from a UUID generator such as
 * https://www.uuidgenerator.net/
 *
 * UUID : {a6a3a74a-77cb-433a-990c-1dfb8a3fbc4c}
 */
#define FTPM_HELPER_TA_UUID \
	{ 0xa6a3a74a, 0x77cb, 0x433a, \
		{ 0x99, 0x0c, 0x1d, 0xfb, 0x8a, 0x3f, 0xbc, 0x4c} }

#define FTPM_HELPER_TA_ECID_LENGTH	8U
#define FTPM_HELPER_TA_SN_LENGTH	10U
#define FTPM_HELPER_TA_EPS_BITS		512U
#define FTPM_HELPER_TA_EPS_BYTES	(FTPM_HELPER_TA_EPS_BITS / 8U)

#define FTPM_HELPER_TA_ECID_LABEL_LENGTH	16U
#define FTPM_HELPER_TA_COMMS_PRIV_KEY_LENGTH	1024U
#define FTPM_HELPER_TA_DEVICE_CERT_LENGTH	2048U
#define FTPM_HELPER_TA_CA_CERT_LENGTH		4096U
#define FTPM_HELPER_TA_COMMS_BASE_URL_LENGTH    64U
#define FTPM_HELPER_TA_ACT_PUB_KEY_LENGTH	512U
#define FTPM_HELPER_TA_SN_ECID_LABEL_LENGTH	(FTPM_HELPER_TA_SN_LENGTH + FTPM_HELPER_TA_ECID_LABEL_LENGTH)
#define FTPM_HELPER_TA_SIGNATURE_SHA256_LENGTH	32U
#define FTPM_HELPER_TA_SHARED_SECRET_BITS_LENGTH	256U // Bits size

/* Challenge code structure size */
#define FTPM_CHGE_CODE_EPHEMERAL_PUB_KEY_SIZE	64U
#define FTPM_CHGE_CODE_NONCE_ENC_IV_SIZE	12U
#define FTPM_CHGE_CODE_NONCE_CIPHER_TXT_SIZE	32U
#define FTPM_CHGE_CODE_NONCE_AUTH_TAG_SIZE	16U
#define FTPM_CHGE_CODE_FROM_CLOUD_TOTAL_SIZE	(FTPM_CHGE_CODE_NONCE_ENC_IV_SIZE + \
                                             	FTPM_CHGE_CODE_NONCE_CIPHER_TXT_SIZE + \
                                             	FTPM_CHGE_CODE_NONCE_AUTH_TAG_SIZE)

/* Nonce code size */
#define FTPM_NONCE_CODE_SIZE		32U

/* EK Certificate buffer size */
#define FTPM_EK_CERT_BUF_SIZE		2048U
#define FTPM_MB2_EVT_LOG_SIG_SIZE	72U
#define FTPM_TOS_EVT_LOG_SIG_SIZE	70U

/* Event log signature buffer size */
#define FTPM_EVT_LOG_SIG_BUF_SIZE		128U
/* Default EK Certificate buffer size */
#define FTPM_EK_CERT_BUF_SIZE			2048U

/* EK CSR buffer size */
#define FTPM_EK_CSR_BUF_SIZE			2048U

/* fTPM Provisioning mode */
#define FTPM_PROV_MODE_OFFLINE			0xff00000a
#define FTPM_PROV_MODE_ONLINE			0xff00000b
#define FTPM_PROV_MODE_UNKNOWN			0xff00000c

/*
 * FTPM_HELPER_TA_CMD_QUERY_SN - Query the device serial number
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_SN			0xff000001

/*
 * FTPM_HELPER_TA_CMD_QUERY_ECID - Query the device ECID
 * (only available in test build)
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_ECID			0xff000002

/*
 * FTPM_HELPER_TA_CMD_QUERY_PROV_MODE - Query the provisioning mode
 * param[0] out (value) a: The defined value of provisioning scheme.
 * param[1] out (value) a: The version major number.
 * param[2] out (value) a: The version minor number.
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_PROV_MODE	0xff000003

/*
 * FTPM_HELPER_TA_CMD_GET_RSA_EK_CERT - Get the fTPM RSA EK Certificate
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_RSA_EK_CERT	0xff000004

/*
 * FTPM_HELPER_TA_CMD_GET_EC_EK_CERT - Get the fTPM EC EK Certificate
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_EC_EK_CERT	0xff000005

/*
 * FTPM_HELPER_TA_CMD_GET_SID_CERT - Get the Silicon ID Certificate
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_SID_CERT		0xffff0006

/*
 * FTPM_HELPER_TA_CMD_GET_FW_ID_CERT - Get the Firmware ID Certificate
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_FW_ID_CERT	0xffff0007

/*
 * FTPM_HELPER_TA_CMD_GET_RSA_EK_CSR - Get the fTPM RSA EK CSR
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_RSA_EK_CSR	0xffff0008

/*
 * FTPM_HELPER_TA_CMD_GET_EC_EK_CSR - Get the fTPM EC EK CSR
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_EC_EK_CSR	0xffff0009

/*
 * FTPM_HELPER_TA_CMD_SIGN_EK_CSR - Receive the EK CSR and sign it
 * param[0] in/out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_SIGN_EK_CSR		0xffff000a

/*
 * FTPM_HELPER_TA_CMD_INJECT_EPS - Inject an EPS into fTPM
 * param[0] in  (memref) the buffer contains EPS
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_INJECT_EPS		0xff00000b

/*
 * FTPM_HELPER_TA_CMD_QUERY_ACTIVATION_STATE - Query the activation state
 * param[0] out (value) activation state
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_ACTIVATION_STATE	0xff000020

/*
 * FTPM_HELPER_TA_CMD_SET_ACTIVATION_STATE - Set activated the activation state
 * (only available in test build)
 * param[0] out (value) activation state after operation
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_SET_ACTIVATION_STATE		0xff000021

/*
 * FTPM_HELPER_TA_CMD_UNSET_ACTIVATION_STATE - Set inactivated the activation state
 * (only available in test build)
 * param[0] out (value) activation state after operation
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_UNSET_ACTIVATION_STATE	0xff000022

/*
 * FTPM_HELPER_TA_CMD_QUERY_ECID_LABEL - Get ECID_LABEL cryptographically transform the ECID
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_ECID_LABEL		0xff000023

/*
 * FTPM_HELPER_TA_CMD_QUERY_EPS - Query EPS
 * (only available in test build)
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_EPS			0xff000024

/*
 * FTPM_HELPER_TA_CMD_QUERY_COMMS_PRIV_KEY - Query communication device private key
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_COMMS_PRIV_KEY		0xff000025

/*
 * FTPM_HELPER_TA_CMD_QUERY_DEVICE_CERT - Query device cert
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_DEVICE_CERT		0xff000026

/*
 * FTPM_HELPER_TA_CMD_QUERY_CA_CERT - Query CA cert
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_CA_CERT		0xff000027

/*
 * FTPM_HELPER_TA_CMD_QUERY_ACT_PUB_KEY - Query fTPM Activation public key
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_ACT_PUB_KEY		0xff000028

/*
 * FTPM_HELPER_TA_CMD_DECODE_CHALLENGE_CLOUD_BASE64 - Decode CHALLENGE with cloud public key base64
 * param[0] in (memref) the CHALLENGE data buffer and size
 * param[1] out (memref) the NONCE data buffer and size
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_DECODE_CHALLENGE_CLOUD_BASE64	0xff000029

/*
 * FTPM_HELPER_TA_CMD_VERIFY_ACTIVATION_SIGNATURE - Verify signature
 * param[0] in (memref) the signature buffer and size
 * param[1] out (value) the activation state
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_VERIFY_ACTIVATION_SIGNATURE		0xff00002a

/*
 * FTPM_HELPER_TA_CMD_GET_SHARED_SECRET - Get shared secret
 * (only available in test build)
 * param[0] in (memref) the peer pubkey buffer and size
 * param[1] out (memref) the shared secret buffer and size
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_SHARED_SECRET		0xff00002b

/*
 * FTPM_HELPER_TA_CMD_INJECT_SN - Inject an SN into fTPM
 * param[0] in  (memref) the buffer contains SN
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_INJECT_SN		0xff00002c

/*
 * FTPM_HELPER_TA_CMD_QUERY_COMMS_BASE_URL - Query communication base URL
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_COMMS_BASE_URL		0xff00002d

/*
 * FTPM_HELPER_TA_CMD_QUERY_SN_EKS - Query Serial number from EKS image
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_SN_EKS		0xff00002e

/*
 * FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_MB2 - Get the signature of the MB2 event log
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_MB2		0xff00002f

/*
 * FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_TOS - Get the signature of the TOS event log
 * param[0] out (memref) data buffer and size
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_GET_EVT_LOG_SIG_TOS		0xff000030

/*
 * FTPM_HELPER_TA_CMD_QUERY_LICENCE_TYPE - Query FTPM Licence Type
 * param[0] out (value) Integer licence type (a)
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_QUERY_LICENCE_TYPE   0xff000031

/*
 * FTPM_HELPER_TA_CMD_DECRYPT_ENC_MODEL - Decrypt ooid
 * (only available in test build)
 * param[0] in (memref) ooid buffer
 * param[1] out (memref) ooinfo buffer
 * param[2] unused
 * param[3] unused
 */
#define FTPM_HELPER_TA_CMD_DECRYPT_ENC_MODEL		0xff000032



#endif /* __FTPM_HELPER_TA_H__ */
