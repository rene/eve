#ifndef _clr_asn1_ecc_h__
#define _clr_asn1_ecc_h__

/*
 * Initializes the value of the attribute passed in the the ECC curve
 * defined by the OID in item.
 * - item is of type OBJECT_IDENTIFIER and the data is an array of unsigned longs defining the OID.
 */
int eccGetCurveFromOID( TEE_Attribute *attr, ltc_asn1_list *item );

/*
 * Sets the attribute buffer value to the appropriate key bytes based on attributeID.
 *
 * - item is of type BITSTRING and the data is concatenated 04+X+Y bits.
 */
int eccGetPublicKeyValues( TEE_Attribute *attr, ltc_asn1_list *item );

#endif
