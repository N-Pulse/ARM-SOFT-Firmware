> **Évolution lot 6 :** les descriptions SIM v1/CRC8, RX SIM déconnectée et
> interdiction d'activation virtuelle ci-dessous sont historiques. Le contrat
> actuel et la boucle fermée PC sont dans [SIMULATION_LOT6](SIMULATION_LOT6.md)
> et le [guide court](SIMULATION_QUICKSTART.md). Les invariants HW restent
> désarmés ; UART réel/Gazebo/moteurs réels ne sont pas testés dans le lot 6.

> **Addendum du 9 octobre 2026 — lot 5 :** ce rapport conserve les résultats
> historiques. [ROBUSTNESS_LOT5](ROBUSTNESS_LOT5.md) précise désormais le contrat
> de désarmement public, la sérialisation HAL, l'engagement UART4 IT et le reset
> encodeur. « Aucun UART sous Motor_Lock » est remplacé par une exception bornée
> pour le démarrage IT effectif, sans attente ; l'ancienne vérification d'epoch
> avant ce démarrage ne suffisait pas. Le flash maintenu exige un SN explicite ;
> RX SIM reste déconnectée et aucun premier mouvement n'est activé.

# Lot 3 : UART4 HW et perte de liaison

Implémentation et validation sur PC le 8 octobre 2026. Aucun flash, port série
ouvert, commit, push, reset Git ni nettoyage destructif. Références conservées :
[STARTUP_LOT1.md](STARTUP_LOT1.md) et [SAFETY_LOT2.md](SAFETY_LOT2.md).
Une compilation réussie ne prouve ni le démarrage ni la latence sur carte.

## Constats vérifiés

Les problèmes annoncés étaient présents : ring alimenté par LPUART1 malgré
UART4 PC10/PC11 configuré à 115200 ; copie du handle HAL ; attente active de
100 ms dans comms à priorité 4, au-dessus de control ; watchdog jamais démarré
ou réarmé, callback de timer utilisant FromISR et arrêt transmis par une file
susceptible d'être pleine ; émissions directes concurrentes et beacons bruts.
Les calculs SIM `1 + joint_count*5` et `1 + count*3` utilisaient bien uint8_t,
sans bornes suffisantes avant les lectures.

## Propriété UART4 et ordonnancement

`huart4`, défini dans main.c, reste la **seule instance HAL**. Le getter
`BoardUART_GetHandle()` retourne son adresse ; la compatibilité COM1 ne crée
plus de copie. Le fichier `uart4_transport.c` possède les transferts et les
callbacks HAL RX/TX/error. `UART4_IRQHandler` appelle HAL avec ce même handle.
Le handler utilisateur LPUART1, son ring et les callbacks concurrents du
backend SIM sont retirés. Les autres UART ne deviennent pas des propriétaires
de cette liaison.

RX HW utilise **HAL_UART_Receive_IT, un octet**. Ce choix borne chaque réception
et évite d'introduire les états DMA circulaire/IDLE dans ce lot. À 115200 8N1,
le débit maximal théorique est 11 520 octets/s : le coût effectif de ces IRQ
et les marges du ring restent à mesurer. DMA1 channel6/UART4_RX est retiré
du MSP, des IRQ, de main et du .ioc. PC10/PC11, AF5 et leurs réglages GPIO
existants sont conservés. Le connecteur externe, ses niveaux électriques et
son éventuel rôle EMG **ne sont pas confirmés par ces corrections**.

UART4 IRQ a priorité **6**, compatible avec le plafond FreeRTOS **5** ; une
vérification de compilation refuse un plafond incompatible. TIM6/HAL reste
à 4, sans API RTOS, et SysTick appartient toujours au port FreeRTOS du lot 1.
Le MSP fixe la priorité ; le transport active l'IRQ en contexte tâche.

App_Init initialise les moteurs désarmés, puis les buffers/parser/session
et la file TX. Les tâches sont créées avant le scheduler. La tâche comms
démarre RX avec son propre handle de tâche : aucun RX n'est lancé avant les
objets nécessaires. L'ISR copie un octet, son temps HAL, sa génération
d'arrêt et sa génération RX dans un ring de 256 cases (**255 utilisables**),
notifie la tâche avec FromISR et réarme une réception d'un octet.
Aucun protobuf n'est décodé en ISR.

Comms est à priorité **2**, control/guard à 3, safety à 5. Son attente
`ulTaskNotifyTake` a une borne de 10 ms, y compris pour une trame incomplète ;
elle libère le CPU. Le drain est borné à 256 événements par passage. La tâche
TX est à priorité 1. Aucun délai de communication ne relève
`MOTOR_CONTROL_MAX_ELAPSED_MS`, qui reste **50 ms**.

## Protocole HW et parseur

Le format reste `[0xAA][longueur uint8][DeviceMessage protobuf]`, sans CRC
ajouté. La longueur admissible est **1 à 128 octets**, vérifiée avant copie
et désérialisation. Le parseur incrémental de production conserve un candidat
borné ; après longueur invalide ou protobuf/action invalide, il cherche un
autre AA dans le suffixe déjà reçu. Les temps des octets conservés permettent
de conserver la vraie échéance du candidat suivant. AA dans un payload
valide n'est pas traité comme un nouveau début de trame.

Les actions existantes sont conservées : 0 OPEN_HAND, 1 CLOSE_HAND, 2 PINCH,
3 ROTATE_WRIST_R, 4 ROTATE_WRIST_L. L'action 0 omise selon proto3 est valide
si les enveloppes Data/instruction sont présentes. Toute autre action,
message vide, protobuf tronqué/invalide ou Hello de version incompatible
est rejeté. Le parseur de ProsthesisMessage, non utilisé pour RX HW, vérifie
également ses bornes avant copie.

Les temps utilisent une soustraction unsigned, compatible avec le wrap HAL.
Les bornes suivantes sont logicielles, **non validées sur carte** :

| Paramètre | Défaut | Comportement |
| --- | ---: | --- |
| `COMMS_LINK_TIMEOUT_MS` | 2000 ms | Perte de session lorsque l'âge dépasse cette borne. |
| `COMMS_FRAME_TIMEOUT_MS` | 100 ms | Abandon d'un candidat dépassant ce temps depuis AA. |
| `COMMS_INTERBYTE_TIMEOUT_MS` | 50 ms | Abandon après un intervalle supérieur à cette borne. |
| `COMMS_RX_WAIT_MS` | 10 ms | Attente RTOS RX, sans polling UART actif. |
| `COMMS_TX_TIMEOUT_MS` | 25 ms | Attente RTOS de fin d'émission IT. |
| Intent / TX | 8 / 8 trames | Offre sans attente, saturation diagnostiquée. |

Les trois premiers paramètres ont des valeurs par défaut `#ifndef` dans
`comms_config.h`, remplaçables par des définitions de compilation. Les autres
bornes se trouvent dans ce même fichier. Les flags de compilation restent
suivis par GNU Make ; aucune calibration n'est introduite.

Un timeout de trame remet le parseur en recherche de AA sans renouveler la
liaison. Une erreur UART ou un overflow invalide toute la réception en attente
et le candidat. S'il existe une session, l'ISR demande immédiatement LINK à
l'interlock, sans queue d'intents. Le réarmement après erreur se fait en tâche,
avec purge des événements/flags et abort RX. Sans DMA UART4, l'abort HAL local
ne suit pas son chemin d'attente DMA. Une erreur de démarrage/réarmement IT
n'est pas ignorée : diagnostic, invalidation et nouvelle tentative en tâche.

Ce protocole n'a ni CRC ni authentification : des octets corrompus qui forment
accidentellement un message protobuf admissible restent indiscernables d'un
message valide. La resynchronisation est bornée, sans garantie d'intégrité
électrique ou d'authenticité supplémentaire.

## Session, fraîcheur et récupération

1. Au boot, aucune session HW n'existe. Les actions avant Hello sont rejetées ;
   le silence seul ne crée pas de défaut LINK au démarrage désarmé.
2. Un **DeviceMessage.Hello avec proto_ver=1** commence la session. Son
   device_id devient l'identité du pair pour cette exécution ; un autre ID
   est rejeté, y compris après perte de liaison. Aucun nouvel identifiant
   wire n'est créé. La réponse de début/reconnexion est le Hello existant
   du firmware, device_id=1/proto_ver=1/supported_modes=1, mis en file TX.
3. Les Hello admissibles du même pair renouvellent la fraîcheur. Une action
   connue la renouvelle **seulement** si la session est saine, l'interlock
   accepte les commandes et la mise en file réussit. Une action rejetée,
   un overflow de file, bruit ou trame invalide ne la renouvelle pas.
4. Le temps retenu est celui de réception du dernier octet, pas celui du
   décodage tardif. Les événements et intents gardent leur génération
   d'arrêt ; un backlog appartenant à une génération précédente est rejeté.
5. Control et SafetyTask vérifient la session via MotorSafety_Poll ; la prise
   d'intent et la réception vérifient aussi l'échéance. Le timeout verrouille
   **MOTOR_STOP_LINK**, source globale, désactive les huit contrôleurs et
   désarme les drivers, y compris les deux poignets. Il ne dépend d'aucun
   timer RTOS ni d'une place libre dans la file de commandes.
6. Après timeout/erreur/overflow, les actions seules ne restaurent pas la
   liaison. Un nouveau Hello valide du même pair est requis. Ce trafic
   rétablit la condition de liaison, **sans effacer le défaut, armer, activer
   ou reprendre une consigne**.
7. La récupération reste une API explicitement demandée en contexte tâche,
   sans commande UART ajoutée. Dès qu'une session HW a commencé, toute
   récupération exige une liaison fraîche, ainsi que toutes les conditions
   du lot 2 : drivers, guard, feedback, force et timing. Une récupération
   acceptée reste désarmée et change la génération.

La surveillance périodique est soumise à la période safety de 20 ms et à
l'ordonnancement. Elle ne garantit pas un arrêt si tout le scheduler ou le
CPU est bloqué. L'arrêt en exception fatale reste hors lot.

## Files, concurrence et sorties

La file d'intents est désormais un ring de huit commandes protégé par le
**même Motor_Lock que l'interlock**. Validation de session, test de génération,
offre dans le ring et renouvellement de fraîcheur constituent une opération
bornée. Aucun appel RTOS/UART, mutex ou attente n'y intervient.

Tout arrêt et toute récupération purgent les intents encore en attente. Un
intent déjà retiré du ring garde sa génération : IntentRouter utilise
`Motor_SetTargetAtEpoch`, qui revérifie la génération sous le verrou au
moment de modifier chaque consigne. Si l'application précède l'arrêt,
l'inhibition annule la consigne ; si l'arrêt précède l'application, celle-ci
est rejetée. Une récupération puis un armement/une activation explicitement
réalisés par le banc ne rendent pas valide l'ancien intent. Les portes
d'armement/activation et les gains du lot 2 ne sont pas changés.

TX possède une seule tâche et une file de huit **trames complètes copiées**.
Les offres sont sans attente ; aucune émission directe de morceau AA,
longueur ou payload n'est conservée. La tâche utilise HAL_UART_Transmit_IT
avec stockage persistant jusqu'à la notification de fin ou l'abort. Un
échec/timeout est compté ; une attente RTOS silencieuse de 101 ms après une
trame potentiellement partielle laisse le pair se resynchroniser. Les autres
tâches continuent à s'exécuter. Le transport refuse une offre depuis une ISR
ou avec PRIMASK déjà positionné. Aucun UART ne se trouve sous Motor_Lock.

Les trames TX gardent également une génération ; une ancienne trame en file
est rejetée. Les lots SIM préparés conservent leur génération jusqu'à l'offre,
au lieu d'être réhorodatés après un arrêt. **Une trame déjà en vol ne peut
pas être rappelée** ; l'arrêt local HW ne dépend pas d'elle. La livraison et
la coupure distantes SIM, notamment avec saturation, ne sont pas validées
par ce lot. Aucun ACK d'action n'est émis pour prétendre qu'une commande
refusée par la file a été mise en queue.

Diagnostics consultables en RAM : `CommsSession_GetDiagnostics` et
`UART4Transport_GetDiagnostics` (symboles susceptibles d'être éliminés de
l'ELF si inutilisés ; états statiques visibles au debugger avec -g3).
`rx_bytes` compte les octets stockés ; `received` les candidats complets
présentés au traitement ; `valid` les messages/action/version admissibles ;
`accepted` les validations de session/safety réussies ; **`queued` seul
compte les mises en file réussies**. `queue_full`, `rejected`, `stale`,
`discarded`, erreurs RX, overflow, timeout/erreur/fin TX sont distincts.
Une action admissible mais sans place peut augmenter accepted et queue_full,
jamais queued ni la fraîcheur. Les compteurs du parseur distinguent longueurs
invalides, candidats invalides et timeouts.

Les beacons bruts app/comm/guard/initialisation SIM/task-start sont supprimés.
Ils mélangeaient le framing SIM et HW et effectuaient des attentes directes.
Le guard conserve ses causes/masques en RAM ; la LED et la télémétrie sans
émission restent inchangées. Aucun printf n'est raccordé à UART4. Les anciens
fichiers non compilés peuvent contenir des chemins historiques : ils sont
conservés, sans devenir des consommateurs actifs de la liaison.

## Scripts hôte et SIM

`fw/comm-stack/PyUART/uart_emg.py` et le bridge bmi_serial_bridge utilisent
le framing protobuf mais répondent au Hello par **ProsthesisMessage.Config** ;
ils ne transmettent pas le Hello périodique exigé ici. Cette Config n'ouvre
ni ne renouvelle la session RX DeviceMessage HW. Il faut faire émettre un
DeviceMessage.Hello proto_ver=1 à l'ouverture puis périodiquement (par exemple
toutes les **500 ms**, inférieures au timeout 2000 ms), et les actions existantes
dans Data/instruction. Les écritures doivent porter sur la trame complète,
avec un seul propriétaire côté hôte.

`tools/serial_emg_client.py` et `tools/ros2_bridge/serial_bridge.py` utilisent
`AA/version/type/longueur/payload/CRC8`, avec heartbeat ACK 0x12 : ce sont des
clients **SIM**, incompatibles avec le RX protobuf HW. Leur heartbeat existant
ne peut pas être supposé compatible. Ces scripts historiques ne sont pas
modifiés dans ce lot.

`tools/uart4_hw_client.py` fournit un client manuel Hello HW périodique,
sans armement ni récupération et sans action par défaut. Il nécessite
pyserial et le runtime Python protobuf, absents de l'environnement Python
utilisé ici ; aucun port n'a été ouvert. Son test Python de codec est donc
explicitement **SKIPPED**, sans être présenté comme une validation matérielle.
Le décodeur protobuf firmware de production, lui, est compilé et exécuté dans
les tests C/C++ pour les identifiants 0..4.

En build SIM, RX n'est jamais démarré et aucun parseur HW/SIM n'est actif.
Les émissions SIM existantes passent par le propriétaire TX UART4, avec leurs
identifiants et CRC8 conservés, sans les beacons ni l'ancien watchdog inactif.
Les fonctions `process_joint_feedback` et `process_fsr_feedback`, extraites
dans motor_sim_feedback.c, sont testées mais **ne sont raccordées à aucun RX**.
Elles utilisent size_t, bornent count par le payload et MOTOR_COUNT, vérifient
tous les IDs avant le premier feedback et rejettent les tailles incohérentes.
Les conversions Q15 existantes sont conservées ; elles ne constituent pas
une nouvelle calibration. Le transport SIM complet reste à terminer.

## Commandes et résultats réels

Outils locaux : ARM GCC **14.3.1** du bundle STM32Cube,
GNU Make **3.82.90**, GCC/G++ hôte MinGW **6.3.0**, Python **3.12.8**.
GNU Make et les scripts de compilation des lots précédents sont conservés.

Depuis la racine PowerShell, commandes finales :

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-sim.ps1 -Rebuild
python tests/comms/run_tests.py
python tests/safety/run_tests.py
python tests/startup/run_tests.py
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/startup/test_build_modes.py
```

Toutes terminent avec **code 0**. HW et SIM ont chacun **66 objets/unités
recompilés** ; 132 objets isolés sont ensuite inchangés lors de l'alternance
HW → SIM → HW/Debug. L'alias HW est préservé ; SIM_MODE=2 est rejeté.

| Configuration | ELF | text | data | bss |
| --- | --- | ---: | ---: | ---: |
| HW | build/HW/n-pulse.elf | 86 556 | 384 | 31 344 |
| SIM | build/SIM/n-pulse.elf | 72 436 | 384 | 27 448 |

Tailles en octets obtenues par arm-none-eabi-size. Un warning préexistant
`sDefaultTaskHandle` inutilisé subsiste dans les deux configurations. Les
piles comms/TX valent 768/384 mots ; le TCB ARM mesuré dans main.o vaut
168 octets. Le heap RTOS de 16 KiB n'est pas augmenté. Ces tailles statiques
ne prouvent pas les marges de pile/heap en fonctionnement.

Résultats des validations :

- **Lot 3 hôte : 17 scénarios de production** parser/protobuf/comm/session/
  UART/interlock/router, avec HAL/BSP/ordonnancement RTOS simulés. Trames
  fragmentées/consécutives, IDs 0..4, longueurs nulles/excessives, protobuf
  invalide, action inconnue, octet perdu, trame interrompue, overflow, erreur
  UART et échec HAL de réarmement suivis de reprise ; expiration session et
  parser au wrap ; file pleine/absence de faux enqueue ; timeout malgré
  saturation ; bruit/rejets ne renouvelant pas la liaison ; backlog et intent
  déjà retiré rejetés après récupération/activation explicites du banc ;
  récupération toujours désarmée et refusée si la force persiste ; les deux
  ordres application/arrêt ; saturation, erreur et timeout de TX complet.
- **SIM bounds** : cas joint_count=52/count=86, IDs invalides, tailles/capacités,
  plus **24 576 couples count/longueur**, pour chacun des deux décodeurs.
  GCC local ne trouve ni libasan ni libubsan ; clang n'est pas disponible
  et WSL n'est pas installé. Le probe est conservé : **sanitizers indisponibles**,
  bornes testées sans ASan/UBSan, sans prétendre à leur couverture mémoire.
- **SIM TX ciblé** : staging/encoding de production, annulation des sorties
  poignet/previews en attente, saturation sans attente, comparaison de trois
  trames complètes TARGET/PREVIEW/ALL_STOP avec les fonctions CRC/framing du
  client SIM existant. Aucun RX SIM ni simulateur distant n'est exécuté.
- **Lot 2** : 24 scénarios dans chacun des deux bancs, soit **48 exécutions**,
  repassent avec comms_session de production lié, supervision inactive avant
  le démarrage d'une session HW.
- **Lot 1** : les **7 scénarios** timebase/encodeurs/ADC repassent.
- **ELF/objets/disassembly** : bons vecteurs/handlers forts du port et HAL,
  SysTick RTOS sans double tick HAL ; UART4 fort à la bonne adresse vectorielle,
  getter vers huart4, callbacks HW et unique rearm IT, appels de notification
  RTOS retenus. LPUART1 reste le handler faible Default_Handler, aucun ring
  historique actif, aucun handle UART4 DMA/copie hcom. Pas de HAL_UART_Transmit
  ou HAL_UART_Receive polling retenu. En SIM, HAL_UART_Receive_IT et les deux
  parseurs de réception sont absents de l'ELF. L'interlock LINK et les contrôles
  d'époque sont vérifiés, sans UART dans la voie d'arrêt verrouillée.
- Le vérificateur startup du lot 1 est **inchangé** : aucun symbole d'armement,
  démarrage PWM/HRTIM, HAL_ADC_Start_DMA ou HAL_OPAMP_Start retenu par défaut.
  Le vérificateur lot 2 remplace seulement son attente de l'ancien helper
  enqueue_cmd SIM supprimé ; la porte Motor_OutputAllowedLocked reste exigée,
  et le vérificateur lot 3 vérifie les nouvelles voies SIM vers TX central.

La preuve d'attente RX est **logicielle** : le test appelle l'itération de
production avec un candidat incomplet/aucun octet et observe une notification
RTOS avec délai non nul ; l'ELF appelle ulTaskGenericNotifyTake. Le banc simule
FreeRTOS, il ne mesure pas un réveil ni une latence réelle sur STM32.

Logs : `build/validation/lot3/` (hw/sim-build, comms/safety/startup-tests,
comms/safety/startup-elf, build-modes). Probe sanitizer dans build/comms-tests.
Une sauvegarde initiale et le diff limité au lot sont conservés localement.
Comparaison : **1117 fichiers initiaux, 24 adaptés dans ce lot, 1093 identiques,
aucun disparu**, index Git inchangé. Le linker modifié avant ce lot, les autres
modifications préexistantes et les fichiers non suivis sont conservés.

## Fichiers du lot

Modifiés (24) :

- Core : Inc/stm32g4xx_it.h ; Src/main.c, stm32g4xx_it.c,
  stm32g4xx_hal_msp.c ; N-PULSE FIRMWARE.ioc.
- fw/app : app.c, intent_router.c/.h.
- fw/comm : comm.c/.h, board_uart_compat.c/.h.
- fw/comm-stack : rx.cpp/.h, tx.cpp/.h.
- fw/motors/motor_control : motor_control.c/.h, motor_safety.c/.h,
  motor_backend_sim.c.
- tools/firmware.mk ; tests/safety/run_tests.py et verify_build.py.

Ajoutés (20) :

- fw/comm : comms_config.h, comms_parser.c/.h, comms_session.c/.h,
  uart4_transport.c/.h.
- fw/motors/motor_control : motor_sim_feedback.c/.h.
- tests/comms : run_tests.py, test_comms.c, test_sim_bounds.c, test_sim_tx.c,
  verify_build.py, quatre en-têtes mocks.
- tools/uart4_hw_client.py ; ce rapport.

## Limites et vérification sur carte, puissance moteurs coupée

Restent hors lot : arrêt en exception fatale, script flash, réception/transport
SIM complet, acquittement d'arrêt distant, calibrations, réglage PID, seuils
mécaniques, validation des formes d'onde HRTIM et protection de courant.
ADC reste désactivé par défaut ; aucune mesure de courant/force n'est inventée.
La configuration linker préexistante n'est pas corrigée par ce lot.

Après une régénération CubeMX, vérifier/réappliquer les zones manuelles :
unique huart4, aucune copie hcom, aucun DMA UART4_RX, IRQ UART4 priorité 6,
handler vers huart4, callbacks transport uniques, RX démarré uniquement après
initialisation des objets/tâches. Le .ioc conserve PC10/PC11 et 115200.
CubeMX peut réactiver l'IRQ dans le MSP ; il ne doit pas lancer RX précocement.
Vérifier également toutes les adaptations TIM6/FreeRTOS et Make du lot 1,
puis relancer les trois suites et vérifications ELF. Aucune régénération
CubeMX n'a été exécutée ici.

Procédure réservée à l'utilisateur, non exécutée par l'agent :

1. Déconnecter la puissance moteur. Confirmer électriquement les signaux
   PC10 TX/PC11 RX, la masse et les niveaux compatibles avant de connecter
   un adaptateur ; ne pas déduire le connecteur ou le rôle EMG du seul code.
   Charger manuellement le build HW par la méthode validée pour la carte,
   sans utiliser le script flash historique.
2. Répéter les contrôles de boot/ticks du lot 1 : UART4 115200 8N1, IRQ 6,
   SysTick RTOS et TIM6 HAL cohérents ; aucun assert/error. Contrôler SLEEP
   bas, huit moteurs disabled, compares zéro, drivers désarmés et ADC inactif.
   Ne pas appeler Arm/Enable.
3. Sur un environnement hôte doté de pyserial/protobuf, lancer manuellement
   `python tools/uart4_hw_client.py --port COMx --duration 10`, en remplaçant
   COMx par le port confirmé. Observer Hello, les compteurs RX/valid/hello
   et un Hello firmware correctement framé. Aucun ancien beacon brut ou
   Config/heartbeat SIM ne doit apparaître. Une réponse absente avec TX pleine
   doit être distinguée d'une absence d'acceptation de session.
4. Avec un outil maîtrisé, fragmenter et concaténer les exemples HW ; essayer
   `AA 04 12 02 0A 00` (OPEN) et `AA 06 12 04 0A 02 08 01` (CLOSE), moteurs
   toujours désarmés. Observer queued après succès, queue_full sans faux
   acquittement si le consommateur est suspendu. Mesurer périodes control,
   latence RX/TX, charge IRQ et marges de pile/heap avec du trafic soutenu.
5. Cesser le trafic après une session : après timeout et surveillance périodique,
   LINK doit rester mémorisé, huit contrôleurs disabled et SLEEP bas, même si
   la file d'intents était pleine. Reprendre Hello : liaison fraîche mais
   défaut toujours verrouillé et aucune reprise de cible.
6. Via un banc de debug dédié et toujours puissance coupée, provoquer erreur
   UART/overflow et vérifier purge, cause LINK, réarmement en tâche puis reprise
   après un nouveau Hello. Ne pas suspendre tout le scheduler pour conclure
   à l'absence de watchdog logiciel.
7. Tester la récupération explicite sur un banc instrumenté où l'API est
   retenue (elle peut être éliminée de l'ELF normal faute de caller). Refus
   tant que liaison ou autre cause persiste ; succès restant désarmé, file
   vide et anciennes commandes inapplicables. Aucun test de moteur alimenté
   ni armement n'est autorisé par cette procédure.
