#ifndef __CLR_ASN1_H__
#define __CLR_ASN1_H__

#include "tee_api.h"
#include "trace.h"
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>

/*
 * Return codes
 * Used internally in LTC stuff. Mapped as best as possible
 */
#define CLR_ASN1_OK 			 TEE_SUCCESS
#define CLR_ASN1_INVALID_ARG     TEE_ERROR_BAD_PARAMETERS
#define CLR_ASN1_BUFFER_OVERFLOW TEE_ERROR_SHORT_BUFFER
#define CLR_ASN1_INVALID_PACKET  TEE_ERROR_BAD_FORMAT
#define CLR_ASN1_OUT_OF_MEMORY   TEE_ERROR_OUT_OF_MEMORY
#define CLR_ASN1_NOP			 TEE_ERROR_NOT_IMPLEMENTED
#define CLR_ASN1_PK_INVALID_TYPE TEE_ERROR_CORRUPT_OBJECT

/*
 *  Attribute IDs
 *  These will be used to request attributes that are defined in the list
 *  in addition to the attribute IDs in tee_api_defines.h TEE_ATTR_*.
 *
 *  If the TEE is updated then these values need to be checked to make sure they
 *  do not conflict.
 */
//Reference attributes
#define CLR_ASN1_ATTR_CERT_SUBJECT_ID  0x10000000
#define CLR_ASN1_ATTR_CERT_AUTH_KEY_ID 0x10000001
#define CLR_ASN1_ATTR_CERT_SUBJECT     0x10000002
#define CLR_ASN1_ATTR_CERT_ISSUER      0x10000003
#define CLR_ASN1_ATTR_CERT_SIGNATURE   0x10000005
#define CLR_ASN1_ATTR_CERT_START_DATE  0x10000006
#define CLR_ASN1_ATTR_CERT_END_DATE    0x10000007
#define CLR_ASN1_ATTR_CERT_VERSION     0x10000008
#define CLR_ASN1_ATTR_CERT_SERIAL_NUM  0x10000009
#define CLR_ASN1_ATTR_CERT_TBS_CERT	   0x1000000B
#define CLR_ASN1_ATTR_CERT_PUBKEY_BITSTRING 0x1000000C

//Value attributes
#define CLR_ASN1_ATTR_CERT_SIG_ALG_ID  0x20000004
#define CLR_ASN1_ATTR_PUBKEY_TYPE    0x2000000A



/*
 * Key Decoding Functions
 */
/*
 * This function will decode the DER string and then fill the attributes based on the
 * attributeID passed in. If the attribute is a reference and not a value then the memory
 * held in TEE_Attribute.content.ref.buffer must be freed with TEE_Free when it is no
 * longer needed.
 *
   This function will populate the following attributes if available.
   TEE_ATTR_ECC_PUBLIC_VALUE_X
   TEE_ATTR_ECC_PUBLIC_VALUE_Y
   TEE_ATTR_ECC_PRIVATE_VALUE
   TEE_ATTR_ECC_CURVE

    Result: Will return a CLR_ASN1* return value
 */
int clrAsn1EccDecodeKey( TEE_Attribute *attrs, size_t attrlen, const uint8_t *der, size_t derlength );

/*
 * This function will decode the DER string and then fill the attributes based on the
 * attributeID passed in. If the attribute is a reference and not a value then the memory
 * held in TEE_Attribute.content.ref.buffer must be freed with TEE_Free when it is no
 * longer needed.
 *
   This function will populate the following attributes if available.
   TEE_ATTR_RSA_MODULUS
   TEE_ATTR_RSA_PUBLIC_EXPONENT

    Result: Will return a CLR_ASN1* return value
 */
int clrAsn1RsaDecodeKey( TEE_Attribute *attrs, size_t attrlen, const uint8_t *der, size_t derlength );

/*
 * This function will decode the DER string and then fill the attributes based on the
 * attributeID passed in. If the attribute is a reference and not a value then the memory
 * held in TEE_Attribute.content.ref.buffer must be freed with TEE_Free when it is no
 * longer needed.
 *
   This function will populate the following attributes if available.
   TEE_ATTR_DSA_PRIME
   TEE_ATTR_DSA_SUBPRIME
   TEE_ATTR_DSA_BASE
   TEE_ATTR_DSA_PUBLIC_VALUE
   TEE_ATTR_DSA_PRIVATE_VALUE

    Result: Will return a CLR_ASN1* return value
 */
int clrAsn1DsaDecodeKey( TEE_Attribute *attrs, size_t attrlen, const uint8_t *der, size_t derlength );

/*
 * This function will decode the DER string and then fill the attributes based on the
 * attributeID passed in. If the attribute is a reference and not a value then the memory
 * held in TEE_Attribute.content.ref.buffer must be freed with TEE_Free when it is no
 * longer needed.
 *
   This function will populate the following attributes if available.
   TEE_ATTR_SECRET_VALUE

    Result: Will return a CLR_ASN1* return value
 */
int clrAsn1SecretValueDecodeKey( TEE_Attribute *attrs, size_t attrlen, const uint8_t *der, size_t derlength );

/*
 * Parses the DER and determines the key type.
 * This will return TEE_TYPE* in the type parameters.
 *
 * Success or Error in return code
 */
int clrAsn1DecodeKeyType( uint32_t *type, const uint8_t *der, size_t derlength);

/*
 * Cert Decoding Functions
 */
/*
 * This function will decode a DER string representing an x509 v3 certificate and fill the
 * attributes based on the attributeID passed in.
 * If the attribute is a reference and not a value then the memory
 * held in TEE_Attribute.content.ref.buffer must be freed with TEE_Free when it is no
 * longer needed.
 *
 * This function will populate the following certificate attributes if available.
 * All of these will be reference values
 *  CLR_ASN1_ATTR_CERT_SUBJECT_ID
	CLR_ASN1_ATTR_CERT_AUTH_KEY_ID
	CLR_ASN1_ATTR_CERT_SUBJECT
	CLR_ASN1_ATTR_CERT_ISSUER
	CLR_ASN1_ATTR_CERT_SIGNATURE
	CLR_ASN1_ATTR_CERT_START_DATE
	CLR_ASN1_ATTR_CERT_END_DATE
	CLR_ASN1_ATTR_CERT_VERSION
	CLR_ASN1_ATTR_CERT_SERIAL_NUM

 * These will be value attributes
	CLR_ASN1_ATTR_CERT_SIG_ALG_ID
	CLR_ASN1_ATTR_PUBKEY_TYPE
 *
 * This function will populate the public key attributes if available.
   TEE_ATTR_RSA_MODULUS
   TEE_ATTR_RSA_PUBLIC_EXPONENT

   TEE_ATTR_ECC_PUBLIC_VALUE_X
   TEE_ATTR_ECC_PUBLIC_VALUE_Y
   TEE_ATTR_ECC_CURVE
 */
int clrAsn1DecodeCertificate( TEE_Attribute *attrs, size_t attrlen, const uint8_t *der, size_t derlength );

int clrAsn1GetTBSCertificate( uint8_t *derbuffer, size_t derlength, uint8_t **tbs, size_t *tbslen );

///Frees the buffer values in the attribute array
int clrAsn1FreeTeeAttributes( TEE_Attribute *attrs, size_t attrlen);

#endif
