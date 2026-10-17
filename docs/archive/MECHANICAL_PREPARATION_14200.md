# Mechanical preparation on build 14200

Body/constraint geometry, warm-start calculations and accumulated impulse comparisons.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Preparazione meccanica e vincoli — continuazione del 3 ottobre

## Continuazione del 10 ottobre: stato degli impulsi e aggiornamento LMU

Implementato `constraint_warm_group`: memoria tipizzata degli impulsi e relativi
limiti, corrispondenza con identità/tipo/indici del vincolo, applicazione in ordine
alle velocità dei corpi condivisi. Un dato invalido nell'ultimo vincolo lascia
invariato tutto l'output. Questa operazione è solo numerica e richiede velocità
precedenti al warm-start: applicarla a velocità già elaborate dal solver sarebbe
un doppio impulso. I limiti sono ancora input forniti dal percorso nativo;
non viene rivendicata la copertura di bias, plasticità o storia completa.

Build: `Build-ConstraintWarm.ps1`. Fixture di accumulo su tre corpi condivisi,
clamp e rifiuto senza output parziale superata. Replay indipendente dei quattro
tipi: 609 campioni, errore massimo `1,78e-15`; rapporto
`logs/20261010_warm_group_replay.json`. Le registrazioni sono quelle del 3 ottobre.

LMU ora installato: Steam `25661166`, EXE SHA256
`66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf`.
La versione dichiarata è ancora 1.4.2.0 / 14200. Adapter e UI sono cambiati,
i tre header SDK sono identici. Non sovrascritti i profili precedenti.

`relocate_steam_25661166.py --pid <PID>` cerca firme nel codice caricato, poi
confronta le istruzioni archiviate. Nessuna lettura heap, hook o scrittura.
L'immagine su disco non produceva corrispondenze; il confronto usa `.text`
del processo. Candidati univoci per i prefissi di 64 istruzioni:

| Routine | RVA aggiornato | Confronto con riferimento archiviato |
| --- | --- | --- |
| Dynamics | `ECB4E0` | 426 istruzioni, 1 differenza di campo |
| Contacts | `EF8880` | 1.401 istruzioni, 0 differenze normalizzate |
| Forces | `ECAB30` | 249 istruzioni, 4 differenze di campo |
| Bar | `3EF740` | 410 istruzioni, 0 differenze normalizzate |
| Ball | `3E4030` | 552 istruzioni, 0 differenze normalizzate |
| Hinge | `3F57C0` | 321 istruzioni, 0 differenze normalizzate |
| Ori | `3FC4B0` | 379 istruzioni, 0 differenze normalizzate |

I riferimenti completi sono disponibili per i quattro giunti; gli altri confronti
riguardano solo i frammenti archiviati. Call/branch e target RIP sono normalizzati:
il confronto non prova l'equivalenza dei target o la compatibilità dell'ABI.
Le differenze reali individuate sono `18B40→18B48` in Dynamics e
`8FE8→8FF0`, `8FD8→8FE0`, `18B58→18B60`, `18B54→18B5C` in Forces.
Non propagare uno spostamento uniforme a tutti gli altri campi.

Il lettore SDK ha un nuovo profilo separato
`compatibility/steam-25661166-telemetry.json`, con nuovo hash UI e discriminante
offline verificato nella UI installata. Esecuzione di 15 secondi:
`logs/20261010T123814Z_update_steam25661166.*`, 138 campioni, exit 0, zero errori
di osservazione. La sessione è Practice offline in guida, ma contiene più di una
vettura: il gate fisico resta `initial_scope_requires_single_vehicle`.
`physics_compatible` e `reset_permitted` restano falsi; nessun modulo caricato
nel processo 21732. L'adattamento degli oggetti/proprietà alla nuova build e il
ripristino completo rimangono da implementare e provare.

Il terminale sandbox non si avviava (`setup refresh had errors`); l'esecuzione
approvata fuori sandbox ha consentito di lavorare. Nessuna modifica alle
impostazioni del sandbox o ai controlli del gioco.

## Risultati del 3 ottobre (build Steam precedente)

### Seconda continuazione del 10 ottobre: snapshot sulla build aggiornata

`probe_topology_steam25661166.py` ha localizzato il nuovo accesso al giocatore:
istruzione `BA5C0F`, globale `1E194C8`, membro dell'owner `159A8` (prima `159A0`).
Le nuove tabelle dei giunti sono Bar `1807698`, Ball `1807BA8`, Hinge `1807D18`,
Ori `1808098`. La prima lettura esterna ha verificato proprietà e collegamenti
dei 31 vincoli della vettura allora caricata, con 1.576 byte letti dall'heap.
Il rapporto separa questa osservazione sequenziale dalla cattura sul thread fisico.

Implementati `snapshot_observer_25661166.cpp` e relativa seconda istanza v2:
un solo hook Dynamics `ECB4E0`, hash EXE aggiornato, player owner aggiornato,
vtable aggiornate. Cattura all'ingresso Dynamics i corpi e le quattro strutture
tipizzate dei giunti, inclusi impulsi/min/max per tutte le righe. Include controllo
di appartenenza dei corpi, unicità degli oggetti, proprietà numeriche e lease.
Non applica gli impulsi e non scrive posa, scoring o altri campi del gioco.
Fixture: originale eseguito una sola volta, tipo sconosciuto scartato, stop e
watchdog con ripristino delle istruzioni. Frame 21.072 byte, IPC 21.168 byte.

Su richiesta esplicita dell'utente, la sola osservazione può funzionare in
Practice offline con AI: entry point distinto `assess_player_observation`.
Restano verifica offline, fonti concordanti/fresche, controllo giocatore, niente
replay e niente rete. Il gate preesistente non cambia; `reset_permitted` resta
sempre falso. Cinque regressioni specifiche superate. La prova riuscita sotto
riportata aveva comunque una sola vettura secondo l'SDK: non viene presentata
come validazione live con AI.

Prima istanza, `20261010T124637950303Z_snapshot_observer`: zero campioni perché
la sessione risultava non realtime e controllo non attivo. Stato 3, errore 0,
istruzioni ripristinate. Dopo conferma «Sono in GUIDA», seconda istanza separata
senza scaricare a caldo la prima, verificata ferma tramite
`audit_snapshot_25661166.py`.

Prova riuscita: `logs/20261010T124833193165Z_snapshot_observer_v2.*`:

- 9.987 callback della vettura del giocatore, 312 osservazioni, 192 campioni
  conservati, zero scarti; intervallo di simulazione 24,72 secondi.
- La vettura di questa prova ha 16 corpi e 30 vincoli (20 Bar, 5 Ball, 4 Hinge,
  1 Ori), 46 equazioni. Il vecchio conteggio di 49 equazioni apparteneva alla
  Cadillac precedente, non è stato imposto a questa vettura.
- Geometria preparata per tutti i campioni; 8.832 impulsi finiti e non nulli;
  proprietà e collegamenti stabili. Errore matrice inerzia 0, omega massimo
  `7,25e-13`. Nessuna applicazione degli impulsi catturati alle velocità registrate.
- Replay `test_snapshot_recording.py` superato, rapporto `.snapshot-replay.json`.
- Stato 3/errore 0, hook Dynamics ripristinato, collector SDK terminato normalmente.

I dati costituiscono uno **snapshot parziale coerente**, non il checkpoint completo:
mancano ancora copertura dei sottosistemi, storia completa/bias/limiti dei giunti,
adattatore di scrittura e prova del ritorno a un punto precedente. La stima resta
circa 45% finché non viene risolto il ripristino effettivo; i conteggi dei test non
sono una misura del completamento.

## Archivio delle prove del 3 ottobre

### Archivio RAM e preparazione del checkpoint — 10 ottobre

`checkpoint_bank.h/.c` implementa uno slot RAM di dati posseduti: corpi, ricette
geometriche e memoria degli impulsi. Il salvataggio invalido conserva lo slot
precedente. La preparazione verifica identità di processo/sessione/vettura/
configurazione e hash eseguibile, topologia e masse; ricostruisce le proprietà
derivate e copia gli impulsi **senza applicarli alle velocità salvate**.

I token di identità sono richiesti al chiamante: il core non li ricava dal gioco.
Nel replay sono etichette sintetiche, non prove di riconoscimento delle sessioni.
Lo slot è per un solo proprietario/thread, non una struttura concorrente.
`full_restore_supported` è sempre zero: non esiste un flag di copertura fornito
dal chiamante che possa fingere di completare i sottosistemi mancanti.

`checkpoint_save_mechanical` e `checkpoint_prepare_mechanical` collegano il
preparatore fuel/wheel/tire già esistente: salvano le dipendenze insieme al
checkpoint e accettano variazioni di massa/inerzia solo quando spiegate da esse.
Fixture: variazione di carburante accettata, configurazione ruota cambiata
rifiutata senza output parziale. Queste API non aggiungono la storia completa
di pneumatici/freni né un adattatore di scrittura.

Build `Build-CheckpointBank.ps1`; replay `test_checkpoint_bank_recording.py`.
Sui 192 snapshot reali della nuova build: 192 salvataggi RAM, 192 preparazioni
dello stesso stato e 384 rifiuti controllati superati. La prova dal primo
snapshot ai successivi, con il percorso rigoroso senza dipendenze, accetta il
primo e rifiuta 191 variazioni di massa/inerzia: risultato atteso, non un restore
fallito nel gioco. Quegli snapshot non contengono ancora le dipendenze necessarie
per eseguire su di essi il percorso meccanico. Rapporto `.bank-replay.json` accanto
alla registrazione `20261010T124833193165Z_snapshot_observer_v2.frames.jsonl`.

Nessun hook nuovo caricato in questa continuazione. Nessuna hotkey, scrittura
fisica o prova di ritorno alla posizione salvata. La prossima integrazione nativa
deve catturare insieme dipendenze meccaniche e identità reali, poi completare gli
altri sottosistemi prima di autorizzare il ripristino completo.

## Dettaglio storico del 3 ottobre

Il reset completo non è ancora operativo. Questa continuazione ha risolto il
rifiuto della preparazione numerica dovuto a carburante e inerzie variabili,
e verificato geometria e applicazione degli impulsi di tutti i 31 vincoli
della Cadillac provata: Bar, BallJoint, PureHinge e OriJoint. La preparazione
integrata è verificata su catture coerenti dei 17 corpi e 31 vincoli.
Nessuna scrittura fisica o dello scoring in queste nuove prove.

## Dipendenze delle ruote e preparazione combinata

Le routine 14200 `EE1780` e `ECAB90` assegnano l'inerzia assiale del corpo ruota:

`I_x = ((wheel[4B8] + wheel[4B8]) + wheel[5D8]) * wheel[4F0] + wheel[130]`.

Conservano le altre due componenti; ricalcolano inversa locale, matrice mondiale,
momento angolare e cache inversa a `wheel+138`. I quattro campi sono ancora
identificati per offset: la verifica della formula non prova la copertura completa
del sistema freni/termica che ne determina l'evoluzione.

Il probe bounded ha confermato la formula su 96 letture. `wheel_inertia_core.*`
e `mechanical_prepare_core.*` preparano i 17 corpi usando dipendenze esplicite
salvate/correnti: carburante, quattro inerzie assiali ruota e quattro partizioni
interne pneumatici. Link distinti e in bounds, configurazione invariata e
corrispondenza delle formule sono richiesti. Le altre variazioni di massa/inerzia
restano rifiutate. Output interamente invariati in caso di errore.

`logs/20261003T121132631872Z_vehicle_observer_v3.*`:

- 12.086 callback, 374 osservazioni, 228 frame, zero scarti, thread 17524.
- Preparazione combinata riuscita su tutti i 228 frame; replay riuscito anche
  dal primo checkpoint numerico a tutti quelli successivi, fino a 29,68 secondi.
- 228 alterazioni sintetiche di un'inerzia trasversale rifiutate senza output
  parziale. Non è stato allentato il controllo generico delle masse.
- Errori delle formule ruota/pneumatici zero. Ordine contatti/forze corretto;
  errore omega/L entro `1,26e-12`.
- Stato 3, errore 0, controller exit 0, tre hook ripristinati.

Il risultato è una preparazione locale coerente dei corpi, **non un checkpoint
completo né una sua applicazione a LMU**. Restano gli stati upstream, gli altri
sottosistemi e l'adattatore di scrittura.

Build: `Build-MechanicalPreparation.ps1`, `Build-VehicleObserverV314200.ps1`.
Replay: `test_mechanical_recording.py`; evidence `.frames.mechanical-replay.json`.

## Geometria Bar e coefficiente del risolutore

`bar_constraint_core.*` ricostruisce le ancore mondiali, i quattro vettori
Jacobiani, i contributi pesati da massa/inerzia e il coefficiente del vincolo.
Ownership Bar verificata tramite il vettore dei vincoli HDV, vtable `18042A8`,
corpi `+A0/+A8` presenti nel vettore corpi del giocatore. Routine A0 `3EF0E0`.

Il coefficiente nativo comprende **1,125 / diagonale**, non soltanto l'inversa
della diagonale: costante EXE a RVA `1812AC0`, letta da `3EF779`. Questa distinzione
è emersa dal confronto sincronizzato ed è stata verificata nel binario pinned.

Il primo probe asincrono conteneva divergenze dovute anche alle fasi del solver.
Il primo osservatore aveva inoltre un errore Python di serializzazione (`before`
inesistente nel formato Bar): zero frame nello stream; ultimo campione recuperato
dall'IPC già arrestata. `20261003T122311614649Z_bar_observer.*` conserva il fallimento.
Hook ripristinato correttamente; nessun tentativo di riattivare/scaricare la DLL.

La versione separata v2 corregge formato e coefficiente:
`logs/20261003T122635998186Z_bar_observer_v2.*`:

- 969.480 chiamate native, 7.539 osservazioni, 159 campioni conservati.
- Tutti i 20 Bar coperti, indici 8–27 del vettore vincoli.
- Errore massimo Jacobiani `2,23e-16`, contributi pesati `8,89e-16`,
  coefficiente `2,67e-15`. Replay indipendente C superato.
- 159 input NaN rifiutati senza output parziale.
- Controller exit 0, stato 3/errore 0, hook Bar ripristinato.

## Impulsi memorizzati dei Bar

`bar_prepare_warm_start` prepara su dati posseduti dal programma la limitazione
dell'impulso ai bounds e l'aggiornamento delle due velocità lineari/angolari.
Usa i bounds prodotti dal percorso nativo: **non ricostruisce ancora la fase
precedente di bias/limiti, plasticità o rottura**.

Il modulo Bar v3 cattura prima/dopo A0, senza applicare esso stesso impulsi.
Il primo preflight ha rifiutato il menu principale; dopo conferma dell'utente
la prova è stata eseguita in Practice offline:
`logs/20261003T123446799616Z_bar_observer_v3.*`:

- 965.640 callback, 7.502 osservazioni, 151 campioni conservati, zero scarti.
- Tutti i 20 Bar coperti. 151 confronti dell'impulso e delle velocità superati;
  zero discrepanze oltre tolleranza, errore massimo `1,78e-15`.
- Geometria/coefficiente ancora concordanti. 151 controlli di errore senza
  pubblicazione parziale superati nel replay.
- Controller exit 0; stato 3/errore 0; hook originale ripristinato.

Le tre versioni Bar hanno mapping, DLL e manifest separati. Il controller verifica
hash, percorso e stato arrestato degli osservatori precedenti prima di caricare
un nuovo modulo. I moduli arrestati restano residenti fino all'uscita dal gioco.

## Controlli e limiti ancora presenti

La navigazione non valida viene ora rifiutata prima di interrogare GetGameState:
al menu tale endpoint risponde 503. Quattro regressioni aggiunte; suite Python
39/39. Fixture geometriche, impulso, preparazione combinata e stop/watchdog dei
nuovi osservatori superate. Non attribuire questi successi a un reset nel gioco.

Audit indipendente alle 14:41 locali: `logs/20261003T124100Z_shutdown_audit_14200.json`.
Tutti e sette i moduli residenti risultano arrestati, errore zero e richieste
disabilitate. Prologhi nativi Contacts, Dynamics, Forces e Bar confermati originali.
L'audit è ripetibile con `src/python/sector/observers/audit_observer_shutdown_14200.py --pid <PID>`.

Restano: bias/limiti/stato persistente dei vincoli;
copertura completa di pneumatici, freni, drivetrain, danni ed energia; adattatore
atomico del checkpoint, contatti dopo spostamento e prova dello scoring con record
preesistente. Nessuna hotkey di reset completo viene esposta come funzionante.

## BallJoint, PureHinge e OriJoint

Prove aggiunte il 3 ottobre, fra le 15:01 e le 15:08 locali. I moduli osservano
prima/dopo la routine nativa, che viene chiamata esattamente una volta. La
preparazione degli impulsi usa i limiti prodotti dal gioco; non ricostruisce
ancora il calcolo dei limiti o la storia persistente del giunto.

| Tipo | Routine A0 | Giunti coperti | Campioni | Errore massimo geometria/coefficiente | Errore impulsi/velocità |
| --- | --- | --- | --- | --- | --- |
| BallJoint | `3E39D0` | 6 | 153 | `2,85e-14` | 0 |
| PureHinge | `3F5160` | 4 | 152 | 0 | 0 |
| OriJoint | `3FBE50` | 1 | 153 | 0 | 0 |

Log: `20261003T130104470730Z_ball_observer`,
`20261003T130740837382Z_hinge_observer`,
`20261003T130619859165Z_ori_observer`, nella cartella `logs`.
I replay indipendenti `.ball-replay.json` / `.angular-replay.json` fissano gli
hash degli input e delle librerie locali. Tutti gli arresti sono terminati con
stato 3, errore 0 e prologhi originali ripristinati. Prima del test Hinge la
fixture ha rifiutato un errore negli offset del mock; corretto e ritestato
prima di caricare il modulo in LMU.

Contratti geometrici 14200 verificati:

| Tipo | Puntatori corpi | Assi/ancore locali | Passo righe | Jacobiani | Softness | Impulso | Min/max | Coefficiente | Pesi |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Ball | `98/A0` | `50/68` | `F0` | `A8` (12 double) | `110` | `118` | `120/128` | `130` | `138` (12 double) |
| Hinge | `98/A0` | `A8` | `A8` | `C0/D8` (3+3) | `F8` | `100` | `108/110` | `118` | `120/138` (3+3) |
| Ori | `30/38` | `60` | `E8` | `98/B0` (3+3) | `D0` | `D8` | `E0/E8` | `F0` | `F8/110` (3+3) |

Ball usa tre assi mondiali fissi e consente ancore coincidenti; Hinge/Ori ruotano
gli assi locali del corpo A, con due/tre equazioni solo angolari. Il coefficiente
di rilassamento resta `1.125`. `ball_constraint_core` e `angular_constraint_core`
preparano tutte le righe prima di pubblicarle; input non validi nell'ultima riga
lasciano invariato l'output.

## Integrazione dei 17 corpi e 31 vincoli

`constraint_group_core` gestisce i quattro tipi mediante valori posseduti e
indici interi dei corpi, senza puntatori del processo. Rifiuta tipi sconosciuti,
indici fuori intervallo, corpi coincidenti e identità duplicate.
`mechanical_constraint_core` prepara prima massa/inerzie e poi geometria usando
le proprietà salvate: un errore nell'ultimo vincolo lascia invariato l'intero
output, compresi corpi e carburante. Si tratta di pubblicazione locale dopo la
validazione, **non di una transazione atomica sul gioco**.

Fixture con 17 corpi, 31 vincoli e 49 equazioni superata; copre cambiamenti di
carburante e inerzie, errore nell'ultimo corpo/riga e topologia non valida.
Il dispatcher comune supera anche 609 campioni reali e 1.827 prove di rifiuto:
`logs/20261003_constraint_group_replay.json`. Le registrazioni separate non sono
state presentate come un unico snapshot simultaneo.

L'osservatore veicolo v4 aggiunge catture prima/dopo Dynamics di tutti i vincoli,
verificando tipo, appartenenza ai corpi, unicità e stabilità dei collegamenti.
Le fixture stop/watchdog verificano anche che un tipo sconosciuto venga scartato
senza impedire la chiamata originale. ABI frame 38.248 byte, IPC 38.344 byte.

Prova reale: `logs/20261003T132700257453Z_vehicle_observer_v4.*`:

- 30 secondi; 12.049 callback, 373 osservazioni, 230 campioni conservati, zero scarti.
- 17 corpi, 20 Bar + 6 Ball + 4 Hinge + 1 Ori, 49 equazioni, ordine contatti/forze valido.
- Preparazione integrata: 230/230 confronti fra passi consecutivi e 230/230 dal
  primo campione ai successivi, intervallo massimo 29,6 secondi.
- 460 errori introdotti nell'ultimo vincolo rifiutati senza output parziale.
- Controller exit 0, stato 3/errore 0, hook Dynamics/Contacts/Forces ripristinati.

Replay: `src/python/sector/replay/test_mechanical_constraint_recording.py`, rapporto
`20261003T132700257453Z_vehicle_observer_v4.frames.integrated-replay.json`.
Il replay non prova il ritorno alla posizione salvata: prepara solo valori locali.

Audit finale alle 15:29 locali:
`logs/20261003T132903Z_shutdown_audit_14200.json`: tutti gli 11 moduli residenti
arrestati, richieste disabilitate, nessun errore; sette prologhi nativi originali.
Suite Python generale ancora 39/39. Nessuna scrittura alla posa o allo scoring.

La stima comunicata del 45% è indicativa e riguarda l'obiettivo completo, non una
percentuale misurata dai test. Mancano ancora copertura dello stato persistente
dei vincoli e dei sottosistemi, adattatore di ripristino, contatti dopo spostamento,
scoring/record e comandi utente. Non aumentare la stima contando soltanto i test.
## Continuazione 10 ottobre: dipendenze snapshot v3

La cattura `20261010T125859007033Z_snapshot_observer_v3.frames.jsonl` contiene
231 campioni su 29,6 secondi. Il bridge del banco RAM espone ora salvataggio e
preparazione con dipendenze meccaniche; il replay supporta sia v2 sia v3.
Risultato v3: 89 salvataggi e preparazioni dello stesso istante, 142 rifiuti di
snapshot incoerenti e 457 verifiche negative con conservazione del banco/output.
Dal primo snapshot accettato ai successivi: un successo sullo stesso istante,
88 rifiuti. Identità di replay sintetiche; nessuna scrittura fisica.

`diagnose_snapshot_dependencies.py` rende riproducibile la diagnosi: 924 inerzie
ruota coincidono esattamente; 756 su 2.772 componenti di inerzia pneumatico
superano 32 epsilon double, errore relativo massimo 1,273533e-6. La causa non è
ancora provata: un disallineamento della fase di aggiornamento è un'ipotesi.
Inoltre cambia la massa interna di tutti e quattro gli pneumatici, non inclusa
nelle dipendenze attualmente ricostruite. Non si allargano le tolleranze per
nascondere il problema. Il prossimo adattatore deve catturare anche questa
dipendenza e provare la coerenza della fase di aggiornamento.

Audit finale `20261010_snapshot_shutdown.json`: tre osservatori residenti ma
arrestati, status 3/error 0, lease disabilitate, prologo Dynamics originale.
