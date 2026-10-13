# LMU Active Reset - sviluppo in corso

**Ultimo avanzamento live, 13 ottobre:** il ciclo esterno e Dynamics possiedono la stessa sezione critica nativa sul thread 20020: 14 campioni stabili. Collegati e testati localmente i comandi al ripristino protetto nel ciclo esterno; corretto il rollback dopo perdita del proprietario. Resta da verificare la copertura del blocco sul ciclo di vita degli oggetti. Nessuna scrittura di reset nel gioco. [Risultati e limiti](PHYSICS_THREAD_2026-10-13.md).

**13 ottobre 2026:** coordinatore di ripristino fisica/scoring/contatti e recupero integrati e verificati su memoria locale. Aggiunto adattatore contatti a quattro ruote con preflight completo. UI sospesa; reset completo nel gioco ancora non operativo. [Stato e prove attuali](RESTORE_INTEGRATION_2026-10-13.md).

**Ultima prova: v28, 12 ottobre.** Cattura dinamica avviata sull'acceleratore:
74 campioni, marce 1..4, 16 variazioni del timestamp di cambiata, 73 preparazioni
native senza rifiuti. Piano da 10.074 campi fisici, replay e rollback locale
superati. Nessun checkpoint completo ripristinato nel gioco. Evidenze e limiti
in [REA_ANALYSIS_25661166.md](REA_ANALYSIS_25661166.md).

**Contatti e transazione congiunta:** ricostruzione nativa riuscita sulle quattro
ruote da fermo nella build attuale; piano fisica/scoring da 10.077 campi provato
con backend Win32 e rollback su memoria fixture. Dettagli in
[JOINT_RESTORE_25661166.md](JOINT_RESTORE_25661166.md). Nessun checkpoint fisico
completo ripristinato nel gioco.

**Prova dinamica v22 completata:** 70 campioni, 16 variazioni di marcia,
69 variazioni degli output trasmissione e zero rifiuti nativi. Piano da 10.060
campi verificato su memoria fixture con rollback; nessun reset fisico applicato.
La v22 usa lo stesso codice funzionale della v21 con identita' DLL/IPC distinta
per una nuova acquisizione senza sovrascrivere i moduli residenti.

**12 ottobre 2026:** [analisi REA/Ghidra e nuovo banco trasmissione v20](REA_ANALYSIS_25661166.md).
Sei routine decompilate dal codice caricato; aggiunti quattro output al piano.
Test locali e cattura live v20 superati: 49 campioni e piano da 10.060 campi
(254.968 byte), eseguito soltanto su memoria fixture. I nuovi valori sono rimasti
costanti nella cattura; nessuna prova di reset fisico nel gioco.
La v21 aggiunge il rifiuto dei guasti terminali non ancora ripristinabili:
74 campioni live senza errori, 73 preparazioni native; transizioni di guasto
provate soltanto su fixture. La vettura e' rimasta in folle durante la cattura.

**10 ottobre 2026: il reset completo non e' ancora operativo.** Mancano le hotkey
di salvataggio/ripristino. Steam 25661166 conserva appBuild 14200 ma cambia
eseguibile, adapter e offset: le DLL fisiche precedenti non sono compatibili
tramite semplice sostituzione dell'hash.

Il banco RAM nativo salva e prepara sul thread Dynamics. Conserva corpi, vincoli
e cache storiche, carburante, cache esatte di massa/inerzia, settori pneumatico
e stati parziali motore/trasmissione. Il piano v19 contiene 10.056 campi (254.936 byte),
con filtro tipizzato delle destinazioni. Scrittura, verifica e rollback sono
provati su memoria isolata, anche attraverso il nuovo backend Win32; gli
osservatori nel gioco restano di sola lettura.
Nessun ripristino fisico e' stato eseguito.

Le versioni v14/v15 conservano inoltre in RAM 2.592 elementi termici (cinque
strati ciascuno) e tre campi di storia per 2.016 nodi del battistrada. Questi
payload sono inclusi nel piano dalla v17. La v16 collega angolo e velocita'
di rotazione delle ruote; la v18 aggiunge sette valori per ruota relativi ai
freni e alla loro cache d'inerzia. I contatti sono stati
riverificati in osservazione sulla build attuale, senza ricostruzione attiva.

La mappa scoring corrente distingue il nuovo cache intero a +27360 dal vecchio
offset +2735C, ora float. La transazione di invalidazione dei tre flag (12 byte)
passa i test di scrittura e rollback su memoria isolata; non e' applicata nel gioco.

La cattura dinamica v6 documenta marce da -1 a 5. Le acquisizioni successive
risolvono le dipendenze temporali della massa, ma sono a vettura ferma: non
costituiscono una prova di ripristino in movimento. Risultati, errore della v10
corretto nella v11, limiti e lavoro mancante sono negli
[stati persistenti della build attuale](PERSISTENT_STATES_25661166.md).

## Sviluppo precedente (build del 3 ottobre)

Continuazione piÃƒÂ¹ recente: [preparazione combinata di corpi e vincoli](MECHANICAL_PREPARATION_14200.md).
Il componente locale `constraint_warm_group` ora accumula gli impulsi nell'ordine
dei vincoli anche sui corpi condivisi, senza pubblicare risultati parziali:
609 campioni precedenti concordanti. Non ricostruisce ancora bias/limiti e non
deve applicare una seconda volta impulsi a velocitÃƒÂ  giÃƒÂ  successive al solver.
ÃƒË† disponibile anche `checkpoint_bank`: archivio RAM tipizzato, preparazione del
ripristino e integrazione delle dipendenze meccaniche. Non ÃƒÂ¨ ancora collegato a
hotkey o scritture nel gioco; l'identitÃƒÂ  reale di sessione/configurazione deve
essere fornita dall'adattatore nativo, ancora da completare.
Geometria e impulsi verificati per tutti i 31 vincoli della vettura provata:
20 Bar, 6 BallJoint, 4 PureHinge e 1 OriJoint. La cattura integrata sul thread fisico
prepara 17 corpi e 49 equazioni: 230 confronti consecutivi e 230 dal primo campione
ai successivi, fino a 29,6 secondi. Sono calcoli locali su dati osservati;
nessun checkpoint completo ÃƒÂ¨ stato ripristinato nel gioco.

La build verificata ÃƒÂ¨ **LMU 1.4.2.0 / 14200**. Le DLL 14130/14150 restano
incompatibili e non vanno abilitate cambiando soltanto gli hash.

## Risultati verificati

- Ricostruzione nativa dei contatti da fermo sulle quattro ruote, provata nel
  gioco sulla 14200: 194 contatti, istruzioni originali ripristinate a fine prova.
  Nessun teletrasporto, checkpoint completo o modifica ai tempi.
- Cattura coerente di 17 corpi della Cadillac a Fuji, carburante e batteria sul
  thread fisico: 155 frame, zero errori di ordine contatti/forze. Replay locale
  valido su 2.635 stati corpo. La successiva preparazione combinata risolve il
  rifiuto tra istanti diversi dovuto a carburante e inerzie variabili.
- Ricostruzione ruota/mozzo: 345 gruppi coerenti verificati sui dati in movimento.
- Preparazione numerica della massa legata al carburante e della partizione
  delle inerzie pneumatici, con validazione completa prima di pubblicare output.
  Il secondo osservatore verifica 752 stati pneumatico coerenti in movimento;
  la prova termina anticipatamente per HTTP 503, con hook ripristinati.
- Nuovi offset dello scoring 14200 individuati in sola lettura; modello locale
  verificato su 108 combinazioni. Nessuna prova nativa di conservazione record.

## Componenti e build

I profili in `compatibility/14200-*.json` fissano separatamente file, routine e
moduli sperimentali. `physics_compatible`/`reset_permitted` non autorizzano un
reset completo. I manifest degli osservatori certificano solo le rispettive
fixture e identitÃƒÂ  del binario.

Gli script di build nuovi accettano `-ZigPath` con il percorso di Zig installato:
`Build-FuelRestore.ps1`, `Build-TireInertia.ps1`, `Build-Contact14200.ps1`,
`Build-VehicleObserver14200.ps1`, `Build-VehicleObserverV214200.ps1`.
Per la preparazione integrata: `Build-MechanicalConstraint.ps1`; per i successivi
osservatori: `Build-BallObserver14200.ps1`, `Build-HingeObserver14200.ps1`,
`Build-OriObserver14200.ps1`, `Build-VehicleObserverV414200.ps1`.
Le fixture precedono la pubblicazione delle DLL. Le librerie di replay locale
sono prodotte in `../../work`, non caricate nel gioco.

Per la sola telemetria, dalla cartella di questo progetto:

```powershell
python .\src\capture_preparation_14200.py --pid <PID_ATTUALE> --seconds 180
```

Questo comando verifica la build, legge SDK e GET locali; non carica DLL o
impartisce comandi alla vettura. `allowed` descrive l'osservazione, non il reset.

Suite Python:

```powershell
python -m unittest discover -s .\src -p "test_*.py"
```

Gli osservatori e l'esperimento contatti sono strumenti di sviluppo per Practice
offline senza EAC, con identitÃƒÂ  del processo, gate continuo e watchdog. Non sono
un'interfaccia utente del reset. I moduli caricati vengono arrestati e gli hook
ripristinati, ma restano residenti fino alla chiusura di LMU: chiudere l'istanza
prima dell'uso online.

## Lavoro mancante ed evidenze

Restano stato completo di pneumatici/termica, vincoli e impulsi della sospensione,
drivetrain, danni/energia, adattatore atomico del checkpoint, contatti dopo uno
spostamento, integrazione dello scoring e collaudo record/giro successivo.

I log separano scritture reali dei contatti, acquisizioni e replay locali.
`CONTACT_REBUILD.md` e `KINEMATIC_INTEGRATION.md` conservano le prove 14150;
`PHYSICAL_RESET_STATUS.md`, `RIGID_BODY_CONTRACT.md`, `FINDINGS.md` e
`TEST_RESULTS.md` conservano la ricerca 14130. Non usare le loro costanti nella
build nuova. Header proprietari, UI e disassemblazioni restano in `work`.

MinHook 1.3.4 ÃƒÂ¨ distribuito con licenza BSD in `third_party/minhook/LICENSE.txt`.

Aggiornamento v23 (2026-10-12): aggiunti 58 byte di controllo powertrain,
verificati in cattura coerente e replay locale. Piano fisico 10.066 campi;
piano con punteggio 10.069 campi. Zero scritture fisiche durante la cattura.
Vedere REA_ANALYSIS_25661166.md e il replay della registrazione
20261012T185803738431Z. Non Ã¨ ancora disponibile un reset completo in gioco.

Calcoli offline v28 completati: 2.775 coppie di checkpoint valide e sei piani
completi rappresentativi verificati. **Avanzamento stimato circa 70%**, con reset
live completo ancora da integrare e provare. [Stato e attivitÃ  residue](STATO_2026-10-12.md).
