from dataclasses import dataclass, field


@dataclass
class TestResult:
    """One test outcome. Suites return a list of these."""

    name: str
    passed: bool
    message: str
    details: dict = field(default_factory=dict)
