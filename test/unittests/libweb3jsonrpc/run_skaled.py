import subprocess
import time

# Set these
GIT_REPO = "/d/skaled"
BUILD_DIR = "cmake-build-debug"
DATA_DIR = "/tmp/test_skaled"

skaled_executable = GIT_REPO + "/" + BUILD_DIR + '/skaled/skaled'



def run_skaled():
    while True:
        try:
            print("Starting /tmp/skaled...")
            process = subprocess.Popen([GIT_REPO + "/" + BUILD_DIR + '/skaled/skaled', '-d',    DATA_DIR,
                                        '--config', GIT_REPO + '/test/historicstate/configs/basic_config.json'])
            process.wait()
        except Exception as e:
            print(f"Exception occurred: {e}")
        finally:
            print("/tmp/skaled crashed or terminated. Restarting in 5 seconds...")
            time.sleep(1)

if __name__ == "__main__":
    run_skaled()