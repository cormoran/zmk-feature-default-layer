#!/usr/bin/env python3
"""Hook to restrict certain gh commands."""

import json
import sys

data = json.load(sys.stdin)
tool_name = data.get("tool_name", "")
tool_input = data.get("tool_input", {})

if tool_name == "Bash":
    command = tool_input.get("command", "")
    forbidden = ["gh pr create", "gh pr merge", "gh pr close"]
    for f in forbidden:
        if f in command:
            print(f"Blocked: {f}", file=sys.stderr)
            sys.exit(1)

sys.exit(0)
