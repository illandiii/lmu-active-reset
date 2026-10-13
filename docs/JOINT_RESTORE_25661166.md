# Transazione fisica/scoring e contatti — 12 ottobre 2026

Il reset completo non e' operativo. Questa fase chiude due passaggi separati:
ricostruzione contatti da fermo nel gioco e transazione congiunta su memoria
isolata. Non sono ancora una sequenza di reset nativa completa.

## Contatti nella build corrente

Prova `20261012T184234634757Z_contact_v2_25661166`: quattro ruote completate,
maschera 15, nessun fallimento, 211 profondita' di contatto ritirate attraverso
sentinella e refresh originale. Conteggi successivi 52/52/54/53, tutti positivi,
normali unitarie e nessuno spostamento di posa nel refresh. Gli indici sono
stati riutilizzati: `removed/added=0` non implica mancato rinnovo dei nodi.
Tutti i flag freddi risultano azzerati e il refresh ha ricostruito profondita'
positive. Osservazione successiva 5,03 s; arresto pulito e prologo ripristinato.

Il gate ha verificato Practice offline, vettura sola, ai box, folle, acceleratore
rilasciato e velocita' lineari/angolari native basse su tutti i corpi. Questa
prova **non** verifica contatti dopo teletrasporto o ripristino in movimento.
Audit: `logs/20261012_contact_v2_shutdown_audit.json`.

## Transazione congiunta

`restore_joint_25661166` aggiunge tre campi scoring al piano fisico e instrada
un'unica transazione sui due adattatori. Slot scoring 2700, offset esatti
160/27290/27360, quattro byte ciascuno, destinazione zero. Scoring incompleto,
vecchio offset, valori estranei e campi duplicati sono rifiutati. I flag del
giro successivo e i byte dei record restano fuori dal piano.

`restore_joint_native_memory_execute` congela i binding fisici e l'indirizzo
scoring, controlla il thread proprietario, tutte le destinazioni tipizzate e
gli alias fra intervalli fisici/scoring. Usa Read/WriteProcessMemory sul proprio
processo, con readback e rollback inverso sui riferimenti originali. Il backend
e' collegato soltanto alla fixture, non agli osservatori nel gioco.

Test su piano derivato dalla cattura dinamica v22: **10.063 campi**, di cui tre
scoring (12 byte). Verificati esecuzione Win32, contenuti desiderati di tutti i
campi, byte scoring esterni inalterati, rollback fisico quando fallisce ognuna
delle tre scritture scoring, rollback nativo quando il contesto scade dopo
l'inizio dello scoring, rifiuto di sovrapposizione fisica/scoring. I precedenti
test Win32 su thread errato, binding congelati e pagine non scrivibili passano.

Il test compatto `Build-JointRestore.ps1` verifica inoltre quattro errori di
scrittura parziale e quattro perdite di contesto. Il replay completo e' nel
rapporto v22 `.vehicle-partial-replay.json`, sotto `joint_physics_scoring`.

## Passaggi ancora necessari

Copertura e identita' del checkpoint restano parziali; cambio/frizione sono
oggetto di una nuova analisi REA. Occorre collegare questi componenti sul thread
fisico, gestire un eventuale fallimento del refresh contatti dopo le scritture,
verificare il giro interrotto e quello successivo nel gioco, e realizzare le
hotkey. Non esiste ancora una prova di conservazione reale dei record dopo reset.
