# Development notes: 2 October 2026

Build 14200 migration and the mechanical-state investigations performed that day.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Sviluppo del 2 ottobre 2026 — build 14200

Questa pagina descrive la prima fase del 2 ottobre. La successiva cattura con
DLL dei collegamenti ruota/mozzo, completata nella stessa serata, e il lavoro del
3 ottobre sono riportati in [DEVELOPMENT_20261003.md](DEVELOPMENT_20261003.md).

Il reset completo non è operativo. Questa sessione ha aggiunto la compatibilità
di sola osservazione con LMU 1.4.2.0 e un componente del ripristino delle ruote,
verificato sui dati della vettura in movimento. Nessuna DLL caricata, nessuna
scrittura alla fisica, nessun ripristino di posa o modifica dello scoring.

## Ambiente e compatibilità

Workspace recuperato in `C:\Users\aless\Documents\ChatGPT\rewind`.
Build Steam 25611540; appBuild/native 14200. Identità dei file e contratto UI in
`compatibility/14200-telemetry.json`. EXE SHA256:
`B37A1DD3A5912391DB236051995CAADD6D5558631ED9CE5D68F8397ABE2F6A8C`.

PluginsAdapter e SharedMemoryInterface.hpp sono cambiati. Il nuovo header
contiene anche l'attesa degli eventi Hold/Data. Il lettore usa il fast path
non bloccante del lock SDK, ancora presente: la ricompilazione con l'SDK nuovo
conferma dimensioni e offset mediante static_assert, schema e 24 test nativi.
`LMU_Data` è mappata in lettura; soltanto il lock condiviso viene acquisito e
rilasciato secondo il protocollo SDK. Nessun reset del lock.

Il profilo 14200 è separato, usa `diagnostic-14200.exe` e il wrapper
`capture_preparation_14200.py`. Il gate richiede selezione esplicita della build;
gli strumenti precedenti restano rifiutati. `reset_permitted` resta sempre falso.
Il contratto singleplayer è stato letto nel nuovo bundle UI installato:
`settingMode === SETTING_GRANDPRIX`. Non sono redistribuiti header SDK o codice UI.

## Registrazione reale

Cadillac Hypercar a Fuji, Practice1 offline. Registrazione di 180 secondi:
`logs/20261002T214501Z_fuji_hypercar_14200.*`.

- 1.640 campioni nativi, 1.585 in guida; velocità massima 312,80 km/h.
- Marce 0–7, carburante 1,95–13,60 litri, batteria 0,693–1.
- Miglior giro osservato da 1:35.045 a 1:30.072; nessuna prova post-reset.
- Gate: 621 osservazioni ammesse, 21 vietate, 7 sconosciute;
  zero errori REST, lettore terminato normalmente con exit code 0.

## Ricostruzione dei collegamenti nella build nuova

Analisi read-only del codice caricato, ancorata alle stringhe del profiler e alle
entry della tabella unwind. Le routine sono state individuate nuovamente, senza
assumere uno spostamento costante degli indirizzi.

| Collegamento | Evidenza 14200 |
| --- | --- |
| Owner globale | istruzione RVA BA31BF, next RIP BA31C6 + disp 1273022 = 1E161E8 |
| Owner → HDV | offset **159A0**, diverso da 15978 della 14150 |
| HDV Dynamics | EC87E0, sei sottopassi |
| ForcesAndTorques | EC7E30 |
| HDV Integrate | EC9F20 |
| Integrazione corpi | ECA670, vettore 23F8, chiamata 101D8B0 |
| Collegamento mozzi/ruote | ECA8E0 → EE1310 |
| Trasformazione ruota attaccata | EE1503–EE174C, frammenti unwind di EE1310 |
| Inerzia world e L da omega | 101B9A0 e 101DDA0 |

Le quattro ruote rimangono a HDV+618, passo 6B0. Corpo ruota a +1D0;
mozzo normalmente a +1D8; per le posteriori esiste la selezione del riferimento
condiviso HDV+20D8. Angolo a +120 e velocità angolare a +128. Il ramo attaccato,
senza deformazione, ruota le colonne Y/Z della matrice del mozzo e somma alla
omega del mozzo la velocità di rotazione lungo il suo asse X. Poi ricalcola
inerzia world e momento angolare. Il ramo deformato usa anche +308/+310/+318;
non è incluso nel componente implementato e viene rifiutato.

Questi risultati non approvano hook, contatti o scritture per 14200. In
particolare il cambiamento dell'owner dimostra che aggiornare soltanto hash/RVA
delle DLL vecchie sarebbe scorretto.

## Nuovo codice di ripristino delle ruote

`wheel_restore_core.h/.c` contiene solo valori numerici posseduti dal chiamante:

- `wheel_prepare_spin`: ricostruisce orientamento, omega, L e inerzia inversa
  world delle quattro ruote da stato salvato dei mozzi, angolo/rate e massa.
- `wheel_prepare_consistent_checkpoint`: confronta la ricostruzione con i corpi
  ruota salvati; rifiuta incongruenze superiori a 1e-8. Pubblica i quattro
  risultati soltanto dopo la validazione dell'intero gruppo.

I test analitici coprono mozzo ruotato, angolo di 90 gradi, inerzia anisotropa,
ruota staccata/deformata, NaN e inerzia nulla sull'ultima ruota senza output
parziale. Non vengono copiati oggetti del motore, puntatori o cache native.

`probe_wheel_binding_14200.py` ha letto esclusivamente campi numerici e
collegamenti necessari, con verifica delle istruzioni dell'owner, contesto,
limiti di lettura e doppia lettura dei valori. Su 908 tentativi: 607 doppie
letture stabili e 301 rifiutate. 416.510 byte totali letti in 30 secondi.
La doppia lettura non garantisce una cattura coerente sul thread fisico.

Replay dei 607 campioni attraverso il modulo C reale, caricato soltanto nel
processo Python locale: errore rispetto alla formula indipendente <=1,43e-14;
606 concordanze con il gioco. Una lettura della ruota 1 è incoerente: errore di
matrice 0,09856 e omega 1,9154 rad/s. Il validatore la rifiuta anche inserendola
come ultima ruota del gruppo, mantenendo l'output completamente intatto.
L'incoerenza è compatibile con una cattura tra fasi, ma la causa non è dimostrata
dalla lettura asincrona. Non viene usata come checkpoint.

Log e risultato: `logs/20261002T215024Z_wheel_binding_14200.json` e
`.replay.json`. La fixture usa inerzie isotrope sintetiche per fornire le omega
osservate; non pretende di aver misurato il momento nativo in questa acquisizione.
Suite Python: 35 test passati.

## Compilazione e prossimi blocchi tecnici

`Build-WheelRestore.ps1 -ZigPath <zig.exe>` compila ed esegue le fixture in work.
`Build-Telemetry14200.ps1 -ZigPath <zig.exe>` ricompila il diagnostico in work
senza sovrascrivere il binario con hash approvato. Nessuno dei due carica DLL in LMU.

Restano necessari: cattura coerente e applicazione sul thread fisico della
14200; stati autorevoli di sospensioni/vincoli e trasmissione; storie degli
pneumatici e stati termici, carburante, danni e ibrido; ricostruzione contatti
dopo cambio posa nella build nuova; gestione del giro interrotto e prova di
conservazione dei tempi. I contatti provati sulla 14150 restano un risultato
storico, non una validazione della 14200. Non è richiesto un altro gesto del
pilota per proseguire l'analisi del codice.
