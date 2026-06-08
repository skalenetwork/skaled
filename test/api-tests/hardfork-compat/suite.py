"""
hardfork-compat test suite -- entry point for run.py.

Verifies that the 5.2.0 skaled binary derives byte-identical state from a
chain produced by the 5.1.0 binary.  A 5.1.0 node (primary) produces blocks
with a mixed transaction workload; a 5.2.0 node (sync, syncNode=true,
archiveMode=true) replays every block and recomputes its own state.  The
suite then compares the per-block stateRoot of every block between the two
binaries.

Delegates all test logic to pytest.  run.py calls ``deploy()`` and
``run_tests()``; pytest manages everything else including node lifecycle,
fixtures, and test ordering.
"""

import json
import logging
import os
import tempfile
from pathlib import Path

import pytest

from result import TestResult

logger = logging.getLogger("hardfork-compat.suite")

SUITE_DIR = Path(__file__).resolve().parent


# ---------------------------------------------------------------------------
# run.py entry points
# ---------------------------------------------------------------------------

def deploy(cfg: dict, env) -> None:
    """No-op.  Node lifecycle is managed by pytest fixtures."""


class _ResultCollector:
    """Pytest plugin that translates test reports to TestResult objects."""

    def __init__(self):
        self.results: list[TestResult] = []

    def pytest_runtest_logreport(self, report) -> None:
        if report.when != "call":
            return
        name = report.nodeid.split("::")[-1]
        if report.passed:
            self.results.append(TestResult(name=name, passed=True, message="passed"))
        elif report.skipped:
            self.results.append(TestResult(
                name=name, passed=False,
                message=f"SKIP: {report.longrepr[-1] if isinstance(report.longrepr, tuple) else report.longrepr}",
            ))
        else:
            msg = str(report.longrepr) if report.longrepr else "failed"
            self.results.append(TestResult(name=name, passed=False, message=msg[:500]))


def run_tests(cfg: dict, env) -> dict[str, list[TestResult]]:
    """Run the hardfork-compat pytest suite and return results for run.py."""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", delete=False
    ) as f:
        json.dump(cfg, f)
        cfg_file = f.name

    collector = _ResultCollector()
    try:
        os.environ["HARDFORK_COMPAT_CFG_JSON"] = cfg_file
        pytest.main(
            [
                str(SUITE_DIR / "test_hardfork_compat.py"),
                "-v",
                "--tb=short",
                "-p", "no:cacheprovider",
            ],
            plugins=[collector],
        )
    finally:
        os.environ.pop("HARDFORK_COMPAT_CFG_JSON", None)
        os.unlink(cfg_file)

    return {"hardfork-compat": collector.results}
