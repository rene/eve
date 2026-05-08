#ifndef __CLR_DER_H__
#define __CLR_DER_H__

#include "tee_api.h"
#include "trace.h"
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>
#include "clr_asn1.h"

#define XMALLOC malloc
#define XREALLOC realloc
#define XCALLOC calloc
#define XFREE free
#define XMEMCPY memcpy
#define XMEMCMP memcmp

#define CLR_ASN1_MAX_INT_SIZE 4096
#define LTC_DER_MAX_PUBKEY_SIZE 1024

enum {
   PK_PUBLIC=0,
   PK_PRIVATE=1
};

#define PK_STD          0x1000
typedef enum {
   PKA_RSA,
   PKA_DSA,
   PKA_ECC
} ASN1_PK_TYPE;

typedef struct asn1OID_t {
    unsigned long OID[16];
    /** Length of DER encoding */
    unsigned long OIDlen;
} asn1OID;

/*
 * Public Key Object Identifiers
 */
extern const asn1OID RSA_PUBKEY_OID;
extern const asn1OID DSA_PUBKEY_OID;
extern const asn1OID ECC_PUBKEY_OID;

/*
 * ECDSA Signature Object Identifiers
 */
extern const asn1OID ECDSA_SHA1_OID;
extern const asn1OID ECDSA_SHA224_OID;
extern const asn1OID ECDSA_SHA256_OID;
extern const asn1OID ECDSA_SHA384_OID;
extern const asn1OID ECDSA_SHA512_OID;

/*
 * Subject line Object Identifiers
 */
extern const asn1OID SUBJ_COMMON_NAME_OID;

#define ASN1_EXT_SUBJ_ID 14
#define ASN1_EXT_AUTH_KEY_ID 35

/* DER handling */
typedef enum ltc_asn1_type_ {
 /*  0 */
 LTC_ASN1_EOL,
 LTC_ASN1_BOOLEAN,
 LTC_ASN1_INTEGER,
 LTC_ASN1_SHORT_INTEGER,
 LTC_ASN1_BIT_STRING,
 /*  5 */
 LTC_ASN1_OCTET_STRING,
 LTC_ASN1_NULL,
 LTC_ASN1_OBJECT_IDENTIFIER,
 LTC_ASN1_IA5_STRING,
 LTC_ASN1_PRINTABLE_STRING,
 /* 10 */
 LTC_ASN1_UTF8_STRING,
 LTC_ASN1_UTCTIME,
 LTC_ASN1_CHOICE,
 LTC_ASN1_SEQUENCE,
 LTC_ASN1_SET,
 /* 15 */
 LTC_ASN1_SETOF,
 LTC_ASN1_RAW_BIT_STRING,
 LTC_ASN1_TELETEX_STRING,
 LTC_ASN1_CONSTRUCTED,
 LTC_ASN1_CONTEXT_SPECIFIC,
} ltc_asn1_type;
extern const char *LTC_NAME_LIST[];

/** A LTC ASN.1 list type */
typedef struct ltc_asn1_list_ {
   /** The LTC ASN.1 enumerated type identifier */
   ltc_asn1_type type;
   /** The data to encode or place for decoding */
   void         *data;
   /** The size of the input or resulting output */
   unsigned long size;
   /** The used flag, this is used by the CHOICE ASN.1 type to indicate which choice was made */
   int           used;
   /** prev/next entry in the list */
   struct ltc_asn1_list_ *prev, *next, *child, *parent;
} ltc_asn1_list;

#define LTC_SET_ASN1(list, index, Type, Data, Size)  \
   do {                                              \
      int LTC_MACRO_temp            = (index);       \
      ltc_asn1_list *LTC_MACRO_list = (list);        \
      LTC_MACRO_list[LTC_MACRO_temp].type = (Type);  \
      LTC_MACRO_list[LTC_MACRO_temp].data = (void*)(Data);  \
      LTC_MACRO_list[LTC_MACRO_temp].size = (Size);  \
      LTC_MACRO_list[LTC_MACRO_temp].used = 0;       \
   } while (0)

/*Utility functions*/
void asn1OutputList( ltc_asn1_list *list, int depth );
void asn1OutputData( char *data, unsigned long len);
void asn1ConvertBitStringToBytes( const unsigned char* bitstr, unsigned long len, unsigned char *outstr, unsigned long *outsize);
ltc_asn1_list *asn1GetItemAtIndex( ltc_asn1_list *list, int index, int followchild );

//Returns TEE_ALG* value
int asn1GetSigAlgFromOID( unsigned long* data, unsigned long len );
int asn1GetHashAlgFromOID( unsigned long* data, unsigned long len );

//Returns OID as reference based on public key type
int asn1GetOIDForKeyType( ASN1_PK_TYPE key, asn1OID *oid );

//Subject and issuer string in OpenSSL Format
int asn1GetAttributeStringFromList( ltc_asn1_list *list, char** data, size_t *len);

//Check string type and return as char
int asn1GetStringFromItem( ltc_asn1_list *item, char **strbuffer, size_t *strsize);

//Go through extensions and get the data for extension id
int asn1GetDataForExtension( ltc_asn1_list *extensions, unsigned long id, char **data, size_t *datalen);

uint32_t asn1GetKeyType( ltc_asn1_list *keylist );
uint32_t asn1GetKeyTypeFromOID( asn1OID *oid );


/* SEQUENCE */
int der_encode_sequence_ex(ltc_asn1_list *list, unsigned long inlen,
                           unsigned char *out,  unsigned long *outlen, int type_of);

#define der_encode_sequence(list, inlen, out, outlen) der_encode_sequence_ex(list, inlen, out, outlen, LTC_ASN1_SEQUENCE)

int der_decode_sequence_ex(const unsigned char *in, unsigned long  inlen,
                           ltc_asn1_list *list,     unsigned long  outlen, int ordered);

#define der_decode_sequence(in, inlen, list, outlen) der_decode_sequence_ex(in, inlen, list, outlen, 1)

int der_length_sequence(ltc_asn1_list *list, unsigned long inlen,
                        unsigned long *outlen);

/* SUBJECT PUBLIC KEY INFO */
int der_encode_subject_public_key_info(unsigned char *out, unsigned long *outlen,
        unsigned int algorithm, void* public_key, unsigned long public_key_len,
        unsigned long parameters_type, void* parameters, unsigned long parameters_len);

int der_decode_subject_public_key_info(const unsigned char *in, unsigned long inlen,
        unsigned int algorithm, void* public_key, unsigned long* public_key_len,
        unsigned long parameters_type, ltc_asn1_list* parameters, unsigned long parameters_len);

/* SET */
#define der_decode_set(in, inlen, list, outlen) der_decode_sequence_ex(in, inlen, list, outlen, 0)
#define der_length_set der_length_sequence
int der_encode_set(ltc_asn1_list *list, unsigned long inlen,
                   unsigned char *out,  unsigned long *outlen);

int der_encode_setof(ltc_asn1_list *list, unsigned long inlen,
                     unsigned char *out,  unsigned long *outlen);

/* VA list handy helpers with triplets of <type, size, data> */
int der_encode_sequence_multi(unsigned char *out, unsigned long *outlen, ...);
int der_decode_sequence_multi(const unsigned char *in, unsigned long inlen, ...);

/* FLEXI DECODER handle unknown list decoder */
int  der_decode_sequence_flexi(const unsigned char *in, unsigned long *inlen, ltc_asn1_list **out);
#define der_free_sequence_flexi         der_sequence_free
void der_sequence_free(ltc_asn1_list *in);

/* BOOLEAN */
int der_length_boolean(unsigned long *outlen);
int der_encode_boolean(int in,
                       unsigned char *out, unsigned long *outlen);
int der_decode_boolean(const unsigned char *in, unsigned long inlen,
                                       int *out);
/* INTEGER */
#ifdef CLR_USE_OCTET_INT
int der_encode_integer(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_decode_integer(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_length_integer(unsigned long noctets, unsigned long *outlen);
#else
int der_encode_integer(void *num, unsigned char *out, unsigned long *outlen);
int der_decode_integer(const unsigned char *in, unsigned long inlen, void *num);
int der_length_integer(void *num, unsigned long *len);
#endif

/* INTEGER -- handy for 0..2^32-1 values */
int der_decode_short_integer(const unsigned char *in, unsigned long inlen, unsigned long *num);
int der_encode_short_integer(unsigned long num, unsigned char *out, unsigned long *outlen);
int der_length_short_integer(unsigned long num, unsigned long *outlen);

/* BIT STRING */
int der_encode_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_encode_raw_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_raw_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_bit_string(unsigned long nbits, unsigned long *outlen);

/* OCTET STRING */
int der_encode_octet_string(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_decode_octet_string(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_length_octet_string(unsigned long noctets, unsigned long *outlen);

/* OBJECT IDENTIFIER */
int der_encode_object_identifier(unsigned long *words, unsigned long  nwords,
                                 unsigned char *out,   unsigned long *outlen);
int der_decode_object_identifier(const unsigned char *in,    unsigned long  inlen,
                                       unsigned long *words, unsigned long *outlen);
int der_length_object_identifier(unsigned long *words, unsigned long nwords, unsigned long *outlen);
unsigned long der_object_identifier_bits(unsigned long x);

/* IA5 STRING */
int der_encode_ia5_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_ia5_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_ia5_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

int der_ia5_char_encode(int c);
int der_ia5_value_decode(int v);

/* TELETEX STRING */
int der_decode_teletex_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_teletex_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

int der_teletex_char_encode(int c);
int der_teletex_value_decode(int v);

/* PRINTABLE STRING */
int der_encode_printable_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_printable_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_printable_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

int der_printable_char_encode(int c);
int der_printable_value_decode(int v);

/* UTF-8 */
#if (defined(SIZE_MAX) || __STDC_VERSION__ >= 199901L || defined(WCHAR_MAX) || defined(_WCHAR_T) || defined(_WCHAR_T_DEFINED) || defined (__WCHAR_TYPE__)) && !defined(LTC_NO_WCHAR)
#include <wchar.h>
#else
typedef ulong32 wchar_t;
#endif

int der_encode_utf8_string(const wchar_t *in,  unsigned long inlen,
                           unsigned char *out, unsigned long *outlen);

int der_decode_utf8_string(const unsigned char *in,  unsigned long inlen,
                                       wchar_t *out, unsigned long *outlen);
unsigned long der_utf8_charsize(const wchar_t c);
int der_length_utf8_string(const wchar_t *in, unsigned long noctets, unsigned long *outlen);


/* CHOICE */
int der_decode_choice(const unsigned char *in,   unsigned long *inlen,
                            ltc_asn1_list *list, unsigned long  outlen);

/* UTCTime */
typedef struct {
   unsigned YY, /* year */
            MM, /* month */
            DD, /* day */
            hh, /* hour */
            mm, /* minute */
            ss, /* second */
            off_dir, /* timezone offset direction 0 == +, 1 == - */
            off_hh, /* timezone offset hours */
            off_mm; /* timezone offset minutes */
} ltc_utctime;

int der_encode_utctime(ltc_utctime *utctime,
                       unsigned char *out,   unsigned long *outlen);

int der_decode_utctime(const unsigned char *in, unsigned long *inlen,
                             ltc_utctime   *out);

int der_length_utctime(ltc_utctime *utctime, unsigned long *outlen);

#define LTC_ARGCHK(x) if (!(x)) { DMSG("\nwarning: ARGCHK failed at %s:%d\n", __FILE__, __LINE__); }

#endif
