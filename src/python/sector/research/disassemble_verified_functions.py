"""Export runtime-function disassembly from a hash-verified local code capture."""

import argparse
import hashlib
import json
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def export(capture, output, starts):
    raw = capture.read_bytes()
    manifest = json.loads(capture.with_suffix(".manifest.json").read_text())
    if (
        hashlib.sha256(raw).hexdigest() != manifest["derived_sha256"]
        or manifest["process_writes"] != 0
    ):
        raise ValueError("Unverified capture")
    pe = pefile.PE(data=raw)
    functions = {f.struct.BeginAddress: f.struct.EndAddress for f in pe.DIRECTORY_ENTRY_EXCEPTION}
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    output.mkdir(parents=True, exist_ok=True)
    for start in starts:
        end = functions[start]
        lines = [
            f"{i.address - pe.OPTIONAL_HEADER.ImageBase:08x} {i.mnemonic} {i.op_str}"
            for i in decoder.disasm(
                pe.get_data(start, end - start), pe.OPTIONAL_HEADER.ImageBase + start
            )
        ]
        (output / f"{start:x}.instructions.txt").write_text("\n".join(lines) + "\n")
    return len(starts)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("starts", nargs="+", type=lambda s: int(s, 16))
    args = parser.parse_args()
    print(
        json.dumps(
            {
                "functions_exported": export(args.capture, args.output, args.starts),
                "process_writes": 0,
            }
        )
    )
