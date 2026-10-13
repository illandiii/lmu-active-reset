# Development workflow

Keep each completed, verified part in a separate commit. Record the relevant
tests and remaining limitations before committing. Do not describe fixture
results as evidence that a full reset has succeeded in LMU.

Run `scripts/Test-Core.ps1` after changing restore or callback coordination.
Format owned sources using `scripts/Format-Code.ps1`; leave third-party code
unchanged. Preserve historical observer layouts used by replay tools.

Keep generated captures, proprietary binaries/disassembly, local installation
paths and credentials out of commits. Never overwrite a DLL while it is resident
in a running game, even when its hooks have stopped.
