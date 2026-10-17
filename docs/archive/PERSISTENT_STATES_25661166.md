# Persistent vehicle-state research

Steam 25661166 captures of fuel, inertia, tires, brakes and partial powertrain state.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Stati persistenti - Steam 25661166, 10 ottobre 2026

Il reset fisico resta incompleto. Questa estensione acquisisce e conserva più
stati e prepara payload numerici in RAM; non scrive la fisica del gioco.

## Implementazione

- `tire_persistent_core`: massa totale e otto masse settoriali, cache delle tre
  inerzie, fase di aggiornamento, campi nativi FB0/FB8/FC0. Conserva i settori
  originali; per i calcoli utilizza una copia delle dipendenze basata sulla cache.
- `checkpoint_persistent_bank`: conserva queste dipendenze insieme a corpi e
  vincoli e permette il cambio della massa pneumatico solo dopo la validazione
  della formula osservata. Nessuna tolleranza numerica è stata allargata.
- `checkpoint_vehicle_partial`: conserva quattro campi motore (1F8, 250, 260,
  280) e sette HDV (48A0, 48A8, 48B0, 48B8, 48E8, 48F0, 48F8).
- `checkpoint_transmission_partial`: conserva due campi dell'albero (0, 8),
  marcia del cambio (+64) e quattro input HDV (18, 20, 30, 38).
- Snapshot v4, v5 e v6: cattura di questi campi sul thread Dynamics del giocatore,
  con moduli separati e limiti di durata. Nessuna routine virtuale viene invocata
  dal probe esterno. Le fixture verificano chiamata originale, stop e watchdog.

Gli offset non interpretati restano esplicitamente nominati come tali. Non si
afferma che rappresentino tutto lo stato termico, motore, frizione o cambio.
Il tipo motore osservato è limitato alla vtable RVA 1793578 della build fissata.

## Evidenza nativa

Disassembly locale in `work/live-disassembly-25661166`:

- F120C0: massa interna = C18 meno otto masse a 28 + i*60, nell'ordine nativo;
  aggiornamento della massa a F123D6. Più avanti EF7020 aggiorna i settori.
- F1222A/F12389: uso e ricostruzione della cache C20/C28/C30 delle inerzie.
- ECDD90: routine FuelAndEngineTemps, letture e scritture dei sette campi HDV.
- Getter B19100: restituisce engine+1F8; la scala rad/s è supportata dalla
  precedente comparazione SDK. La vtable e il getter sono stati riverificati.
- ECD670 passa HDV+4900 ad ADA710, che usa +18 per l'albero e +20 per il cambio:
  i collegamenti correnti sono HDV+4918 e HDV+4920. Gli input provengono da
  HDV+18/+20 e dal prodotto (1-input30)*(1-input38).

## Prove

| Cattura | Campioni | Preparazioni dal primo stato valido | Rifiuti |
| --- | ---: | ---: | ---: |
| 130734 snapshot v4, 29,68 s | 234 | 234 | 0 |
| 131113 snapshot v5, 29,6 s | 234 | 234 | 0 |
| 131416 snapshot v6, 59,6 s | 477 | 271 | 206 |

UTC del 10 ottobre. V4/v5 hanno corpi e carburante variabili, ma i campi motore
e termici della v5 sono rimasti costanti: non è una prova delle loro transizioni.
La v6, durante la guida dell'utente, copre marce -1, 0, 1, 2, 3, 4, 5 e regime
da 190,87 a 982,51 rad/s. Il replay verifica la conservazione byte per byte dei
campi salvati, corpi, impulsi e dipendenze originali. Le 2.037 verifiche negative
comprendono valori non finiti, tipo sconosciuto, cambio sessione e rifiuti nativi
previsti, senza pubblicare output o sostituire il banco precedente.

Nel test dinamico le inerzie coincidono usando la cache persistente. In 206
campioni almeno una massa interna non coincide invece con il residuo dei
settori correnti. L'ordine nativo consente un aggiornamento dei settori dopo
quello della massa: va rappresentata e verificata anche questa cache temporale.
Il replay calcola indipendentemente il mismatch per evitare di mascherare
qualsiasi errore come un rifiuto atteso. Questi campioni restano rifiutati.

## Estensione successiva: cache, banco nativo e piano di scrittura

La discrepanza della massa è risolta conservando la cache originale a tire+AC8,
confrontata con la massa inversa a tire+B18. `checkpoint_cached_mass` non ricava
questa cache dai settori correnti e non modifica quelli salvati. La prova v7
ha 458 campioni accettati e 3.664 controlli negativi. Il replay sintetico della
v6, che usa il reciproco della massa inversa, è separato e non è evidenza nativa.

`checkpoint_solver_history` aggiunge forze, coppie, contatore DWORD dei corpi,
bias/coefficienti/Jacobiani/pesi delle righe dei vincoli e HDV+46C8. V8:
236 preparazioni e 2.360 controlli negativi. Le cache storiche vengono conservate;
gli impulsi non vengono applicati nuovamente alle velocità già integrate.

`checkpoint_engine_integration` aggiunge sette double a engine+208, tre float
a +240 e fino a due coppie di stato a +98 con passo 70. Il numero di canali
osservato è zero: i rami con uno o due canali non sono validati nel gioco.
I nomi basati sugli offset non attribuiscono unità o semantica non dimostrate.

Dalla v9, il banco RAM è dentro LMU: salvataggio e preparazione avvengono sullo
stesso thread Dynamics. La v11 termina con 1 salvataggio, 371 preparazioni e
zero rifiuti nell'ultimo frame persistito; 225 frame passano il replay e 2.700
controlli negativi. La mediana di cattura + preparazione + piano è 541,2 µs,
massimo 948,7 µs: NON è una misura della latenza di reset.

La v10 aveva invece rifiutato tutti i salvataggi per un errore nel validatore:
il contatore di fase pneumatico può raggiungere 0x80000, mentre 0x7FFFF è la
maschera usata al passo seguente. Il limite errato è stato rimosso. Il replay
corretto della v10 passa 227 campioni, ma questo non cambia l'esito nativo
negativo della DLL v10 già arrestata. La v11 verifica la correzione nativa.

V12 aggiunge `NativeCache`: massa, inerzia locale, inversa mondiale e velocità
angolare lette direttamente per tutti i corpi; primi momenti e due aggregati
per ciascuno degli otto settori di quattro pneumatici. I valori esatti
sostituiscono nel piano quelli derivati, mantenendo il collegamento alla sequenza
salvata. Sono 223 frame in 29,68 s, tutti accettati, e 6.467 verifiche negative.
La cattura dei settori NON include la griglia interna dei nodi pneumatico.

`vehicle_restore_plan` e `native_cache_plan` producono un piano di 821 campi,
19.648 byte, per la vettura osservata. `restore_transaction` esegue il piano su
memoria di prova, verifica ogni scrittura e ripristina i valori precedenti in
caso di errore, inclusa una scrittura parzialmente riuscita. Test superati:
fallimento parziale in ogni posizione della fixture, falso successo del backend,
perdita di contesto, rollback fallito segnalato, dati correnti diversi, limiti e
sovrapposizioni. Il test del piano reale verifica anche i byte non interessati.

`vehicle_memory_bindings` aggiunge una lista tipizzata degli esatti offset e
dimensioni ammessi, con controllo delle sovrapposizioni degli indirizzi assoluti
fra slot diversi. L'osservatore v13 usa riferimenti congelati e questo filtro.
Il backend nativo resta di sola lettura: callback di scrittura assente.

L'identità del processo usa il FILETIME di creazione reale. La generazione del
banco avanza all'invalidazione osservata (lease, topologia, configurazione o
tempo). Non equivale a un identificatore completo e certificato di sessione:
il token di configurazione copre solo i parametri già acquisiti. Gli eventi SDK
sono una coda, non contatori monotoni di sessione. Le identità dei replay Python
restano sintetiche. `full_restore_supported` è sempre zero.

## Lavoro ancora necessario

- Completamento della storia pneumatici/freni; altri stati interni dei
  gruppi motore, frizione, cambio e differenziali; danni ed energia.
- Verifica dinamica estesa degli stati di rotazione ruota/mozzo collegati dalla
  v16, incluse le condizioni non ancora supportate di ruota staccata/deformata.
- Copertura completa dei parametri e della vita di sessione/vettura/configurazione.
- Collegamento del backend Win32 al reset nel gioco e ricostruzione dei contatti
  della build attuale: il backend e' collaudato soltanto nel processo fixture.
- Scoring: invalidazione del tentativo interrotto senza perdere tempi precedenti,
  con verifica del primo giro completo successivo.
- Hotkey configurabili e prove end-to-end di salvataggio e reset in movimento.

Nessuna scrittura fisica è stata eseguita. I moduli caricati non vengono
sovrascritti né scaricati a caldo; ogni revisione usa una DLL distinta e viene
arrestata ripristinando le istruzioni originali di Dynamics.

## Griglia termica e accumuli: v14/v15

Il percorso EF7020 usa 72 sezioni angolari, sette nodi di battistrada con passo
110 e due elementi termici con passo 60, nei pool A10/A18. La tabella A28 viene
confrontata con questi tre pool e riordinata in una rappresentazione canonica
senza puntatori. EFA860 aggiorna cinque double a +0/+8/+10/+18/+20 usando quattro
coefficienti locali a +40/+48/+50/+58 e parametri del materiale. La cattura
comprende tutti questi valori per 2.592 elementi: 187.776 byte numerici e indici.

`checkpoint_tire_thermal` conserva e prepara la griglia in RAM. La v14 acquisisce
24 frame coerenti: 1 salvataggio e 23 preparazioni native, nessun rifiuto. In 23
frame la griglia cambia rispetto al primo. Replay: 24 recuperi esatti e 120
controlli negativi, senza modificare banco/output in caso di errore. Cattura ogni
320 callback per limitarne il costo: mediana 2.072,25 microsecondi, massimo
2.520,7; questa misura comprende cattura e preparazione, non un reset.

La v15 aggiunge `checkpoint_tread_history`: tre double (+28, +30, +100) per
2.016 nodi del battistrada. +28 viene passato all'integrazione e reinizializzato;
+30 entra nel getter della massa 3AC3E0; +100 viene aggiornato e limitato in
EF7020. Sono nomi di offset: l'identificazione completa di usura e storia resta
da dimostrare. I 24 frame della v15 passano il recupero RAM e 120 verifiche
negative. Questi tre campi restano costanti nella prova a vettura ferma: nessuna
transizione dinamica è stata verificata per essi.

Il banco combinato termico/storia occupa 236.168 byte. La preparazione termica è
collegata alla stessa sequenza del banco meccanico nativo; gli indici o i pool
cambiati invalidano la compatibilità. **Dalla v17 la griglia e questi accumuli
sono inclusi nel piano di scrittura**, verificato soltanto su memoria isolata.
La configurazione completa dei materiali,
altri stati dei nodi e la storia di contatto non sono dichiarati coperti.

## Contatti della build corrente

Il modulo `contact_native_25661166` riloca il refresh a EF8880 e il riferimento
mondo a 1AA4090, derivato dal chiamante F12406/F1241F. Le fixture passano stop,
watchdog e il modello di ricostruzione monouso. La prova nel gioco è solo
osservativa: 96.346 chiamate, 2.980 osservazioni native, 916 campioni delle ruote
persistiti senza errori; intervalli di contatti 52-53, 52, 50-51, 57. Nessuna
variazione della posa durante il refresh e tutti i contatti finali positivi.
Il controller della build corrente rifiuta `--rebuild-once`: manca la verifica
del gate di scrittura nativo aggiornato e della ricostruzione dopo spostamento.
I risultati di ricostruzione delle vecchie build non sono trasferiti alla nuova.

Evidenze consolidate: `logs/20261010_native_bank_summary.json`, le registrazioni
v14/v15 e i rispettivi rapporti `.thermal-replay.json` /
`.tread-history-replay.json`, oltre a
`20261010T135631421544Z_contact_25661166.observation-analysis.json`.
L'audit aggiornato legge le IPC dei 19 snapshot, del modulo contatti e dello
scoring, verifica
stato arrestato/errore zero e confronta i prologhi Dynamics/Contacts originali.
Nessun hook del progetto resta attivo nell'istanza verificata.

## Integrazione v16-v18 e scoring

La v16 cattura angolo/velocità a wheel+120/+128 e verifica la relazione fra
ruota e mozzo; il piano passa a 825 campi. Replay: 24 recuperi, 23 variazioni,
120 controlli negativi. Ruote staccate o deformate vengono rifiutate.

La v17 collega 9.216 campi termici/storici tramite 2.592 riferimenti congelati
e filtrati per tipo. Il piano passa a 10.041 campi e 254.720 byte. Il controllo
degli intervalli sovrapposti usa ordinamento, evitando confronti quadratici.

La v18 aggiunge cinque double a wheel+5D8, uno a +630 e la cache +138;
+5D8 era già nel piano meccanico e viene verificato senza duplicare la scrittura.
Riferimenti: chiamante ECD890 e integrazione C6ED00. La cache +138 concorda con
l'inerzia inversa del corpo ruota. Piano finale: **10.053 campi, 254.912 byte**.
24 frame, 23 variazioni dei freni, 72 controlli negativi specifici; 24 replay
meccanici e 696 controlli negativi. Transazione e rollback dell'intero piano
passano su memoria isolata. Non comprende il flag guasto a game_vehicle+1CF9C.

Lo scoring Steam 25661166 risiede a game_vehicle+1CEC8. Il cache intero
last_computed è +27360: +2735C è ora un accumulo float. Il primo probe delle
14:26:21 UTC è esplicitamente superato nel profilo scoring revision 2. Il piano
separato modifica solo current/remembered/last_computed (12 byte); preserva i
flag next_timed/off_track. Fixture: scrittura, readback, rollback parziale su
ciascuno dei tre campi, rifiuto di dati/context cambiati e del vecchio offset;
108 combinazioni del modello giro. Nessuna applicazione nativa o prova reale
dei record e del giro successivo è ancora avvenuta.

## Ultimo stato: v19, scoring e backend Win32

La v19 include tre cache di coppia del differenziale a HDV+49D8/+4A18/+4A58,
aggiornate in ADB300 e lette dal percorso ADAFF0/ADC3A0. Non sono tre nuovi
oggetti: sono valori numerici nel gruppo driveline a HDV+4900. Il tipo di
trazione entra nell'identità di topologia e deve coincidere al recupero.
24 preparazioni dal primo campione, 14 variazioni e 96 controlli negativi.
Il piano integrato contiene ora **10.056 campi / 254.936 byte**. Restano mancanti
la copertura completa di frizione/cambio/differenziale e i relativi transitori.

L'osservatore scoring intercetta CF19F0, chiamato da CC86BF con score* in RCX e
float in XMM1. Le fixture verificano inoltro esattamente una volta, selezione
del giocatore, lease, flag invalidi, stop e watchdog. Prova reale delle 14:39 UTC:
1.010 callback, 995 osservazioni, 153 frame persistiti, zero scarti; thread 14584,
uguale a Dynamics in questa sessione. Tutte le proposte azzerano i tre cache e
preservano next_timed/off_track. Giro fermo a 8 e flag costanti: **nessuna prova
di transizione giro, invalidazione reale o conservazione record**. Il tempo
mediano di 6,8 microsecondi comprende anche l'update originale.

`native_memory_backend` implementa il backend Win32 nel processo
corrente: verifica thread, congela i binding, ammette solo campi tipizzati e
pagine dati scrivibili, scrive/rilegge e mantiene i riferimenti originali durante
il rollback. È collegato solo alla libreria fixture. I test eseguono tutti i
10.056 campi su memoria propria e provocano perdita del contesto a metà:
rollback esatto, nessun byte esterno al piano modificato. Il test separato
verifica anche thread errato, tentativo di rebind e pagina di sola lettura.
Il backend non costituisce una certificazione della copertura del checkpoint.

La revisione contatti v2 è compilata e passa fixture stop/watchdog e preflight.
Il gate nativo usa il vettore corpi corrente HDV+23F8 e richiede velocità lineari
e angolari basse su tutti i corpi, oltre a Practice offline, box e folle dai
dati SDK. Il controller richiede anche il gate nativo positivo per due secondi
prima di armare una sola ricostruzione. **V2 non ancora caricata né provata nel
gioco**: si attende la vettura ai box in folle. La precedente v1 resta arrestata.

Suite Python: 44 test passati. L'audit include ripristino dei prologhi di
Dynamics, Contacts e ScoreUpdate; tutti i 21 moduli osservati sono arrestati,
zero scritture fisiche e zero scritture scoring. Le DLL restano residenti fino
alla chiusura di LMU. Hotkey e reset operativo completo non sono implementati.
## Aggiornamento 12 ottobre: REA e v20

Successiva prova dinamica v22: 70 campioni, 16 variazioni di marcia e 69 variazioni
di due output trasmissione. 68 preparazioni native e zero rifiuti; replay completo
su memoria fixture superato. V21/v22 rifiutano i guasti terminali non ripristinabili.
Dettagli aggiornati nel rapporto REA; il reset completo resta non operativo.

REA/Ghidra hanno decompilato sei routine dal codice caricato. Aggiunto il banco
di quattro output HDV+46D0/+46E0/+46E8/+4B38, osservato sul thread Dynamics.
49 campioni live, nessun errore di cattura; piano 10.060 campi / 254.968 byte.
Replay e rollback su memoria fixture superati. Output costanti e marcia zero:
nessuna prova di transizione dinamica o ripristino fisico. Dettagli e limiti in
[REA_ANALYSIS_25661166.md](REA_ANALYSIS_25661166.md).



## Stato aggiornato dopo v25, 12 ottobre

Le note precedenti sono cronologiche e superate dove indicano che la prova
contatti v2 non è stata eseguita. La ricostruzione ai box è riuscita su quattro
ruote: 211 profondità ritirate, nodi ricostruiti nativamente, nessuno spostamento.
Il checkpoint include ora controllo powertrain e transitori cambio: 10.074
campi fisici / 255.090 byte, 10.077 campi con scoring nella transazione locale.
V25: 55 campioni persistiti in folle, 54 preparazioni, zero rifiuti; test integrali
Win32 e rollback superati su memoria posseduta. Nessun checkpoint completo è
stato caricato nel gioco. Tutti i moduli sono arrestati; Dynamics/Contacts
originali. Consultare REA_ANALYSIS_25661166.md per prove, hash e limiti attuali.
