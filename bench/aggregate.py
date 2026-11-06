#!/usr/bin/env python3
"""Select each operation's complete minimum-timing row from exactly three runs."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

from compare import SCHEMA_ID, load_results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", type=Path, nargs=3, help="Three complete benchmark JSON runs.")
    args = parser.parse_args()
    if len({path.resolve() for path in args.samples}) != 3:
        parser.error("three distinct sample files are required")

    runs = [load_results(path) for path in args.samples]
    names = runs[0].keys()
    if any(run.keys() != names for run in runs[1:]):
        sys.stderr.write("error: sample operation sets differ; every run must be complete\n")
        return 2

    result = {
        "schema": SCHEMA_ID,
        "aggregation": {
            "method": "minimum ns/op per operation from exactly three complete runs",
            "samples": [
                {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for path in args.samples
            ],
        },
        "results": [min((run[name] for run in runs), key=lambda row: row["ns_per_op"])
                    for name in sorted(names)],
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
