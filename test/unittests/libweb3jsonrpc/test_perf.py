import run_skaled
from test.unittests.libweb3jsonrpc.run_skaled import TOTAL_NODES, wait_for_exit

TOTAL_NODES = 1


def main():
    for i in range(TOTAL_NODES):
        run_skaled.start_skaled(i + 1, TOTAL_NODES)

    for i in range(TOTAL_NODES):
         run_skaled.wait_for_exit(i)


if __name__ == "__main__":
    main()
