#!/usr/bin/env python3
"""Report conservative ESP-IDF cleanup candidates without deleting files.

All peripheral HAL/register/ROM headers and SoC sources are retained as roots.
Only unreferenced IDF private service headers, unused Kconfig inputs, and the
unsupported P4 v3 register tree are considered for removal. This is a source
include audit, not a replacement for building every supported target.
"""

import argparse
import json
import os
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".h", ".S", ".s", ".cpp", ".hpp", ".cc"}
INCLUDE = re.compile(r'^\s*#\s*(?:include|include_next)\s*[<"]([^>"\n]+)[>"]', re.M)
HAS_INCLUDE = re.compile(r'__has_include\s*\(\s*[<"]([^>"\n]+)[>"]\s*\)')
P4_V3 = "core/soc/esp32p4/register/hw_ver3/"
PRIVATE_SERVICES = "core/esp_hw_support/include/esp_private/"
SERVICE_HEADERS = {
    "core/esp_hw_support/include/clk_ctrl_os.h",
    "core/esp_hw_support/include/esp_sleep.h",
}
# This header implements useful register-DMA descriptor builders inline.
# Its private location alone is not a reason to remove a low-level API.
PRESERVED_PRIVATE_HEADERS = {"core/esp_hw_support/include/esp_private/regdma_link.h"}


def source_files():
    # Avoid Git metadata and generated build directories, including submodules.
    for directory, dirs, names in os.walk(ROOT):
        dirs[:] = [
            name for name in dirs
            if name not in {".git", "build", "CMakeFiles", "_deps"}
            and not name.startswith(("build-", "build_"))
        ]
        for name in names:
            path = Path(directory) / name
            if path.suffix in SOURCE_SUFFIXES:
                yield path.relative_to(ROOT).as_posix()


def audit():
    files = set(source_files())
    contents = {name: (ROOT / name).read_text(errors="replace") for name in files}
    services = {
        name for name in files
        if (name.startswith(PRIVATE_SERVICES) or name in SERVICE_HEADERS)
        and name not in PRESERVED_PRIVATE_HEADERS
    }
    supported = {name for name in files if not name.startswith(P4_V3)}
    index = defaultdict(set)
    for name in supported:
        index[Path(name).name].add(name)
        # Include paths vary by component and target. Preserve every matching
        # target variant, even for includes inside inactive preprocessor branches.
        for marker in ("include/", "register/", "hw_ver1/"):
            if marker in name:
                index[name.split(marker, 1)[1]].add(name)

    retained = set()
    pending = list(supported - services)
    while pending:
        name = pending.pop()
        if name in retained:
            continue
        retained.add(name)
        for include in INCLUDE.findall(contents[name]) + HAS_INCLUDE.findall(contents[name]):
            local = Path(os.path.normpath(Path(name).parent / include)).as_posix()
            if local in supported:
                pending.append(local)
            pending.extend(index.get(include, ()))

    groups = {
        "unsupported_p4_v3_registers": sorted(
            path.relative_to(ROOT).as_posix()
            for path in (ROOT / P4_V3).rglob("*") if path.is_file()
        ),
        "unused_kconfig_inputs": sorted(
            path.relative_to(ROOT).as_posix()
            for path in (ROOT / "core").rglob("Kconfig*") if path.is_file()
        ),
        "unreferenced_idf_service_headers": sorted(services - retained),
    }
    # A new CMake reference should trigger manual review instead of removal.
    cmake_text = "\n".join(
        path.read_text(errors="replace")
        for folder in ("cmake", "core")
        for path in (ROOT / folder).rglob("*")
        if path.is_file() and (path.name == "CMakeLists.txt" or path.suffix == ".cmake")
    ) + (ROOT / "CMakeLists.txt").read_text()
    if "hw_ver3" in cmake_text:
        groups["unsupported_p4_v3_registers"] = []
    if "Kconfig" in cmake_text:
        groups["unused_kconfig_inputs"] = []

    return {
        "scope": "Preserve peripheral HAL, register, ROM, and SoC APIs for supported targets",
        "groups": {
            group: {
                "count": len(paths),
                "bytes": sum((ROOT / name).stat().st_size for name in paths),
                "files": paths,
            }
            for group, paths in groups.items()
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="print full file lists as JSON")
    parser.add_argument("--check", action="store_true", help="exit 1 when candidates remain")
    args = parser.parse_args()
    report = audit()
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        for group, result in report["groups"].items():
            print(f"{group}: {result['count']} files, {result['bytes']:,} bytes")
        print("Read-only audit; inspect candidates before removing them.")
    return int(args.check and any(group["count"] for group in report["groups"].values()))


if __name__ == "__main__":
    raise SystemExit(main())
