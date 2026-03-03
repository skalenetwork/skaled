"""
BITE / BITE2 compatibility test suite — entry point for run.py.

Delegates all test logic to pytest (test_bite_compat.py + conftest.py).
run.py calls ``deploy()`` and ``run_tests()``; pytest manages everything
else including node lifecycle, fixtures, and test ordering.
"""

import json
import logging
import os
import tempfile
from pathlib import Path

import pytest

from result import TestResult

logger = logging.getLogger("bite-compat.suite")

SUITE_DIR = Path(__file__).resolve().parent


# ---------------------------------------------------------------------------
# run.py entry points
# ---------------------------------------------------------------------------

def deploy(cfg: dict, env) -> None:
    """No-op.  Node lifecycle is managed by the pytest session fixture."""


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
    """Run the BITE/BITE2 pytest suite and return results for run.py."""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", delete=False
    ) as f:
        json.dump(cfg, f)
        cfg_file = f.name

    collector = _ResultCollector()
    try:
        os.environ["BITE_COMPAT_CFG_JSON"] = cfg_file
        pytest.main(
            [
                str(SUITE_DIR / "test_bite_compat.py"),
                "-v",
                "--tb=short",
                "-p", "no:cacheprovider",
            ],
            plugins=[collector],
        )
    finally:
        os.environ.pop("BITE_COMPAT_CFG_JSON", None)
        os.unlink(cfg_file)

    return {"bite-compat": collector.results}
