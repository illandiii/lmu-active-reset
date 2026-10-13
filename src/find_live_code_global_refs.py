"""Find decoded RIP-relative references in the verified live-code capture.

Candidate byte matches are accepted only at instruction boundaries obtained by
disassembling their PE runtime function. No process access or game execution.
"""

import argparse, bisect, hashlib, json, struct
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_AC_WRITE
from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP


def scan(path, targets):
    raw = path.read_bytes()
    manifest = json.loads(path.with_suffix(".manifest.json").read_text())
    if (
        hashlib.sha256(raw).hexdigest() != manifest["derived_sha256"]
        or manifest["process_writes"] != 0
    ):
        raise ValueError("Unverified code capture")
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    section = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    code = section.get_data()
    text_rva = section.VirtualAddress
    functions = sorted(
        (x.struct.BeginAddress, x.struct.EndAddress) for x in pe.DIRECTORY_ENTRY_EXCEPTION
    )
    starts = [x[0] for x in functions]
    candidates = set()
    unmapped = []
    for i in range(len(code) - 4):
        destination = text_rva + i + 4 + struct.unpack_from("<i", code, i)[0]
        if not any(destination + tail in targets for tail in (0, 1, 4)):
            continue
        rva = text_rva + i
        j = bisect.bisect_right(starts, rva) - 1
        if j >= 0 and functions[j][0] <= rva < functions[j][1]:
            candidates.add(functions[j])
        else:
            unmapped.append(hex(rva))
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    refs = []
    for start, end in sorted(candidates):
        for instruction in decoder.disasm(pe.get_data(start, end - start), base + start):
            for operand in instruction.operands:
                if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
                    target = instruction.address + instruction.size + operand.mem.disp - base
                    kind = "rip_memory"
                elif operand.type == X86_OP_IMM and instruction.mnemonic in ("call", "jmp"):
                    target = operand.imm - base
                    kind = "direct_branch"
                else:
                    continue
                if target in targets:
                    refs.append(
                        dict(
                            function_rva=hex(start),
                            instruction_rva=hex(instruction.address - base),
                            target_rva=hex(target),
                            write=kind == "rip_memory" and bool(operand.access & CS_AC_WRITE),
                            kind=kind,
                            instruction=instruction.mnemonic + " " + instruction.op_str,
                        )
                    )
    return dict(
        capture_sha256=manifest["derived_sha256"],
        references=refs,
        unmapped_candidates=len(unmapped),
        unmapped_displacement_rvas=unmapped,
        limitations="Direct RIP/branch references in PE runtime functions only; indirect writers and unregistered code are not excluded.",
        process_writes=0,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("rvas", nargs="+", type=lambda x: int(x, 16))
    a = p.parse_args()
    result = scan(a.capture, set(a.rvas))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2))
    print(
        json.dumps(
            dict(
                output=str(a.output),
                references=len(result["references"]),
                writes=sum(r["write"] for r in result["references"]),
                unmapped_candidates=result["unmapped_candidates"],
            )
        )
    )
