import time
import argparse
from run_skaled import Chain


def main():
    parser = argparse.ArgumentParser(description="Run chain tests with specified parameters.")
    parser.add_argument(
        "-nodes", type=int, default=16, help="Number of nodes in the chain (default: 16)"
    )
    parser.add_argument(
        "-test", type=str,
        help="Test: just_run, perf_calls, perf_sendManyParalelEthTransfers, perf_sendManyParalelEthMTMTransfers,"
             "perf_sendManyParalelEthType1Transfers, perf_sendManyParalelEthMTMTransfers, perf_sendManyParalelEthType1Transfers"
             "perf_sendManyParalelEthType2Transfers, perf_sendManyParalelEthPowTransfers, perf_sendManyParalelERC20Transfers",
        default="perf_sendManyParalelEthTransfers"
    )

    args = parser.parse_args()

    chain = Chain(args.nodes)
    chain.start()
    if args.test == "just_run":
        time.sleep(100000)
    else:
        chain.run_test(args.test)
    chain.stop(20)
    chain.remove_created_files()


if __name__ == "__main__":
    main()
