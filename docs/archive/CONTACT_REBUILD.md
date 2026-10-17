# Native tire-contact reconstruction

Build-specific contact experiments on 14150 and 14200, including four-wheel verification.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Ricostruzione contatti - stato al 21 settembre 2026

Aggiornamento: la ricostruzione da fermo è stata migrata e provata anche sulla
**14200 il 3 ottobre**, con 194 contatti. Routine, risultati e limiti sono in
[DEVELOPMENT_20261003.md](DEVELOPMENT_20261003.md). Gli indirizzi e i comandi
14150 riportati sotto restano evidenza storica, non istruzioni per la build attuale.

**Ricostruzione nativa dei contatti implementata e provata da fermo sulle quattro ruote.** Nella Practice offline 14150 del 21 settembre sono stati ritirati 207 contatti; il percorso nativo ne ha ricostruiti 208, con normali unitarie e riferimenti di superficie coerenti. Sono state effettuate scritture mirate nel processo. Non è stato salvato o ripristinato un checkpoint, né spostata l'auto.

La prova copre la rigenerazione geometrica alla posa corrente. Restano da validare il caso successivo a un riposizionamento, il ripristino delle storie dinamiche dello pneumatico e l'integrazione con l'intero stato dell'auto. La compatibilità generale Active Reset resta `false`.

## Percorso trovato nella build 14150

1. HDV Dynamics `0xEB79E0` esegue sei sottopassi. Prima di forze e integrazione, chiama l'aggiornamento ruota `0xED0E70` per le quattro ruote, a `HDV+0x618`, passo `0x6B0`.
2. L'aggiornamento ruota passa l'oggetto pneumatico `*(wheel+0x3E8)` a `0xEFE6F0`. La funzione non è una semplice ricostruzione di contatti: aggiorna anche temperature, storie di forza e un corpo interno al pneumatico.
3. Ai sottopassi selezionati dal resto della divisione per tre, il parametro di aggiornamento geometrico è diverso da zero e `0xEFE6F0` richiama `0xEE5030`. Contratto agganciato: `void(tire*, double dt, double horizon, world*, double angularSpeed)`, convenzione Windows x64 (RCX, XMM1, XMM2, R9, quinto argomento sullo stack). La fixture verifica tutti e cinque gli argomenti e la singola chiamata originale; dal vivo sono stati osservati `dt=1/2400 s`, `horizon=3*dt` e il riferimento mondo atteso a RVA `0x1A870C0`.
4. `0xEE5030` elimina elementi scaduti, predispone una posa prevista del corpo interno, costruisce campioni geometrici, interroga il mondo attraverso `0x1001170` e aggiorna nodi di contatto. Tiene conto anche di correzioni di superficie tramite `0xD49230`; quindi un semplice piano sotto il centro ruota non riproduce questo percorso.
5. Alla fine richiama `0xEE6B60` con la struttura `tire+0xA60`, il riferimento `*(tire+0xFE8)` e una vista dei contatti attivi. La routine associa i nodi alle superfici tramite l'indice `uint16` a `node+0x62`, mantiene una tabella di elementi da `0x58` byte e ne marca l'utilizzo in un bitset. Ogni elemento include superficie, materiale, riferimenti a dati dei vertici e coefficienti geometrici. Può crescere il vettore con gli allocatori nativi; non va sostituita con una copia di puntatori. Il confronto iniziale con l'indice memorizzato non ha un controllo di bounds locale: il modulo verifica tale indice prima di intervenire.

Il percorso `0xEB9B10 -> 0xED0360` riguarda le trasformazioni delle ruote. Non basta a ricostruire l'impronta del pneumatico. Anche la query `0x1004600`, che restituisce una superficie/altezza e opzionalmente una normale, è distinta dalla query multicampione impiegata da `0xEE5030`.

## Strutture candidate ricavate dalle istruzioni

Gli offset seguenti sono relativi all'oggetto pneumatico o ai suoi nodi, **non alla telemetria condivisa**. Sono riferimenti di ricerca per la build esatta, non un'autorizzazione alla scrittura.

| Oggetto e offset | Evidenza | Limite |
|---|---|---|
| Pneumatico `+0x318` | base di nodi indicizzati con passo `0x110` | dimensione e vita dell'intero pool non dimostrate |
| Pneumatico `+0x9F0/+0x9F8/+0xA00` | begin/end/capacity del vettore di puntatori ai contatti attivi | possono cambiare durante l'aggiornamento |
| Nodo `+0x60` | flag impostato quando il nodo viene aggiunto, azzerato alla rimozione | non basta per invalidare tutte le cache |
| Nodo `+0x67` | azzerato durante la rimozione | ruolo completo da determinare |
| Nodo `+0x62` | indice a 16 bit nella cache delle superfici | deve restare nei bounds e riferire la stessa superficie di `+0xF8` |
| Nodo `+0xD0` | massimo di una distanza orientata; inizializzato con sentinella negativa | candidato profondità firmata, non forza di contatto |
| Nodo `+0xD8` | tre componenti normalizzate usate nella distanza orientata | normale candidata, da confrontare dal vivo |
| Nodo `+0xF0` | prodotto scalare normale/punto | termine di piano candidato |
| Nodo `+0xF8` | riferimento alla superficie scelta | non conservabile come semplice puntatore di checkpoint |
| Pneumatico `+0xAC0` | corpo interno integrato e accoppiato al corpo ruota | il suo stato è distinto dall'aggregato dell'auto |
| Pneumatico `+0xA30` e `+0xA98` | raccolta temporanea e struttura di ricerca geometrica | contratto di ricostruzione incompleto |
| Pneumatico `+0xA60/+0xA68/+0xA70` | vettore della cache superfici, passo `0x58` | mantenuto dal percorso nativo, mai copiato nel checkpoint |
| Pneumatico `+0xA78/+0xA80/+0xA88` | vettore di parole a 32 bit della maschera di utilizzo | ogni contatto attivo deve avere il bit corrispondente impostato |

Riferimenti puntuali: rimozione a `0xEE50F0..0xEE5168`; inserimento e crescita del vettore a `0xEE62C8..0xEE63ED`; aggiornamento distanza/normale/piano/superficie a `0xEE63F1..0xEE6483`; chiamata finale a `0xEE6907..0xEE693A`.

La rimozione nativa scambia un elemento con l'ultimo e riduce la fine del vettore. Azzerare soltanto il conteggio lascerebbe i flag dei nodi incoerenti. Il modulo non cancella vettori o puntatori: scrive la sentinella negativa nativa nel solo campo profondità, all'ingresso della funzione sul thread fisico, quindi lascia che la chiamata originale effettui rimozione, query e aggiornamento cache. La sola scrittura non è considerata un successo: vengono controllati anche i risultati dopo il ritorno.

Le routine senza unwind entry `0x1001BD0` e `0x1002930` sono state esaminate con decodifica lineare limitata. Il prefisso mostra aggiornamenti di bounds geometrici dal basso verso l'alto; non sono semplici funzioni di svuotamento. Il limite di 256 byte non ne copre tutto il contratto. L'esperimento le lascia eseguire al percorso nativo senza chiamarle direttamente.

## Lettore esterno e verifica iniziale

`src/python/sector/research/probe_contacts_readonly.py --pid <PID_ATTUALE>` verifica hash, processo, UI e Practice attiva; rifiuta anche il menu principale quando conserva un'etichetta Practice precedente. Risolve la base del processo e la catena proprietario derivata dal codice di salvataggio 14150, poi osserva quattro pneumatici, nodi e collegamenti alla cache superfici.

Limiti di lettura: massimo 512 contatti per ruota, 512 KiB totali, letture individuali entro 4 KiB. Verifica allineamento degli indici, unicità dei puntatori e stabilità di proprietario, vettori e appartenenza dei contatti prima/dopo. Questi controlli non forniscono atomicità e non escludono un riciclo di oggetti allo stesso indirizzo. Un cambiamento rilevato scarta l'osservazione.

I log conservano valori e indici relativi; non salvano indirizzi di processo o riferimenti di superficie. Ogni errore ha un proprio esito registrato. Le letture reali delle 17:48 e 17:53 UTC hanno rilevato circa 50 contatti per ruota; nella seconda, nessun indice, riferimento o bit di utilizzo incoerente. La lettura asincrona successiva delle 18:07 è stata correttamente scartata perché il vettore cambiava durante l'osservazione. Non costituisce un fallimento della ricostruzione: le verifiche coerenti sul thread fisico hanno zero errori.

## Modulo nativo e limiti operativi

`contact_native.cpp`, `contact_ipc.h` e `run_contact_experiment.py` implementano un esperimento separato dalla vecchia DLL 14130. Hash EXE esatto, prologo esatto, ownership del giocatore e riferimento mondo sono verificati. Il modulo osserva il ritorno della funzione sulle quattro ruote; il writer deve restare sullo stesso thread. Non chiama l'intero aggiornamento pneumatico una seconda volta e non avanza artificialmente il tempo.

La richiesta è monouso, con scadenza di tre secondi. Prima di armarla servono almeno 64 osservazioni valide per ruota e due secondi di condizioni stabili: Practice offline verificata da UI e telemetria, un veicolo, guida locale, auto ai box, folle, acceleratore nullo e velocità entro 0,01 m/s. Il freno non è necessario per questa prova senza spostamento. All'applicazione il modulo ricontrolla il permesso recente (massimo 300 ms), owner, valori dei contatti, indici, normali, accessibilità in scrittura dei soli campi profondità e stato nativo. La telemetria viene letta con tentativo non bloccante del lock SDK, rilasciato prima di ogni modifica. Si controlla anche la velocità viva del corpo auto.

Tutti i nodi della singola ruota vengono validati prima della prima scrittura. Le quattro operazioni avvengono nelle rispettive chiamate naturali e non costituiscono una transazione atomica di ripristino auto; un errore viene riportato con una maschera parziale e blocca ulteriori scritture. Nel test reale tutte e quattro sono terminate nello stesso tick millisecondo. Nessun puntatore di superficie, indice, vtable o allocatore è scritto dal modulo.

Dopo il ritorno devono risultare contatti presenti con profondità positive, normali unitarie, riferimenti/bit cache corretti, flag `+0x67` azzerati e posa interna invariata. Il watchdog disattiva l'hook se il controller perde contatto, allo stop o dopo quattro minuti. I byte originali vengono confrontati dopo la disattivazione. DLL, mapping e trampoline rimangono residenti fino alla chiusura: non c'è hot-unload né una seconda richiesta nella stessa istanza. Chiudere quell'istanza prima dell'uso online.

## Risultato reale del 21 settembre, 18:06 UTC

Evidenze: `logs/20260921T180631160598Z_contact_experiment.summary.json`, `.analysis.json`, `.observations.jsonl` e `.native.jsonl`. Build 14150; Practice1 singleplayer; PID 17028; thread fisico 30320. SHA-256 DLL: `8ef7a05adfac0f2e38d18ac6b268ffb71603fb73c79a982499e4b11e872a2efb`.

| Ruota | Contatti ritirati | Contatti ricostruiti | Errori di invarianti | Durata della sola chiamata nativa |
|---|---:|---:|---:|---:|
| 0 | 50 | 51 | 0 | 12,4 µs |
| 1 | 50 | 50 | 0 | 9,2 µs |
| 2 | 54 | 54 | 0 | 8,8 µs |
| 3 | 53 | 53 | 0 | 9,7 µs |

207 scritture a `node+0xD0`; 208 contatti finali tutti positivi, con flag `+0x67` azzerato. Il conteggio può cambiare perché viene ricalcolata la geometria. Sono state pubblicate 988 osservazioni coerenti, senza scarti o errori, su 32.400 chiamate intercettate. Differenza della posa interna attraverso ogni chiamata: zero. Le durate non includono l'osservazione del wrapper e non misurano la latenza di un Active Reset.

Nei 117,156 secondi di telemetria successiva (1.064 campioni), velocità massima 0,00824 m/s e distanza massima dalla posizione precomando circa 0,173 mm; nessun valore di movimento non finito. Sono misure di questa prova statica, non una garanzia per il caso in movimento. L'hook è terminato con stato 3, errore 0 e istruzioni originali ripristinate. Il lettore è uscito normalmente: 1.108 campioni totali, quattro tentativi saltati per contesa del lock, nessun lettore rimasto in esecuzione.

Non era presente un miglior giro valido (`-1`): conservazione record e validità del giro dopo reset restano da provare. Non è stata effettuata alcuna scrittura dello scoring, della posa, del carburante, del motore o dello stato ibrido. Nessun file del gioco o configurazione è stato modificato dal progetto.

## Riproduzione

`Build-Contact.ps1` compila DLL e fixture, esegue due processi di test (stop esplicito e watchdog) e aggiorna il manifest soltanto se passano. Le fixture verificano ABI, singola chiamata originale, osservazione delle quattro ruote, errore sull'ultimo nodo senza scritture precedenti, rifiuto di movimento/lease/scadenza, monouso e ripristino dei byte. Sono un modello del ciclo nativo, distinto dalla prova reale sopra. Nove test Python coprono lettore, cache, requisito di sosta e osservazioni recenti.

Su una nuova istanza offline senza EAC, già in Practice:

```powershell
## Solo controllo preliminare: nessuna DLL caricata.
python .\src/python/sector/observers/run_contact_experiment.py --pid <PID_ATTUALE>

## Esperimento esplicito, con auto ferma ai box in GUIDA e in folle.
python .\src/python/sector/observers/run_contact_experiment.py --pid <PID_ATTUALE> --seconds 120 --load --rebuild-once --offline-launch-confirmed
```

Il secondo comando osserva prima di intervenire e non forza condizioni sconosciute. Senza `--rebuild-once`, il caricamento abilita solo osservazione. I profili fisici precedenti rimangono invariati; il nuovo profilo non abilita un checkpoint completo.
