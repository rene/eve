#!/usr/bin/env python3

# Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
import sys
import subprocess
import struct
import shutil
from typing import Optional


SUCCESS = 0
FAILED = 1
output_dir = "oem_out"
output_file_template = output_dir + "/eks_{}.img"
cmd_gen_ekb = "./gen_ekb.py"
# Temp files to store EPS seed,
#   RSA EK certificate and EC EK certificate for offline provision mode, and
#   RSA EK CSR, EK EK CSR, and Silicon ID certificate for online provision mode.
temp_eps_seed = output_dir + "/temp_eps_seed.hex"
temp_rsa_cert = output_dir + "/temp_rsa_cert.der"
temp_ec_cert = output_dir + "/temp_ec_cert.der"
temp_rsa_csr = output_dir + "/temp_rsa_csr.der"
temp_ec_csr = output_dir + "/temp_ec_csr.der"
temp_sid_cert = output_dir + "/temp_sid_cert.der"


class Context:
    def __init__(self):
        self.odm_ekb: str = ""


def initialize(odm_ekb: str) -> Optional[Context]:
    if odm_ekb == None or not os.path.isdir(odm_ekb):
        print("The fTPM ODM EKB directory can't be found.")
        return None
    odm_ekb_path = os.path.abspath(odm_ekb)

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

    context = Context()
    context.odm_ekb = odm_ekb_path
    return context

# Return SN and create 3 temp files
def handle_odm_ekb_file(name: str) -> str:
    result = ""
    file_valid = True
    eps_seed_found = False
    rsa_cert_found = False
    ec_cert_found = False
    sid_cert_found = False
    rsa_csr_found = False
    ec_csr_found = False

    with open(name, 'rb') as file:
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
        tag_rsa_ek_cert = 3
        tag_ec_ek_cert = 4
        tag_sid_cert = 5
        tag_rsa_ek_csr = 6
        tag_ec_ek_csr = 7

        total_len, magic, major, minor = struct.unpack("<I8sHH", file.read(16))
        if magic != magic_id or major != version_major or minor != version_minor:
            print("Invalid ODM EKB file detected. File header is incorrect.")
            return result
        read_len = 12

        while True:
            tag, length = struct.unpack('<II', file.read(8))
            # Check for end-tag
            if tag == 0 and length == 0:
                read_len += 8
                break

            value = file.read(length)
            read_len += 8 + length;
            if tag == tag_sn:
                result = value.hex()
            elif tag == tag_eps_seed:
                with open(temp_eps_seed, "w") as fh_eps_seed:
                    fh_eps_seed.write(value.hex())
                eps_seed_found = True
            elif tag == tag_rsa_ek_cert:
                with open(temp_rsa_cert, "wb") as fh_rsa_cert:
                    fh_rsa_cert.write(value)
                rsa_cert_found = True
            elif tag == tag_ec_ek_cert:
                with open(temp_ec_cert, "wb") as fh_ec_cert:
                    fh_ec_cert.write(value)
                ec_cert_found = True
            elif tag == tag_sid_cert:
                with open(temp_sid_cert, "wb") as fh_sid_cert:
                    fh_sid_cert.write(value)
                sid_cert_found = True
            elif tag == tag_rsa_ek_csr:
                with open(temp_rsa_csr, "wb") as fh_rsa_csr:
                    fh_rsa_csr.write(value)
                rsa_csr_found = True
            elif tag == tag_ec_ek_csr:
                with open(temp_ec_csr, "wb") as fh_ec_csr:
                    fh_ec_csr.write(value)
                ec_csr_found = True
            else:
                print("Invalid ODM EKB file detected. Wrong tag: ", tag)
                file_valid = False
                break

        if read_len != total_len:
            print("Invalid ODM EKB file detected. Wrong size: ", read_len)
            file_valid = False

        if len(result) == 0 or not eps_seed_found:
            print("Invalid ODM EKB file detected. SN or EPS seed doesn't exist.")
            file_valid = False

        if not online_prov:
            if not rsa_cert_found or not ec_cert_found:
                print("Invalid ODM EKB file detected. RSA or EC certificate is missing.")
                file_valid = False
        else:
            if not sid_cert_found or not rsa_csr_found or not ec_csr_found:
                print("Invalid ODM EKB file detected. RSA, EC CSR, or Silicon ID certificate is missing.")
                file_valid = False            

        if file_valid == False:
            result = ""

    return result

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

def finalize():
    if os.path.exists(temp_eps_seed):
        os.remove(temp_eps_seed)
    if os.path.exists(temp_rsa_cert):
        os.remove(temp_rsa_cert)
    if os.path.exists(temp_ec_cert):
        os.remove(temp_ec_cert)

def display_device_sn(sn: str) -> str:
    return "{}-{}".format(sn[:4], sn[4:])

def main() -> int:
    parser = argparse.ArgumentParser(description='The tool used by OEM to generate the EKB images.')
    parser.add_argument('-oem_k1_key', required=False, help="oem_k1 key (32 bytes) file in hex format")
    parser.add_argument('-oem_k2_key', required=False, help="oem_k2 key (32 bytes) file in hex format")
    parser.add_argument('-in_sym_key', required=False, help="32-byte symmetric key file in hex format")
    parser.add_argument('-in_sym_key2', required=False, help="16-byte symmetric key file in hex format")
    parser.add_argument('-in_auth_key', required=False, help="16-byte symmetric key file in hex format")
    parser.add_argument('-in_ftpm_odm_ekb', required=True, help="The directory which saves ODM EKB files")
    parser.add_argument('-in_ftpm_comms_priv_key', required=True, help="TLS private key file in hex format")
    parser.add_argument('-in_ftpm_comms_device_cert', required=True, help="TLS device cert file in hex format")
    parser.add_argument('-in_ftpm_comms_ca_cert', required=True, help="TLS CA cert file in hex format")
    parser.add_argument('-in_ftpm_act_pub_key', required=True, help="Activation public key file in hex format")
    parser.add_argument("-prov_mode", type=str, default="offline", help="The fTPM provisioning mode (offline or online). The default is offline.")

    args = parser.parse_args()
    context = initialize(args.in_ftpm_odm_ekb)
    if context == None:
        return FAILED

    if (args.prov_mode == "online"):
        online_prov = True
    else:
        online_prov = False
   

    for dirpath, _, filenames in os.walk(context.odm_ekb):
        for filename in filenames:
            fn_target = os.path.join(dirpath, filename)
            print("Parsing ODM EKB file: ", fn_target)
            device_sn = handle_odm_ekb_file(fn_target)
            if len(device_sn) == 0:
                continue

            # Generate the final EKB image for this device
            cmd = [cmd_gen_ekb]
            cmd.extend(["-chip", "t234"])
            if args.oem_k1_key != None:
                cmd.extend(["-oem_k1_key", args.oem_k1_key])
            if args.oem_k2_key != None:
                cmd.extend(["-oem_k2_key", args.oem_k2_key])
            if args.in_sym_key != None:
                cmd.extend(["-in_sym_key", args.in_sym_key])
            if args.in_sym_key2 != None:
                cmd.extend(["-in_sym_key2", args.in_sym_key2])
            if args.in_auth_key != None:
                cmd.extend(["-in_auth_key", args.in_auth_key])

            cmd.extend(["-in_ftpm_sn", device_sn])
            cmd.extend(["-in_ftpm_eps_seed", temp_eps_seed])
            if not online_prov:
                cmd.extend(["-in_ftpm_rsa_ek_cert", temp_rsa_cert])
                cmd.extend(["-in_ftpm_ec_ek_cert", temp_ec_cert])
            else:
                cmd.extend(["-in_sid_cert", temp_sid_cert])
                cmd.extend(["-in_ftpm_rsa_ek_csr", temp_rsa_csr])
                cmd.extend(["-in_ftpm_ec_ek_csr", temp_ec_csr])
            cmd.extend(["-out", output_file_template.format(display_device_sn(device_sn))])
            
            cmd.extend(["-in_ftpm_comms_priv_key", args.in_ftpm_comms_priv_key])
            cmd.extend(["-in_ftpm_comms_device_cert", args.in_ftpm_comms_device_cert])
            cmd.extend(["-in_ftpm_comms_ca_cert", args.in_ftpm_comms_ca_cert])
            cmd.extend(["-in_ftpm_act_pub_key", args.in_ftpm_act_pub_key])
            run_command(cmd)

    finalize()
    return SUCCESS


if __name__ == '__main__':
    ret_code = main()
    sys.exit(ret_code)
