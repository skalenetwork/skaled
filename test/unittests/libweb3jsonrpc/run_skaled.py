import subprocess
import sys
import shutil
from pathlib import Path

import time

# Set these
TOTAL_NODES: int = 16
BUILD_DIR: str = "cmake-build-debug"
skaled_executable: str = "../../../" + BUILD_DIR + '/skaled/skaled'

processes = []


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
        processes.append(process)

    except Exception as e:
        print(f"Exception occurred: {e}")
    finally:
        print("skaled crashed or terminated.")



def main():
    for i in range(TOTAL_NODES):
        run_skaled(i + 1, TOTAL_NODES)

    for i in range(TOTAL_NODES):
        processes[i].wait()


if __name__ == "__main__":
    main()
