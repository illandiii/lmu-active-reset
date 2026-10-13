> Aggiornamento 16 settembre 2026: il gioco è ora 14150. Le sezioni seguenti descrivono la ricerca storica su 14130; per codice nuovo, migrazione, prove e limiti vedere [DEVELOPMENT_20260916.md](DEVELOPMENT_20260916.md). La DLL precedente non è abilitata sulla nuova build.

# Inventario locale — 12 settembre 2026

## Ambiente verificato

Windows nativo `10.0.26200`, processo PowerShell x64, PowerShell 7.6.5. Accesso ai file LMU e al mapping condiviso nativo effettivamente riuscito. Processo LMU già aperto all'inizio del lavoro; non è stato avviato o riavviato dall'agente. L'interfaccia del gioco non è stata pilotata dall'agente.

Non sono stati trovati `AGENTS.md` negli antenati del workspace controllati né nei percorsi LMU ricercati. Workspace separato dall'installazione in `outputs/lmu-active-reset` e file di lavoro in `work/`.

Git disponibile in `C:\Program Files\Git\cmd\git.exe`. `dotnet` disponibile ma `dotnet --list-sdks` non ha restituito SDK. `cl`, `cmake`, `python`, `py`, `dumpbin`, `llvm-readobj` non risolti sul PATH. Le ricerche mirate non hanno individuato un compilatore C++ o Ghidra: questo non è un inventario completo di tutti i dischi. Una ricerca nella cartella AppData/Local/Programs ha restituito accesso negato.

## LMU

| Voce | Valore |
|---|---|
| Percorso | `D:\SteamLibrary\steamapps\common\Le Mans Ultimate` |
| Steam AppID | 2399420 |
| Steam build ID | 25081074 |
| File/Product version | 1.4.1.3 |
| PE Machine | 0x8664, x64 |
| Eseguibile | 29.659.648 byte |
| SHA-256 eseguibile | `BB8661CF6F96A056374AFAC29018C1FDD71F86164C158A71043BB7C244F7407E` |
| gameVersion osservato | 14130 |
| Mapping / evento dati | `LMU_Data` / `LMU_Data_Event` |
| Lock / evento lock | `LMU_SharedMemoryLockData` / `LMU_SharedMemoryLockEvent` |
| REST locale del processo | localhost:6397; sola interrogazione GET |

Il manifest Steam è stato consultato per build e percorso. Il manifest completo non è incluso perché contiene anche identificativi non necessari.

## Header effettivi e ABI compilata

Cartella `Support/SharedMemoryInterface`: `SharedMemoryInterface.hpp`, `InternalsPlugin.hpp`, `PluginObjects.hpp`. Hash e dimensioni dei tre header e di `PluginsAdapter.exe` sono in `inventory.json`.

La licenza degli header consente l'uso per sviluppare estensioni per prodotti S397 supportati e vieta modifica/ridistribuzione degli header. Il progetto include i file originali dal loro percorso locale.

| Struttura/tipo | Dimensione in byte |
|---|---:|
| puntatore / long / bool | 8 / 4 / 1 |
| SharedMemoryLayout | 324824 |
| SharedMemoryGeneric | 332 |
| ScoringInfoV01 | 548 |
| VehicleScoringInfoV01 | 584 |
| TelemInfoV01 | 1888 |
| TelemWheelV01 | 260 |
| ApplicationStateV01 | 260 |

Gli header interni applicano packing 4 e poi ripristinano quello precedente; il wrapper shared-memory usa l'allineamento esterno. Offset `scoring=1632`, `telemetry=128464`; dentro lo scoring `mSession=64`, `mGameMode=284`; dentro la telemetria `mLapInvalidated=745`. Valori prodotti dal compilatore, non dedotti dal mapping Python online. Il controllo è riferito all'ABI dei dati, non all'ABI delle classi plugin C++ per un'eventuale DLL.

## File preesistenti

Nella cartella Plugins sono già presenti `ChatTransceiver.dll`, `GOFastMemoryMap.dll`, `rF2SharedMemeryMapPlugin.dll`, `rFactor2SharedMemoryMapPlugin64.dll`, `TrackTitanRf2Plugin.dll`. Non sono stati aggiunti, rimossi o alterati. La loro presenza non dimostra che siano caricati nel processo principale: l'installazione include anche `PluginsAdapter.exe`.

Sono inoltre presenti cartella EasyAntiCheat, launcher protetto e altri componenti/modifiche preesistenti. Non è stata dedotta da questo la modalità EAC dell'istanza aperta e non è stata dichiarata l'installazione pulita per prove online.

## Dipendenze del progetto

| Dipendenza | Versione / provenienza |
|---|---|
| Zig portatile | 0.16.0, sito ufficiale, archivio 97.217.739 byte |
| SHA-256 archivio Zig | `68659EB5F1E4EB1437A722F1DD889C5A322C9954607F5EDCF337BC3684A75A7E` |
| Capstone | 5.0.7, installazione circoscritta a `work/pydeps` |
| pefile | 2024.8.26, stessa cartella |
| Python | runtime distribuito con Codex; versione in `toolchain.json` |

Download verificato di Zig; per i pacchetti Python sono fissate le versioni ma non è stato creato un lockfile con gli hash delle wheel. Il lettore compilato non richiede Python, Capstone o pefile per funzionare.



## Aggiornamento: UI e accesso al codice caricato

Individuato anche `Bin/UI.zip`, 1.083.052.870 byte; non appariva nell'elenco iniziale non forzato. Membro esaminato `start/assets/app-JoLu7W8D.js`, 6.109.796 byte; SHA-256 `6CA1BEA5BCCC7627AD137A1885668E1BA0E0591E484CC5A81D563E32DAA35057`. `ui-contract.json` fissa il discriminante applicativo singleplayer e i riferimenti. Estratti proprietari soltanto in work, esclusi dai deliverable.

Nella prima analisi del codice, accesso ordinario in lettura al processo riuscito: sezione `.text` e intestazione del modulo. Successivamente sono state aggiunte letture mirate dell'heap e una DLL con hook osservazionale temporaneo, poi disattivato. Nessuna scrittura nei campi fisici del veicolo. Riferimenti e limiti in `live-code-findings.json`, `observer-manifest.json` e `PHYSICAL_RESET_STATUS.md`. La base del modulo è dinamica e va rilevata ogni volta. L'assenza apparente di moduli EAC non certifica la modalità di avvio dell'istanza.
