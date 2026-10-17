# Kinematic preparation experiments

The 21 September capture and local replay of coherent rigid-body states.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Integrazione cinematica - 21 settembre 2026

Aggiornamento 3 ottobre: acquisiti **17 corpi della Cadillac sulla 14200**,
carburante e batteria; identificata la dipendenza delle inerzie pneumatici.
Risultati e nuovo osservatore in [DEVELOPMENT_20261003.md](DEVELOPMENT_20261003.md).
Questa pagina conserva la prova Mercedes/14150 e non abilita il reset nella build nuova.

**Acquisizione coerente dei 16 corpi e ordine contatti/forze verificati nel gioco. Preparazione cinematica aggiornata e provata sui dati registrati. Nessun ripristino della posa applicato al processo.** La prova dei contatti precedente resta distinta: [CONTACT_REBUILD.md](CONTACT_REBUILD.md).

## Osservazione reale

Nella stessa Practice offline 14150 è stato caricato `lmu-pose-observer-14150.dll`, SHA-256 `a6291b8061bb762e5f11e091dc1153be639106fec23db1df5218dc97912f3489`. Il controller ha verificato che il precedente modulo contatti fosse già fermo, il relativo manifest e i prologhi originali prima di creare i nuovi hook. Nessun modulo è stato scaricato a caldo o riattivato.

Il modulo osserva tre funzioni, inoltrandone gli argomenti e chiamando ciascuna originale una volta:

| Funzione | RVA 14150 | Contratto osservato |
|---|---|---|
| Dynamics | `0xEB79E0` | `void(vehicle*, double simulationTime)`; il double non viene trattato come dt |
| Aggiornamento contatti | `0xEE5030` | contratto a cinque argomenti già verificato nella prova contatti |
| Forze | `0xEB7030` | `void(vehicle*)` |

Un contesto locale al thread associa le chiamate annidate al ciclo osservato. Si acquisisce un ciclo ogni 32. Prima e dopo l'originale Dynamics vengono letti soltanto i campi numerici dichiarati, ricontrollando owner, vettori, appartenenza e puntatori degli pneumatici. Gli indirizzi non vengono salvati nel log. Il controller mantiene il controllo Practice offline e una lease massima di 300 ms; questo modulo non ha un comando di scrittura fisica.

Risultati conservati in `logs/20260921T184750615694Z_pose_observer.*`:

- 4.839 chiamate Dynamics intercettate, 148 osservazioni pubblicate, 91 coppie prima/dopo conservate, nessuna osservazione scartata.
- Un solo thread fisico, ID 30320; 12 corpi nel vettore principale e quattro corpi interni agli pneumatici, distinti dai primi.
- I due corpi che contribuiscono all'aggregato sono gli indici 0 e 1. I puntatori corpo delle quattro ruote corrispondono agli indici 6, 7, 8 e 9. Gli indici 12–15 sono i corpi interni degli pneumatici.
- Sei chiamate al calcolo forze per ciclo; due aggiornamenti contatti per ciascuna ruota. Tutte e quattro le ruote completano un aggiornamento prima del primo e del quarto calcolo forze: maschere `[15,0,0,15,0,0]` in tutti i 91 cicli conservati, zero errori di ordine.
- La ricostruzione dell'inerzia mondiale coincide con i valori letti. Errore massimo della relazione `omega = inverseInertia * L`: `1,2144e-17` circa.
- Freno realmente rilevato tra 0,2443 e 0,2475 durante la prova. Il precedente controllo preliminare aveva invece letto zero: il dato è stato corretto nella comunicazione e non usato per autorizzare una scrittura.
- Stato finale 3, errore 0, byte originali ripristinati per tutte e tre le funzioni. Lettore terminato normalmente: 127 campioni, nessuna contesa. Nessuna scrittura a posa, contatti, scoring o parametri auto in questa acquisizione.

Mediana della chiamata Dynamics originale 314,9 µs; mediana della misura comprendente acquisizioni e calcoli 412,4 µs. Sono misure campionate dell'osservatore, non la latenza di un reset o una misura completa dei frame time.

## Massa e inerzia non sono costanti

Il nucleo originario di preparazione passa sui valori prima e dopo il ciclo presi separatamente. Rifiuta invece tutti i 91 tentativi di preparare lo stato precedente con le proprietà correnti (`RIGID_PREPARE_MASS_CHANGED`). Non è un errore di layout:

- L'inverso della massa del corpo 10 cambia in tutte le 91 coppie.
- L'inerzia locale dei corpi pneumatico 12–15 cambia in 56–59 coppie per corpo.

La variazione del corpo 10 è compatibile con il consumo osservato; l'identificazione completa del relativo oggetto e del percorso di aggiornamento resta da completare. Non si assume che tutte le variazioni derivino dal carburante: quelle degli pneumatici riguardano anche l'inerzia. Non sono state alterate le tolleranze per fingere che le proprietà coincidano.

## Preparazione parziale esplicita

La nuova funzione `rigid_prepare_kinematics_current_mass` è separata da `rigid_prepare_restore`. Conserva posizione, orientamento, velocità lineare e velocità angolare salvate, usando intenzionalmente massa e inerzia **correnti**:

1. Verifica i valori salvati e ricava la loro velocità angolare.
2. Trasforma tale velocità nel riferimento locale e calcola il momento angolare con l'inerzia corrente.
3. Ricostruisce tensore mondiale e velocità angolare; pubblica il risultato solo dopo la validazione di tutti i corpi.

Non conserva l'energia cinetica dello stato precedente quando cambia l'inerzia; non ripristina carburante, temperatura, pressione, danni o storie dello pneumatico. È una base per un esperimento di sola cinematica, non un sostituto silenzioso del checkpoint completo richiesto. Il percorso completo resta rigoroso e continua a rifiutare proprietà differenti.

Prove eseguite:

- Caso analitico con rotazione di 90°, inerzie diverse e momento angolare atteso `(10,9,28)`, mantenendo velocità angolare `(2,3,4)`.
- Errori nell'ultimo corpo (massa non valida, overflow, rotazione non valida): nessuna modifica dell'output dell'intero gruppo.
- Preparazione del vero nucleo C su tutte le 91 coppie registrate: **1.456 corpi preparati**, massa/inerzia correnti mantenute, posa e velocità lineare salvate invariate, errore massimo della velocità angolare `4,34e-18`.
- Le 91 chiamate al percorso completo continuano a essere rifiutate, senza output parziale. Dati d'ingresso immutati.
- Test originari ancora superati: 512 rotazioni, energia, gruppo di 32 corpi, errori tardivi e 12 corpi storici registrati.

Log: `logs/20260921-pose-preparation-test.json`. `Build-KinematicPreparation.ps1` genera una DLL di fixture utilizzata soltanto nel processo Python locale, mai caricata in LMU. La DLL osservazionale usata dal vivo era stata compilata prima dell'aggiunta della funzione parziale e contiene soltanto la preparazione rigorosa.

## Passaggio ancora necessario

L'ordine osservato identifica un candidato per l'applicazione della posa prima dei contatti. Non dimostra da solo che tutti i vincoli e gli stati dello pneumatico siano ripristinabili con quei 16 payload: il percorso ruota aggiorna anche angoli, velocità derivate, deformazioni e storie. Occorre completare queste dipendenze e il legame con sessione/configurazione prima di applicare un checkpoint nel processo. Non sono ancora disponibili due hotkey operative o una prova di conservazione dei tempi dopo ripristino.

I moduli sono inattivi ma restano residenti fino alla chiusura del gioco. Chiudere questa istanza prima dell'uso online.

## Comandi riproducibili

```powershell
.\Build-PoseObserver.ps1
## Nuova osservazione offline; rifiuta un osservatore posa già residente.
python .\src/python/sector/observers/run_pose_observer.py --pid <PID_ATTUALE> --seconds 12 --load --offline-launch-confirmed

.\Build-KinematicPreparation.ps1
python .\src/python/sector/replay/test_pose_recording.py .\logs\20260921T184750615694Z_pose_observer.frames.jsonl
```
