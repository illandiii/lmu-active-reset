"""Static candidate relocation from prior verified disassembly; no hooks or heap.

Reports matching instruction prefixes, never ABI/semantic equivalence.
"""

import argparse, hashlib, json, re, struct, sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT
sys.path.insert(0, str(ROOT / "work/pydeps"))
import capstone


def normalized(mnemonic, operands):
    if mnemonic == "call" or mnemonic.startswith("j"):
        operands = re.sub(r"^0x[0-9a-f]+$", "TARGET", operands)
    operands = re.sub(r"\[rip [+-] 0x[0-9a-f]+\]", "[RIP_TARGET]", operands)
    return mnemonic + " " + operands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    args = parser.parse_args()
    inventory = json.loads((PROJECT / "compatibility/steam-25661166-inventory.json").read_text())
    old = json.loads((PROJECT / "compatibility/14200-telemetry.json").read_text())
    raw = (Path(old["install_path"]) / "Le Mans Ultimate.exe").read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != "66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf":
        raise ValueError("Unrecognized executable")
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    opt = struct.unpack_from("<H", raw, pe + 20)[0]
    if not 0 < n <= 96:
        raise ValueError("Section limit")
    sections = [struct.unpack_from("<8sIIIIIIHHI", raw, pe + 24 + opt + 40 * i) for i in range(n)]
    section = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
    code = raw[section[4] : section[4] + section[3]]
    if args.pid:
        from run_physics_observer import modules, k, read_process

        listed = modules(args.pid)
        if any(
            "easyanti" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
        ):
            raise ValueError("EAC module present")
        image = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
        if (
            Path(image["path"]).resolve()
            != (Path(old["install_path"]) / "Le Mans Ultimate.exe").resolve()
        ):
            raise ValueError("Executable path mismatch")
        if not 0 < section[1] <= 32 * 1024 * 1024:
            raise ValueError("Code read budget exceeded")
        handle = k.OpenProcess(0x410, False, args.pid)
        if not handle:
            raise OSError("Read-only process access failed")
        try:
            code = read_process(handle, image["base"] + section[2], section[1])
        finally:
            k.CloseHandle(handle)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    targets = []
    for profile in (
        "14200-vehicle-observer-v4.json",
        "14200-bar-observer-v3.json",
        "14200-ball-observer.json",
        "14200-hinge-observer.json",
        "14200-ori-observer.json",
    ):
        targets.extend(json.loads((PROJECT / "compatibility" / profile).read_text())["targets"])
    results = []
    targets.extend(
        [
            dict(name="FuelMass", rva=0xC6E170, prolog="f20f105160"),
            dict(name="AdditionalMass", rva=0xEE2730, prolog="488b81c04b0000"),
        ]
    )
    for target in targets:
        oldrva = target["rva"]
        folder = ROOT / "work/live-disassembly-14200"
        source = folder / f"{oldrva:08x}-complete.asm"
        if not source.exists():
            source = folder / f"{oldrva:08x}.asm"
        if not source.exists():
            source = folder / f"{oldrva:08x}-leaf.asm"
        lines = source.read_text().splitlines()
        reference = []
        for line in lines[:64]:
            parts = line.split(" ", 2)
            if len(parts) == 3:
                reference.append(normalized(parts[1], parts[2].split(" ; ")[0]))
        candidates = []
        prefix = bytes.fromhex(target["prolog"])
        matches = list(re.finditer(re.escape(prefix), code))
        if len(matches) > 4096:
            raise ValueError("Candidate budget exceeded")
        for match in matches:
            rva = section[2] + match.start()
            actual = list(md.disasm_lite(code[match.start() : match.start() + 2048], rva))[
                : len(reference)
            ]
            equal = 0
            for prior, (_, _, mnemonic, operands) in zip(reference, actual):
                if prior != normalized(mnemonic, operands):
                    break
                equal += 1
            total_equal = sum(
                prior == normalized(mnemonic, operands)
                for prior, (_, _, mnemonic, operands) in zip(reference, actual)
            )
            near_match = len(reference) >= 12 and total_equal >= len(reference) - 2
            if equal >= min(12, len(reference)) or near_match:
                candidates.append(
                    dict(
                        rva=hex(rva),
                        matching_prefix_instructions=equal,
                        reference_instructions=len(reference),
                        matching_total_instructions=total_equal,
                        complete_prefix_match=equal == len(reference),
                        delta=rva - oldrva,
                    )
                )
        candidates = sorted(candidates, key=lambda x: -x["matching_prefix_instructions"])[:12]
        comparison = None
        exact = [
            c
            for c in candidates
            if c["complete_prefix_match"]
            or (
                c["reference_instructions"] >= 12
                and c["matching_total_instructions"] >= c["reference_instructions"] - 2
            )
        ]
        if len(exact) == 1:
            delta = exact[0]["delta"]
            differences = []
            references = []
            checked = 0
            for line in lines:
                parts = line.split(" ", 2)
                if len(parts) != 3:
                    continue
                address = int(parts[0], 16)
                mnemonic = parts[1]
                operands = parts[2].split(" ; ")[0]
                offset = address + delta - section[2]
                if offset < 0 or offset + 16 > len(code):
                    raise ValueError("Instruction outside code section")
                decoded = next(md.disasm_lite(code[offset : offset + 16], address + delta), None)
                if decoded is None:
                    raise ValueError("Undecodable candidate instruction")
                actual = normalized(decoded[2], decoded[3])
                expected = normalized(mnemonic, operands)
                checked += 1
                if actual != expected:
                    differences.append(
                        dict(
                            old_rva=hex(address),
                            new_rva=hex(address + delta),
                            expected=expected,
                            actual=actual,
                        )
                    )
                if actual == expected and operands != decoded[3]:
                    references.append(
                        dict(
                            old_rva=hex(address),
                            new_rva=hex(address + delta),
                            old_operand=operands,
                            new_operand=decoded[3],
                        )
                    )
            comparison = dict(
                reference_file=source.name,
                complete_reference=source.name.endswith("-complete.asm"),
                instructions_checked=checked,
                normalized_mismatches=len(differences),
                differences=differences[:24],
                relocated_references=references,
            )
        results.append(
            dict(
                name=target["name"],
                old_rva=hex(oldrva),
                signature_matches=len(matches),
                candidates=candidates,
                instruction_comparison=comparison,
            )
        )
    report = dict(
        steam_buildid=inventory["steam_buildid"],
        exe_sha256=digest,
        code_source="loaded .text" if args.pid else "disk .text",
        heap_bytes_read=0,
        targets=results,
        physics_compatible=False,
        hook_loaded=False,
        limitations="Instruction prefixes normalize call/branch and RIP targets. Those targets, complete functions, ownership and live ABI still require verification.",
    )
    (PROJECT / "compatibility/steam-25661166-relocation-candidates.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(
        json.dumps(
            dict(
                code_source=report["code_source"],
                physics_compatible=False,
                targets=[
                    dict(
                        name=t["name"],
                        candidates=t["candidates"],
                        instructions_checked=(t["instruction_comparison"] or {}).get(
                            "instructions_checked"
                        ),
                        normalized_mismatches=(t["instruction_comparison"] or {}).get(
                            "normalized_mismatches"
                        ),
                        complete_reference=(t["instruction_comparison"] or {}).get(
                            "complete_reference"
                        ),
                    )
                    for t in results
                ],
            ),
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
