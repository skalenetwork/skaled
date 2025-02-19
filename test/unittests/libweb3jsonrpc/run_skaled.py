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
NUM_REQUIRED_PORTS = 6 if TOTAL_NODES > 1 else 5
BUILD_DIR: str = "cmake-build-debug"
skaled_executable: str = "../../../" + BUILD_DIR + '/skaled/skaled'


class Skaled:
    index: int
    total_nodes: int
    process: subprocess.Popen = None
    stdout_thread: threading.Thread

    def __init__(self, _index: int, _total_nodes: int):
        assert _total_nodes > 0
        assert _index > 0
        self.total_nodes = _total_nodes
        self.index = _index

    def check_if_online(self) -> bool:
        return len(get_listening_ports_by_pid(self.process.pid)) == NUM_REQUIRED_PORTS

    def wait_until_online(self, _timeoutSec: int):
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
            time.sleep(1)
        test_print(f"Timeout reached. Node {self.index} did not get online.")
        assert False

    def terminated_if_not_exited(self, _timeoutSec: int):
        for i in range(_timeoutSec):
            if not self.is_running():
                return
            time.sleep(1)
        test_print(f"Timeout reached. Node {self.index} did not exit. Terminating")
        self.process.kill()

    def graceful_exit(self):
        if self.is_running():
            self.process.terminate()

    def is_running(self) -> bool:
        return self.process.poll() is None

    def kill(self):
        if self.is_running():
            self.process.terminate()

    def print_line(self, _line: str):
        print(f"                 SKALED {self.index}: {_line.strip()}", flush=True)


    def read_stdout(self):
        """Reads lines from the subprocess's stdout and prints them."""
        for line in iter(self.process.stdout.readline, ''):
            if line:
               self.print_line(line)
        self.process.stdout.close()


    def run(self):
        skaled_dir: str = f"/tmp/skaled_{self.index}_of_{self.total_nodes}"
        skaled_path = Path(skaled_dir)

        # Create directory if it doesn't exist
        if not skaled_path.exists():
            skaled_path.mkdir(parents=True, exist_ok=True)
            print("Directory created successfully.")

        shutil.copy2(skaled_executable, skaled_dir + "/skaled")

        configFile: str = f"../../historicstate/configs/test_{self.index}_of_{self.total_nodes}.json"
        self.process = subprocess.Popen([skaled_dir + "/skaled",
                                         '--config', configFile], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        text=True)

        Skaled.stdout_thread = threading.Thread(target=Skaled.read_stdout, args=([self]))
        Skaled.stdout_thread.daemon = True  # Daemonize thread so it exits when main thread exits
        Skaled.stdout_thread.start()

    @staticmethod
    def exit_all(_graceful_timeout_before_kill):
        for skaled in skaleds:
            skaled.graceful_exit()
        for skaled in skaleds:
            skaled.wait_for_exit()


skaleds: list[Skaled] = []


def get_listening_ports_by_pid(pid: int):
    listening_ports = set(
        conn.laddr.port
        for conn in psutil.net_connections(kind='inet')
        if conn.status == psutil.CONN_LISTEN and conn.pid == pid
    )
    return listening_ports


def test_print(_s: str):
    print(f"TEST_SCRIPT:{_s}", flush=True)


def main():
    print("TEST SCRIPT: Starting all nodes ")

    start_chain()

    exit_chain()


def exit_chain():
    test_print("Sending graceful signal to all nodes")
    for i in range(TOTAL_NODES):
        skaled: Skaled = skaleds[i]
        skaled.graceful_exit()
    test_print("Waiting all nodes to exit")
    for i in range(TOTAL_NODES):
        skaled: Skaled = skaleds[i]
        skaled.terminated_if_not_exited(50)
    test_print("Exited. Waiting for processes to stop.")
    for i in range(TOTAL_NODES):
        skaled: Skaled = skaleds[i]
        skaled.process.wait()


def start_chain():
    for i in range(TOTAL_NODES):
        skaled = Skaled(i + 1, TOTAL_NODES)
        skaleds.append(skaled)
        skaled.run()
    test_print("Waiting nodes are online ")
    for i in range(TOTAL_NODES):
        skaled: Skaled = skaleds[i]
        skaled.wait_until_online(100)
    test_print("All nodes online")


if __name__ == "__main__":
    main()
