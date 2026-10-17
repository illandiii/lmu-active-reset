# Restore integration: 13 October 2026

The coordinated physics/scoring/contact transaction and its local recovery tests.

> **Historical report.** Dates, offsets, field counts, commands and statements of
> unfinished work below apply to the recorded development stage. For current
> behavior, read [reset status](../PHYSICAL_RESET_STATUS.md) and the
> [documentation index](../README.md).

The original technical record follows so that measurements and limitations remain
traceable. Referenced local capture files are not included in Git.

---

## Integrazione reset - 13 ottobre 2026

Il reset fisico completo nel gioco non è ancora operativo. La UI è sospesa su richiesta dell’utente. Nessun accesso o scrittura alla memoria LMU effettuato in questa fase.

## Percorso implementato

`active_reset_native` collega la richiesta di salvataggio/ripristino al controller sul thread proprietario, alla preparazione del piano congiunto e al backend Win32. La richiesta ha identità processo/sessione/vettura/configurazione, numero monotono e durata massima di 1500 ms. Una sessione diversa invalida il checkpoint. Il salvataggio fallito deve conservare quello precedente.

`restore_flow` congela piano e callback, entra nel confine di esecuzione con oggetti mantenuti validi e mondo fermo, esegue fisica e scoring, poi richiama la ricostruzione dei contatti. Se questa fallisce, ripristina i byte originari e ricostruisce nuovamente i contatti. Se anche il recupero fallisce, imposta un blocco persistente e richiede all’adattatore di impedire la ripresa. La compensazione è interna al coordinatore: non è un’API utilizzabile per scrivere un piano arbitrario.

`contact_batch_25661166` riutilizza il layout nodi verificato dall’osservatore contatti v2. Controlla tutte e quattro le ruote prima della prima scrittura: vettori, superfici, bitset, duplicati, geometria finita, normali, profondità scrivibili e thread proprietario. Ritira le profondità con la sentinella già studiata e chiama la routine di refresh fornita dal chiamante una volta per ruota. Controlla contatti positivi, flag storici azzerati, posa invariata e validità del proprietario. Non cancella manualmente vettori o nodi. `contact_batch_flow_contacts` è utilizzabile come callback del coordinatore, anche durante il recupero.

Questo adattatore richiede contatti a terra: non abilita il caso di vettura in aria. Non identifica autonomamente una build, una sessione autorizzata o una funzione nativa; questi sono obblighi dell’adattatore LMU. Un errore dopo l’inizio delle scritture deve essere trattato dal coordinatore con recupero o blocco. Il modello di refresh nei test è una fixture, non la routine eseguita nel gioco.

Il filtro hotkey e la coda limitata erano già integrati nel banco locale. Corretto il contatore dei comandi: riconfigurare i tasti conserva la sequenza ed evita che i nuovi comandi vengano scartati come duplicati. Nessuna nuova UI o gestione tray realizzata in questa fase.

## Verifiche eseguite

- `Build-RestoreFlow.ps1`: commit, errori di scrittura parziale, contesto scaduto, recupero contatti, piano congelato e blocco dopo recupero fallito.
- `Build-CheckpointBank.ps1`: checkpoint RAM e DLL di replay compilati.
- `Build-ContactBatch.ps1`: quattro ruote, nodo errato nell’ultima ruota senza scritture precedenti, duplicati, thread errato, perdita del proprietario, flag non azzerati, spostamento della posa; integrazione con commit, compensazione completa e blocco.
- `Build-ActiveResetRuntime.ps1`: richieste dai tasti al backend Win32, identità/scadenza, cambio sessione, conservazione record nella fixture, riconfigurazione dopo comandi già eseguiti.
- `analysis/offline-v28-flow-matrix-20261013/summary.json`: 74 campioni; 2775 coppie cronologiche e 2701 richieste temporali inverse rifiutate; sei piani rappresentativi da 10074 campi fisici e 10077 con scoring. Esecuzione e rollback soltanto su memoria della fixture. I test di flusso completi usano qui contatti sintetici; il test separato contact-batch usa il layout dei nodi.

Tutti questi test sono passati. Le pressioni reali della tastiera e il reset nel gioco non sono stati verificati in questa fase.

## Collegamento al gioco ancora necessario

1. Identità affidabile di sessione, circuito, vettura e setup e invalidazione delle generazioni.
2. Confine sul thread fisico che mantenga validi gli oggetti fisici/scoring, impedisca avanzamento durante commit e contatti e possa realmente arrestare la ripresa in caso di recupero fallito.
3. Collegamento delle quattro gomme e degli argomenti della routine nativa nel medesimo confine; verifica dei contatti dopo un effettivo ripristino di posizione.
4. Prova integrata di invalidazione del giro interrotto, conservazione dei record e validità del giro seguente.
5. Completamento/limitazione esplicita della copertura degli stati non ancora provati, inclusi danni e sistemi specifici della vettura.

Non è corretto abilitare le scritture nel gioco usando il solo successo delle fixture come prova di questi requisiti. All’ultimo controllo di questo turno il processo LMU non risultava aperto.
