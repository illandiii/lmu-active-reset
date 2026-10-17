# Prova del reset e delle collisioni - host v15

Procedura aggiornata per Sector 0.1.6; evidenza live del 16 ottobre 2026. La v15 corregge
il volume delle collisioni che attraversava la posizione precedente e quella
salvata: ripristina anche le cache cinematiche, aggiorna la geometria con le
routine native e rimuove le coppie di collisione obsolete. I test locali e sette
ripristini reali sono riusciti: tutti i 10.084 campi verificati, velocità attuale
fino a 242,6 km/h e distanza fino a 797 metri. L'utente conferma il teletrasporto
anche in curva. Le finestre dettagliate dei primi 20 passi fisici non mostrano
salti da collisione. La procedura seguente serve ad ampliare la verifica.

## Preparazione

1. Avvia LMU senza EAC, in Practice offline. Per confrontare la cattura precedente,
   usa possibilmente Spa e Mercedes GT3, con una vettura sana.
2. Apri **`Sector.exe`** dalla cartella della build portabile (oppure esegui
   `./scripts/launch/Start-ResetGui.ps1` dal checkout configurato). Si apre la UI;
   se sei al monitor, il programma attende GUIDA. Il modulo selezionato è v15.
3. Entra in GUIDA, attendi lo stato **Connected**, torna a dare il focus a LMU e
   rilascia F6 e F7. I tasti non devono avere altri comandi di gioco assegnati.
   Per arrestarlo scegli **Quit Sector** nel popup di chiusura;
   **Run in background** mantiene attivo il listener.

## Prima prova: controllo da fermo

1. Rimani fermo in un punto libero, in folle e con il freno premuto.
2. Premi e rilascia **F6**. Attendi il segnale di salvataggio riuscito.
3. Premi e rilascia **F7 una volta sola**. Attendi cinque secondi.
4. Annota se il reset è accettato, se la vettura si sposta e se LMU rimane attivo.
   In caso di rifiuto o instabilità, fermati qui e conserva il messaggio.

## Seconda prova: velocità salvata diversa da quella attuale

Eseguila solo se la prima prova è passata e sei disponibile a riprodurre il difetto.
Scegli **Quit Sector** e attendi che termini; riapri `Sector.exe` per avere una
cattura separata. Non sovrapporre due istanze. Se hai cambiato le hotkey nella
pagina Active Reset, usa i tasti selezionati al posto di F6/F7.

1. In un rettilineo libero premi **F6 intorno a 80 km/h**, con l'auto stabile e
   senza urti. Attendi il segnale di salvataggio riuscito.
2. Prosegui e accelera. Premi **F7 una sola volta** a velocità maggiore, fino a
   circa 200 km/h se il tratto lo consente. Questi valori definiscono la prova,
   non sono limiti introdotti nel software.
3. Osserva per cinque secondi: deve tornare alla posizione, orientamento e
   velocità salvati. La sola scritta «stato applicato» non dimostra la stabilità.
4. Se si verifica lo sbalzo o un crash, non ripetere F7. Se LMU resta aperto,
   scegli **Quit Sector** per arrestare il listener. Se LMU si chiude, attendi
   che la UI rilevi la disconnessione e salvi il riepilogo nei log indicati sotto.

## Terza prova: sterzata prima del ripristino

Dopo una seconda prova stabile, salva su asfalto libero con F6. Più avanti sterza
e premi F7 una volta, con orientamento diverso da quello salvato. Non è necessario
provocare un urto: nella cattura del 16 ottobre il difetto è comparso durante una
sterzata fuori pista. Attendi cinque secondi e interrompi alla prima instabilità.

Comunica soltanto: esito, velocità approssimative di F6/F7, eventuale urto e se
si è chiuso LMU oppure soltanto il programma delle hotkey. Non serve filmare.

## Ripristino dopo un urto, versione 0.2.1

1. In Practice offline senza EAC, salva con F6 quando la vettura è sana.
2. Dopo un urto che lascia le quattro ruote attaccate, premi F7 una sola volta.
3. Controlla che torni al checkpoint con la velocità salvata e che possa ancora
   accelerare, sterzare e frenare. Non basta il messaggio di successo.
4. Conserva i log anche se il ripristino viene rifiutato. Annota eventuali ruote
   staccate o il ritorno automatico al monitor: la ricostruzione di parti mancanti
   non è implementata e un cambio di vettura invalida il checkpoint.

La v16 supera la prova locale con guasto codice 4 e verifica il rollback del
guasto se fallisce la ricostruzione dei contatti. Questa prova in gioco è ancora
da eseguire.

## Dati da conservare

Nel checkout i file vengono salvati in `logs/`. Nella build portabile sono
nella sottocartella `logs/` del runtime in `%APPDATA%/sector/runtime/<versione>-<id>/`
(con il percorso predefinito di Electron). Per la struttura vedi la
[guida alla release](RELEASE.md). I file condividono il prefisso
`<data>_physical_reset_host_v17`:

- `.physical.jsonl`: corpi fisici, contatti pneumatici, corpo collisioni e stato
  prima/dopo le routine native di preparazione e correzione.
- `.reports.jsonl` e `.summary.json`: richieste, esiti, campi verificati e arresto.
- `.native.jsonl`: contesto e diagnostica del modulo.

Se LMU termina, conserva anche il suo `UserData/Log/trace_*.txt` più recente prima
di fare altre prove. Non cancellare i log del progetto. Il buffer circolare mantiene
512 campioni in memoria e viene svuotato su disco; segue i primi 20 passi Dynamics
dopo ciascun reset. Può continuare a registrare nelle sessioni lunghe. Eventuali
campioni sovrascritti prima della lettura sono indicati con `trace_gap`.

## Analisi tecnica dopo la prova

Dal checkout, con il Python locale del progetto:

```powershell
python scripts/sector.py analysis.analyze_physical_reset_trace <file.physical.jsonl> --output <file.analysis.json>
```

Verificare nell'ordine:

1. `saved_vs_applied`: velocità, posa e momento dei cinque corpi coincidono con
   F6 subito dopo F7; `verified_fields` deve coincidere con `fields`, senza
   `mismatched_fields`. Il numero di campi dipende dalla vettura. Dopo PREPARE
   (azione 2) deve comparire RESTORE (azione 3), con scritture e quattro contatti.
2. `collision_rebuilds` (fasi 15/16, prima della fase 3): geometria riallineata,
   flag pendente azzerato dalla routine nativa e corpi fisici invariati.
3. `collision_corrections` (fasi 11/12): cercare il primo cambiamento dei corpi
   dentro `F4EAC0`, confrontandolo con `proxy_vs_exported`, flag e feedback.
4. `collision_preparations` (13/14): verificare quando `F4E7B0` riallinea il corpo
   collisioni alla posa esportata. La differenza prima della preparazione può
   includere il normale avanzamento: non è da sola prova di un difetto.
5. `between_dynamics_changes` e `dynamics_changes`: distinguere una trasformazione
   comune dei cinque corpi dalla successiva divergenza delle forze.
6. Le chiamate senza uscita, i gruppi non leggibili e i valori non finiti devono
   restare indicati come evidenza incompleta, mai essere interpretati come zeri.

Le fasi 7/8 e 9/10 restano disponibili nel file grezzo per outer update e scoring.
Se il salto non compare nelle fasi 11/12, l'ipotesi della correzione collisioni
non è confermata e va seguita la prima frontiera divergente della traccia.
