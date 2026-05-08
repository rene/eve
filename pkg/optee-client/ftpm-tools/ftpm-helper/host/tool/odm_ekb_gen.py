#!/usr/bin/env python3

# Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import csv
import os
import sys
import subprocess
import struct
import shutil
from typing import Tuple
from typing import Optional
from io import TextIOWrapper
from cryptography.hazmat.primitives import hmac, hashes
from cryptography.hazmat.primitives.kdf import kbkdf
from cryptography.hazmat.primitives.kdf import hkdf
from cryptography.hazmat.backends import default_backend


SUCCESS = 0
FAILED = 1
output_dir = "odm_out"
output_file_template = output_dir + "/ftpm_eks_{}.img"
output_verify_file = output_dir + "/ftpm_keys.txt"
cmd_gen_ek_csr = "./ftpm_manufacturer_gen_ek_csr.sh"
cmd_gen_ek_certs = "./ftpm_manufacturer_ca_simulator.sh"
cmd_openssl = "openssl"
cmd_sign_sid_csr = "./ftpm_manufacturer_ca_sign_sid_csr.sh"
ca_output_dir = "ca_out"
ek_cert_rsa_template = ca_output_dir + "/ek_cert_rsa-{}.der"
ek_cert_ec_template = ca_output_dir + "/ek_cert_ec-{}.der"

sid_cert_template = ca_output_dir + "/sid_cert-{}.der"
ftpm_output_dir = "ftpm_out"
ek_csr_rsa_template = ftpm_output_dir + "/ek_csr_rsa-{}.der"
ek_csr_ec_template = ftpm_output_dir + "/ek_csr_ec-{}.der"
sid_cert_template = ca_output_dir + "/sid_cert-{}.der"
ftpm_output_dir = "ftpm_out"
ek_csr_rsa_template = ftpm_output_dir + "/ek_csr_rsa-{}.der"
ek_csr_ec_template = ftpm_output_dir + "/ek_csr_ec-{}.der"

class Context:
    def __init__(self):
        self.kdk_db: str = ""
        self.verify: bool = False
        self.fh_ftpm_keys: Optional[TextIOWrapper] = None


def encode(data: str, is_hex: bool) -> bytes:
    result = b''
    if is_hex:
        # Convert hex string to bytes
        try:
            result = bytes.fromhex(data)
        except Exception as e:
            print("Converting hex string: ", data, " to bytes failed: ", e)
            result = b''
    else:
        # Encode as ASCII bytes
        try:
            result = data.encode("ascii")
        except Exception as e:
            print("Converting non hex string: ", data, " to bytes failed: ", e)
            result = b''
    return result

def kdf(kdk: bytes, label: bytes, context: bytes, L: int = 256, rlen: int = 32, order = None) -> bytes:
    """Derive KDF from root key and NIST SP800-108 input (label and context)."""
    if not context:
        context = b"\x00"

    if order is None:
        order = kbkdf.CounterLocation.BeforeFixed

    hkdf = kbkdf.KBKDFHMAC(
        algorithm=hashes.SHA256(),
        mode=kbkdf.Mode.CounterMode,
        length=L // 8,
        rlen=rlen // 8,
        llen=4,
        location=order,
        label=label,
        context=context,
        fixed=None,
        backend=default_backend(),
    )
    return hkdf.derive(kdk)

def display_device_sn(sn: bytes) -> str:
    sn_hex = sn.hex()
    return "{}-{}".format(sn_hex[:4], sn_hex[4:])

def convert_sn_endian(sn: bytes) -> bytes:
    # The device SN is 10 bytes, and has the format:
    # <oem_id, 2 bytes><sn, 8 bytes>
    # oem_id is stored in fuse "odm_info"
    # All 4-byte or less Tegra fuses are stored in little-endian so we need
    # to convert the endian of oem_id in device SN before doing the key calculation
    # The left 8 bytes don't have an endian issue - it is big-endian everywhere
    result = bytearray(sn)
    result[0] = sn[1]
    result[1] = sn[0]
    return bytes(result)

def gen_silicon_id(kdk: str, sn: bytes) -> bytes:
    kdk_nrk = kdf(encode(kdk, True), encode("NRK", False), encode("00", True))
    # Only in PSC_BL1, sn is treated as little-endian when calculating the key
    # For all other places that SN involves, we require SN to be big-endian
    silicon_id = kdf(kdk_nrk, encode("ECA_SEED", False), convert_sn_endian(sn))
    return silicon_id

def gen_ftpm_eps_contents(silicon_id: bytes, sn: bytes) -> Tuple[bytes, bytes, bytes]:
    ftpm_seed_origin = kdf(silicon_id, encode("fTPM", False), encode("00", True))
    hm = hmac.HMAC(ftpm_seed_origin, hashes.SHA256(), backend=default_backend())
    hm.update("fTPM".encode(encoding="utf8"))
    ftpm_seed = hm.finalize()

    root_seed_hkdf = hkdf.HKDF(algorithm=hashes.SHA256(), length=64,
                               salt=encode("00", True),
                               info=encode("Root_Seed", False))
    root_seed = root_seed_hkdf.derive(ftpm_seed)

    eps_seed = os.urandom(32)
    eps_hkdf = hkdf.HKDF(algorithm=hashes.SHA256(), length=64, salt=eps_seed, info=sn)
    eps = eps_hkdf.derive(root_seed)
    return eps_seed, ftpm_seed, eps

def generate_ftpm_ekb_contents(sn: bytes, eps_seed: bytes, online_prov: bool) -> bytes:    
    # File format
    # content size | magic id | version major | version minor
    # tag | length | value
    # tag | length | value
    # ......
    # end-tag(\0\0\0\0) | end-tag-len(\0\0\0\0)

    magic_id = b"NVFTPM\0\0"
    version_major = 1
    version_minor = 1
    tag_sn = 1
    tag_eps_seed = 2
    tag_ek_cert_rsa = 3
    tag_ek_cert_ec = 4
    tag_sid_cert = 5
    tag_ek_csr_rsa = 6
    tag_ek_csr_ec = 7

    ek_cert_rsa_file = ek_cert_rsa_template.format(display_device_sn(sn))
    ek_cert_ec_file = ek_cert_ec_template.format(display_device_sn(sn))
    if online_prov:
        sid_cert_file = sid_cert_template.format(display_device_sn(sn))
        ek_csr_rsa_file = ek_csr_rsa_template.format(display_device_sn(sn))
        ek_csr_ec_file = ek_csr_ec_template.format(display_device_sn(sn))

    if not online_prov:
        if not os.path.exists(ek_cert_rsa_file):
            raise Exception("RSA EK certificate can't be found.")
        if not os.path.exists(ek_cert_ec_file):
            raise Exception("EC EK certificate can't be found.")

        with open(ek_cert_rsa_file, "rb") as fd:
            ek_cert_rsa = fd.read()
        with open(ek_cert_ec_file, "rb") as fd:
            ek_cert_ec = fd.read()
    else:
        if not os.path.exists(sid_cert_file):
            raise Exception("Silicon ID certificate can't be found.")
        if not os.path.exists(ek_csr_rsa_file):
            raise Exception("RSA EK CSR can't be found.")
        if not os.path.exists(ek_csr_ec_file):
            raise Exception("EC EK CSR can't be found.")

        with open(sid_cert_file, "rb") as fd:
            sid_cert = fd.read()
        with open(ek_csr_rsa_file, "rb") as fd:
            ek_csr_rsa = fd.read()
        with open(ek_csr_ec_file, "rb") as fd:
            ek_csr_ec = fd.read()

    content_size = 12    # magic ID + version major + version minor
    content_size += 8 + len(sn)
    content_size += 8 + len(eps_seed)
    if not online_prov:
        content_size += 8 + len(ek_cert_rsa)
        content_size += 8 + len(ek_cert_ec)
    else:
        content_size += 8 + len(sid_cert)
        content_size += 8 + len(ek_csr_rsa)
        content_size += 8 + len(ek_csr_ec)
    content_size += 8    # end tag and end tag length

    header_fmt = "<I8sHH"
    entry_fmt = "<II"
    file_blob = struct.pack(header_fmt, content_size, magic_id, version_major, version_minor)
    file_blob += struct.pack(entry_fmt, tag_sn, len(sn)) + sn
    file_blob += struct.pack(entry_fmt, tag_eps_seed, len(eps_seed)) + eps_seed
    if not online_prov:
        file_blob += struct.pack(entry_fmt, tag_ek_cert_rsa, len(ek_cert_rsa)) + ek_cert_rsa
        file_blob += struct.pack(entry_fmt, tag_ek_cert_ec, len(ek_cert_ec)) + ek_cert_ec
    else:
        file_blob += struct.pack(entry_fmt, tag_sid_cert, len(sid_cert)) + sid_cert
        file_blob += struct.pack(entry_fmt, tag_ek_csr_rsa, len(ek_csr_rsa)) + ek_csr_rsa
        file_blob += struct.pack(entry_fmt, tag_ek_csr_ec, len(ek_csr_ec)) + ek_csr_ec
    file_blob += struct.pack("xxxxxxxx")

    return file_blob

def generate_ftpm_keys(sn: bytes, ftpm_seed: bytes, eps: bytes, context: Context) -> bool:
    sn_string = display_device_sn(sn)
    if context.fh_ftpm_keys == None:
        print("Invalid fTPM keys file handle. Device SN: ", sn_string)
        return False

    ek_cert_rsa_file = ek_cert_rsa_template.format(sn_string)
    ek_cert_ec_file = ek_cert_ec_template.format(sn_string)

    cmd = [cmd_openssl]
    cmd.extend(["x509"])
    cmd.extend(["-inform", "der"])
    cmd.extend(["-in", ek_cert_rsa_file])
    cmd.extend(["-pubkey"])
    cmd.extend(["-noout"])
    rsa_ek_pubkey = run_command(cmd)

    cmd = [cmd_openssl]
    cmd.extend(["x509"])
    cmd.extend(["-inform", "der"])
    cmd.extend(["-in", ek_cert_ec_file])
    cmd.extend(["-pubkey"])
    cmd.extend(["-noout"])
    ec_ek_pubkey = run_command(cmd)

    context.fh_ftpm_keys.write("# device_sn\n")
    context.fh_ftpm_keys.write("{}\n".format(sn_string))
    context.fh_ftpm_keys.write("# eps\n")
    context.fh_ftpm_keys.write("{}\n".format(eps.hex()))
    context.fh_ftpm_keys.write("# rsa_ek_pubkey\n")
    context.fh_ftpm_keys.write(rsa_ek_pubkey)
    context.fh_ftpm_keys.write("# ec_ek_pubkey\n")
    context.fh_ftpm_keys.write(ec_ek_pubkey)
    context.fh_ftpm_keys.write("# ftpm_seed\n")
    context.fh_ftpm_keys.write("{}\n".format(ftpm_seed.hex()))
    context.fh_ftpm_keys.write("\n")

    return True

def run_command(cmd) -> str:
    print("Running command: ", " ".join(cmd))

    try:
        # Run the command and capture its output, error, and return code
        result = subprocess.run(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, universal_newlines=True)
        output = result.stdout
        error = result.stderr
        return_code = result.returncode
    except subprocess.CalledProcessError as e:
        print("Run command error: ", e)
        raise e

    if return_code != 0:
        print(output)
        print(error)
        raise ValueError("Running command failed with error: {}".format(return_code))
    return output

def initialize(kdk_db: str, verify: bool) -> Optional[Context]:
    if kdk_db == None or not os.path.isfile(kdk_db):
        print("The KDK db file can't be found.")
        return None
    kdk_db_path = os.path.abspath(kdk_db)

    # Set the working directory
    try:
        wd = os.path.dirname(os.path.abspath(__file__))
        os.chdir(wd)
    except Exception as e:
        print("Changing current working directory failed.")
        print(e)
        return None

    if os.path.exists(output_dir) and not os.path.isdir(output_dir):
        print("Creating the output folder: ", output_dir, " failed. File exists.")
        return None
    if os.path.exists(output_dir):
        print("Cleaning the output directory...")
        try:
            shutil.rmtree(output_dir)
        except Exception as e:
            print("Cleaning the output folder: ", output_dir, " failed.")
            return None

    try:
        os.makedirs(output_dir)
    except Exception as e:
        print("Creating the output folder: ", output_dir, " failed.")
        return None

    fh_ftpm_keys = None
    if verify:
        try:
            fh_ftpm_keys = open(output_verify_file, 'w', newline='')
        except Exception as e:
            print("Creating the verify output file failed: ", e)
            return None

    context = Context()
    context.kdk_db = kdk_db_path
    context.verify = verify
    if verify:
        context.fh_ftpm_keys = fh_ftpm_keys
    return context

def handle_rows_in_kdk_and_sid_csr_db(row_kdk_db, row_sid_csr_db, online_prov: bool, context):
    try:
        oem_id = int(row_kdk_db[0], 16)
        sn = int(row_kdk_db[1], 16)
    except Exception:
        print("Warning: Illegal oem_id(", row_kdk_db[0], ") or sn(", row_kdk_db[1], "), ignored.")
        return

    if oem_id.bit_length() > 16:
        print("Warning: oem_id: ", oem_id, " must be a 16-bit integer, ignored.")
        return
    if sn.bit_length() > 64:
        print("Warning: sn: ", sn, " must be a 64-bit integer, ignored.")
        return

    if online_prov:
        oem_id_from_sid_csr_db = int(row_sid_csr_db[0], 16)
        sn_from_sid_csr_db = int(row_sid_csr_db[1], 16)

        if oem_id != oem_id_from_sid_csr_db or sn != sn_from_sid_csr_db:
            print("Warning: The KDK and Silicon ID CSR database doesn't match.")
            return

        device_sid_csr = row_sid_csr_db[2]

    device_kdk = row_kdk_db[2]
    # device_sn is a combination of oem_id and sn
    device_sn = oem_id.to_bytes(2, "big")
    sn_high = (sn >> 32).to_bytes(4, "big")
    sn_low = (sn & 0xffffffff).to_bytes(4, "big")
    device_sn += sn_high
    device_sn += sn_low

    print("Creating odm EKB outputs for device: ", display_device_sn(device_sn))
    silicon_id = gen_silicon_id(device_kdk, device_sn)
    ftpm_eps_seed, ftpm_eps = gen_ftpm_eps_contents(silicon_id, device_sn)

    try:
        if not online_prov:
            # Generate EK CSRs: ek_csr_rsa-{device_sn}.der and ek_csr_ec-{device_sn}.der
            cmd = [cmd_gen_ek_csr]
            cmd.extend(["-s", display_device_sn(device_sn)])
            cmd.extend(["-e", ftpm_eps.hex()])
            run_command(cmd)

            # Generate EK certificates by signing the CSRs generated above
            cmd = [cmd_gen_ek_certs]
            cmd.extend(["-s", display_device_sn(device_sn)])
            run_command(cmd)
        else:
            # Generate EK CSRs: ek_csr_rsa-{device_sn}.der and ek_csr_ec-{device_sn}.der
            cmd = [cmd_gen_ek_csr]
            cmd.extend(["-s", display_device_sn(device_sn)])
            cmd.extend(["-e", ftpm_eps.hex()])
            run_command(cmd)

            # Sign Silicon ID CSR
            cmd = [cmd_sign_sid_csr]
            cmd.extend(["-i", device_sid_csr])
            cmd.extend(["-s", display_device_sn(device_sn)])
            run_command(cmd)
    except Exception:
        return

    # Generate EKB DB files for per-device basis.
    #   The content of the EKB DB file:
    #     Offline provision mode:
    #       - SN (Serial Number)
    #       - fTPM EPS Seed
    #       - RSA EK Certificate
    #       - EC EK Certificate
    #     Online provision mode:
    #       - SN (Serial Number)
    #       - fTPM EPS Seed
    #       - Silicon ID Certificate
    #       - RSA EK CSR
    #       - EC EK CSR
    #
    # The EKB DB files will be parsed by oem_ekb_gen.py to generate the final EKB images.
    #
    blob = generate_ftpm_ekb_contents(device_sn, ftpm_eps_seed, online_prov)
    fn_ftpm_ekb = output_file_template.format(display_device_sn(device_sn))
    with open(fn_ftpm_ekb, "wb") as fh_ftpm_ekb:
        fh_ftpm_ekb.write(blob)

    if context.verify:
        generate_ftpm_keys(device_sn, ftpm_eps, context)

def main() -> int:
    parser = argparse.ArgumentParser(description='The tool used by ODM to generate fTPM SN, EPS Seed and certificates.')
    parser.add_argument('--kdk_db', help='The csv file which contains the KDK list.')
    parser.add_argument("--prov_mode", type=str, default="offline", help="The fTPM provisioning mode (offline or online). The default is offline.")
    parser.add_argument("--silicon_id_csr_db", help="The csv file of the Silicon ID CSRs that is needed when prov_mode is online.")    
    parser.add_argument("--verify", action="store_const", const=True, help='Save fTPM keys to verify on a Jetson device.')

    args = parser.parse_args()

    if (args.prov_mode == "online" and not args.silicon_id_csr_db):
        parser.print_help()
        return

    context = initialize(args.kdk_db, args.verify)
    if context == None:
        return FAILED

    # fh: file handle, cr: csv reader
    if (args.prov_mode == "online"):
        fh_sid_csr_db = open(args.silicon_id_csr_db, 'r', newline='')
        cr_sid_db = csv.reader(fh_sid_csr_db, delimiter=' ', quotechar='|', quoting=csv.QUOTE_MINIMAL)
    else:
        cr_sid_db = ""
    
    with open(args.kdk_db, mode='r', newline='') as fh_kdk_db:
        cr_kdk_db = csv.reader(fh_kdk_db, delimiter=' ', quotechar='|', quoting=csv.QUOTE_MINIMAL)

        if (args.prov_mode == "online"):
            for row_kdk_db, row_sid_csr_db in zip(cr_kdk_db, cr_sid_db):
                if not handle_rows_in_kdk_and_sid_csr_db(row_kdk_db, row_sid_csr_db, True, context):
                    continue
        else:
            for row_kdk_db in cr_kdk_db:
                if not handle_rows_in_kdk_and_sid_csr_db(row_kdk_db, [], False, context):
                    continue

    if context.fh_ftpm_keys != None:
        context.fh_ftpm_keys.close()
    return SUCCESS


if __name__ == '__main__':
    ret_code = main()
    sys.exit(ret_code)
