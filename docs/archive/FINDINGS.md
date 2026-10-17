# Initial executable and session findings

Early build 14130/14150 observations, hypotheses and unresolved research questions.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

> Aggiornamento 16 settembre 2026: il gioco è ora 14150. Le sezioni seguenti descrivono la ricerca storica su 14130; per codice nuovo, migrazione, prove e limiti vedere [DEVELOPMENT_20260916.md](DEVELOPMENT_20260916.md). La DLL precedente non è abilitata sulla nuova build.

## Riscontri, ipotesi e blocchi

## Fatti verificati

1. **SDK locale:** `InternalsPlugin.hpp` documenta `mSession`: 0 Test Day, 1–4 Practice, 5–8 qualifica, 9 warmup, 10–13 gara. `mGameMode` documenta soltanto 1 server, 2 client, 3 server+client. Il valore 0 non ha una semantica positiva di offline documentata nell'header.
2. **Baseline reale:** l'utente ha dichiarato di essere ai box in Practice offline. Le acquisizioni iniziali mostrano sessione 1, rete 0, un veicolo, scoring e telemetria associati allo stesso slot. Nel monitor: `mInRealtime=false`, `mControl=1`, `mGamePhase=5`; il tempo di sessione avanza mentre l'auto è ferma. Quindi fase verde non implica guida attiva.
3. **REST:** GET `/rest/watch/sessionInfo` restituisce `session=PRACTICE1`, `gameMode=UNKNOWN`. GET `/rest/sessions/GetGameState` conferma `MONITOR_MENU`, monitor attivo, replay non attivo. `/rest/multiplayer/join/state` restituisce `JOIN_IDLE`: questo descrive il join, non certifica la modalità offline. Il percorso `/rest/sessions/settings`, individuato come stringa, ha restituito 404 in GET. Nelle prime acquisizioni nessuna richiesta REST mutante. Successivamente sono stati usati esclusivamente i comandi ordinari drive/returnToMonitor per brevi prove ai box; non loadGame, saveGame o un comando di reset.
4. **Campi nuovi rispetto al passaggio:** telemetria con `mLapInvalidated`, stato ABS/TC, classe/campionato, batteria, motore elettrico, SoC, rigenerazione ed energia virtuale. Restano osservabili, non comandi né stato fisico completo.
5. **Callback disponibili:** inizio/fine sessione e realtime, output telemetria/scoring costante, hardware/FFB, condizioni meteo, regole e pit menu. Non è stata individuata negli header una API di acquisizione/ripristino dello stato veicolo o una API diretta di impostazione della validità del giro.
6. **Array eventi:** osservati valori come `[15,11,16,...]`, non contatori monotoni per evento. Sembrano ID evento con valore SME_MAX come riempimento; questa interpretazione è un'ipotesi. La funzione d'esempio `CopySharedMemoryObj` controlla invece posizioni dell'array come booleani. Il lettore non la usa e non deduce generazioni di sessione dall'array.
7. **Lock SDK:** il ramo che torna dall'attesa in `SharedMemoryLock::Lock` appare restituire successo senza riacquisizione CAS e senza decrementare i waiters; l'inizializzazione controlla inoltre GetLastError dopo il mapping. Sono problemi da verificare rispetto all'implementazione effettiva del produttore. Il nostro lettore apre soltanto oggetti già esistenti e usa il percorso CAS immediato. Evitare quei rami nel lettore non dimostra che il produttore o altri consumatori non li usino.

## Stato fisico: analisi dell'eseguibile

Il file PE locale contiene nomi RTTI riferiti a `GameMemento::SerializeVehicle`, `SerializeTires`, `SerializeTireInventory` e `SerializeSession`, oltre a stringhe per caricamento/salvataggio nativo e fasi della fisica. La ricerca iniziale su disco è stata poi estesa al codice caricato, come descritto sotto.

L'analizzatore iniziale ha letto sezioni PE e 51.307 intervalli della tabella delle funzioni runtime, poi cercato riferimenti RIP-relative LEA a 14 stringhe pertinenti. **Zero riferimenti risolti nel file su disco.** La sezione `.text` ha entropia circa 7,99999 bit/byte e il PE contiene `.bind`. Questo è compatibile con un'immagine su disco trasformata, ma la misura non identifica la causa.

La successiva lettura ordinaria del codice caricato nel processo è riuscita e ha risolto **18 riferimenti**. Questa analisi legge solo codice. La prova successiva aggiunge letture mirate dell'heap (352 byte per osservazione); in quella fase non erano state eseguite scritture dirette nel processo o injection. Nella successiva prova Mercedes è stato caricato il modulo osservazionale descritto sotto; nessuna scrittura nello stato fisico. Le routine seguite comprendono integrazione fisica, salvataggio su file, serializzazione della sessione e del record veicolo. `PHYSICAL_RESET_STATUS.md` contiene le evidenze e i limiti; `live-code-findings.json` i riferimenti riproducibili. Gli RVA sono riferimenti di analisi, non un contratto di chiamata verificato.

## Discriminante applicativo singleplayer

L'archivio installato `Bin/UI.zip` contiene `start/assets/app-JoLu7W8D.js`. `useMainState/getIsSingleplayer` confronta `stateDto.state.settingMode` con `SETTING_GRANDPRIX`; il ramo multiplayer usa `SETTING_MULTIPLAYER`. GET `/navigation/state`, individuato nello stesso codice, restituisce la modalità e la sessione. Hash e punti di lettura sono fissati in `ui-contract.json`. È una semantica positiva applicativa, più forte degli indizi di rete dei primi log.

`context_gate.py` combina due letture REST coerenti con build, Practice 1–4, stato guida, assenza replay, singolo veicolo e campioni nativi freschi. Il ramo `allowed` riguarda esclusivamente il contesto osservato: `reset_permitted=false` sempre. Il controllo non sostituisce la rivalidazione sul thread fisico al momento di una futura modifica. Prima prova nel monitor: 265 rifiuti e un dato iniziale sconosciuto. Dopo la correzione di NAV_REALTIME, 25 osservazioni allowed reali in guida; nessuna autorizza scritture fisiche.

## Copertura richiesta

| Sottosistema | Diagnostica disponibile | Ripristino |
|---|---|---|
| Posizione/orientamento/velocità | Baseline, hook Integrate, campi e inerzie dei corpi verificati numericamente | Contratto parziale; nessun ripristino |
| Motore/trasmissione | RPM, marcia, temperature ecc. negli header; subset registrato | Non individuato |
| Gomme/sospensioni/contatti | Header ruote; nomi serializzazione gomme | Stato autorevole/cache ignoti |
| Carburante/danni | Esportazioni parziali; carburante registrato | Non individuato |
| Ibrido | Campi esportati; non provato su Hypercar | Non individuato |
| Input e FFB | Input osservati; callback FFB esistente | Politica di ripresa non implementata |
| Validità e tempi | Campi scoring/telemetria leggibili | Nessun meccanismo mutante verificato |
| Identità di sessione/setup | Callback esistenti nell'SDK | Generazione robusta esterna non individuata |

## Blocchi effettivi e azioni per superarli

- **Baseline:** acquisiti ingresso in guida, breve movimento e ritorno al monitor. Pausa/ripresa confermate dall'utente, con plateau dei timestamp di circa 4,276 s. Restano fine sessione e giro valido completo.
- **Contesto:** discriminante positivo trovato nella UI locale; verificato anche dal vivo in guida; restano identità/generazione sessione e rivalidazione sul thread fisico. `uniqueSessionID` esiste nel JSON nativo ma il ciclo di vita del valore non è stato verificato.
- **Snapshot consistente:** verificare il protocollo effettivo del produttore e i suoi percorsi di attesa, oltre all'associazione temporale scoring/telemetria. I campioni attuali non possono essere usati come payload fisico autorevole.
- **Reset in pista:** mancano funzioni con contratti noti, ownership, thread e ricostruzione contatti. Per procedere serve un'interfaccia supportata o una sessione di analisi compatibile su un avvio offline senza EAC effettivamente verificato. La sola chiusura/riapertura del gioco non risolve la mancanza dei contratti.
- **Record:** miglior tempo `-1` nelle acquisizioni iniziali; nessuna prova di conservazione di un record o invalidazione del giro tramite reset eseguita.

La documentazione ufficiale consultata continua a indicare l'avvio diretto dell'eseguibile per usare LMU senza EAC. Non era stata applicata all'istanza iniziale già aperta. In seguito è stato chiesto all'utente il riavvio diretto prima di usare la DLL osservazionale: [procedura S397](https://guide.lemansultimate.com/hc/en-gb/articles/14590657733903-How-do-I-run-the-game-without-Easy-Anti-Cheat-EAC).


## Osservazione sul thread fisico: Mercedes GT3 a Spa

È stato compilato, provato su fixture e caricato nel gioco un modulo basato su MinHook 1.3.4. Il hook a RVA 0xEAF350 usa una firma verificata e controlli di hash/prologo esatti. Il controller rinnova un segnale di attività; il modulo interrompe le osservazioni quando manca il contesto valido e disattiva il hook alla scadenza del controller. Non espone alcun comando di reset.

Nella prova reale: 23.496 callback, 2.862 osservazioni pubblicate dal modulo, **71 campioni conservati nel log**. Il passo osservato è 0,416667 ms; gli spostamenti sono coerenti con velocità × passo. La velocità massima nei 71 campioni è circa 1,47 km/h: è una prova breve ai box, non un collaudo ad alta velocità. I tempi misurati della chiamata originale non sono latenze di reset né una misura completa del costo dell'hook.

A fine prova: status 3, error 0, istruzioni originali ripristinate e zero reset. Il ritorno al monitor è stato confermato dall'utente e verificato con una lettura successiva. La DLL rimane caricata, inattiva, fino alla chiusura di LMU: **chiudere completamente LMU prima di passare all'online**. Non esiste riarmo nella stessa istanza; non ricompilare sul file DLL caricato mentre LMU è aperto.

File principali: `observer-analysis.json`, `observer-manifest.json`, `RIGID_BODY_CONTRACT.md`, `rigid-body-layout-observation.json` e log `20260912T153551Z_physics_observer.*`. I sorgenti MinHook inclusi mantengono la licenza BSD in `third_party/minhook/LICENSE.txt`.

La nuova lettura numerica nel monitor conferma 12 corpi integrati sulla Mercedes, due dei quali contribuiscono all'aggregato. Matrici e inerzie rispettano le relazioni ricostruite dal codice. Mancano ancora i contratti di ripristino dei contatti e degli altri sottosistemi, e la gestione del giro interrotto. **Il reset fisico richiesto rimane incompleto.**
