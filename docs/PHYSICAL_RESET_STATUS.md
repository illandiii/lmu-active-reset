> Aggiornamento 21 settembre 2026: ricostruzione nativa dei contatti provata da fermo sulla build 14150, con 207 scritture mirate e quattro ruote verificate; vedere [CONTACT_REBUILD.md](CONTACT_REBUILD.md). Nessun checkpoint dell'auto ripristinato. Le sezioni seguenti conservano la ricerca storica su 14130; la vecchia DLL rimane esclusa dalla nuova build.

# Reset fisico — indagine del 12 settembre 2026

## Esito della richiesta

**Nessun reset eseguito.** Non è disponibile un checkpoint fisico completo e non sono verificati il contratto di ripristino e la ricostruzione dei contatti. È stato osservato il thread di integrazione, ma non una finestra valida per ripristinare tutti i sottosistemi. L'analisi seguente è un avanzamento della ricerca, non una mod funzionante.

## Prima fase: lettura del codice, prima della DLL

Lettura ordinaria con `PROCESS_VM_READ | PROCESS_QUERY_INFORMATION` riuscita sul PID verificato. Controllati percorso dell'immagine, SHA-256 del file e intestazione del modulo. Letti 17.562.128 byte della sola sezione `.text`; nessuna lettura dell'heap. Il codice caricato è analizzabile e differisce da quello su disco. Non sono stati richiesti diritti di scrittura, attivati privilegi debug o aggirate negazioni di accesso al gioco.

Il file `live-code-findings.json` contiene 18 riferimenti RIP-relative, verificati sui confini delle istruzioni e sugli intervalli `.pdata`. Gli indirizzi sotto sono RVA della build fissata, riferimenti di analisi. In seguito è stato verificato e agganciato soltanto Integrate a 0xEAF350, come descritto nell'aggiornamento finale. Le altre routine non sono comandi di ripristino verificati.

| RVA | Evidenza | Significato e limite |
|---|---|---|
| `0xFC4E20` | Riferimenti ProcessQueue, PreInput, PostInput | Candidato ciclo fisico; thread, blocchi e finestra coerente non verificati dinamicamente. |
| `0xEAF350` | Profilo HighDetailVehicle::Integrate; chiamate a componenti e differenze di vettori | Individuata integrazione, non un metodo di salvataggio/ripristino. Gli offset interni non sono un layout completo. |
| `0xB89840` | Begin/Finished LoadGame; session preset, ambiente e setup | Caricamento con effetti sulla sessione; non il reset della sola auto richiesto. |
| `0xB8E230` | Costruzione memento, tempGarage.svm, chiamata `0xB8C4E0` | Percorso di salvataggio con I/O su disco, non checkpoint immediato in RAM. |
| `0xB8C4E0` | Percorsi Saves, estensione JSON, scrittura file | Conferma il percorso su file; mai chiamato dall'agente. |
| `0xB836C0` | Serializzazione contenitore; chiavi SessionPreset, Weather, VehicleSetup | Coordina dati dell'intera sessione. |
| `0xB16F20` | Begin/Finished Serializing Session; playerVehicle | Serializza sessione e richiama `0xB142C0` sul veicolo giocatore. |
| `0xB142C0` | slotID, fuel, battery, countLapFlag, bestLapTime, danni e ruote | Serializzazione di un record veicolo; nessuna prova che il record sia l'oggetto fisico autorevole. |
| `0xBCAA00` circa, dentro `0xBCA9A0` | Predicato di rete e rifiuto del salvataggio multiplayer | Indizio sul controllo interno; contiene anche una transizione di modalità, quindi non è stato trattato come getter puro. |
| `0xF3D4A0` | Differenza tra coordinate, soglia, messaggio teleported | Rileva e registra uno spostamento già avvenuto; non esegue il teletrasporto. |
| `0xA81DF0` | Lettura della sezione [TELEPORT] e GridIndex | Parsing di configurazione; non identificato come comando di checkpoint in guida. |

## Cosa serializza il record del veicolo

La lettura delle istruzioni di `0xB142C0` mostra chiavi e riferimenti per carburante, batteria, energia virtuale, temperature acqua/olio, danni aerodinamici, ruote, componenti staccati, usura e vita motore, oltre a giri, record, validità, penalità e strategia. Contiene strutture annidate e funzioni ausiliarie, non un blocco piatto da copiare.

Il campo fuel è referenziato a `record+0x4E0`, battery a `record+0x508`, wheels a `record+0x570`: questi sono offset del **record serializzato candidato**, non indirizzi dello stato vivo da scrivere. I campi di movimento completi non sono stati identificati nel percorso esaminato; ciò non dimostra che siano assenti da tutto il motore o da ogni sottofunzione.

La routine serializza prima slotID, poi salta gli altri campi se l'intero a inizio record è `-2`. Questo spiega un possibile caso di JSON ridotto a slotID. Il precedente GET getSaveJSON restituiva proprio solo slotID nel playerVehicle, ma non ne è stato usato o verificato il valore: il collegamento resta un'ipotesi, non la prova di uno snapshot acquisito.

## Perché non basta chiamare queste routine

Il memento comprende anche progressione e risultati. Applicarlo integralmente rischierebbe di riavvolgere dati che il requisito impone di conservare. Non è dimostrata la copertura di orientamento, velocità, trasmissione, stati delle sospensioni, contatti/collisioni, input e stato ibrido completo. Non sono stati verificati ownership, durata di vita, allocatori o convenzioni delle funzioni. La presenza di una serializzazione JSON non certifica il ripristino della fisica in movimento.

## Passo tecnico necessario

Su un avvio senza EAC verificato, occorre seguire il ciclo fisico e le chiamate al veicolo, inizialmente con osservazione: stabilire istanza autorevole, thread, vita degli oggetti e dipendenze di ciascun componente; individuare come il motore ricostruisce trasformazione, velocità e contatti; verificare separatamente invalidazione del giro e conservazione dei record. Soltanto questi contratti consentono un esperimento circoscritto di ripristino. Riavviare LMU, installare MinHook o aggiungere due hotkey non risolve da solo questo blocco.

Nessuna misura di latenza reset, nessuna prova di conservazione record e nessuna ripetizione di reset esistono ancora.


## Aggiornamento dopo la richiesta «Riesci a completarla?»

È stata aggiunta osservazione mirata dell'heap, oltre alla precedente lettura del codice. La catena candidata deriva dall'istruzione a RVA 0xB8E2AF: slot globale RVA 0x1DD94D8, puntatore proprietario, campo +0x15978. Prima e dopo la lettura vengono ricontrollati i puntatori e il contesto Practice singleplayer; le letture non sono atomiche rispetto alla simulazione.

Osservati quattro vettori di puntatori referenziati dall'integrazione: a +0x23F8, +0x2410, +0x2428 e +0x2440, con conteggi 13, 31, 3 e 2 in questa configurazione. Non sono stati assegnati tipi completi ai quattro gruppi. Il primo elemento dell'ultimo gruppo viene letto a sua volta dall'integrazione e contribuisce alle grandezze aggregate del veicolo.

Cinque osservazioni in guida, con campione telemetrico distante 0–78 ms, mostrano:

- il vettore del corpo a +0x40 segue la posizione mondiale; rispetto al riferimento esportato, lo scarto locale medio è circa (-0,00015; +0,00251; -1,17414) metri. L'escursione longitudinale fra i campioni è 6,9 mm, compatibile con campioni acquisiti in tempi diversi; non è una tolleranza certificata.
- corpo +0x28 e veicolo +0x2498 sono compatibili con velocità mondiale, dopo trasformazione della velocità locale esportata; gli errori del secondo variano tra 0,0012 e 0,0082 m/s.
- il campo +0xB0 vale 0,0025 s. Nel monitor i vettori dinamici tornano a zero.
- l'ipotesi iniziale che corpo +0x10 fosse posizione è smentita dai valori. La vecchia etichetta resta nei log grezzi per tracciabilità; non usarla come layout.

`component-comparison.json` e `component-comparison-analysis.json` conservano dati e calcoli. Il riferimento geometrico preciso, le unità di tutti i campi e l'ownership completa rimangono da verificare.

È stata corretta anche l'analisi degli intervalli PE: alcune procedure sono suddivise in più RUNTIME_FUNCTION collegati tramite UNW_FLAG_CHAININFO. Seguendo le catene si ricostruiscono, per esempio, i frammenti 0xEAF560, 0xEAF671 e 0xEAF963 della stessa procedura. Il frammento centrale combina posizione e velocità dei diversi corpi con coefficienti letti da +0x2458. Questo rafforza la necessità di ripristinare lo stato dei componenti, non soltanto i vettori aggregati. Formato verificato sulla [documentazione Microsoft](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170). Un intervallo interamente decodificato non equivale da solo a una funzione interamente compresa.

Il controllo esterno richiede ora NAV_REALTIME durante la guida: 25 osservazioni allowed dal vivo, 46 forbidden e una unknown nella prova finale, zero errori. Tutte mantengono reset_permitted=false. Il ritorno al monitor è stato verificato.

Questi risultati riducono l'incertezza sull'accesso alla fisica. **Non dimostrano ancora la fattibilità del reset completo e non permettono di garantirne la consegna.** Restano sincronizzazione, ricostruzione contatti, stato semantico di ciascun componente, motore/ibrido e gestione dei giri. Nessuna scrittura diretta nella fisica e nessun checkpoint ripristinato.


## Osservazione sul thread fisico: Mercedes GT3 a Spa

È stato compilato, provato su fixture e caricato nel gioco un modulo basato su MinHook 1.3.4. Il hook a RVA 0xEAF350 usa una firma verificata e controlli di hash/prologo esatti. Il controller rinnova un segnale di attività; il modulo interrompe le osservazioni quando manca il contesto valido e disattiva il hook alla scadenza del controller. Non espone alcun comando di reset.

Nella prova reale: 23.496 callback, 2.862 osservazioni pubblicate dal modulo, **71 campioni conservati nel log**. Il passo osservato è 0,416667 ms; gli spostamenti sono coerenti con velocità × passo. La velocità massima nei 71 campioni è circa 1,47 km/h: è una prova breve ai box, non un collaudo ad alta velocità. I tempi misurati della chiamata originale non sono latenze di reset né una misura completa del costo dell'hook.

A fine prova: status 3, error 0, istruzioni originali ripristinate e zero reset. Il ritorno al monitor è stato confermato dall'utente e verificato con una lettura successiva. La DLL rimane caricata, inattiva, fino alla chiusura di LMU: **chiudere completamente LMU prima di passare all'online**. Non esiste riarmo nella stessa istanza; non ricompilare sul file DLL caricato mentre LMU è aperto.

File principali: `observer-analysis.json`, `observer-manifest.json`, `RIGID_BODY_CONTRACT.md`, `rigid-body-layout-observation.json` e log `20260912T153551Z_physics_observer.*`. I sorgenti MinHook inclusi mantengono la licenza BSD in `third_party/minhook/LICENSE.txt`.

La nuova lettura numerica nel monitor conferma 12 corpi integrati sulla Mercedes, due dei quali contribuiscono all'aggregato. Matrici e inerzie rispettano le relazioni ricostruite dal codice. Mancano ancora i contratti di ripristino dei contatti e degli altri sottosistemi, e la gestione del giro interrotto. **Il reset fisico richiesto rimane incompleto.**
