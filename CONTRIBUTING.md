# Development workflow

Keep each completed, verified part in a separate commit. Record the relevant
tests and remaining limitations before committing. Do not describe fixture
results as evidence that a full reset has succeeded in LMU.

Run `scripts/Test-Core.ps1` after changing restore or callback coordination.
Format owned sources using `scripts/Format-Code.ps1`; leave third-party code
unchanged. Preserve historical observer layouts used by replay tools.

Write code comments and docstrings in English. Explain ownership, synchronization
and non-obvious native constraints; avoid comments that merely repeat a statement.
Keep UI presentation separate from native admission and reset sequencing. Desktop
and future overlay views must display acknowledged state rather than infer success
from a keypress.

Check Python changes with `ruff check src`, `ruff format --check src` and
`python -m unittest discover -s src -p 'test_*.py'`. Import cleanup must preserve
wire types exported to other capture and replay modules.

Keep generated captures, proprietary binaries/disassembly, local installation
paths and credentials out of commits. Never overwrite a DLL while it is resident
in a running game, even when its hooks have stopped.
