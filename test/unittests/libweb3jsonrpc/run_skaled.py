import subprocess
import sys
import shutil
from pathlib import Path

import time

# Set these
TOTAL_NODES: int = 1
BUILD_DIR: str = "cmake-build-debug"
skaled_executable: str = "../../../" + BUILD_DIR + '/skaled/skaled'

class Skaled:
    index : int
    total_nodes : int
    process : subprocess.Popen
    def __init__(self,  _index: int, _total_nodes: int, _process : subprocess.Popen ):
        assert (_total_nodes > 0)
        assert (_index > 0)
        assert (_process != None)
        self.total_nodes = _total_nodes
        self.index = _index
        self.process = _process

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
                                    '--config', configFile], stdout=sys.stdout, stderr=sys.stderr)
        skaled = Skaled( _schain_index, _total_nodes, process);
        skaleds.append(skaled)

    except Exception as e:
        print(f"Exception occurred: {e}")
    finally:
        print("skaled crashed or terminated.")



def main():
    for i in range(TOTAL_NODES):
        run_skaled(i + 1, TOTAL_NODES)

    time.sleep(20)

    for i in range(TOTAL_NODES):
        skaled : Skaled = skaleds[i]
        skaled.graceful_exit()

    for i in range(TOTAL_NODES):
        skaled : Skaled = skaleds[i]
        skaled.process.wait()


if __name__ == "__main__":
    main()
