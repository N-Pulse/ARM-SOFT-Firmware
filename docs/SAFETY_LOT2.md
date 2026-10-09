> **Addendum du 9 octobre 2026 — lot 5 :** ce rapport conserve les résultats
> historiques. [ROBUSTNESS_LOT5](ROBUSTNESS_LOT5.md) précise désormais le contrat
> de désarmement public, la sérialisation HAL, l'engagement UART4 IT et le reset
> encodeur. « Aucun UART sous Motor_Lock » est remplacé par une exception bornée
> pour le démarrage IT effectif, sans attente ; l'ancienne vérification d'epoch
> avant ce démarrage ne suffisait pas. Le flash maintenu exige un SN explicite ;
> RX SIM reste déconnectée et aucun premier mouvement n'est activé.

# Lot 2 : arrêts persistants et validité du contrôle moteur

Travail et validation PC le 8 octobre 2026. Aucun flash, commit, push, reset
Git ni nettoyage destructif. Les modifications et fichiers non suivis présents
au début du lot sont conservés. Le lot 1 reste la référence pour le démarrage :
[STARTUP_LOT1.md](STARTUP_LOT1.md).

## Défauts vérifiés et corrigés

Les cinq défauts annoncés étaient présents dans les sources locales :

| Défaut | Comportement corrigé |
| --- | --- |
| Le guard appelait seulement `Motor_DRV8848_Stop`. | Chaque déclenchement du guard demande un arrêt global verrouillé, désactive les huit contrôleurs, annule leurs consignes et désarme les drivers, y compris WRIST_X et WRIST_Y. |
| `feedback_valid` ne périmait pas ; NaN conservait l'ancienne position valide. | La position reçue est horodatée. Absence initiale, expiration, perte de calibration et donnée non finie empêchent l'activation ; un contrôleur actif dans cet état déclenche l'arrêt global. |
| ControlTask passait toujours 10 ms au PID. | `Motor_Update()` mesure HAL_GetTick. Chaque PID utilise aussi le temps depuis son dernier calcul accepté. Un délai excessif verrouille un arrêt. SafetyTask surveille l'absence de ControlTask. |
| La force n'était contrôlée qu'à une commande suivante. | Un retour marqué FORCE déclenche immédiatement l'arrêt au seuil dur ; le contrôle périodique vérifie aussi la force et la péremption d'un flux précédemment reçu. Le seuil doux limite également la sortie périodique. |
| `MotorSafety_Recover` effaçait aveuglément le défaut. | Récupération explicite, conditionnelle et commune à `Motor_RecoverFromFault`. Elle laisse tous les contrôleurs désactivés, les drivers désarmés et les anciennes consignes annulées. |

## Propriété des arrêts et concurrence

`MotorSafety_RequestStopCause(cause, source)` mémorise les causes sous forme
de bits et les moteurs sources dans des masques. Les causes sont MANUAL,
GUARD, FEEDBACK, CONTROL_DELAY, FORCE, DRIVER et PID. Une source `MOTOR_COUNT`
signifie une cause globale. Toutes les causes entraînent un arrêt global.
`Motor_StopAll()` utilise cette même voie avec MANUAL. Les nouvelles commandes
ne sont pas acceptées tant que le défaut est verrouillé.

Le verrou `Motor_Lock` sauvegarde/restaure PRIMASK, y compris lorsqu'il est
imbriqué, et fonctionne avant le scheduler ainsi que pour les demandes d'arrêt
en ISR. Les données partagées control/guard/safety/feedback et les écritures
locales de sortie utilisent ce même domaine de sérialisation. Les sections
critiques sont bornées à des copies d'état, contrôles, remises à zéro et
écritures GPIO/registres. Aucun UART, attente, mutex ou démarrage/arrêt HAL
bloquant ne s'y trouve. Leur durée effective reste à mesurer sur carte.

À la demande d'arrêt, dans cette même section critique :

1. Une génération d'arrêt est incrémentée ; les causes sont verrouillées.
2. Les huit contrôleurs sont désactivés, PID réinitialisés, limites PWM mises
   à zéro, anciennes consignes annulées. Les axes précédemment activés sont
   mémorisés pour les conditions de récupération.
3. Les drivers sont marqués désarmés, SLEEP est mis à zéro puis les seize
   comparaisons sont mises à zéro avec le mapping existant.
4. Les sorties SIM encore en attente locale sont annulées.

Les calculs PID se font hors section critique sur une copie par moteur. Au
commit, la génération d'arrêt ET la révision de consigne/activation doivent
encore correspondre. Le contrôleur doit rester autorisé ; fraîcheur,
calibration, délai et limite de force sont revérifiés. Le commit HW et la
demande d'arrêt sont donc ordonnés : si le commit précède l'arrêt, l'arrêt
l'efface ; si l'arrêt précède le commit, le résultat préparé est rejeté.
Même un arrêt suivi d'une récupération et d'un réarmement explicite ne rend
pas valide un résultat appartenant à l'ancienne génération.

`Motor_ServiceStop()` termine en contexte tâche les arrêts HAL des timers
et l'éventuel ALL_STOP SIM, hors verrou. La récupération est interdite pendant
ce travail. La coupure GPIO/comparaisons n'attend pas cette étape. Le beacon
UART du guard est également envoyé après la coupure, hors verrou.

Le backend SIM prépare désormais ses commandes sans attente sous le verrou
et vérifie la génération de ses lots avant transmission. Le format du
protocole n'est pas changé. **Une trame UART déjà en vol ne peut pas être
rappelée par cet interlock local** ; l'arrêt du simulateur distant et la
sérialisation complète du transport restent à valider dans le lot transport.
Les preuves de concurrence de ce lot concernent les sorties physiques
locales et la préparation locale, pas un acquittement distant.

## Validité et seuils logiciels

`motor_feedback.c` partage le traitement `valid_fields` entre HW et SIM.
Un paquet FORCE seul ne rafraîchit ni ne remplace la position. Un champ non
annoncé est ignoré, même si sa valeur est NaN ou dépasse un seuil. Une position
annoncée non finie, ou une vitesse annoncée non finie, invalide l'ancienne
position. La vitesse transmise n'est pas utilisée pour le PID ; D conserve
son calcul existant à partir de la position.

Une position identique reçue de nouveau rafraîchit son horodatage. Le backend
HW considère la lecture périodique d'un encodeur calibré comme un nouvel
échantillon, même si le compteur n'a pas bougé. La fraîcheur ne prouve donc
pas que le signal électrique de cet encodeur fonctionne. L'immobilité sous
sortie non nulle est surveillée séparément par le guard à partir des comptes
bruts du BSP ; aucun timer encodeur n'est redémarré par le guard.

Valeurs dans `motor_safety_config.h`, remplaçables par des définitions de
compilation. Ce sont des paramètres **logiciels non validés sur carte** :

| Paramètre | Valeur par défaut | Règle |
| --- | ---: | --- |
| `MOTOR_FEEDBACK_TIMEOUT_MS` | 100 ms | Position et force reçue périment au-delà de 100 ms, avec arithmétique unsigned compatible avec le wrap HAL. |
| `MOTOR_CONTROL_MAX_ELAPSED_MS` | 50 ms | Intervalle de control, âge de son dernier passage, temps depuis le dernier PID accepté et retard au commit : arrêt au-delà de 50 ms. |
| `MOTOR_GUARD_PERIOD_MS` | 20 ms | Période de la tâche guard. |
| `MOTOR_GUARD_STALL_MS` | 180 ms | Absence de progrès pendant au moins 180 ms sous sortie non nulle. |
| `MOTOR_GUARD_EPS_COUNTS` | 6 comptes | Progrès brut minimal ; aucune conversion mécanique ajoutée. |
| `MOTOR_FORCE_SOFT_LIMIT_MN` | 1200 | Seuil existant de réduction de PWM. |
| `MOTOR_FORCE_HARD_LIMIT_MN` | 1600 | Seuil dur existant pour doigts et paume. |
| `MOTOR_FORCE_WRIST_HARD_LIMIT_MN` | 2000 | Seuil dur existant des deux axes de poignet. |
| `MOTOR_FORCE_REDUCED_SPEED` | 25 % | Limite existante après dépassement du seuil doux. |

SafetyTask conserve sa période de 20 ms et passe à la priorité tâche 5,
au-dessus de comms (4), control/guard/SIM TX (3), pour pouvoir interrompre
une tâche applicative qui monopolise le CPU. Il s'agit de priorités FreeRTOS
de tâches, sans changement des priorités NVIC du lot 1 : la détection d'un control absent
est soumise à cette période et à l'ordonnancement, ce n'est pas un watchdog
matériel. Si le scheduler entier est bloqué, cette tâche ne peut pas garantir
la coupure. Une interruption fatale n'est pas traitée dans ce lot.

Les gains PID, sa limite `max_dt_s=0.05`, les limites angulaires et la rampe
existantes sont conservés. Le PID impose sa propre borne de dt même si la
borne globale est augmentée. Les overrides doivent être cohérents avec ces
limites ; GNU Make mémorise les flags et reconstruit lorsqu'ils changent.

Aucun flux de force n'est inventé en HW. Sans FORCE valide reçu, aucune
protection de force mesurée n'est disponible. Dès qu'un flux FORCE a été reçu,
un contrôleur actif exige qu'il reste frais ; sa disparition ne devient pas
une force nulle. Les seuils hérités ne constituent pas une calibration ni une
protection matérielle validée. `CURRENT_ACQUISITION=0` reste le défaut :
aucune acquisition ADC/DMA et aucune protection par courant fonctionnelle.

## Récupération explicite

`Motor_RecoverFromFault()` et `MotorSafety_Recover()` sont des API de contexte
tâche. Aucun nouveau protocole UART/SIM ni réarmement automatique n'est ajouté.
Les API de récupération/activation peuvent être éliminées de l'ELF normal par
`--gc-sections`, puisqu'aucun chemin applicatif normal ne les appelle.

La récupération échoue tant que :

- FAULT DRV est actif, un driver reste armé ou une transition/fin d'arrêt est
  en cours ;
- le guard déclenché n'a pas observé un déplacement d'au moins EPS comptes
  depuis le déclenchement, ou son entrée locale reste indisponible ;
- un retour requis est absent, périmé, non fini ou son encodeur non calibré.
  Cela concerne le moteur source GUARD/FEEDBACK/PID et **tous les axes qui
  étaient activés au moment de l'arrêt global** ;
- la force ayant causé un défaut n'est pas à nouveau fraîche et sous son
  seuil dur, ou un flux de force précédemment utilisé reste périmé ;
- la cause de retard/PID persiste : control doit avoir exécuté un intervalle
  positif acceptable et son dernier passage doit rester récent ; la
  configuration PID requise doit rester valide.

Le mouvement brut après un déclenchement du guard ne prouve pas que
l'obstacle mécanique est supprimé. Il fournit seulement une condition
logicielle conservative pour une récupération explicitement demandée.
Il n'efface jamais le verrou de lui-même. Les lectures BSP du guard portent
une révision : une lecture préparée avant un poll concurrent et son nouveau
déclenchement ne peut pas lever la condition plus récente.

Une récupération acceptée efface les causes et change la génération, sans
appeler Arm, Enable ou reprendre une consigne. Toute utilisation future
exige un armement explicitement autorisé puis une activation validée, puis
une **nouvelle consigne après l'activation**. Ces opérations ne sont ajoutées
à aucun chemin de démarrage de ce lot.

## Compilation et vérifications sur PC

Outils locaux : ARM GCC 14.3.1 (bundle STM32Cube), GNU Make 3.82.90,
GCC hôte MinGW 6.3.0 et Python 3.12.8. Les règles restent celles de
`Makefile` / `tools/firmware.mk`, avec les objets séparés `build/HW` et
`build/SIM`, et les alias HW `Debug/N-PULSE FIRMWARE.elf` / SIM
`Debug/SIM/N-PULSE FIRMWARE.elf`.

Commandes exactes depuis la racine PowerShell :

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-sim.ps1 -Rebuild
python tests/safety/run_tests.py
python tests/startup/run_tests.py
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/startup/test_build_modes.py
```

Les deux reconstructions complètes finales réussissent avec le code **0** :
**62 unités recompilées par configuration**. Tailles `arm-none-eabi-size`, en
octets, sans prétendre mesurer les marges de pile à l'exécution :

| Build | ELF | text | data | bss |
| --- | --- | ---: | ---: | ---: |
| HW | `build/HW/n-pulse.elf` | 74 032 | 384 | 26 864 |
| SIM | `build/SIM/n-pulse.elf` | 78 064 | 384 | 27 192 |

Un warning préexistant de variable inutilisée reste en HW
(`sDefaultTaskHandle`) ; SIM ajoute les deux warnings préexistants
`isr_send_intent_ack` et `s_max_velocity_rad_s`. Le code safety remplacé
ne conserve plus son ancien limiteur de rampe inutilisé.

Tests hôte : **24 scénarios dans chacun des deux bancs, soit 48 exécutions**.
Ils compilent les fonctions de production control/safety/guard/feedback/PID,
la configuration PID inchangée et le driver DRV8848. Le second banc compile
également le backend HW de production. HAL, clock et BSP encodeur sont
simulés ; le premier banc remplace seulement l'I/O backend pour représenter
une réception asynchrone absente. Aucun firmware n'est exécuté sur carte.

Les tests couvrent : état initial désarmé ; arrêt guard puis douze cycles PID
et nouvelles consignes ; arrêt global incluant les deux poignets ; guard du
poignet Y ; absent/périmé/NaN/Inf et `valid_fields` ; durée réelle de 40 ms ;
retard et absence de control ; force immédiatement sans commande, flux périmé
et seuil doux ; FAULT DRV persistant ; refus de récupération et récupération
restant désarmée ; absence de reprise d'ancienne consigne ; les deux ordres
arrêt/sortie ; arrêt entre calcul PID et commit, même après récupération et
réarmement explicite du banc ; force ou retard pendant le calcul ; changement
concurrent de consigne, wrap du compteur de millisecondes, sortie directe NaN
et annulation d'un armement explicite pendant une demande d'arrêt. Un poll
concurrent du guard ne peut pas autoriser la récupération à partir d'une
ancienne lecture BSP préparée avant le déclenchement.

Les injections utilisent l'instrumentation GCC hôte après le retour du PID
réel et après une lecture BSP simulée. Aucun hook de test n'est ajouté au firmware. Les écritures de compare
et les appels HAL des mocks vérifient aussi la discipline du verrou : les
écritures sont protégées ; les démarrages, arrêts HAL et délais sont hors
section critique. Ces interleavings déterministes ne sont pas une preuve
exhaustive de tous les ordonnancements possibles sur STM32.

Vérifications finales, toutes terminées avec le code **0** :

- Lot 1 hôte : les **sept scénarios** timebase/encodeurs/ADC repassent.
- Lot 1 ELF : SVC/PendSV/SysTick forts du port ARM_CM4F, bons vecteurs,
  tick SysTick RTOS sans HAL_IncTick, TIM6/callback HAL forts. Aucun symbole
  d'armement, démarrage PWM/HRTIM, ADC DMA ou OPAMP retenu par défaut.
  `tools/verify_startup_build.py` n'a pas été modifié.
- Séparation Make : HW → SIM → HW via Debug, **124 objets isolés** inchangés
  en empreinte et date ; alias HW conservé ; SIM_MODE=2 rejeté.
- Lot 2 ARM : voies d'inhibition globale, guard utilisant le BSP, HAL_GetTick,
  surveillance périodique et portes d'application de sortie retenues dans
  les deux ELF. Voies de feedback retenues dans l'ELF HW.
- En SIM, `MotorBackend_OnFeedback` et `MotorSafety_OnFeedback` sont compilées
  et vérifiées dans leurs objets, mais éliminées de l'ELF par gc-sections :
  aucun chemin de réception actif ne les retient actuellement. La correction
  de leur logique est testée sur hôte ; **la réception de feedback SIM en
  firmware n'est pas fonctionnellement validée ni terminée par ce lot**.
- Préservation : **1104 fichiers initiaux**, **11 fichiers modifiés dans le
  périmètre**, **1093 strictement identiques**, aucun fichier initial disparu.
  Le diff de l'index Git est identique à sa sauvegarde initiale. Le diff
  limité à ce lot est conservé dans `build/validation/lot2/lot2-only.patch`.

Le vérificateur binaire du démarrage est conservé sans affaiblir l'absence
d'armement par défaut.
La vérification binaire lot 2 contrôle les appels retenus dans les ELF vers
l'inhibition globale, le guard BSP, l'horloge réelle, la surveillance périodique
et l'arrêt sur feedback HW ; les entrées SIM non retenues sont contrôlées
dans leurs objets. Elle ne mesure pas les sorties électriques.

Logs locaux : `build/validation/lot2/hw-build.log`, `sim-build.log`,
`safety-tests.log` et rapports de validation dans le même répertoire.
Une compilation réussie ne démontre pas le bon démarrage ni la sûreté sur carte.

## Fichiers du lot

Modifiés : `Core/Src/main.c`, `tools/firmware.mk`, et dans
`fw/motors/motor_control/` : `motor_control.c/.h`, `motor_safety.c/.h`,
`drv8848.c/.h`, `motor_backend.h`, `motor_backend_hw.c`, `motor_backend_sim.c`.

Ajoutés : `motor_lock.h`, `motor_safety_config.h`, `motor_guard.c/.h`,
`motor_feedback.c` dans ce même répertoire ; `tests/safety/run_tests.py`,
`test_safety.c`, `verify_build.py`, quatre en-têtes mocks ; ce rapport.

Aucune association physique de broches, forme d'onde HRTIM, calibration,
rapport mécanique, paramètre électrique ou réglage PID n'est modifié.
Les corrections du lot 1, les scripts de build et leurs chemins sont conservés.

## Limites et vérification sur carte, moteurs désarmés

Restent hors lot : réception UART4, protocole et transport SIM complet,
validation des formes d'onde HRTIM, arrêt en exception fatale, calibrations,
réglage PID et mesure/protection par courant. Aucun test matériel n'est réalisé.

Procédure à réaliser par l'utilisateur uniquement :

1. Couper/déconnecter la puissance des moteurs ; identifier la cible unique
   STM32G474QET6. Charger manuellement l'ELF HW par la méthode validée pour
   cette carte. Ne pas utiliser le script historique multi-cartes.
2. Répéter les contrôles de ticks et clocks du lot 1. Observer sans appeler
   Arm/Enable : `s_armed=false`, huit `enabled=false`, PWM/compare à zéro,
   SLEEP bas selon le mapping actuel, aucune acquisition ADC/DMA.
3. Observer HAL_GetTick, l'horodatage de control et ses intervalles. Mesurer
   les durées des sections critiques et les marges de pile des tâches
   control/safety/guard. Vérifier leurs périodes et l'absence de fault/assert.
4. Sans activer de moteur, injecter un arrêt MANUAL ou GUARD par la fonction
   de production retenue via un banc de debug maîtrisé. Vérifier causes et
   masques mémorisés, sorties à zéro et désarmement après plusieurs cycles
   et commandes. Observer explicitement les deux axes de poignet.
5. Toujours désarmé, vérifier l'invalidation d'un retour marqué POSITION NaN,
   l'expiration sans nouvelle réception et un retour FORCE dépassant le seuil
   logiciel via le point d'entrée de feedback. Ce sont des données de test,
   pas des mesures ni une calibration. Contrôler les causes sans réarmer.
6. Pour tester la récupération/API non retenue ou le guard sous sortie active,
   utiliser ensuite un banc instrumenté distinct avec puissance coupée et
   procédures validées. Ne pas contourner la calibration/activation du build
   normal. Une récupération acceptée doit laisser huit moteurs désactivés et
   SLEEP bas. Les tests de mouvement alimenté sont reportés à un autre lot.
