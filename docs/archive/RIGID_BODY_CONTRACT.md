# Early rigid-body capture contract

Historical layouts and invariants from the 14130/14150 body-capture investigation.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

> Aggiornamento 21 settembre 2026: acquisizione coerente di 16 corpi sulla build 14150 e preparazione cinematica parziale provata su 1.456 stati, in [KINEMATIC_INTEGRATION.md](KINEMATIC_INTEGRATION.md). Nessun ripristino di posa nel processo. Le sezioni seguenti descrivono la ricerca storica su 14130; la DLL precedente non è abilitata sulla nuova build.

## Corpi fisici: contratto parziale verificato

Build: EXE SHA-256 `BB8661CF6F96A056374AFAC29018C1FDD71F86164C158A71043BB7C244F7407E`. Questi risultati descrivono letture e aritmetica osservate; **non costituiscono un contratto completo di ripristino**.

## Evidenza dinamica

Il modulo osservazionale ha intercettato `HighDetailVehicle::Integrate` a RVA `0xEAF350` nella Practice offline a Spa con Mercedes GT3. Firma usata: `void (void* vehicle, double dt)` nella convenzione Windows x64. Il trampolino ha chiamato l'originale una volta per callback. Un solo thread ha pubblicato osservazioni; nessuna scrittura nello stato fisico.

Il passo passato alla funzione era 1/2400 s. Nei 71 campioni conservati, `post_position - pre_position` coincide con `post_velocity * dt` entro 1,17e-13 m. È una verifica del singolo sottopasso; non dimostra la frequenza di aggiornamento di tutti i sottosistemi. Il precedente campo `vehicle+0xB0 = 0,0025 s` appartiene a un altro livello temporale e non va usato come dt di questa chiamata.

La lettura successiva nel monitor (`rigid-body-layout-observation.json`) ha esaminato 4.224 byte, con limiti espliciti e ricontrollo dei puntatori. Mercedes: 12 elementi nel vettore `vehicle+0x23F8`; due nel vettore `vehicle+0x2440`, entrambi inclusi nel primo. Il precedente conteggio 13 apparteneva alla Cadillac, non è un limite universale. Nessun tipo di ruota/sospensione è stato assegnato arbitrariamente agli indici.

## Campi del singolo corpo

Tutti gli offset sono relativi al puntatore del componente, non al veicolo né alla telemetria esportata. La semantica deriva dalle operazioni delle routine sotto e dai controlli numerici; la copertura resta parziale.

| Offset | Formato | Significato ricostruito |
|---|---|---|
| `0x10` | 3 double | Forza mondiale accumulata F; non posizione |
| `0x28` | 3 double | Velocità lineare mondiale v |
| `0x40` | 3 double | Posizione mondiale x |
| `0x58` | double | Inverso della massa |
| `0x60` | 6 double | Inerzia inversa mondiale simmetrica, ordine xx, xy, yy, xz, yz, zz |
| `0x90` | 9 double | Matrice R per la trasformazione da locale a mondo, per righe |
| `0xD8` | int32 | Contatore usato per rinormalizzare R periodicamente; stato accessorio da gestire |
| `0xE0` | 3 double | Velocità angolare mondiale ω |
| `0xF8` | 3 double | Diagonale dell'inerzia inversa locale |
| `0x110` | 3 double | Momento angolare mondiale L |
| `0x128` | 3 double | Momento della forza accumulato τ |
| `0x140` | 3 double | Diagonale dell'inerzia locale |

## Relazioni ricostruite

- `0x3D4F60`: `v ← v + F * inverse_mass * dt`.
- `0x3D4F90`: `x ← x + v * dt`.
- `0xFFF500`: chiama l'aggiornamento lineare, poi `L ← L + τ * dt` e aggiorna ω.
- `0xFFD5F0`: `I_world_inverse ← R * diag(I_local_inverse) * transpose(R)`.
- `0x3D50C0`: `ω ← I_world_inverse * L`.
- `0xFFF9F0`: `L ← R * diag(I_local) * transpose(R) * ω`.
- `0xFFF000`: aggiorna posizione e orientamento; include termini giroscopici, rinormalizzazione periodica di R e ricalcolo dell'inerzia mondiale. Non è stato riprodotto come integratore alternativo.

Le due routine lineari non hanno una voce unwind corrispondente. L'analizzatore ora accetta soltanto i bersagli di chiamata individuati, con decodifica rettilinea limitata a 256 byte fino a RET; non attribuisce più al bersaglio l'intervallo della funzione precedente. Le procedure con unwind concatenato restano ricostruite tramite `UNW_FLAG_CHAININFO`.

Sui 12 corpi letti nel monitor: errore massimo di ortogonalità 4,45e-16; determinante entro 4,45e-16 da +1; errore massimo assoluto dell'inerzia inversa mondiale 1,43e-14; errore massimo dei prodotti delle diagonali reciproche 1,12e-16. Sono riscontri sul layout in questa configurazione. La lettura esterna non è un'acquisizione atomica della fisica e non misura l'evoluzione rotazionale durante la guida.

## Implicazioni per un futuro ripristino

Posizione, orientamento e velocità del veicolo aggregato vengono ricalcolati da più corpi. Scrivere soltanto `vehicle+0x24B0` e `vehicle+0x2498` non ripristina questi corpi. Occorre inoltre mantenere coerenti R, inerzia, L e ω. I puntatori e le risorse degli oggetti non vanno copiati dentro un checkpoint.

Questo contratto non copre ancora: cache di collisione/contatto e vincoli, stato completo di gomme e sospensioni, motore e trasmissione, input filtrati, carburante/danni, generazione di sessione/configurazione, né progressione del giro. Il punto osservato è un sottopasso dell'integrazione; non è ancora dimostrato come finestra valida per ripristinare tutti i sottosistemi. Le routine elencate non sono state chiamate direttamente come comandi di ripristino.

Nessun checkpoint completo in RAM, reset reale, test di conservazione dei record o misura di latenza del reset è stato eseguito.
