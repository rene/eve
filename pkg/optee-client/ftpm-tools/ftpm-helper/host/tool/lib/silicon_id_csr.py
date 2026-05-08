# Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

from asn1crypto import csr, keys, x509
from oscrypto import asymmetric

class Device_Gen_Silicon_ID_CSR():
    def __init__(self, common_name, org_name, country_name, sid_priv_key_der, sid_pub_key_der):
        # setup subject info
        self.cn = common_name
        self.org_name = org_name
        self.Cn = country_name

        # Load Silicon ID key pair
        self.sid_priv_key = asymmetric.load_private_key(sid_priv_key_der)
        self.sid_pub_key_raw = keys.PublicKeyInfo.load(sid_pub_key_der)

    def build_csr(self):
        _csr_subject = x509.Name.build(
            {
                "common_name" : self.cn,
                "organization_name" : self.org_name,
                "country_name" : self.Cn,
            }
        )

        _csr_pub_key_info = self.sid_pub_key_raw

        _csr_extensions = []
        _csr_extensions.extend([
            {
                "extn_id" : "basic_constraints",
                "critical" : True,
                "extn_value" : { "ca" : True, "path_len_constraint" : 0 },
            },
            {
                "extn_id" : "key_usage",
                "critical" : True,
                "extn_value" : x509.KeyUsage(set(['key_cert_sign'])),
            },
            {
                "extn_id" : "key_identifier",
                "extn_value" : _csr_pub_key_info.sha1,
            }
        ])

        _csr_attributes = []
        _csr_attributes.append(
            {
                "type" : "extension_request",
                "values" : [_csr_extensions],
            }
        )

        _csr_info = csr.CertificationRequestInfo(
            {
                "version" : "v1",
                "subject" : _csr_subject,
                "subject_pk_info" : _csr_pub_key_info,
                "attributes" : _csr_attributes,
            }
        )

        # Signing the CSR
        if self.sid_priv_key.algorithm == 'rsa':
            _sig_algo = "sha256_rsa"
            sign_func = asymmetric.rsa_pkcs1v15_sign
        elif self.sid_priv_key.algorithm == 'ec':
            _sig_algo = "sha256_ecdsa"
            sign_func = asymmetric.ecdsa_sign
        else:
            raise Exception("Error: The Silicon ID private key is invalid.")

        _signature = sign_func(self.sid_priv_key, _csr_info.dump(), "sha256")

        self.csr = csr.CertificationRequest(
            {
                "certification_request_info" : _csr_info,
                "signature_algorithm" : { "algorithm" : _sig_algo },
                "signature" : _signature,
            }
        )

        csr_bin = bytearray(self.csr.dump())

        return csr_bin.hex()
