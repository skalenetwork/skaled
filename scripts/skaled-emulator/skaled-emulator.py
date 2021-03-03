#!/usr/bin/python3
import os
import sys
import glob
import argparse
import time
import signal

from http.server import HTTPServer, BaseHTTPRequestHandler
import json

import random

def check_dir(path):
    print(f"Checking {path}:")
    try:
        print(f"    exists: {os.path.isdir(path)}")
        print(f"    contains: {len(os.listdir(path))}")
    except:
        pass

def fail(text):
    print("FAILURE: " + text)
    sys.exit(0)

def handler(signum, dummy):
    print(f"Got signal {signum}")
    if error_terminating > 0:
        print(f"Hanging for 40s and exit {error_terminating}")
        time.sleep(4)
        sys.exit(error_terminating)
    else:
        print("Finishing successfully")
        sys.exit(0)

signal.signal(signal.SIGPIPE, signal.SIG_IGN)
signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGINT, handler)
signal.signal(signal.SIGHUP, handler)

print("I'm skaled")
print("Called as: " + " ".join(sys.argv))

parser = argparse.ArgumentParser(description='Emulator script for runtime parameters/exit codes of skaled')
parser.add_argument('-d', '--db-path', metavar='<data_dir>', type=str,
                    help='data dir for skaled')
parser.add_argument('--config', metavar='<config_path>', type=str,
                    help='path to config json')
parser.add_argument('--http-port', metavar='<http_port>', type=int,
                    help='HTTP port')
parser.add_argument('--download-snapshot', metavar='<unused>', type=str,
                    help='start from remote snapshot')

args, unknown_args = parser.parse_known_args()

print(f"--db-path -> {args.db_path}")
print(f"--config -> {args.config}")

print(f"NO_ULIMIT_CHECK={os.environ.get('NO_ULIMIT_CHECK', '')}")
print(f"DATA_DIR={os.environ.get('DATA_DIR', '')}")

data_dir = ''

if args.db_path:
    data_dir = args.db_path
else:
    data_dir = os.path.expanduser("~/.ethereum")
    
check_dir(data_dir)

DATA_DIR=''
if 'DATA_DIR' in os.environ:
    DATA_DIR=os.environ.get('DATA_DIR')
    check_dir(DATA_DIR)
else:
    DATA_DIR="/tmp"
    print(f"/tmp/*.db: {len(glob.glob('/tmp/*.db'))}")

listen_port = args.http_port

########################## start work #########################################

# randomly select failure type

error_download = 0
error_working = 0
error_terminating = 0

corrupted_db = False

download_duration = 0
work_duration = 0

if args.download_snapshot:
    r = random.randint(1, 5)
else:
    r = random.randint(2, 4)

if   r == 1:
    error_download = 1
elif r == 2:
    error_working = 1
elif r == 3:
    error_working = 200
    corrupted_db = True
#elif r == 4:
#    error_workig = 1
#    corrupted_db = True
else:
    pass

r = random.randint(0, 2)
if r == 0:
    error_terminating = 14

print("\nFailure configuration:")
print(f"error_download = {error_download}")
print(f"error_working = {error_working}")
print(f"corrupted_db = {corrupted_db}")
print(f"error_terminating = {error_terminating}")
print(f"download_duration = {download_duration}")
print(f"work_duration = {work_duration}")

##############################


print("\nEmulating work")

if args.download_snapshot:
    print("Emulating snapshot download")
    
    if not os.path.isdir(data_dir):
        fail(f"{data_dir} absent")
    if len(os.listdir(data_dir)) > 0:
        print(f"WARNING unclean data_dir {data_dir}")
        if os.path.isfile(f"{data_dir}/corrupted_snapshot.txt"):
            os.unlink(f"{data_dir}/corrupted_snapshot.txt")

    if len(glob.glob(f"{DATA_DIR}/*.db")) > 0:
        fail(f"found existing {DATA_DIR}/*.db")

    if download_duration > 0:
        print("Emulating long process")
        time.sleep(download_duration)   
        
    try:
        if error_download > 0:
            print("Will emulate corrupted download")
            with open(data_dir+"/corrupted_snapshot.txt", "w") as f:
                f.write("I'm a corrupted snapshot")
            print(f"Exiting {error_download}")
            sys.exit(error_download)
        else:
            with open(data_dir+"/snapshot.txt", "w") as f:
                f.write("I'm a snapshot")
    except Exception as ex:
        fail(f"cannot write to {data_dir}: " + str(ex))

if not os.path.isdir(data_dir):
    fail(f"data_dir: {data_dir} does not exist")

if os.path.isfile(f"{data_dir}/corrupted_snapshot.txt"):
    fail(f"started with corrupted downloaded snapshot in data_dir: {data_dir}")
if os.path.isfile(f"{data_dir}/corrupted_db.txt"):
    with open(f"{data_dir}/corrupted_db.txt", "r") as f:
        r = int(f.readline())
    print(f"Started with corrupted data_dir: {data_dir}, exiting {r}")
    sys.exit(r)

############################## check config ################################### 

try:
    with open(args.config, "r") as c:
        config = json.load(c)
except Exception as e:
    fail(f"cannot read config file {args.config}: {str(e)}")

########################## simulate short work ################################

with open(data_dir+"/working.txt", "w") as f:
    f.write(str("working ok"))

if work_duration > 0:
    print("Emulating long process")
    time.sleep(work_duration)

if corrupted_db:
    print(f"Emulating data_dir corruption")
    with open(data_dir+"/corrupted_db.txt", "w") as f:
        if error_working != 0:
            f.write(str(error_working))
        else:
            f.write("1")

if error_working > 0:
    print(f"Emulating failure {error_working}")
    sys.exit(error_working)

######################### JSON-RPC stuff ######################################

start_time = int(time.time())

if not listen_port:
    print("Exiting normally (no listening)")
    exit(0)

print("Starting listening")

class RpcHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        request_length = int(self.headers['Content-Length'])
        request_content = self.rfile.read(request_length)
        #print(request_content)
        
        try:
            req_json = json.loads(request_content)
            method = req_json["method"]
            id     = req_json["id"]
        except:
            self.send_response(400)
            error_msg = "Bad JSON-RPC request".encode('utf-8')
            self.send_header("Content-type", "text/plain")
            self.send_header("Content-length", len(error_msg))
            self.addCORS()
            self.end_headers()
            self.wfile.write(error_msg)
            return

        resp = ""
        if method == "rpc_modules":
            resp = {
                'id': id,
                'jsonrpc': "2.0",
                'result':{
                    "eth":"1.0",
                    "net":"1.0",
                    "skale":"0.1",
                    "web3":"1.0"
                }
            }

        elif method == "eth_blockNumber":
            resp = {
                'id': id,
                'jsonrpc': "2.0",
                'result':int(time.time()) - start_time
            }
        
        elif method == "eth_getBlockByNumber":
            number = int(req_json["params"][0], 16)
            resp = {
                'id': id,
                'jsonrpc': "2.0",
                'result':{
                      'difficulty': 0,
                      'gasLimit': 0,
                      'gasUsed': 0,
                      'number': number,
                      'size': 0,
                      'timestamp': number+start_time,
                      'totalDifficulty': 0
                }
            }

        if not resp:
            self.send_response(400)
            error_msg = "Unsupported method".encode('utf-8')
            self.send_header("Content-type", "text/plain")
            self.send_header("Content-length", len(error_msg))
            self.addCORS()
            self.end_headers()
            self.wfile.write(error_msg)
            return

        resp_str = json.dumps(resp)
        resp_bytes = resp_str.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.send_header("Content-length", len(resp_bytes))
        self.addCORS()
        self.end_headers()
        self.wfile.write(resp_bytes)
            

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.addCORS()
        self.end_headers()

    def addCORS(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "content-type")

server = HTTPServer(('', listen_port), RpcHandler)
server.serve_forever()