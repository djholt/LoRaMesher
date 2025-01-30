#!/usr/bin/env python3

import json
import subprocess
import sys
import threading
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
                print('Warning: node address or name', addr_or_name, 'is not recognized or is not currently available.')

    nodes_to_deploy = sorted(list(nodes_to_deploy))
    return [{ 'addr': a, 'name': addr_to_name_map[a] } for a in nodes_to_deploy]

def build_firmware():
    run_proc = subprocess.Popen('pio run', shell=True)
    out, err = run_proc.communicate()
    return run_proc.returncode == 0

def run_process(node):
    agent = node['name'].lower()
    cmd = f'pio remote -a {agent} run -t upload'
    #cmd = f'pio remote -a {agent} device list'
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    t1 = time.time()
    out, err = p.communicate()
    t2 = time.time()
    node['elapsed'] = t2 - t1
    node['output'] = out
    node['returncode'] = p.returncode

    msg = 'succeeded in' if node['returncode'] == 0 else 'failed after'
    print('Upload to node', node_desc(node), msg, round(node['elapsed'], 1), 'seconds')
    #print(node['output'].decode('utf-8'))

def upload_firmware(nodes):
    threads = [threading.Thread(target=run_process, args=[node]) for node in nodes]
    for t in threads: t.start()
    for t in threads: t.join()

def send_admin_command(op, nodes):
    data = { 'op': op, 'nodeIds': [n['addr'] for n in nodes] }
    data = json.dumps(data).encode('utf-8')
    req = urllib.request.Request(API_ROOT + '/nodes/admin', data=data)
    req.add_header('Content-Type', 'application/json')
    res = urllib.request.urlopen(req)
    return res.status == 200

def node_desc(node):
    return f"{node['name']} [{node['addr']}]"

def main(args):
    if len(args) == 0:
        print('Aborting: must specify node addresses and/or names to deploy, or "all" to deploy all nodes.')
        sys.exit(1)

    addresses = None if args[0].lower() == 'all' else args
    nodes = get_nodes_to_deploy(addresses)
    print('DEPLOYING FIRMWARE TO NODES:', ', '.join([node_desc(node) for node in nodes]))

    print('BUILDING FIRMWARE...')
    if not build_firmware():
        print('Aborting: build failed!')
        sys.exit(1)

    print('CLOSING SERIAL...')
    if not send_admin_command('serial_close', nodes):
        print('Aborting: API request failed!')
        sys.exit(1)

    print('UPLOADING FIRMWARE...')
    upload_firmware(nodes)

    print('OPENING SERIAL...')
    if not send_admin_command('serial_open', nodes):
        print('Aborting: API request failed!')
        sys.exit(1)

    print('DONE!')

if __name__ == '__main__':
    main(sys.argv[1:])
