# Prova del reset e delle collisioni — host v14

Aggiornata il 16 ottobre 2026 per la prossima sessione. Non occorre aprire LMU
durante il lavoro offline. Il difetto dello sbalzo non è ancora risolto: questa
versione registra il passaggio che può provocarlo, senza disattivare collisioni
o limitare velocità e distanza.

## Preparazione

1. Avvia LMU senza EAC, in Practice offline. Per confrontare la cattura precedente,
   usa possibilmente Spa e Mercedes GT3, con una vettura sana.
2. Apri **una sola volta `Start-Reset.cmd`** nella cartella del progetto. Si apre
   la UI; se sei al monitor, il programma attende GUIDA. La build selezionata è v14.
3. Entra in GUIDA, attendi lo stato **Connected**, torna a dare il focus a LMU e
   rilascia F6 e F7. I tasti non devono avere altri comandi di gioco assegnati.
   Il programma resta attivo finché chiudi la UI o termina la connessione.

## Prima prova: controllo da fermo

1. Rimani fermo in un punto libero, in folle e con il freno premuto.
2. Premi e rilascia **F6**. Attendi il segnale di salvataggio riuscito.
3. Premi e rilascia **F7 una volta sola**. Attendi cinque secondi.
4. Annota se il reset è accettato, se la vettura si sposta e se LMU rimane attivo.
   In caso di rifiuto o instabilità, fermati qui e conserva il messaggio.

## Seconda prova: velocità salvata diversa da quella attuale

Eseguila solo se la prima prova è passata e sei disponibile a riprodurre il difetto.
Chiudi prima la UI e attendi che termini; riapri `Start-Reset.cmd` per avere una
cattura separata. Non sovrapporre due istanze. Se hai cambiato le hotkey nelle
impostazioni, usa i tasti selezionati al posto di F6/F7.

1. In un rettilineo libero premi **F6 intorno a 80 km/h**, con l'auto stabile e
   senza urti. Attendi il segnale di salvataggio riuscito.
2. Prosegui e accelera. Premi **F7 una sola volta** a velocità maggiore, fino a
   circa 200 km/h se il tratto lo consente. Questi valori definiscono la prova,
   non sono limiti introdotti nel software.
3. Osserva per cinque secondi: deve tornare alla posizione, orientamento e
   velocità salvati. La sola scritta «stato applicato» non dimostra la stabilità.
4. Se si verifica lo sbalzo o un crash, non ripetere F7. Se LMU resta aperto,
   chiudi la UI per arrestare il listener. Se LMU si chiude, attendi che la UI
   rilevi la disconnessione e che il riepilogo venga salvato in `logs/`.

Comunica soltanto: esito, velocità approssimative di F6/F7, eventuale urto e se
si è chiuso LMU oppure soltanto il programma delle hotkey. Non serve filmare.

## Dati da conservare

I file vengono salvati automaticamente in `logs/`, con lo stesso prefisso
`<data>_physical_reset_host_v14`:

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

Eseguire `python src/analyze_physical_reset_trace.py <file.physical.jsonl>
--output <file.analysis.json>` usando il Python locale del progetto.

Verificare nell'ordine:

1. `saved_vs_applied`: velocità, posa e momento dei cinque corpi coincidono con
   F6 subito dopo F7; il rapporto deve riportare tutti i 10.077 campi verificati.
2. `collision_corrections` (fasi 11/12): cercare il primo cambiamento dei corpi
   dentro `F4EAC0`, confrontandolo con `proxy_vs_exported`, flag e feedback.
3. `collision_preparations` (13/14): verificare quando `F4E7B0` riallinea il corpo
   collisioni alla posa esportata. La differenza prima della preparazione può
   includere il normale avanzamento: non è da sola prova di un difetto.
4. `between_dynamics_changes` e `dynamics_changes`: distinguere una trasformazione
   comune dei cinque corpi dalla successiva divergenza delle forze.
5. Le chiamate senza uscita, i gruppi non leggibili e i valori non finiti devono
   restare indicati come evidenza incompleta, mai essere interpretati come zeri.

Le fasi 7/8 e 9/10 restano disponibili nel file grezzo per outer update e scoring.
Se il salto non compare nelle fasi 11/12, l'ipotesi della correzione collisioni
non è confermata e va seguita la prima frontiera divergente della traccia.
