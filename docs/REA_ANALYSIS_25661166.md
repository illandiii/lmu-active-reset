# REA e stati trasmissione — 12 ottobre 2026

REA 6.4.0 e Ghidra 12.1.4 sono stati usati per decompilare sei routine della
build Steam 25661166. Il reset completo non e' ancora operativo.

## Identita' e acquisizione

- Eseguibile originale SHA256: `66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf`.
- I byte su disco agli RVA ECB4E0/ECD670/ADA710 differiscono dal codice caricato.
  La prima analisi diretta e' stata interrotta e non viene usata come prova.
- Acquisiti 17.706.480 byte della sola `.text` dal PID 5472, identici in due
  letture, con ingressi Dynamics/ECD670 verificati. Zero scritture al processo.
- Copia derivata SHA256: `1aa6b2eed9f85f6cf1383c4ad298cc7b3f77f97c5516c915ade68a54c62660c9`.
  Combina codice caricato e dati/header originali: non e' un'immagine dello heap.
  Nessuna relocazione DIR64 in `.text` da normalizzare nella cattura corrente.
- Evidenze complete e snapshot REA: `analysis/rea-25661166/live-code/`.
  La sessione Ghidra e' stata chiusa con successo dopo sei analisi.

## Osservazioni e conseguenze

| RVA | Risultato osservato | Conseguenza |
| --- | --- | --- |
| ECD670 | Copia quattro output di ADA710 in HDV+46D0/+46E0/+46E8/+4B38 | Nuovo banco `DrivelineOutputs`, incluso nell'osservatore v20 |
| ADA710 | Restituisce cinque valori; ECD670 conserva gli elementi 1..4 | Conservare esattamente i quattro valori; nessun nome fisico dedotto da tipi incerti |
| ADB300 | Dossier completo del solver e chiamate virtuali | Non dimostra copertura completa degli oggetti virtuali |
| ADC3A0 | Calcolo restituito senza scritture dirette visibili sull'oggetto parametro | Non aggiungere automaticamente gli ingressi come stato dinamico; le chiamate restano da classificare |
| ECDD90 | Flag game_vehicle+1CF9C impostato a 9 con carburante esaurito, a 1 con vita motore esaurita | Guasti condivisi: ripristino coordinato ancora da implementare |
| ECD890 | Lo stesso flag puo' diventare 5 per la condizione di guasto freni | Non trattarlo come un semplice campo esclusivo dei freni |

La decompilazione include firme e tipi ricostruiti, anche imprecisi per argomenti
in registri XMM. Le decisioni sugli offset sono confrontate con l'assembly
acquisito; non si usa il solo pseudocodice per dedurre ABI o sincronizzazione.

## Implementazione e verifiche

`checkpoint_driveline_outputs` salva quattro double finiti con sequenza del banco,
prepara quattro destinazioni tipizzate e verifica contesto e sovrapposizioni.
La v20 li legge sul thread Dynamics e li aggiunge al piano, senza backend di
scrittura sul gioco. A topologia uguale alla v19: quattro campi e 32 byte in piu'.

Test locali superati: ripristino esatto su memoria isolata, byte confinanti
inalterati, rollback di cinque scritture parziali, rifiuto di sequenza errata,
NaN, contesto scaduto, destinazione vietata, lettura fallita e sovrapposizione.
La fixture osservatore supera arresto e watchdog con IPC di 306.080 byte.

La prima prova live v20 non ha caricato il modulo: il vecchio PID 5472 era
terminato. La nuova Practice era nel PID 33788; rilevato anche il processo
EasyAntiCheat_EOS, e' stato richiesto il riavvio senza EAC prima della prova nativa.
Nessun campione live v20 e nessun reset fisico dimostrato da questi test locali.

### Prova successiva dopo riavvio senza EAC

La v20 ha poi acquisito 49 campioni in 38,4 secondi dal PID 26116, thread Dynamics
17176: 15.964 callback, zero campioni saltati, arresto con status 3/error 0 e
istruzioni originali ripristinate. Audit successivo conferma modulo fermo.
Fonte: `logs/20261012T182050451896Z_snapshot_observer_v20.frames.jsonl`, SHA256
`10937b56feae82644ba07cab5f03f84fb59dca037b3c70ae87e84e355a3f55b3`.

Replay: 49 preparazioni valide, 441 verifiche negative dei nuovi output e 1.421
del banco meccanico. Piano di 10.060 campi / 254.968 byte: esecuzione Win32 su
memoria posseduta dalla fixture, verifica di tutti i byte e rollback con perdita
di contesto superati. Il backend non e' collegato alle scritture sul gioco.

I quattro nuovi output erano costanti e la marcia era zero: la prova dimostra
cattura e preparazione, non transizioni dinamiche. Shaft+8 contiene valori
estremamente piccoli; l'inventario asincrono conferma il puntatore HDV+4A80 e
i byte `5800000000000000` a +8. La semantica/inizializzazione di questo valore
resta da verificare in movimento; non e' stata inventata una correzione numerica.

## Riproduzione

### V22: verifica in movimento

Nuova identita' DLL/IPC per lo stesso osservatore v21, senza modifiche al modello
dei campi. Log `20261012T183824116210Z_snapshot_observer_v22`: 70 campioni su
55,515 secondi, 22.531 callback, zero salti/errori. Il banco nativo ha eseguito
2 salvataggi e 68 preparazioni, zero rifiuti; durante interruzioni del contesto
la cattura viene sospesa e il banco viene rinnovato.

Osservate 16 variazioni di marcia, minimo 0/massimo 4. Shaft+8 varia fra
386,081433 e 790,105640; HDV+46D0 fra -64,956260 e 533,779493 e HDV+4B38 fra
-116,601399 e 525,098063. Gli ultimi due output cambiano in tutti i 69 intervalli.
HDV+46E0/+46E8 restano zero: nessuna prova dinamica di questi due campi sulla
configurazione corrente. Il valore quasi nullo di shaft+8 da fermo non e' stato
alterato o normalizzato.

Replay: 70 preparazioni valide, 630 controlli negativi sugli output e 2.030 sul
banco meccanico. Piano completo di 10.060 campi / 254.968 byte: scrittura Win32,
verifica, byte esterni inalterati e rollback per perdita di contesto superati
esclusivamente su memoria fixture. Flag guasto sempre zero.
Fonte SHA256 `2c6a1f5c1e34b963bb1618bbe08ac13e1359040a0ba72401d3c4beb153e8c5a7`.
La cattura dinamica non equivale a un reset in movimento; `full_restore_supported`
resta falso. I moduli restano residenti ma arrestati fino all'uscita dal gioco.

### V21: rifiuto dei guasti non ripristinabili

La v21 osserva il flag condiviso `game_vehicle+1CF9C`. Qualunque valore nonzero
invalida il banco precedente e impedisce la preparazione di un piano; il flag
non viene scritto. Quando torna zero si crea un nuovo banco, senza riutilizzare
quello precedente al guasto. Questo e' un limite esplicito del prototipo, non
supporto al ripristino dei danni. Fixture con valori 1, 5, 9 e FFFFFFFF, ritorno
a zero, arresto e watchdog: tutte superate.

Prova live `20261012T183448537516Z_snapshot_observer_v21`: 74 campioni in 58,4 s,
23.900 callback, zero salti/errori, 1 salvataggio e 73 preparazioni native.
Flag sempre zero; originali ripristinati. Replay locale: 74 preparazioni,
666 verifiche negative sugli output e 2.146 sul banco meccanico; piano completo
da 10.060 campi con verifica e rollback Win32 su memoria fixture superati.
Fonte SHA256 `a7835361ad6a352e0edfb28092279aa52de8e4d403b980a17ade260afb303892`.
Marcia, ingressi e output sono rimasti costanti: nessuna transizione dinamica
dimostrata. La verifica del guasto e del recupero resta una prova su fixture.

Dipendenze locali: `work/rea/package-lock.json` (REA 6.4.0),
`work/tools/ghidra_12.1.4_PUBLIC`, JDK completo indicato al wrapper.
ZIP ufficiale Ghidra verificato SHA256
`ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`.

1. `src/python/sector/research/capture_rea_code.py --pid <PID senza EAC> --output <nuova copia .bin>`.
2. `Invoke-ReaAnalysis.ps1 -CodeCapture <copia> -OutputDirectory <nuova directory>`.
3. Consultare le evidenze gia' salvate prima di ripetere un'importazione Ghidra.
4. `Build-DrivelineOutputs.ps1 -ZigPath <zig.exe>` e
   `Build-SnapshotObserverV2025661166.ps1 -ZigPath <zig.exe>`.

Restano incompleti cambio/frizione, gestione guasti e identita' completa della
sessione/configurazione, collegamento del ripristino nativo, contatti, scoring,
hotkey e prova completa di reset in movimento.

## 2026-10-12: controllo powertrain parziale, osservatore v23

REA C6FA00/C6FBB0 identifica il blocco HDV+4740, indici e tabelle del controllo
powertrain. Non equivale alla copertura completa di cambio/frizione. EBDF40
ha superato il limite della risposta MCP (10.584.820 byte); l'evidenza completa
è stata recuperata dallo snapshot, senza ripetere l'analisi:
`analysis/rea-25661166/gearbox/ebdf40.recovered-evidence.json`, evidenza
`ev_6718bb4ce308b4efdb9bd6adc426344a0a8de993c87f41e943dfad98673d86c4`.

Aggiunti `checkpoint_powertrain_control.{h,c}` e osservatore v23: 6 intervalli,
58 byte (rampa, indici, flag, timer/fattori e due valori motore). I nomi sono
basati sugli offset, non su interpretazioni ancora non dimostrate. Identità
parziale delle quattro tabelle ottenuta da conteggi e valori; tabella vuota
ammessa solo con indice zero. Il token entra nella configurazione del banco.

Cattura `20261012T185803738431Z_snapshot_observer_v23`: 55 campioni, 54
preparazioni, zero rifiuti, thread 17176, 10.066 campi / 255.026 byte.
SHA256 registrazione `7842b68573f96691a172ee33cd85b48a84ae2fc5ef4a886b22f1b773e569f285`.
Nessuna scrittura fisica; Dynamics originale ripristinata e modulo arrestato.
I nuovi valori sono rimasti costanti: transizioni dinamiche non dimostrate.

Test: 660 verifiche negative del controllo, 1.595 sul banco meccanico,
rollback delle sette possibili scritture parziali nel test dedicato;
transazione Win32 locale completa e con punteggio (10.069 campi), inclusi
rollback e conservazione dei byte non coinvolti. Fixture osservatore anche
con tabelle [0,2,2,2] e valori non nulli. Questi sono test su memoria posseduta,
non un reset completo della vettura in LMU.

Rimane da ricostruire lo stato di cambiata EBA940: richieste, timer,
accumulatori e interazioni con notifiche/tempo di simulazione. I timestamp
non vanno copiati ciecamente fra istanti diversi. L'inventario asincrono ha
confermato sei marce e i tipi iniziali sulla Mercedes; non sostituisce la
cattura coerente sul thread fisico.

## 2026-10-12: transitori cambio v25

Analisi REA completata e sessione chiusa in `analysis/rea-25661166/shift-transitions`:

- EBA940: `ev_4d134a1ac71bdccc70d80b4ab0bac01c29b56922f8a771fcc5c7108d6a802671`.
- B36020: `ev_8708ea17620e64cfaf6241b82507c05f8da55b849aef6c4e619e30057002a53e`.
- C70000: `ev_337d82123f052fa10b37ff3096e6fd65313e82ca2d31212bd7895eb348c9f906`.

EBA940 scrive richiesta/precedente/direzioni del cambio, timer, controllo e
ultimo evento. B36020 aggiorna l'accumulatore float HDV+4B30 limitandolo a [0,1].
EBDF40 aggiorna anche +4BB8; questo secondo accumulatore non viene arbitrariamente
limitato a 1 nel checkpoint. Il timer +4AC0 può essere negativo, come mostrano
i confronti in EBDF40: non viene forzato a zero.

`checkpoint_shift_transient.{h,c}` salva e prepara tali stati. Il timestamp
HDV+50 viene riallineato al clock HDV+A8 conservando l'età della richiesta;
clock e timestep non vengono ripristinati. Il piano verifica la coerenza della
marcia selezionata con il campo gearbox+64 già presente. Configurazioni con
numero di marce/timestep differente, tempo regressivo, stati non finiti,
richieste fuori intervallo o preparazioni temporalmente incoerenti sono rifiutate.

V24 ha osservato clock/timer (74 campioni). V25 collega salvataggio, preparazione
e 8 intervalli aggiuntivi (64 byte) al piano nativo, sempre senza writer fisico.
Registrazione `20261012T190917312227Z_snapshot_observer_v25`: 56 osservazioni,
55 campioni persistiti, 54 preparazioni nell'ultimo campione persistito, zero
rifiuti; 10.074 campi / 255.090 byte. Marcia zero e nessun evento nuovo:
la transizione dinamica del nuovo stato resta da verificare.
SHA256 fonte `119a3dd6745d86da553e68bfa3995325cf65430474d8a5345fc4090c79da7795`.

Replay: 55 preparazioni, età della richiesta preservata, differenza fra tempo
Dynamics e clock+A8+dt circa 5,46e-14. Piano completo su memoria locale e piano
con punteggio da 10.077 campi superano scrittura, rilettura e rollback, inclusa
la perdita di contesto durante lo scoring. Marcia incoerente e timestamp non
riallineato sono rifiutati senza pubblicare un piano parziale.

L'RTTI della copia analizzata identifica 1ABE948 come ElectricMotor, 1ABE970
come EngineInterface e 1AE5CE0 come InternalCombustionEngine. Il ramo C6FBB0
ha dipendenze del motore elettrico ancora non coperte; la Mercedes osservata
utilizza il percorso a combustione C6FA00. Non estendere il supporto agli ibridi
sulla base delle sole prove GT3.

Audit `logs/20261012_v25_shutdown_audit.json`: sette moduli arrestati, prologhi
Dynamics e Contacts originali. Le sole 211 scritture fisiche appartengono alla
ricostruzione contatti ai box, non al ripristino di un checkpoint. Restano:
transizioni live del cambio, identità completa sessione/setup, copertura dei
restanti stati e danni, coordinamento writer/contatti/scoring con recupero degli
errori successivi al commit, hotkey e reset completo in movimento.

## Cattura v26, 12 ottobre: rientro al monitor durante la finestra

Registrazione `20261012T191646763504Z_snapshot_observer_v26`: 20 campioni in
15,2 secondi, 19 preparazioni native senza rifiuti. Marcia selezionata/richiesta
sempre 1, precedente -1, timestamp dell'evento 3382,4. Questo aggiunge un caso
reale con timestamp non nullo, ma non dimostra transizioni durante la cattura.
Successivamente il gate ha rilevato `monitor_or_not_realtime` (327 controlli)
e ha sospeso l'osservazione. Arresto regolare, istruzioni Dynamics ripristinate,
zero scritture fisiche. Replay completo, conservazione dell'età della richiesta
e rollback fisica/scoring su memoria locale superati.
SHA256 fonte `a4ead615974768844ea34b6e3e1b3f8cc277dda22f2c733250423052255a04e6`.
V27 compilata e testata, non ancora caricata: pronta per la prossima finestra
di guida continuativa. V26/V27 mantengono il comportamento della v25 e usano
identità DLL/IPC distinte per non sovrascrivere moduli residenti.

## Cattura v27 e avvio sincronizzato sul gas

`20261012T192505390879Z_snapshot_observer_v27`: 74 campioni, 73 preparazioni,
zero rifiuti/errori. Telemetria indipendente e memoria nativa concordano:
prima marcia, motore circa 1438 rpm, gas zero, vettura ferma ai box. Nessuna
transizione del cambio osservata. Replay temporale superato su tutti i campioni.
SHA256 fonte `4cac2279c95372a120c0f1dffcca7ce7475b18ab159e7362e57ca28d18f2f35e`.

Aggiunto `src/python/sector/observers/run_motion_triggered_snapshot.py`: attende tre campioni freschi
con gas superiore al 10% e contesto offline GUIDA verificato, poi avvia la
cattura v28 per 60 secondi. Il tempo d'attesa è limitato a 10..180 secondi;
nessuna DLL viene caricata se il trigger non arriva. Il lettore di attesa viene
arrestato prima del controller nativo, che ripete i controlli di contesto,
identità, EAC, moduli residenti e prologhi. Il trigger non autorizza scritture
fisiche. V28 è funzionalmente identica a v25, con identità distinta.

Esito attesa sul gas: finestra di 180 secondi terminata senza trigger; telemetria
sempre gas zero e vettura ai box. V28 non caricata. Audit finale
`logs/20261012_v27_shutdown_audit.json`: nove moduli arrestati, Dynamics/Contacts
originali. Prima di un'altra registrazione occorre una variazione reale degli
input oppure, se l'utente conferma di avere accelerato, verificare la discrepanza
fra la sua azione e il lettore; non assumere che un'altra finestra identica risolva.

## V28: cattura dinamica riuscita con trigger acceleratore

Registrazione `20261012T193143600414Z_snapshot_observer_v28`, avviata dopo tre
campioni di acceleratore verificati: 74 campioni su 58,4 secondi, marce 1..4,
16 variazioni del timestamp di richiesta, 14 variazioni del timer (minimo -0,095)
e quattro variazioni dell'accumulatore HDV+4B30, osservato fra 0 e 1.
Direzione precedente osservata in entrambi i segni. Il campionamento non ha
catturato una direzione corrente diversa da zero; controllo +4668 e accumulatore
+4BB8 sono rimasti zero. Non attribuire a questa prova copertura di tutti i rami
durante una cambiata o un ripristino effettivo nel gioco.

Un salvataggio e 73 preparazioni native senza rifiuti; piano da 10.074 campi,
255.090 byte. Replay di 74 stati e conservazione dell'età della richiesta
superati; differenza clock Dynamics rispetto ad A8+dt <=5,1e-13. Replay integrale
con writer Win32 locale e rollback superato, inclusi i 10.077 campi con scoring.
Controllo powertrain: 888 verifiche negative; questi valori sono rimasti costanti.
Fonte SHA256 `23bfff58de10d485ef55f822c075343c8ed6b8c4457888b4e36253d01e430056`.

Zero scritture fisiche durante la cattura. Audit `logs/20261012_v28_shutdown_audit.json`:
dieci moduli arrestati; prologhi Dynamics/Contacts originali. La discrepanza delle
prove precedenti non si ripresenta con avvio sincronizzato sul gas; telemetria
e campi interni ora mostrano guida e cambiate. Restano invariati i blocchi per il
reset completo: copertura residua, identità completa e coordinamento della
scrittura fisica con contatti/scoring e recupero dopo errori successivi al commit.

## Matrice offline dopo chiusura del gioco

Completato `src/python/sector/replay/verify_offline_checkpoint_matrix.py` sui 74 campioni v28:
2.775 coppie temporali valide preparate, 2.701 regressioni temporali rifiutate,
errore massimo di conservazione dell'età della richiesta pari a zero nei dati.
Sei piani rappresentativi da 10.074 campi superano esecuzione/rollback locale,
incluso scoring (10.077 campi). Residuo rotazioni 2,465e-14; errore omega massimo
1,223e-12. Report in `analysis/offline-v28-checkpoint-matrix/summary.json`.
Avanzamento tecnico stimato circa 70%; dettagli e lavoro residuo in
[STATO_2026-10-12.md](STATO_2026-10-12.md). Nessun reset live completo ancora provato.
