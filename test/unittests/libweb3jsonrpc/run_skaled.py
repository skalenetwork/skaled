import subprocess
import sys
import shutil
import psutil
from pathlib import Path

import threading
import time

from libconsensus.sgxwallet.rapidjson.thirdparty.gtest.googlemock.test.gmock_test_utils import Subprocess

# Set these
TOTAL_NODES: int = 1
BUILD_DIR: str = "cmake-build-debug"
skaled_executable: str = "../../../" + BUILD_DIR + '/skaled/skaled'

class Skaled:
    index : int
    total_nodes : int
    process : subprocess.Popen
    stdout_thread : threading.Thread
    def __init__(self,  _index: int, _total_nodes: int, _process : subprocess.Popen ):
        assert _total_nodes > 0
        assert _index > 0
        assert _process != None
        self.total_nodes = _total_nodes
        self.index = _index
        self.process = _process

    def check_if_online(self) -> bool:
        return len(get_listening_ports_by_pid(self.process.pid)) == 5


    def wait_until_online(self, _timeoutSec: int) :
        """
            Waits until the instance is online or until the timeout expires.

            Args:
            _timeoutSec (int): The timeout duration in seconds.
        Returns:
            bool: True if the instance is online within the timeout, False otherwise.
    """
        for i in range(_timeoutSec):
            if self.check_if_online():
                return
            print("Waiting for instance to be online Instance is not online.", flush= True)
            time.sleep(1)
        print(f"Timeout reached. Node {self.index} did not get online.", flush= True)
        assert False




    def wait_for_exit(self) :
            self.process.wait()

    def graceful_exit(self):
        if self.process.poll() is None:
            self.process.terminate()
    def kill(self):
        if self.process.poll() is None:
            self.process.terminate()


    @staticmethod
    def exit_all(_graceful_timeout_before_kill):
        for skaled in skaleds:
            skaled.graceful_exit()
        for skaled in skaleds:
            skaled.wait_for_exit()


skaleds : list[Skaled]  = []


def get_listening_ports_by_pid(pid: int):

    listening_ports = set(
        conn.laddr.port
        for conn in psutil.net_connections(kind='inet')
        if conn.status == psutil.CONN_LISTEN and conn.pid == pid
    )
    print(listening_ports, flush = True)
    return listening_ports

def read_stdout(pipe):
    """Reads lines from the subprocess's stdout and prints them."""
    for line in iter(pipe.readline, ''):
        if line:
            print(f"                 SKALED: {line.strip()}", flush=True)
    pipe.close()

def run_skaled(_schain_index: int, _total_nodes: int):
    try:
        print("Starting skaled...")
        skaled_dir : str = f"/tmp/skaled_{_schain_index}_of_{_total_nodes}"
        skaled_path = Path(skaled_dir)


        # Create directory if it doesn't exist
        if not skaled_path.exists():
            skaled_path.mkdir(parents=True, exist_ok=True)
            print("Directory created successfully.")

        shutil.copy2(skaled_executable, skaled_dir + "/skaled")


        configFile: str = f"../../historicstate/configs/test_{_schain_index}_of_{_total_nodes}.json"
        process = subprocess.Popen([skaled_dir + "/skaled",
                                    '--config', configFile], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text = True)
        skaled = Skaled( _schain_index, _total_nodes, process);
        skaleds.append(skaled)

        Skaled.stdout_thread = threading.Thread(target=read_stdout, args=(process.stdout,))
        Skaled.stdout_thread.daemon = True  # Daemonize thread so it exits when main thread exits
        Skaled.stdout_thread.start()

    except Exception as e:
        print(f"Exception occurred: {e}")



def main():
    for i in range(TOTAL_NODES):
        run_skaled(i + 1, TOTAL_NODES)


    print("TEST SCRIPT: Waiting until all nodes in the chain are online ")
    for i in range(TOTAL_NODES):
        skaled : Skaled = skaleds[i]
        skaled.wait_until_online(20)
    print("TEST SCRIPT: All nodes are online ...")

    for i in range(TOTAL_NODES):
        skaled : Skaled = skaleds[i]
        skaled.graceful_exit()

    for i in range(TOTAL_NODES):
        skaled : Skaled = skaleds[i]
        skaled.process.wait()


if __name__ == "__main__":
    main()
