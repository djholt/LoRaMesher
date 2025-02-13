#!/usr/bin/env python3

import base64
import io
import json
import os
import subprocess
import sys
import tarfile
import time
import urllib.request

API_ROOT = 'https://mesh.holt.dj'

def get_nodes_to_deploy(addresses_and_or_names=None):
    with urllib.request.urlopen(API_ROOT + '/nodes') as body:
        available_nodes = json.load(body)
    with urllib.request.urlopen(API_ROOT + '/nodemap.json') as body:
        addr_to_name_map = json.load(body)
        name_to_addr_map = dict((v,k) for k,v in addr_to_name_map.items())

    nodes_to_deploy = set()
    if addresses_and_or_names == None:
        nodes_to_deploy.update(available_nodes)
    else:
        for addr_or_name in addresses_and_or_names:
            addr_or_name = addr_or_name.upper()
            if addr_or_name in available_nodes:
                nodes_to_deploy.add(addr_or_name)
            elif addr_or_name in name_to_addr_map and name_to_addr_map[addr_or_name] in available_nodes:
                nodes_to_deploy.add(name_to_addr_map[addr_or_name])
            else:
                print(f'Aborting: node address or name {addr_or_name} is not recognized or is not currently available.')
                sys.exit(1)

    nodes_to_deploy = sorted(list(nodes_to_deploy))
    return [{ 'addr': a, 'name': addr_to_name_map[a] if a in addr_to_name_map else '?' } for a in nodes_to_deploy]

def build_firmware(build_dir):
    run_proc = subprocess.Popen('pio run', shell=True, cwd=build_dir)
    out, err = run_proc.communicate()
    return run_proc.returncode == 0

def upload_firmware(build_dir, nodes):
    script_dir = os.path.dirname(os.path.realpath(__file__))
    bins = [
        f'{script_dir}/Makefile',
        f'{build_dir}/.pio/build/heltec_wifi_lora_32_V3/bootloader.bin',
        f'{build_dir}/.pio/build/heltec_wifi_lora_32_V3/firmware.bin',
        f'{build_dir}/.pio/build/heltec_wifi_lora_32_V3/partitions.bin',
        '~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',
    ]
    file_paths = [os.path.realpath(os.path.expanduser(b)) for b in bins]

    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w:gz') as ball:
        for path in file_paths:
            if not os.path.exists(path):
                print(f'Aborting: {path} does not exist!')
                sys.exit(1)
            print(f'Uploading: {path}')
            ball.add(path, arcname=os.path.basename(path))
    payload = base64.b64encode(buffer.getbuffer()).decode('utf-8')
    send_admin_command('upload', nodes, data_payload=payload)

def send_admin_command(op, nodes, data_payload=None):
    data = { 'op': op, 'nodeIds': [n['addr'] for n in nodes] }
    if data_payload:
        data['data'] = data_payload
    data = json.dumps(data).encode('utf-8')
    req = urllib.request.Request(API_ROOT + '/nodes/admin', data=data)
    req.add_header('Content-Type', 'application/json')
    res = urllib.request.urlopen(req)
    return res.status == 200

def node_desc(node):
    return f"{node['name']} [{node['addr']}]"

def main(args):
    if len(args) < 2:
        print(f'usage: {os.path.basename(__file__)} build_dir all')
        print(f'       {os.path.basename(__file__)} build_dir node_addr_or_name ...')
        sys.exit(1)

    build_dir = args[0]
    if not os.path.exists(build_dir):
        print(f'Aborting: build_dir {build_dir} does not exist!')
        sys.exit(1)

    addresses = None if args[1].lower() == 'all' else args[1:]
    nodes = get_nodes_to_deploy(addresses)
    print('DEPLOY FIRMWARE TO NODES:', ', '.join([node_desc(node) for node in nodes]))

    if input('Proceed? (y/n) ').lower() != 'y':
        sys.exit(1)

    print('BUILDING FIRMWARE...')
    if not build_firmware(build_dir):
        print('Aborting: build failed!')
        sys.exit(1)

    print('UPLOADING FIRMWARE...')
    upload_firmware(build_dir, nodes)
    print('DONE!')

if __name__ == '__main__':
    main(sys.argv[1:])
