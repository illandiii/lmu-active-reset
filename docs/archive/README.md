# Research archive

Historical reports preserve the measurements, rejected hypotheses and build-specific
contracts used to develop Sector. They are not current launch instructions or
compatibility guarantees. Local logs and captured binaries referenced here are
not distributed with the repository.

Use the [current documentation](../README.md) and [reset status](../PHYSICAL_RESET_STATUS.md)
for today's behavior. Statements such as ‘no reset performed’ describe the dated
stage in that report, not the current v15 implementation.

| Report | Scope |
| --- | --- |
| [Native tire-contact reconstruction](CONTACT_REBUILD.md) | Build-specific contact experiments on 14150 and 14200, including four-wheel verification. |
| [Development notes: 16 September 2026](DEVELOPMENT_20260916.md) | Migration to build 14150 and the state of capture, runtime hooks and validation at that stage. |
| [Development notes: 2 October 2026](DEVELOPMENT_20261002.md) | Build 14200 migration and the mechanical-state investigations performed that day. |
| [Development notes: 3 October 2026](DEVELOPMENT_20261003.md) | Build 14200 body and constraint capture work preceding the complete reset host. |
| [Initial executable and session findings](FINDINGS.md) | Early build 14130/14150 observations, hypotheses and unresolved research questions. |
| [Local installation and SDK inventory](INVENTORY.md) | The installation, ABI and toolchain recorded during the initial September investigation. |
| [Joint physics/scoring restore experiments](JOINT_RESTORE_25661166.md) | The 12 October Steam 25661166 transaction plan and native contact evidence. |
| [Kinematic preparation experiments](KINEMATIC_INTEGRATION.md) | The 21 September capture and local replay of coherent rigid-body states. |
| [Mechanical preparation on build 14200](MECHANICAL_PREPARATION_14200.md) | Body/constraint geometry, warm-start calculations and accumulated impulse comparisons. |
| [Object lifetime and admission research](OBJECT_LIFETIME.md) | Lifecycle hooks, asynchronous ownership handoff and the 13 October live admission observations. |
| [Persistent vehicle-state research](PERSISTENT_STATES_25661166.md) | Steam 25661166 captures of fuel, inertia, tires, brakes and partial powertrain state. |
| [Physical reset host development](PHYSICAL_RESET_HOST.md) | The initial bounded host, subsequent revisions and the evidence available before the final v15 moving-reset trials. |
| [Initial physical-reset investigation](PHYSICAL_RESET_STATUS.md) | September executable analysis and observer-only milestones, before a full checkpoint restore existed. |
| [Physics-thread synchronization: 13 October 2026](PHYSICS_THREAD_2026-10-13.md) | Lock ownership, callback boundaries and the measured synchronization evidence. |
| [REA analysis and transmission-state captures](REA_ANALYSIS_25661166.md) | Loaded-code analysis, powertrain probes and native preparation results from 12 October. |
| [Restore integration: 13 October 2026](RESTORE_INTEGRATION_2026-10-13.md) | The coordinated physics/scoring/contact transaction and its local recovery tests. |
| [Early rigid-body capture contract](RIGID_BODY_CONTRACT.md) | Historical layouts and invariants from the 14130/14150 body-capture investigation. |
| [Offline development status: 12 October 2026](STATO_2026-10-12.md) | The implementation and remaining work after the offline calculations on that date. |
| [Initial diagnostic and observer test results](TEST_RESULTS.md) | September fixture checks, live session captures and observation-only results. |
| [Vehicle adapter contract and fixtures](VEHICLE_ADAPTER.md) | The adapter's original typed restore contract and owned-memory validation before live integration. |
