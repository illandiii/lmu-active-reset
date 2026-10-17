# Physics-thread synchronization: 13 October 2026

Lock ownership, callback boundaries and the measured synchronization evidence.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Collegamento al thread fisico - 13 ottobre 2026

## Risultato nel gioco

Nella Practice offline del processo 14652, il nuovo osservatore congiunto ha rilevato Dynamics e ScoreUpdate sul thread 20020: 14 campioni stabili, nessuna sovrapposizione e nessun cambio di thread. Questa osservazione non dimostra da sola l'assenza di ogni altro scrittore nativo.

La v29 prepara ora il piano fisica/scoring dentro il callback Dynamics, risolvendo il puntatore scoring dal proprietario corrente. Il nuovo `restore_joint_native_preflight` applica controlli di binding, thread, sovrapposizioni e lettura/confronto di ogni valore atteso senza scrivere. Superati 13 piani da 10077 campi; il primo campione crea il checkpoint e non tenta il preflight.

La v30 osserva anche le chiamate naturali a Contacts e raccoglie i cinque argomenti, thread e timestamp per ciascuna ruota. Dentro Dynamics verifica gomme correnti, world, thread e freschezza (massimo 100 ms), quindi esegue il preflight a quattro ruote. Superati 13 piani congiunti e 13 controlli dei contatti; maschera 15, età massima degli argomenti 32 ms. Il punto di integrazione e i riferimenti dei contatti sono ora verificati insieme nel gioco.

**Nessun reset fisico è stato eseguito.** I contatti sono stati soltanto letti, senza sentinelle o chiamate aggiuntive al refresh. Il percorso di scrittura e ricostruzione resta verificato su memoria locale. Preparare un piano sul thread corretto non equivale a ripristinarlo nel gioco.

## Codice e test

- `phase_native_25661166.cpp`: osservazione dei due callback; fixture con stesso thread, chiamata annidata, migrazione del thread, lease, arresto e watchdog.
- `snapshot_observer_v29_25661166.cpp`: piano congiunto e preflight completo sul thread fisico.
- `snapshot_observer_v30_25661166.cpp`: acquisizione degli argomenti Contacts e preflight a quattro ruote nello stesso callback.
- `contact_batch_preflight_25661166`: validazione di sola lettura condivisa con la ricostruzione.
- Test negativi del preflight nativo: thread errato, scoring cambiato e contesto scaduto. I byte restano invariati.
- Replay dei 14 campioni v30: 406 controlli negativi, piano completo, esecuzione Win32 e rollback su memoria locale passati.

Build e fixture stop/watchdog di entrambi i nuovi osservatori passati. Audit finale: i tre moduli sono fermi, Dynamics/ScoreUpdate/Contacts hanno le istruzioni originali. I moduli restano residenti fino all'uscita dal gioco, senza hook attivi.

## Passaggi ancora aperti

Il blocco dopo recupero fallito ora ha un'implementazione nativa (`simulation_fence`, `fenced_restore`), verificata con hook reali nel processo delle fixture. Resta da collegarla al confine corretto del ciclo di LMU e da dimostrare la durata degli oggetti durante la transazione. Non è attiva nel gioco. Solo allora si può eseguire il primo ripristino fisico e verificare i contatti dopo riposizionamento, i record e il giro seguente. Restano i limiti di identità della sessione/setup e di copertura degli stati descritti nel documento precedente.

Non sono richieste manovre specifiche dell'utente per queste prove. UI sospesa.

Evidenze: `logs/20261013_joint_phase_evidence.json`, `logs/20261013_phase_joint_shutdown_audit.json`, catture phase/v29/v30 e relativo replay v30.

## Prosecuzione autonoma: protezione, contesto e REA

`simulation_fence` registra un confine esterno, scoring e contatti. Ammette una transazione soltanto sul thread proprietario, senza callback concorrenti o annidati. Dopo un recupero fallito impedisce sia la chiamata originale corrente sia le successive, e rifiuta lo sgancio degli hook. Un heartbeat non rimuove questo blocco. Le sette prove MinHook coprono successo, recupero e guasti; il modello comprende anche le scritture che il chiamante esegue dopo Dynamics. Questa protezione copre soltanto i callback registrati: non costituisce da sola un blocco degli allocatori o di tutti gli scrittori del gioco.

La v31 aggiunge una generazione del contesto all'IPC. Anche se nessun callback fisico arriva durante un intervallo non consentito, il successivo callback scarta il vecchio checkpoint. Prova reale: 14 campioni, generazione del contesto 1→3, checkpoint 1→2, 12 piani congiunti e preflight contatti validi. L'intervallo artificiale sospende soltanto l'osservatore per mezzo secondo. Non mette in pausa LMU e non modifica la vettura. Evidenza: `logs/20261013_context_generation_evidence.json`.

Il backend controlla tutti i permessi di destinazione prima della prima scrittura. Una pagina readonly nello scoring non può quindi causare un ripristino fisico parziale. I permessi vengono riutilizzati solo all'interno del singolo preflight; ogni scrittura li ricontrolla. Test passati anche per campi a cavallo di pagine, PAGE_NOACCESS, PAGE_GUARD e permessi cambiati fra operazioni. Il replay v30 passa tutti i 406 controlli negativi e il piano congiunto di 10077 campi. L'ottimizzazione dei permessi è verificata localmente; la v31 residente contiene ancora la versione precedente, senza cache.

REA/Ghidra ha completato le analisi in `analysis/rea-25661166/lifecycle-20261013` e `lifecycle-outer-20261013`. I byte delle tre routine esterne sono stati confrontati con il processo corrente. Catena confermata: `CD1F40 → CC8210 → F680A0 → ECB4E0 (Dynamics)`. Scoring `CF19F0` è chiamato da `CC8210` prima dell'aggiornamento fisico. Dopo Dynamics, `F680A0` esegue altre scritture e chiama `F59B50`; dopo `F680A0`, anche `CC8210` continua con callback e altre operazioni. Bloccare soltanto Dynamics non impedirebbe la prosecuzione su uno stato parziale.

`CD1F40` distingue il percorso di pausa tramite il byte `1A5A59D`, gestisce la copia PhysicsToBuffers e continua con servizi e input. La routine `CDB250` è chiamata con lo stato di pausa richiesto. Questi sono risultati statici, non un'autorizzazione tecnica a scrivere direttamente il byte: notifiche, sincronizzazione e durata degli oggetti devono ancora essere ricostruite.

Audit dopo v31: quattro moduli fermi, istruzioni originali di Dynamics, Contacts e ScoreUpdate, zero scritture fisiche. Evidenza: `logs/20261013_fence_context_shutdown_audit.json`.

## Ciclo esterno verificato nel gioco

Il nuovo `boundary_native_25661166.cpp` osserva anche `CD1F40`, conservando l'argomento uint64 del ciclo esterno. Prima della prova live, la fixture ha verificato le tre firme, le chiamate dentro e fuori dal ciclo, l'annidamento, il cambio di thread, il lease e il ripristino dei tre prologhi sia su stop sia su watchdog.

Cattura `20261013T184249498161Z_boundary_observer`: 4767 callback fisici e 14 campioni persistiti, tutti stabili. Profondità del ciclo 1, thread 20020, zero callback selezionati osservati fuori dal ciclo, zero sovrapposizioni e cambi di thread. La relazione inferita da REA è quindi confermata in questa esecuzione. Non dimostra l'assenza di scrittori esterni né il mantenimento in vita degli oggetti durante modifiche alla sessione.

L'osservatore ha completato l'arresto e ripristinato Dynamics, ScoreUpdate e OuterUpdate. L'audit successivo trova cinque moduli fermi e verifica anche Contacts: `logs/20261013_outer_boundary_shutdown_audit.json`. Riassunto dei campioni: `logs/20261013_outer_boundary_evidence.json`. Nessuna chiamata aggiuntiva alle routine del gioco e nessun reset fisico.

REA ha infine decompilato `CDB250` (`analysis/rea-25661166/pause-lifecycle-20261013`). La routine confronta lo stato richiesto, invia una notifica virtuale al gestore eventi, aggiorna `1A5A59D` e gestisce la scritta PAUSED alla ripresa. Non emerge un blocco degli oggetti dalla routine stessa. Scrivere il solo flag salterebbe la notifica; chiamare la routine non dimostrerebbe comunque che gli oggetti sono mantenuti in vita. Il risultato separa quindi la pausa della simulazione dal contratto di durata degli oggetti ancora necessario al ripristino.

## Integrazione dei comandi nel confine esterno

`active_reset_boundary_dispatch` collega il confine esterno al controllo save/load, al backend Win32, al coordinatore protetto e ai contatti. Conserva l'argomento uint64 dell'aggiornamento originale e lo inoltra una sola volta dopo successo o recupero riuscito; in quarantena sopprime la chiamata corrente e quelle successive. Congela gli adattatori per la durata della chiamata. Richiede ancora al chiamante una gestione effettiva della durata degli oggetti: non è caricato nel gioco.

`Build-ActiveResetBoundary.ps1` verifica questa catena su memoria del processo di test: salvataggio, ripristino, compensazione, perdita del proprietario durante la scrittura, recupero contatti fallito, comandi scaduti/duplicati e cambio sessione. Tutti i casi passano.

Corretto inoltre un percorso interno del rollback: il coordinatore ora ricontrolla il proprietario prima di ogni accesso al backend, anche durante il rollback della transazione. Una perdita osservata rimane definitiva per quella transazione e forza la quarantena. Le fixture provocano la perdita durante la lettura iniziale e dopo la prima scrittura; nessun accesso successivo raggiunge gli oggetti invalidati. I controlli non sostituiscono il blocco nativo richiesto fra controllo e accesso. Ripetuti con successo anche i test degli hook e il replay completo v30 da 10077 campi, con 406 controlli negativi.

## Sezione critica nativa individuata e osservata

La ricerca delle scritture al proprietario e alla lista vetture ha individuato i candidati di gestione del ciclo di vita in `analysis/rea-25661166/owner-lifecycle-20261013`. La ricerca decodifica le istruzioni nei runtime function del PE; non esclude scritture indirette o codice fuori da tali intervalli.

Il thunk `CD1F20` salta a `CD1F40`. `CD0BD0` registra il thunk nel puntatore globale `3B8A998` sotto la sezione critica `3B8A968`. Nel chiamante `FE27B0`, `FE2D65` chiama EnterCriticalSection su questa sezione; `FE2FF5` invoca il callback; `FE3009` chiama LeaveCriticalSection. Gli indirizzi IAT sono rispettivamente `10E4988` e `10E4978`. I byte di questi due corpi e di cinque candidati del ciclo di vita corrispondono al processo corrente: `live-code-crosscheck.json` nella stessa cartella.

`native_lock_witness` legge soltanto il proprietario e la ricorsione del blocco già posseduto. L'osservatore boundary v2 lo controlla all'ingresso del ciclo e durante Dynamics. Cattura `20261013T185930054223Z_boundary_observer_v2`: 4755 callback, 14 campioni tutti stabili, proprietario 20020, ricorsione 1, blocco già posseduto all'ingresso in tutti i campioni. Nessuna acquisizione/rilascio aggiuntiva del blocco. La fixture verifica anche una chiamata al ciclo esterno priva del blocco, rifiutata come campione stabile.

Questa è evidenza concreta della sincronizzazione già presente; la copertura del blocco sulle operazioni di distruzione/sostituzione deve ancora essere verificata prima di usarlo come garanzia di durata per il reset. Evidenze: `logs/20261013_native_lock_evidence.json`, `analysis/boundary-dispatch-20261013.json`. Audit finale: sei moduli fermi, tutti gli ingressi originali, zero scritture fisiche (`logs/20261013_native_lock_shutdown_audit.json`).

La batch REA sul ciclo di vita è terminata. `CCE090` inizializza il pool di 105 contenitori da 0x47318 byte. `F73DE0` cambia il controllo None/Player/AI/Remote e crea o libera il puntatore HDV: nel ramo None azzera il campo a `owner+159A8`, chiama il distruttore `EE3320` e libera 0x4C58 byte. Il fatto che il contenitore sia statico non mantiene quindi in vita la fisica. `CC2200` effettua lo smontaggio e azzera il giocatore; `CCD2D0` inizializza le vetture.

Nel chiamante `CD36B0`, EnterCriticalSection a `CD36CF` e LeaveCriticalSection a `CD3F0F` sulla sezione `3B8A968` racchiudono la chiamata all'inizializzazione `CCD2D0` a `CD3A7C`. Il percorso di smontaggio `CCFC60 → CC2200` richiede ancora la verifica della sincronizzazione esterna: la funzione chiama `CDA420` con stato 6 prima dello smontaggio, ma questo non è ancora dimostrato equivalente al blocco del worker. Prossimo punto concreto: ricostruire questo arresto e i chiamanti di cambio controllo, prima di attivare il writer nel gioco.

La protezione dei callback ora registra quattro gomme distinte, rifiuta configurazioni duplicate/invalide e impedisce tutte e quattro le chiamate native dopo un recupero fallito. Verificato sia nel test della barriera sia nelle sette fixture MinHook. La vecchia interfaccia a una gomma resta disponibile per le fixture precedenti; l'integrazione a quattro ruote deve fornire esplicitamente i quattro riferimenti.

## Uscita ordinaria e obbligo del blocco nel coordinatore

Chiarito il punto aperto sullo smontaggio ordinario: `CDA420` registra e cambia lo stato, senza acquisire un blocco o attendere il worker. La sincronizzazione sta nel ritorno da `CD0BD0`: acquisisce `3B8A968` a `CD1DA2`, azzera entrambi i callback `3B8A990` e `3B8A998`, rilascia il blocco a `CD1DC5`, quindi termina. Nel chiamante `CDB630`, la chiamata a `CD0BD0` a `CDD2DB` precede il ramo verso `CCFC60` a `CDD2EB`; un risultato zero torna invece nel ciclo. Il percorso ordinario di smontaggio attende quindi il callback corrente e disabilita quelli successivi prima di liberare le vetture. Evidenza statica nella cattura verificata: `session-callers.json`, `cdb630.instructions.txt`, `cd0bd0.instructions.txt` e `cda420.instructions.txt` in `analysis/rea-25661166/owner-lifecycle-20261013`. Questa conclusione non copre automaticamente altri chiamanti indiretti o i cambi di controllo.

Il coordinatore `active_reset_boundary_dispatch` ora richiede un riferimento esplicito al blocco nativo già posseduto dal chiamante. Lo controlla all'ingresso, durante gli accessi protetti e prima di proseguire nel callback originale. La perdita impone quarantena, invalida il checkpoint e impedisce di contare il reset come riuscito. Se il blocco viene perso durante `leave`, il coordinatore registra `FLOW_NATIVE_LOCK_LOST` senza richiamare un host la cui durata è già terminata. Il test distingue questa quarantena esterna da quella notificata prima dell'uscita.

Passati `Build-ActiveResetBoundary.ps1` (sei scenari, blocco mancante, comandi scaduti/duplicati e sessione cambiata) e `Build-SimulationFence.ps1` (barriera concorrente, flusso integrato e sette processi MinHook). Tutte le scritture sono su memoria delle fixture. Il writer del gioco resta disabilitato; non è stato effettuato un reset fisico reale.

La ricerca di chiamanti diretti di `F73DE0` ha trovato 36 riferimenti e due candidati non mappati (`control-callers.json`). Sono presenti percorsi che impostano None per tutte le 105 vetture (`CCE1A0`, `CCDD80`, `CCDA60`) e il passaggio Player/AI in `CD81F0`. Rimane da ricostruire la sincronizzazione dei loro chiamanti prima di considerare il blocco una garanzia completa di durata degli oggetti.

Restano: completare tale copertura; collegare il checkpoint completo e le quattro gomme all'adattatore reale; provare il primo ripristino e la compensazione in Practice; verificare stabilità fisica, transitori e validità di giro/record; concludere le prove delle hotkey e dei cambi di sessione/setup. UI rimandata.
