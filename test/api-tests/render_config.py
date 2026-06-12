#!/usr/bin/env python3
"""Render run.toml from run.toml.j2 using values from environment variables."""

import os
import sys

from jinja2 import Environment, FileSystemLoader, StrictUndefined

HERE = os.path.dirname(os.path.abspath(__file__))

private_key = os.environ.get("FUNC_TEST_PRIVATE_KEY", "")
if not private_key:
    print("::error::Missing required FUNC_TEST_PRIVATE_KEY secret", flush=True)
    sys.exit(1)

context = {
    "private_key":   private_key,
    "skaled_binary": os.environ.get("SKALED_BINARY", ""),
    "build_skaled":  os.environ.get("BUILD_SKALED", "true"),
}

env = Environment(
    loader=FileSystemLoader(HERE),
    undefined=StrictUndefined,
    keep_trailing_newline=True,
)
rendered = env.get_template("run.toml.j2").render(context)
open(os.path.join(HERE, "run.toml"), "w").write(rendered)
