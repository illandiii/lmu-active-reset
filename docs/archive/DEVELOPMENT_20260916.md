# Development notes: 16 September 2026

Migration to build 14150 and the state of capture, runtime hooks and validation at that stage.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Sviluppo Active Reset - 16 settembre 2026

**Il reset nel gioco non è ancora implementato.** Sono disponibili due componenti di preparazione provati fuori dal gioco, la diagnostica reale e l'analisi delle due build. Non esiste un comando operativo di salvataggio/ripristino né una prova di conservazione dei record dopo reset.

## Codice aggiunto

`src/native/physics/rigid_restore_core.c` prepara un gruppo di massimo 32 corpi usando valori numerici posseduti dal modulo: posizione e velocità mondiali, orientamento, momento angolare, massa e inerzie locali. Ricostruisce l'inerzia inversa mondiale come `R × diag(I^-1) × R^T` e la velocità angolare come `I^-1_world × L`. Gli offset del motore non entrano in questo nucleo. Nessun puntatore di processo viene serializzato.

La preparazione verifica tutti i corpi prima di pubblicare il risultato. NaN, overflow, riflessioni, rotazioni non ortonormali e variazioni di massa/inerzia impediscono l'intera operazione; un errore sull'ultimo corpo non lascia output parziale. La soglia numerica di validazione delle rotazioni è una scelta del componente, non una tolleranza di reset misurata su LMU. Il rifiuto delle variazioni di massa è conservativo: finché carburante, danni e massa non saranno ripristinati coerentemente, anche un checkpoint apparentemente simile può essere rifiutato.

`src/native/physics/lap_guard_core.h` prepara l'azzeramento congiunto dei tre flag individuati nella build 14130: corrente, memorizzato e ultimo calcolato. Preserva la restrizione eventualmente già presente sul giro successivo. Azzerare solo il flag corrente produce una regressione nel modello dei rami esaminati: il giro regolare successivo può ereditare uno zero rimasto nella cache. Il nucleo non modifica record, tempi, oggetti scoring o memoria del gioco. I relativi offset non sono ancora riverificati sulla build 14150.

Non è presente un adattatore che applichi questi payload a LMU. Mancano la copertura di tutti i sottosistemi, l'identità completa del checkpoint e la verifica della finestra di ripristino e della ricostruzione dei contatti.

## Prove dei componenti

- Preparazione cinematica: tensore anisotropo con rotazione nota, 512 rotazioni arbitrarie con velocità angolare non nulla e conservazione dell'energia di rotazione, gruppo di 32 corpi, otto errori sull'ultimo corpo senza pubblicazione parziale, argomenti errati rifiutati.
- Dodici corpi della Mercedes registrati nel monitor il 12 settembre: ricostruzione di inerzia inversa e velocità angolare coincidente con i numeri del file. Le velocità angolari del file erano nulle; i casi dinamici sono sintetici. Questa prova non è un ripristino nel gioco.
- Gestione giro: 108 combinazioni del modello dei flag; tentativo interrotto escluso, tempi sentinella conservati, giro successivo regolare recuperato e penalità pregresse preservate. Le sentinelle appartengono alla fixture, non ai record reali di LMU.
- Venticinque test Python superati, inclusi il rifiuto della nuova build nei vecchi percorsi fisici, l'accordo delle fonti per il nuovo profilo di sola telemetria e la gestione della perdita del processo durante l'attesa.

Comandi locali:

```powershell
.\Build-RigidRestore.ps1
python .\src/python/sector/replay/test_rigid_recording.py
.\Build-LapGuard.ps1
python -m unittest discover -s .\src -p "test_*.py"
```

`logs/rigid-restore-core-test.json` conserva hash del file di dati, hash della fixture ed esito nativo. I test non eseguono accessi a LMU.

## Motore osservato sulla build precedente

La prova del 12 settembre `20260912T163200Z_drivetrain_cycle` contiene 79 confronti asincroni, 61 con RPM positivi. In 26 campioni a regime stabilizzato dopo l'avviamento, il candidato motore `+0x1F8`, convertito con `30/pi`, coincide con 1426,299701 RPM esportati: errore massimo circa 0,000014 RPM. Trattarlo direttamente come RPM produce circa 1277 RPM di errore. È una conferma della scala al minimo, non di tutto lo stato motore. Il cambio è rimasto in folle.

Il primo ciclo precedente aveva motore spento, il secondo zero osservazioni di guida. Un'attesa successiva è fallita alla chiusura del processo prima di creare il lettore nativo; l'errore nel salvataggio dell'esito durante il cleanup è stato corretto e coperto da test. Non contare quei tentativi come prove dinamiche riuscite. Il lettore successivo ha completato il ciclo, restituito il monitor e terminato senza errori.

## Aggiornamento del gioco

Il processo aperto il 16 settembre usa **LMU 1.4.1.5 / 14150, Steam 25256080**. EXE e PluginsAdapter sono cambiati; tutti e tre gli header SDK hanno gli stessi hash della build precedente. SHA-256 dell'EXE nuovo: `0FE1F3572094C2CB77C1F0BE7A4A1B18EE709A645DDD3B94E6EC070795DC5160`.

Il controllo della build 14130 ha rifiutato il nuovo processo prima di qualsiasi accesso alla fisica o caricamento DLL. `inventory.json` e i controlli del vecchio osservatore rimangono fissati alla build 14130. La DLL della vecchia prova non è stata caricata nel processo nuovo.

È stato aggiunto **solo un profilo per telemetria** in `compatibility/14150-telemetry.json`. Verifica EXE, adapter, header, lettore diagnostico e nuovo membro UI `start/assets/app-BnHQL6RF.js`. Il confronto singleplayer è stato riletto nel nuovo codice UI: `stateDto.state.settingMode === "SETTING_GRANDPRIX"`, linea 46733. Il gate mantiene `reset_permitted=false`, anche quando l'osservazione è allowed. Il parametro del gate per la nuova build non viene passato dai vecchi strumenti fisici, che continuano a rifiutarla.

```powershell
python .\src/python/sector/telemetry/capture_preparation.py --pid <PID_ATTUALE> --seconds 180
```

Questo comando legge solo telemetria e GET locali; non preme GUIDA, non ritorna al monitor, non apre l'heap e non carica DLL.

## Baseline dei giri di preparazione

`logs/20260915T220818Z_preparation_14150.*` è registrato in UTC, corrispondente al 16 settembre locale. Il lettore ha terminato normalmente dopo 180 secondi: 1.649 campioni nativi, 1.497 campioni con guida del giocatore; 577 decisioni allowed, 59 forbidden, quattro unknown, zero errori di osservazione. Sono stati osservati giri da 1 a 3 e un miglior tempo valido di **211,0864105 s (3:31.086)**. Nessuna prova di conservazione del tempo dopo reset è stata eseguita.

La telemetria include marce da folle a settima, circa 331,3 km/h di velocità massima e valori non nulli di carica batteria. Questa registrazione non va etichettata come la vecchia prova Mercedes GT3; identità auto/pista e copertura ibrida non sono state certificate per un checkpoint. Il gioco è rimasto sotto il controllo dell'utente durante la registrazione.

## Migrazione delle routine fisiche: ricerca, non abilitazione

`src/python/sector/research/inspect_build_code.py` verifica il profilo nuovo e legge intestazione più sola sezione codice del processo (17.604.784 byte), senza heap o scritture. Ricerca riferimenti a stringhe profiler e segue gli intervalli PE concatenati; le disassemblazioni restano in `work`, escluse dal pacchetto. I seguenti RVA sono stati ritrovati indipendentemente nella build nuova:

| Routine | Build 14130 | Build 14150 |
|---|---:|---:|
| Integrazione HDV | `0xEAF350` | `0xEB9150` |
| Dinamica HDV | `0xEADBE0` | `0xEB79E0` |
| Trasmissione | `0xAC6210` | `0xAD0140` |
| Forze e coppie | `0xEAD230` | `0xEB7030` |
| Carburante/temperature motore | `0xEB04A0` | `0xEBA2A0` |
| Ricalcolo aggregati HDV | `0xEAF560` | `0xEB9360` |
| Integrazione corpi/vincoli | `0xEAFAA0` | `0xEB98A0` |
| Trasformazioni ruote | `0xEAFD10` | `0xEB9B10` |
| Aggiornamento ruota/pneumatico | `0xEC7240` | `0xED0E70` |
| Integrazione forza/coppia del corpo | `0xFFF500` | `0x1009010` |
| Integrazione posizione/orientamento del corpo | `0xFFF000` | `0x1008B10` |
| Segnalazione giro invalidato fuori pista | `0xCCC0E0` | `0xCD5F00` |

Le istruzioni consultate mantengono gli offset delle liste corpi, dei vettori aggregati, degli oggetti motore e delle ruote. Non significa che tutti i campi, chiamanti e sottosistemi siano equivalenti. Il prologo di Integrate e il passaggio di `double` in XMM1 coincidono nel codice osservato; non è stata ripetuta la prova del hook sulla build nuova. La nuova ownership e la vita degli oggetti non sono ancora validate dinamicamente. `physics_compatible` resta false.

## Lavoro ancora necessario per un Active Reset operativo

1. Validare i contratti del processo 14150 sul thread corretto e l'identità sessione/auto/setup, anche dopo un riavvio della stessa sessione.
2. Completare lo stato di motore, trasmissione, ruote/sospensioni, pneumatici, carburante, danni, comandi e ibrido; ricostruire contatti e cache senza riutilizzare riferimenti scaduti.
3. Integrare acquisizione e applicazione atomica sul thread fisico, gestione della progressione giro e due hotkey configurabili.
4. Provare reset reale in movimento, almeno 20 ripetizioni, record preesistente, giro interrotto, giro successivo e latenza. Nessuno di questi test è ancora superato.

Non sono stati modificati file dell'installazione, configurazioni, protezioni o driver.
