#ifndef OPAGE_PKG_H
#define OPAGE_PKG_H

#include <tee_internal_api.h>
#include <stdint.h>
#include <stddef.h>

// Opaque Object Context structure
typedef struct OpaqueObjectCtx_t {
    TEE_OperationHandle hCipherOp;
    TEE_OperationHandle hDigestOp;
    TEE_ObjectHandle hKey;
    uint32_t exp_len;
    uint8_t exp_hash[32];
    uint32_t count;
    uint32_t hash_len;
} OpaqueObjectCtx;


TEE_Result decode_opaque_pkg(const uint8_t *pkg, uint32_t pkgLen,
                             uint8_t **payload, uint32_t *payloadLen);

void free_opaque_object_resources(OpaqueObjectCtx *ooctx);

TEE_Result init_opaque_object(uint8_t *info, uint32_t infoLen,
                             OpaqueObjectCtx *ooctx);

TEE_Result decrypt_and_verify_model(OpaqueObjectCtx *ooctx,
                                   uint8_t *enc_model, size_t enc_model_len,
                                   uint8_t *decrypted_model, size_t *decrypted_len);


#endif // OPAGE_PKG_H