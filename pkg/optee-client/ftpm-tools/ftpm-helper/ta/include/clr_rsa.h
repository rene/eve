#ifndef __CLR_RSA_H__
#define __CLR_RSA_H__

/*
 * Sets the attribute buffer value to the appropriate key bytes based on attributeID.
 *
 * - item is of type BITSTRING and the data is SEQUENCE of two INTEGERS for modulus and public exponent
 */
int rsaGetPublicKeyValues( ltc_asn1_list *item, uint8_t **modulus, unsigned long *modlen, uint8_t **exponent, unsigned long *explen );

#endif
