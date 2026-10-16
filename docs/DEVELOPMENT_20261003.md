# Sviluppo del 3 ottobre 2026 — LMU 14200

La continuazione successiva risolve le dipendenze delle inerzie nella preparazione
locale e verifica geometria/impulsi dei 20 Bar:
[MECHANICAL_PREPARATION_14200.md](MECHANICAL_PREPARATION_14200.md).
Le limitazioni elencate sotto descrivono lo stato al termine della prima sessione.

**Il checkpoint completo non è ancora operativo.** Oggi è riuscita una vera
ricostruzione nativa dei contatti sulle quattro ruote, da fermo; sono stati
acquisiti sul thread fisico i 17 corpi della Cadillac, carburante e batteria.
Nessuna posa o voce dello scoring è stata scritta. Non esistono ancora hotkey
operative di salvataggio/ripristino.

## Ambiente verificato

Cadillac WTR 2026 #101:LM, Fuji Speedway, Practice1 offline, PID 17116.
AppBuild 14200, EXE 1.4.2.0, Steam build 25611540. SHA256 EXE
`b37a1dd3a5912391db236051995caadd6d5558631ed9ce5d68f8397abe2f6a8c`.
Controlli di assenza EAC e contesto offline; nessun bypass o modifica ai file
dell'installazione. La baseline odierna è stata registrata da fermo, in folle,
motore spento e freno zero. Non attribuire a questa prova una verifica in marcia.

La sessione precedente del 2 ottobre era sul PID 29412. Il suo documento descrive
la prima fase, prima dell'osservatore dei collegamenti aggiunto più tardi.

## Collegamenti ruota/mozzo: prova completata nella tarda sessione del 2 ottobre

`logs/20261002T221226206139Z_binding_observer.*`: 6.075 callback, 187 osservazioni,
115 frame conservati, zero scarti. Acquisizioni prima/dopo Dynamics e dopo
l'ultima trasformazione nativa di ogni ruota, sul thread 23908. Sei trasformazioni
per ruota e ciclo, zero discrepanze di identità dei mozzi.

Il replay del codice C ha verificato 345 gruppi, cioè 1.380 stati ruota, usando
le masse, inerzie e momenti angolari acquisiti. Tutti i gruppi concordano;
345 alterazioni sintetiche dell'ultima ruota vengono rifiutate senza risultati
parziali. L'auto era realmente in movimento: fino a circa 249,65 km/h. Nessuna
scrittura fisica in questa prova. Hook ripristinati, stato 3/errore 0.

## Contatti 14200: prova fisica riuscita

Modulo separato `bin/lmu-contact-native-14200.dll`, hash
`eb3c87ac1f26fe825e7a2ce6e82e897575b777f8d3381441fed4dde3a5f4caaf`.
Il contratto a cinque argomenti è stato riconfermato nella routine `0xEF5B80`;
la cache superfici è `0xEF76B0`, il mondo è `0x1AA1080`. Gli indirizzi 14150
non sono stati abilitati cambiando soltanto l'hash.

`logs/20261003T111309532905Z_contact_experiment.analysis.json`:

- 194 scritture mirate della profondità dei vecchi contatti; ricostruzione
  nativa completata su tutte e quattro le ruote, con 48/48/49/49 contatti validi.
- 983 osservazioni sul thread fisico, zero scarti/errori di invarianti.
- Durate native 12,7 / 8,2 / 8,5 / 7,6 microsecondi; eventi distribuiti su 16 ms.
  Questi numeri non misurano la latenza di un checkpoint atomico.
- 1.071 campioni nei 117,047 secondi successivi: zero valori non finiti,
  scostamento massimo dalla posizione iniziale circa 0,013 mm.
- Stato finale 3, errore 0, istruzioni originali ripristinate.

Fixture di stop e watchdog superate prima del caricamento. Questa prova non
valida contatti dopo teletrasporto, storia degli pneumatici o conservazione del
record: non era presente un miglior giro valido.

## Carburante e batteria: proprietà distinte

`HDV+0x448` contiene quantità e derivata; densità a `+0x60`, riferimento al corpo
a `+0x78`, massa base a `+0x80`. La routine nativa `0xC6E170` assegna:

`massa = quantità * densità + massa_base + massa_aggiuntiva`.

Il termine aggiuntivo è zero nel ramo `HDV+0x550 != 0`; l'altro ramo usa
`0xEE2730`, con valori della configurazione e un preciso ordine di somma.
Sulla Cadillac il corpo è l'indice 10: 13,6 * 0,735 + 0,4 = 10,396 kg.
Il probe bounded e la cattura coerente confermano massa e inversa.

`fuel_restore_core.*` prepara questa dipendenza su valori posseduti dal programma.
Consente la variazione di massa del solo corpo carburante identificato; rifiuta
modifiche a configurazione, altri corpi o inerzie. Le fixture coprono consumo,
derivata, collegamento errato, NaN/overflow e assenza di output parziale.
Non contiene un writer del carburante e non copre l'intero sistema termico.

`HDV+0x4F8` è la batteria collegata al motore elettrico, **non l'energia virtuale**.
La relazione di ownership è stata verificata; carica/capacità erano entrambe
4.680.000, frazione batteria 1, energia virtuale circa 0,16. I valori erano
costanti: non è una prova dinamica di consumo/ricarica né dell'intero sistema ibrido.

## Cattura coerente dell'intera topologia osservata

`logs/20261003T112349068667Z_vehicle_observer.*`, modulo
`lmu-vehicle-observer-14200.dll`, tre hook distinti: Dynamics `0xEC87E0`,
contatti `0xEF5B80`, forze `0xEC7E30`. Il controller ha prima verificato che il
modulo contatti fosse già arrestato. Acquisizione numerica, nessuna scrittura fisica.

- 8.052 callback, 249 osservazioni, 155 frame conservati, zero scarti.
- 13 corpi principali più 4 corpi interni pneumatici; ruote agli indici 6–9.
- Sei calcoli forze, due aggiornamenti contatti per ruota e maschere di freschezza
  `[15,0,0,15,0,0]`; zero errori di ordine in tutti i frame.
- Errore massimo inerzia mondiale zero; relazione omega/inversa/L entro
  `2,66e-19`. Preparazioni individuali prima e dopo valide.
- Il replay C passa tutti i 155 frame presi singolarmente: 2.635 stati corpo.
- **Tutte le 155 preparazioni tra istanti diversi restano rifiutate**: cambiano
  le inerzie interne dei quattro pneumatici. Il nuovo supporto carburante non
  nasconde questa dipendenza. I risultati restano intatti sul rifiuto.
- Mediane original Dynamics 274,8 µs, osservatore complessivo 383,7 µs;
  non sono misure della latenza del ripristino.
- Stato finale 3/errore 0; tutti e tre i prologhi originali ripristinati.

## Dipendenza dell'inerzia degli pneumatici e osservatore v2

Il percorso `0xF0F4E6..0xF0F689` converte in double tre float della tabella a
`tire+0xCE4`, somma in ordine otto contributi a `tire+0x48 + i*0x60`, conserva
la somma a `+0xC20` e assegna al corpo interno il residuo. Le routine
`0x101BC60` e `0x101DC40` aggiornano inerzie locali/inverse/mondiali, con diversa
ricostruzione finale di omega o momento angolare. Non sono setter intercambiabili.

`logs/20261003T113314Z_tire_inertia_14200.json` contiene 96 letture bounded:
somma e residuo coincidono esattamente; una lettura dell'inversa differisce di
`1,45e-6`, compatibile con letture esterne che attraversano un aggiornamento.
Non assumere la coerenza temporale di questo probe.

`tire_inertia_core.*` ricostruisce la partizione numerica di quattro pneumatici,
conservando la precisione float della tabella e validando tutti i settori prima
di pubblicare risultati. Fixture con settori disuguali, ultimo pneumatico errato,
NaN, overflow e residuo non positivo superate. Replay C: 96 corrispondenze del
residuo nativo; 24 rifiuti sintetici senza output parziale.

È stato compilato un **osservatore v2 separato**, che acquisisce queste partizioni
insieme ai corpi prima/dopo Dynamics: `Build-VehicleObserverV214200.ps1`,
`src/python/sector/observers/run_vehicle_observer_v2_14200.py`, profilo `14200-vehicle-observer-v2.json`.
IPC 21.664 byte; fixture stop/watchdog superate. Hash DLL
`e20308b3f4dcd9e4dcce22dccd8336a77d3fe2d61fce87939755e37f67e22e80`.
I primi due preflight hanno rifiutato il caricamento: LMU era passato a
`NAV_MAIN_MENU/GSTATE_SETUP` e GetGameState restituiva HTTP 503. Quando la Practice
è tornata attiva, il modulo è stato caricato per una nuova prova osservazionale.
Non è stata forzata l'autorizzazione. Il controller riconosce solo il precedente v1 con hash,
percorso e IPC verificati come arrestati; rifiuta altri osservatori o duplicati.

Risultato: `logs/20261003T114211106304Z_vehicle_observer_v2.*`:

- 4.858 callback, 149 osservazioni, 94 frame conservati, zero scarti del modulo.
- Prima/dopo Dynamics, **752 stati pneumatico concordano esattamente** con la
  partizione calcolata dal codice C: errore massimo dell'inversa zero.
- Tutti i contributi dei settori erano zero in questa configurazione: i settori
  non nulli sono coperti soltanto dalle fixture sintetiche, non dalla prova nativa.
- Auto in movimento, fino a 269,70 km/h; carburante tra 10,29 e 13,6 litri.
  Il replay dei corpi passa 94 frame singoli, 1.598 stati corpo.
- Il ripristino tra istanti diversi rimane rifiutato in tutti i 94 frame: oltre
  alle inerzie interne, cambia la massa del corpo carburante (59 frame) e cambiano
  le inerzie dei corpi ruota 6–9 (40/40/31/31 frame). Le masse delle ruote restano
  costanti. Il contratto delle loro inerzie resta da completare.
- Zero errori di ordine contatti/forze; errore inerzia mondiale zero, omega/L
  entro `1,40e-12`. Mediane Dynamics 381,6 µs, osservatore 509,0 µs.
- Un nuovo HTTP 503 ha terminato anticipatamente il controller (exit 1): questo
  è un test parziale con dati utili, non una sessione completata senza errori.
  Stop regolare del modulo, stato 3/errore 0, tutti e tre gli hook ripristinati.
  Lettore nativo: 202 campioni, una lettura saltata per contesa, chiusura regolare.

La dipendenza numerica interna è dunque verificata anche con lettura coerente.
Non equivale al ripristino di pressioni, temperature, usura e storia interna che
alimentano la tabella, né risolve le proprietà dei corpi ruota.

## Vincoli e scoring

La topologia contiene 31 vincoli: 6 BallJoint, 4 PureHinge, 20 Bar, 1 OriJoint.
Le routine virtuali A0 applicano impulsi memorizzati alle velocità dei corpi:
non vanno invocate come semplici aggiornamenti delle cache. Restano da definire
stato persistente, ricostruzione derivata e ordine di ripristino della sospensione.

Scoring 14200: `*(HDV+0x4BC0)+0x1CEC0`, ownership verificata in sola lettura.
Flag corrente `+0x160`, ricordato `+0x27290`, ultimo calcolato `+0x2735C`,
prossimo giro `+0x164`, off-track `+0x272B8`. Gli offset ricordato/ultimo/off-track
14150 non sono riutilizzabili. Percorsi `0xCEEFB0`, `0xCE6D80`, `0xCE64E0`
analizzati; ricerca in `compatibility/14200-scoring-research.json`.

Ricompilato e rieseguito il modello: 108 combinazioni superate. Nessuna scrittura
allo scoring nativo, nessuna prova di conservazione del miglior giro dopo reset.

## Verifica e lavoro ancora necessario

Suite Python: 35 test superati. Fixture fuel, contatti 14200, osservatore veicolo
v1/v2, partizione pneumatici e modello giro superate. I replay sono verifiche
locali dei dati registrati e non simulano una riuscita del reset nel gioco.

Prossimi passi concreti:

1. Ricostruire le dipendenze delle inerzie dei corpi ruota 6–9 osservate in
   movimento; estendere la validazione della partizione a settori nativi non nulli.
2. Completare stato pneumatici/termico, vincoli con impulsi, drivetrain, danni e
   dipendenze dell'energia; definire invalidazione e ricostruzione per ciascuno.
3. Costruire l'adattatore atomico sul thread fisico e identità completa del
   checkpoint; provare contatti dopo uno spostamento controllato.
4. Integrare lo scoring verificato e provare record preesistente, giro interrotto
   e giro valido successivo, poi esporre le hotkey.

I tre moduli caricati oggi rimangono residenti ma arrestati fino all'uscita da
LMU. Prima dell'uso online chiudere questa istanza. Non è stato scaricato alcun
modulo a caldo. Tutti i test dal vivo conclusi hanno
verificato il ripristino delle istruzioni; nessun collettore è stato lasciato attivo.

Audit finale indipendente alle 13:47 locali:
`logs/20261003_final_shutdown_audit.json`. Hash/percorso e IPC dei tre moduli
confermati arrestati, prologhi Dynamics/Contacts/Forces originali presenti.
