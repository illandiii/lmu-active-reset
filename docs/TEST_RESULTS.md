> Aggiornamento 21 settembre 2026: le nuove prove contatti 14150 (due fixture, nove test Python, ricostruzione reale delle quattro ruote) sono in [CONTACT_REBUILD.md](CONTACT_REBUILD.md) e nei log `20260921T180631160598Z_contact_experiment.*`. Sono state effettuate 207 scritture mirate dei contatti, nessun ripristino di checkpoint. Le sezioni seguenti conservano gli esiti storici precedenti.

# Risultati effettivi — 12 settembre 2026

## Compilazione e test locali

Compilazione C++17 x64 sugli header locali riuscita con Zig 0.16.0. Eseguiti schema probe e 24 controlli del binario: **24 PASS, 0 FAIL** (`logs/self-test.txt`). I controlli comprendono tutte le classi di sessione 0–13, dati non aggiornati, controllo remoto, bounds, contesa e rilascio del lock, ID scoring/telemetria incoerenti e indice veicolo fuori limite.

Sono test locali con fixture, non prove di funzionamento del reset. Il lettore C++ da solo non può produrre `allowed`: non legge la fonte REST singleplayer aggiunta successivamente. Non è presente una DLL sperimentale.

Verificato anche lo script di avvio con una copia isolata dell'inventario contenente un hash intenzionalmente errato: **rifiuto prima dell'acquisizione**, senza modificare il gioco (`logs/wrapper-test.txt`). Totale: 24 controlli C++ più una verifica del wrapper.

## Acquisizioni reali

Condizioni dichiarate dall'utente: «Sono ai box in Practice offline». Condizioni esportate nelle prime due acquisizioni: Practice 1, mGameMode 0, un veicolo, monitor, controllo AI locale, auto in garage, nessun miglior tempo valido (`-1`). Nessuna azione di guida eseguita dall'agente.

| Log UTC | Campioni | Durata | Mediana copia/lock | P99 | Massimo | Esito |
|---|---:|---:|---:|---:|---:|---|
| 10:20:46 pit_baseline | 182 | circa 20 s | 0,7 µs | 12,3 µs | 13,1 µs | Lettura riuscita, monitor |
| 10:21:42 drive_pause_pits | 1640 | circa 180 s | 0,7 µs | 6,3 µs | 91,6 µs | Lettura riuscita, sempre monitor |
| 10:25:18 extended_baseline | 5465 | circa 600 s | 0,7 µs | 8,4 µs | 72,5 µs | Lettura riuscita, sempre monitor |

Nessun tentativo di lock saltato per contesa nelle prime due acquisizioni; sei nella terza. L'intervallo misurato comprende acquisizione, copia e rilascio, ed eventuale scheduling del sistema: **non misura la latenza di reset e non quantifica l'impatto sul frame time del gioco**. La velocità massima iniziale circa 0,0026 km/h è rumore/jitter dell'auto ferma.

Il nome del secondo log indica la sequenza richiesta, non una sequenza completata. I dati non mostrano guida né pausa.

La prima versione interpretava erroneamente gli eventi come contatori: `fresh_observed=false` nel primo log è conservato come evidenza di quel difetto. La versione successiva osserva l'avanzamento di entrambi i timestamp: 1.638 campioni aggiornati su 1.640 nel secondo log, con i primi due prudentemente sconosciuti. L'avanzamento dei timestamp non certifica l'offline o l'atomicità fisica.

Una terza acquisizione estesa è stata avviata alle **10:25:18 UTC**, con durata massima di 600 secondi, per permettere all'utente di completare le transizioni. Il conteggio e le transizioni effettive sono nel file corrispondente e in `log-summary.json`; controllare l'aggiornamento finale sotto.

**Aggiornamento finale:** la terza acquisizione è terminata normalmente: 5.465 campioni, sei tentativi saltati per contesa senza attesa sul lock. Sempre monitor, stessa sessione e stesso numero di giri; nessun tratto di guida o pausa rilevato. Totale delle tre registrazioni: **7.287 campioni**. Nessun lettore del progetto è rimasto in esecuzione alla consegna. Le durate complessive di raccolta sono circa 13 minuti e 20 secondi, escluse le pause tra acquisizioni.

Dopo l'aggiunta delle asserzioni statiche sul layout il binario finale è stato ricompilato e i 24 test sono stati rieseguiti con esito positivo. La verifica dell'inventario ripetuta alle 10:32:01 UTC non rileva differenze negli hash dei cinque file fissati. La scansione delle chiavi JSONL non trova campi di nomi/account/server/IP vietati dalla selezione del logger (`logs/log-field-check.json`).

I metadati delle prime acquisizioni non contengono l'hash del lettore; per quelle iniziali la revisione è descritta sopra. Il wrapper finale ora registra anche l'hash del binario diagnostico. L'hash della build consegnata è in `toolchain.json`.

## Verifiche ancora mancanti

- Fine sessione e identità/generazione dopo un riavvio con la stessa auto e pista.
- Giro valido completo con record preesistente.
- Rivalidazione completa del contesto sul thread fisico prima di una futura mutazione. Il ramo consentito del controllo esterno è stato poi osservato durante la guida; non sono state aperte sessioni online di test.
- Coerenza del protocollo di sincronizzazione in presenza di contesa del produttore/altri consumatori.
- Qualsiasi acquisizione/ripristino fisico, 20 reset consecutivi, conservazione record, validità giro, latenza reset, FFB, danni e consumabili.

## Integrità installazione

Nessun file o configurazione del gioco modificato dal progetto. Gli hash inventariati vengono ricontrollati prima delle acquisizioni. Non è stata installata alcuna DLL; i plugin preesistenti sono rimasti intatti.

## Acquisizioni successive e analisi del reset

| Log UTC | Campioni | Durata | Mediana copia/lock | P99 | Massimo | Esito |
|---|---:|---:|---:|---:|---:|---|
| 10:57:41 ui_transitions | 1641 | circa 180 s | 0,7 µs | 5,8 µs | 17,3 µs | Ingresso in guida, movimento e ritorno al monitor |
| 11:07:13 verified_context | 685 | circa 75 s | 0,8 µs | 6,9 µs | 13,5 µs | Monitor, osservazione REST accoppiata |

Totale aggiornato: **9.613 campioni nativi**. Nel quarto log, realtime/local control iniziano a 23,830 s; ritorno al monitor a 112,728 s. Velocità massima 7,4069 km/h. L'agente ha premuto GUIDA dall'interfaccia; nessun comando di acceleratore è stato generato dall'agente. Miglior giro sempre -1, numero giri 2.

L'utente ha confermato pausa, ripresa e ritorno ai box. `logs/pause-analysis.json` misura un plateau con entrambi i timestamp invariati da 108,342 a 112,618 s (4,276 s, 40 campioni). Compatibile con la pausa dichiarata; la fase rimane 5, quindi non è corretto pretendere di avere osservato fase 9 o dedurre l'esatta durata della pausa UI.

Nel quinto log, le 266 osservazioni REST accoppiate hanno prodotto 265 `forbidden`, una `unknown`, zero errori di lettura e zero reset. Il monitor era attivo. Il ramo `allowed` del controllo osservativo non è stato provato dal vivo. Il ramo di scrittura non esiste.

Eseguiti **20 test Python PASS**, inclusi rifiuti multiplayer, replay, Test Day, altri tipi sessione, build sconosciuta, campioni vecchi/futuri, transizioni, contesto incompleto, traffico e tipi numerici malformati. Anche quando il contesto fixture è allowed, `reset_permitted` resta false (`logs/context-tests.txt`).

La lettura ordinaria della sezione codice dal processo è riuscita, senza richiesta di scrittura; 18 riferimenti risolti e confini istruzioni verificati. Nessun hook, chiamata remota o ripristino fisico testato. Dettagli in `PHYSICAL_RESET_STATUS.md`.


## Ultima prova: oggetto fisico candidato e contesto in guida

Alle 11:37:54 UTC acquisiti altri 320 campioni in 35 s. Il controllo prudente rifiutava il nuovo stato NAV_REALTIME come unknown: 42 unknown e 85 forbidden. Questo ha rilevato un errore della condizione NAV_EVENT, non un errore del vincolo offline.

Correzione verificata alle 11:42:00 UTC: 183 campioni nativi in 20 s; 25 osservazioni contestuali allowed, 46 forbidden, una unknown, zero errori. Sono stati effettuati cinque confronti mirati dei campi fisici con i campioni nativi. Totale aggiornato delle acquisizioni: **10.116 campioni nativi**.

Usati i normali comandi GUIDA e ritorno al monitor per le prove; la seconda sequenza ha verificato `inMonitor=true` al termine. Non eseguiti comandi di acceleratore, caricamenti salvataggi, riavvii o reset al checkpoint. Le velocità massime delle due brevi acquisizioni sono 1,3823 e 1,1103 km/h; non sono test di ripristino in pista.

Suite finale del controllo esterno: **22 test PASS**, inclusi NAV_REALTIME, navigazione sconosciuta e conflitto tra guida e navigazione monitor. Il ramo allowed è ora osservato anche sul gioco reale, ma il controllo continua a non autorizzare scritture fisiche. Gli altri test di accettazione del reset restano mancanti.


## Fixture e prova reale del modulo osservazionale

Compilati con Zig 0.16.0 per Windows x64 il modulo e la fixture, usando MinHook 1.3.4. Eseguiti due processi fixture separati, entrambi con exit code 0:

1. Conservazione dell'argomento double e del risultato deterministico dell'originale; 16 osservazioni su 128 chiamate; altri 128 passaggi con gate chiuso non aggiungono osservazioni; stop esplicito, istruzioni originali ripristinate, chiamata successiva non intercettata.
2. Stessi controlli con scadenza del heartbeat: disattivazione automatica verificata e istruzioni originali ripristinate.

Queste prove erano in un processo fixture, non in LMU. La prova successiva in LMU è registrata separatamente: status 3/error 0, 23.496 callback, 2.862 osservazioni, 71 record conservati, un thread osservato (29480), zero reset. Mediana della durata della chiamata originale con trampolino 4,6 µs, p95 7,7 µs, massimo 11,5 µs. Non è la durata totale dell'osservatore, l'impatto sui frame o la latenza di reset.

Il controllo di accesso nel sandbox impediva l'enumerazione dei moduli dell'utente Windows. La stessa preflight di sola lettura, eseguita con l'autorizzazione dell'ambiente fuori sandbox, è riuscita. Anche il caricamento osservazionale è stato autorizzato dall'ambiente. Nessun privilegio SeDebug o cambiamento a EAC, servizi, driver o configurazioni è stato applicato.

Controllo successivo nel monitor: 12 corpi, 4.224 byte letti, matrici ortogonali e inerzie coerenti entro precisione numerica; risultati in `rigid-body-layout-observation.json`. Lettura asincrona, non checkpoint atomico. Il hook precedente è già disattivato; la DLL rimane residente fino alla chiusura del gioco.

Restano non provati: reset fisico, conservazione record, invalidazione/ripresa del giro, ricostruzione dei contatti, checkpoint di tutti i sottosistemi, identità di sessione su riavvio e tempi dal tasto alla ripresa.

Il controller consegnato aggiunge la conferma esplicita dell'avvio diretto all'opzione di caricamento e non riutilizza più un testo fisso Mercedes/Spa nei metadati delle prove future. Test negativo reale: `--pid 27908 --load` senza conferma è stato rifiutato dall'argparse prima della preflight o di qualsiasi accesso mutante al processo. Nessun secondo caricamento eseguito. La logica C del modulo resta identica alla DLL provata in gioco.
