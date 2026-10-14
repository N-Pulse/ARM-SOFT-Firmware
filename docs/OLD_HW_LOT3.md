# OLD_HW — Lot 3 : lien MASTER/SLAVE et sécurité distribuée

Rapport du 14 octobre 2026, branche `oldHardNewSoft`, HEAD `4c69750`.
Les rapports des lots 1 et 2 ont été lus et conservés. Le point de départ non
commité est sauvegardé dans `build/lot3-baseline/manifest.json` et ses fichiers.
Le code prime sur les Markdown. Aucun checkout, merge, reset Git, rebase,
cherry-pick, commit, push, flash, reset matériel ou mouvement n'a été effectué.
Le `.ioc`, `.vscode/settings.json` et les deux rapports précédents sont inchangés.

## Résultat et portée

USART3 dispose désormais d'un propriétaire unique, d'un protocole bidirectionnel
avec CRC, sessions, numéros de séquence, supervision à deux sens, cibles,
acquittements, feedback et STOP. Chaque carte conserve son contrôleur, ses
encodeurs, sa sécurité et son epoch STOP. Une mesure distante ne nourrit jamais
le PID local. Le même ELF détecte le rôle par PC0.

**Les images ordinaires HW et SIM restent incapables de produire un PWM
physique non nul.** Le lien n'arme aucun moteur. Le verrou DRIVER du build HW
ordinaire et l'absence dans son ELF d'`Arm`/`program_locked` sont conservés.
Les commandes normales sont donc refusées tant que les restrictions actuelles
interdisent l'activation. Cela n'est pas annoncé comme un mouvement réussi.

Les images dédiées de commissioning restent limitées à un seul doigt choisi,
local, avec calibration, feedback, preflight, armement, enable et nouvelle cible
explicites. **Elles refusent le routage de production et les cibles intercartes.**
Elles exigent désormais également un lien sain et un statut distant sans défaut
avant activation. Avec un même binaire dédié sur les deux cartes, le doigt choisi
n'est local que sur une carte. Aucune autorisation de mouvement multicarte n'est
créée. THUMB reste bloqué par `THUMB_EXTI_MOTION_QUALIFIED=0`.

Le lot 3 est une candidature logicielle pour le lot 4, pas une qualification
électrique ou temps réel. Les limitations et conditions de bring-up figurent
en fin de rapport.

## Références et architecture

La référence historique `origin/simulation_pipeline` est consultée uniquement
par `git show`, au commit `15f4d4ec109a4a642b0be77d99fee446246e70cc` :
`board_link.c/.h`, `app.c`, `intent_router.c`, `motor_control.c`, `main.c` et
les handlers UART de `stm32g4xx_it.c`. Des copies de lecture sont dans
`build/lot3-baseline/historical`. Le lien historique utilisait six octets,
un angle Q15 big-endian, un `HAL_UART_Transmit` bloquant et une bague RX avec
pertes silencieuses. Son routage déclenchait le contrôle L298N par durée.
Ces chemins et leurs garanties supposées n'ont pas été restaurés.

| Rôle PC0 | Moteurs locaux | Autorité host |
|---|---|---|
| HIGH : MASTER | LITTLE 4, WRIST_X 5, WRIST_Y 6 | Commandes normales et diagnostic/commissioning local |
| LOW : SLAVE | THUMB 0, INDEX 1, MIDDLE 2, RING 3, PALM 7 | Diagnostic/commissioning local ; actions normales host refusées |

LPUART1 PA2/PA3 reste accessible sur les deux cartes. USART3 PB10/PB11 AF7,
115200 bauds, 8N1, reste préparé par `OldHW_Init` avec le handle unique `huart3`.
`board_link_runtime.c` possède sa réception, sa transmission et sa tâche.
Il n'existe ni deuxième handle copié ni accès MASTER aux timers SLAVE.

`App_Init` prépare les objets avant le scheduler. Une tâche statique `boardlink`
de 768 mots, priorité 2, traite au plus 256 octets par passage puis dort 2 ms.
Sa pile/TCB ne consomment pas le heap dynamique FreeRTOS existant. Le contrôle
reste à 100 Hz, priorité 3 ; Safety à 20 ms, priorité 5. Les algorithmes PID,
garde, feedback, Kalman et le driver du lot 2 restent conservés. Seuls les points
d'intégration du lien sont ajoutés à `motor_safety.c`.

## Trames et sérialisation

Le corps explicite est encodé en COBS puis terminé par `00`. Le zéro n'apparaît
pas dans la partie encodée ; il permet de retrouver une frontière après bruit
ou troncature. Taille maximale : **84 octets bruts, 86 octets sur le fil**.
Aucune structure C n'est envoyée par `memcpy` sur le fil.

Tous les entiers multioctets sont little-endian. Les entiers signés sont en
complément à deux. Positions/vitesses/PWM utilisent des entiers signés int32,
en milliradians, milliradians/s et milli-pourcent. Aucun float ABI ne transite.

| Offset brut | Largeur | Champ |
|---|---|---|
| 0 | u8 | Version, actuellement 1 |
| 1 | u8 | Type |
| 2 | u8 | Rôle source : 1 MASTER, 2 SLAVE |
| 3 | u8 | Longueur payload, 0..48 |
| 4 | u64 | Session source non nulle |
| 12 | u64 | Session destinataire, 0 seulement lors de découverte HELLO |
| 20 | u32 | Séquence de transport, commence à 1 |
| 24 | u32 | Epoch STOP propre à la source |
| 28 | u32 | Epoch destinataire connu lors de la préparation |
| 32 | N | Payload |
| 32+N | u32 | CRC-32/ISO-HDLC de l'en-tête et du payload |

CRC réfléchi `0xEDB88320`, initial/final XOR `0xFFFFFFFF`. La table à 16 entrées
réduit le travail sous PRIMASK. Le CRC détecte des corruptions ; il ne constitue
pas un MAC contre un équipement hostile. Le câblage intercartes est supposé
de confiance.

| Type | Payload exact | Taille fil |
|---|---|---|
| HELLO=1 | Aucun | 38 |
| HEARTBEAT=2 | Dernière séquence reçue u32 ; causes STOP source u32 ; autorisation locale u8 booléenne | 47 |
| TARGET=3 | Identifiant de commande u32 ; moteur u8 ; plafond vitesse u8 ; cible mrad i32 | 48 |
| FEEDBACK=4 | Format ci-dessous | 74 |
| STOP=5 | Causes STOP source u32, notification également des défauts graves | 42 |
| ACK=6 | Identifiant de commande u32 ; moteur u8 ; résultat u8 | 44 |

Résultats ACK : 0 accepté, 1 interdit, 2 invalide, 3 périmé. L'identifiant
de commande est distinct de la séquence de transport. Un ACK indique seulement
l'acceptation par le contrôleur local ; le feedback décrit ensuite l'état réel.
La complétion HAL TX ne prouve ni réception ni acceptation ni mouvement.

FEEDBACK, offsets dans le payload de 36 octets : moteur u8 à 0, flags u8 à 1,
deux réservés nuls à 2, âge source u32 à 4, position i32 à 8, vitesse i32 à 12,
cible i32 à 16, PWM réellement appliqué i32 à 20, compte brut i64 à 24,
causes STOP u32 à 32. Flags : valide=1, calibré=2, enabled=4, armed=8.

## Sessions, ordering et autorisation

La session initiale u64 vient du RNG matériel, alimenté par HSI48, sans IRQ RNG.
Le worker attend avec des délais RTOS bornés hors PRIMASK : 50 ms par phase
oscillateur/génération. Une erreur clock/seed, une expiration, zéro ou répétition
consécutive des valeurs empêche le démarrage du lien et verrouille STOP LINK.
La source HSI48 et les contrôles d'erreur sont confirmés par les headers ST et
les sections RNG du [RM0440](https://www.st.com/resource/en/reference_manual/dm00355726.pdf).
Le RNG n'a pas été testé physiquement. L'unicité entre boots est probabiliste
sur 64 bits ; elle n'est pas présentée comme un compteur persistant garanti.

HELLO échange les sessions et le rôle/version. La session n'est établie que
lorsque le pair renvoie notre session comme destinataire. **Un heartbeat valide
est ensuite nécessaire pour considérer le lien sain** : HELLO seul ne prouve
pas l'état de sécurité distant. Ni HELLO ni heartbeat ni ACK ne récupèrent une
sécurité locale ou n'arment une sortie.

La première découverte, un nouveau pair/reboot ou un HELLO inattendu pendant
une autorisation coordonnée provoquent un STOP local. Une perte du lien fait
évoluer la session locale, vide commandes/ACK attendus/feedback et retire les
autorisations. Les trames de l'ancienne session ne peuvent plus porter une cible
valide. Une session u64 épuisée bloque définitivement jusqu'au redémarrage.

Les séquences sont assignées à la sortie de file, dans l'ordre du fil.
Les séquences répétées ou inférieures sont rejetées, sans renouveler la
fraîcheur. Il n'existe pas de comparaison modulo ambiguë : avant un wrap de
séquence ou d'identifiant de commande, le lien s'arrête et change de session.
Un wrap de l'epoch local impose également cette rupture. Une commande consommée
n'est pas exécutée deux fois. Il n'y a pas de retransmission automatique de cible.
Un ACK absent aboutit à STOP, puis exige récupération et nouvelle commande.

Les epochs des deux cartes sont transportés séparément et ne sont jamais
égalisés. Une cible et son ACK doivent correspondre aux sessions et epochs
attendus. STOP/récupération invalident les travaux préparés. Un changement
d'epoch distant retire l'autorisation ; pendant le contrôle coordonné il
provoque aussi STOP. La récupération d'un pair déjà désarmé ne provoque pas
une boucle de nouveaux défauts uniquement à cause de son nouvel epoch.

Deux opérations Protobuf **additives**, sans renumérotation des champs/opérations
existants, sont ajoutées à `CommissionCommand` et aux bindings C++/Python :

- `LINK_RECOVER=13`, console `link-recover` : récupération conditionnelle de
  la sécurité locale uniquement, via `Motor_RecoverFromFault` inchangé.
- `LINK_AUTHORIZE=12`, console `link-authorize` : autorisation locale du routage,
  après heartbeat sain, absence de défaut sur les deux cartes, toutes sorties
  désarmées/désactivées et validation session host/request/epoch/fraîcheur.

L'opérateur doit récupérer **chaque carte** depuis son diagnostic local,
attendre la propagation de leurs nouveaux epochs/statuts, puis autoriser chacune.
Le heartbeat transporte l'autorisation liée aux epochs courants ; les deux
autorisations sont nécessaires pour des cibles de production. Le protocole
n'offre aucune commande distante d'armement, enable, calibration ou effacement
de défaut. Le commissioning conserve ses propres prérequis locaux et son
autorité exclusive ; `link-authorize` ne contourne pas son refus de production.

## Routage et feedback

Le vrai parser/session host garde la validation moderne. SLAVE refuse les
actions normales host tout en acceptant Hello, diagnostic et STOP, ainsi que
le commissioning local autorisé. MASTER route les poses via l'intent router :
LITTLE et les deux poignets restent locaux, les cinq moteurs SLAVE sont offerts
au lien. Un échec de routage arrête la suite de la pose et verrouille STOP LINK.

Le récepteur vérifie rôle, propriété, session, epochs, autorisations, identifiant,
plage et fraîcheur. Puis il exige un moteur **déjà activé**, un vrai feedback
local frais, calibration, permission driver et limites modernes avant
`Motor_SetTargetAtEpoch`. Il ne change ni calibration ni armement/enable.
La conversion mrad peut faire refuser une valeur arrondie au-delà d'une limite ;
elle ne relâche pas celle-ci. Aucun contrôle moteur historique par durée ne revient.

Les huit feedbacks restent accessibles dans le diagnostic MASTER par sélection
du moteur. Les moteurs distants restent invalides dans le BSP/PID MASTER.
Le cache de statut distant est un objet séparé. Son âge additionne : âge donné
par la source, attente TX source, puis temps écoulé depuis le premier octet reçu
dans l'horloge locale. Aucune soustraction de clocks MASTER/SLAVE n'est faite.
L'âge sature, et un lien perdu, âge >100 ms ou absence de calibration invalide
la mesure. Les floats affichés indisponibles restent NAN.

Le Protobuf diagnostic conserve ses champs : bit 21 des flags = source distante,
bits 31:24 = âge distant en ms saturé à 255 (ancien/inconnu). `timestamp_ms` et
`epoch` sont ceux de la carte interrogée ; `faults` associe défauts locaux et
distants. Les flags de preflight/limites locaux ne sont pas attribués au moteur
distant. `hw_monitor.py` affiche source et âge. Les ADC/courants/Kalman distants
ne sont pas inventés ; le courant physique reste indisponible.

Chaque moteur local est proposé en feedback une fois par tour de 80 ms.
La télémétrie est écartée avant de saturer la file de commandes/ACK. Un instant
de mesure ne constitue pas une preuve de mouvement ou de bonne calibration
mécanique. Les poses ne forment pas une transaction atomique entre cartes et
leurs PID ne sont pas synchronisés en phase. Les poignets restent sur MASTER
et aucune action physique couplée n'est autorisée par ce lot.

## STOP, files et délais

La sécurité locale moderne reste autoritaire : chaque défaut arrête ses sorties
dans le verrou STOP existant. Le service du lien observe les causes/epochs et
émet STOP avec les causes. Le récepteur verrouille son propre STOP LINK et
neutralise ses sorties. Les notifications répétées ne recréent pas indéfiniment
des epochs ; un STOP reçu, même répété, reste conservateur. Un STOP mal formé
avec CRC valide entraîne également une rupture sûre.

RX : 256 emplacements, 255 utiles, timestamp par octet. Débordement/erreur UART
arrêtent immédiatement localement dans le callback, sans dépendre de la file
RTOS ; la tâche abandonne/réarme RX et resynchronise ensuite. Un échec d'abandon
met le transport en quarantaine. Un événement d'une autre UART n'est pas consommé.

TX : huit messages ordinaires et huit commandes en attente d'ACK, plus les slots
réservés HELLO, heartbeat et STOP. La saturation des commandes/ACK provoque STOP
et changement de session ; la découverte de cette nouvelle session arrête aussi
le pair. Les notifications critiques ne sont pas simplement perdues dans une
file pleine. Une trame déjà commencée ne peut pas être rappelée ; les epochs
et la supervision du destinataire empêchent sa réutilisation après STOP.

| Borne logicielle | Valeur / effet |
|---|---|
| Heartbeat | 50 ms |
| Silence RX heartbeat ou absence de progression de son écho de séquence | >150 ms : STOP autonome et nouvelle session |
| File cible avant émission | >50 ms : STOP, cible non émise |
| Assemblage / âge de réception d'une trame | >25 ms : trame abandonnée/refusée |
| ACK de cible depuis son offre | >100 ms : STOP |
| TX HAL sans complétion valide | >15 ms : STOP, abandon hors verrou, quarantaine si échec |
| Âge feedback distant | >100 ms : invalide, sans substitution silencieuse |

Le heartbeat contient la dernière séquence effectivement reçue. Ainsi, recevoir
encore le pair ne suffit pas à masquer une panne dans l'autre sens : l'écho ne
progresse plus et son deadline expire. Les deux cartes supervisent sans attendre
une décision de l'autre. Un retour de liaison ne réarme rien.

Le test de frontière vérifie absence d'expiration à 150 ms et arrêt à 151 ms.
Sur firmware, le SafetyTask vérifie au plus tous les 20 ms, le worker toutes les
2 ms lorsqu'il est planifié. Sous l'hypothèse explicite d'au plus 5 ms de retard
de tâche et d'un tick HAL opérationnel, l'arrêt local suit la dernière preuve
valide en **au plus 175 ms**. En autorisant une dernière trame déjà sur le fil
(86 octets, environ 7,5 ms), retenir **185 ms après rupture physique** comme
budget conservateur à mesurer. Ce n'est pas une garantie face à CPU bloqué,
PRIMASK durable ou scheduler arrêté. Aucun watchdog matériel n'est ajouté.
USART3 ne garantit jamais un STOP distant instantané après débranchement.
Un fatal peut empêcher tout envoi ; le pair doit alors expirer lui-même.

Le modèle à débit fini teste une seconde à 115200/8N1, service 2 ms, dix poses
complètes/s et cadence de feedback normale : les 50 cibles distantes sont
acceptées sans défaut. Le test de charge logique vérifie aussi 1000 cibles,
mais sans prétendre mesurer un débit UART réel. Une fréquence de poses trop
élevée sature les bornes et provoque STOP ; aucun débit arbitraire n'est promis.

## Interruptions et RTOS

USART3 utilise un handler fort et `HAL_UART_IRQHandler(&huart3)` en HW uniquement.
Les callbacks HAL uniques dispatchent d'abord ce handle au propriétaire USART3,
puis laissent LPUART1 au transport host existant. UART4 reste inactif. SIM garde
son transport virtuel et aucun IRQ USART3 actif.

USART3/LPUART1 : priorité NVIC 6 ; plafond syscall FreeRTOS 5 ; THUMB EXTI 3,
TIM6 HAL 4, PendSV/SysTick 15 inchangés. USART3 ne fait ni allocation ni attente
RTOS ni décodage dans l'IRQ ; il reçoit un octet, réarme par l'API HAL IT bornée,
ou signale le défaut. Les abandons HAL sont dans la tâche, hors PRIMASK.
La validation TX et son démarrage IT partagent le verrou STOP, selon la même
exception bornée déjà auditée pour LPUART1 ; aucun HAL TX bloquant n'est utilisé.

Le parser et les commits utilisent PRIMASK pour sérialiser avec les états STOP.
Le travail est borné, mais sa durée maximale et le chemin de STOP doivent être
mesurés sous charge réelle. La priorité EXTI 3 ne protège pas THUMB contre
PRIMASK. Le verrou de qualification THUMB demeure donc indispensable.

## Validation réellement exécutée

Commandes depuis la racine, sans accès matériel :

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Mode OLD_HW -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Mode SIM -Rebuild
& 'C:\Program Files (x86)\GnuWin32\bin\make.exe' rebuild HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=1 OLD_HW_MOTION_OPT_IN=1
& 'C:\Program Files (x86)\GnuWin32\bin\make.exe' rebuild HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=4 OLD_HW_MOTION_OPT_IN=1
python tools/verify_startup_build.py
$env:MAKE = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
python tests/startup/test_build_modes.py
python tests/old_hw/run_tests.py
python tests/old_hw/run_lot2_tests.py
python tests/old_hw/run_lot3_tests.py
$env:PYTHONPATH = (Join-Path $PWD 'build\lot2-tools')
python tests/startup/run_tests.py
python tests/safety/run_tests.py
python tests/comms/run_tests.py
python tests/outputs/run_tests.py
python tests/patch7/run_tests.py
python tests/patch7/test_monitor.py
python tests/simulation/run_tests.py
arm-none-eabi-nm -u build/HW/n-pulse.elf
arm-none-eabi-nm -u build/SIM/n-pulse.elf
arm-none-eabi-nm -u build/HW-single-1-optin/n-pulse.elf
arm-none-eabi-nm -u build/HW-single-4-optin/n-pulse.elf
git diff --check
```

Chaînes inchangées : ARM GCC 14.3.1, GNU Make 3.81, GCC/G++ hôte MinGW 6.3.0,
Python 3.12.8. Protobuf est lu depuis le dossier temporaire du lot 2 ; aucune
nouvelle dépendance globale n'est installée. Logs : `build/lot3-*.log`.

| Vérification | Résultat |
|---|---|
| Clean ARM HW | PASS, exit 0 ; text=117336, data=100, bss=38024 octets |
| Clean ARM SIM | PASS, exit 0 ; text=71392, data=96, bss=31272 octets |
| Clean ARM dédiées INDEX et LITTLE | PASS, exit 0 chacune ; text=123040, data=100, bss=38056 octets chacune |
| Compilations / ELF | Aucun warning/error dans les quatre logs finaux ; aucun symbole non résolu ; SRAM système bornée à 96K |
| Vecteurs et interlocks | PASS : USART3 fort en HW seulement, RTOS/TIM6/EXTI conservés ; Arm/primitive PWM non nulle absents des builds ordinaires ; flags THUMB non qualifié conservés |
| Alternance HW/SIM/HW-Debug | PASS, exit 0 ; 148 objets isolés inchangés, alias HW préservé, options dangereuses non cadrées refusées |
| OLD_HW lots précédents | 43 scénarios PASS, exit 0 |
| Lot 2 | 37 scénarios PASS, exit 0, y compris PWM/epochs/concurrence/THUMB et binding C++ des opérations 11/12/13 |
| Lot 3 protocole | 34 scénarios PASS, exit 0 : rôles/propriété, HELLO non suffisant, cibles, fragmentation/CRC/troncature, duplication/désordre/rejeu, reboot, panne totale/unidirectionnelle, STOP de chaque rôle/en transit/file pleine/mal formé, récupération, wraps, âge/invalidité, version incompatible, charge et modèle baudrate |
| Lot 3 routage | Vraie session host et vrai intent router : huit IDs, poignets locaux, rejet de production SLAVE, invalidation de queue par STOP ; PASS |
| Lot 3 USART3 | Huit scénarios sur le vrai owner/service/callbacks : propriété, RX overflow/error/rearm, TX start/timeout, abandon échoué, RNG indisponible ; PASS |
| Lot 3 deux contrôleurs | Deux scénarios avec vrais BSP/PID/guard/safety/L298N/protocole : feedback, STOP, récupération/nouvelles cibles, perte à sens unique et défaut IRQ THUMB pendant trafic ; PASS |
| Startup / safety | 31 scénarios startup et suites safety async/HW/cleanup/commissioning/BSP PASS |
| Communications / outputs | 24 scénarios production, bornes SIM et codec Python PASS ; registres/driver et 20 scénarios programmeur factice PASS |
| Patch 7 / monitor | Commissioning/Kalman et treize commandes console, âge distant, validation et schéma PASS |
| Simulation native | 27 scénarios PC PASS, aucun ROS/Gazebo/port série réel |

La nouvelle suite conserve **52 commandes finales, toutes exit 0 (0x00000000)**,
dont 45 scénarios exécutés, dans `build/old-hw-lot3-tests/results.json`.
Les deux contrôleurs réels utilisent deux DLL indépendantes, chacune avec ses
propres globals, registres, horloge (400/9200 ms au départ) et historique d'epoch.
Le banc natif emploie des configurations de commissioning distinctes pour
exercer LITTLE et INDEX/THUMB simultanément. Il teste le vrai chemin contrôleur
autorisé localement, sans prétendre autoriser cette configuration dans une image
de production : l'adaptateur de production refuse toujours ces cibles en mode
single-motor. La propriété/runtime de rôle du même firmware est couverte par
les tests OLD_HW et le même protocole instancié avec deux rôles.

ASan/UBSan sont **indisponibles**, bibliothèques hôte absentes ; leurs skips ne
sont pas comptés comme passages instrumentés. Pendant le développement, le
chargement d'une DLL MinGW 32 bits par Python 64 bits a produit WinError 193,
puis le banc a été remplacé par un chargeur natif de même architecture.
Une assertion de fixture a retourné exit 3 : elle attendait TX inactif alors
que le heartbeat suivant avait correctement repris sa propriété ; l'attente a
été corrigée. Les premières erreurs de compilation de déclarations ont été
corrigées. Aucun test n'a été désactivé ; aucun accès mémoire 0xC0000005 n'a été
observé. Aucun résultat hôte ou ELF n'est présenté comme un essai physique.

## Inventaire depuis le lot 2

L'inventaire comparé au snapshot est dans `build/lot3-inventory.json`.

Modifiés, 23 fichiers :

- `Core/Inc/stm32g4xx_it.h`, `Core/Src/main.c`, `Core/Src/stm32g4xx_it.c`, `build.ps1`.
- `fw/app/app.c`, `hw_telemetry.c`, `intent_router.c`.
- `fw/comm-stack/protocol_messages.proto`, `protocol_messages.h`, `PyUART/protocol_messages_pb2.py`.
- `fw/comm/board_link.c`, `board_link.h`, `comm.c`, `comms_session.c`, `uart4_transport.c`.
- `fw/motors/motor_control/motor_safety.c` : points de supervision/récupération/permission uniquement.
- `tests/old_hw/mocks/stm32g4xx_hal.h`, `tests/old_hw/test_preflight_protocol.cpp`, `tests/patch7/test_monitor.py`.
- `tools/build_artifacts.py`, `firmware.mk`, `hw_monitor.py`, `verify_startup_build.py`.

Ajoutés, 12 fichiers :

- `fw/comm/board_protocol.c`, `board_protocol.h`, `board_link_runtime.c`.
- `tests/old_hw/run_lot3_tests.py`, `test_board_protocol.c`, `test_board_routing.c`,
  `test_board_transport.c`, `test_board_controller.c`, `test_two_controllers.c`.
- `tests/old_hw/dual_mocks/FreeRTOS.h`, `task.h`.
- `docs/OLD_HW_LOT3.md`.

Aucun fichier préexistant retiré. Les marqueurs/manifests deviennent
`N_PULSE_OLD_HW_LOT3` / `OLD_HW_LOT3` ; `N_PULSE_DUAL_BOARD_LINK=1` identifie
l'intégration maintenue. Le mapping moteur, startup RET6/96K SRAM, driver
L298N, coupure fatale, PID, garde, BSP, ADC et Kalman ne sont pas remplacés.
Le script de flash et sa politique de refus des images de commissioning
restent inchangés.

## Conditions et dépendances pour le lot 4

**Le logiciel est prêt pour commencer le lot 4 avec les sorties inhibées.**
Aucun défaut logiciel connu restant n'est accepté comme prétexte pour lever
un verrou. Les validations matérielles et RTOS ci-dessous sont encore requises.

1. Vérifier ENA/IN1/IN2, alimentation/masse, jumpers L298N, roue libre, inversion,
   STOP et fatal avant tout armement. Courant/overload restent sans mesure.
2. Vérifier UART croisé, niveaux électriques, baudrate réel, clock/RNG, bruit,
   erreurs/overflow et comportement des deux cartes au reboot séparé.
3. Mesurer latences IRQ, PRIMASK, tâches, TX et délais de STOP sous charge maximale.
   Qualifier le budget de 185 ms et un moyen de coupure indépendant ; aucune
   garantie de ce délai n'existe si les hypothèses de scheduling sont violées.
4. Garder THUMB en acquisition seulement et `THUMB_EXTI_MOTION_QUALIFIED=0`
   jusqu'à qualification séparée des pertes/latences EXTI, du moteur et de son
   encodeur. Aucun feedback courant ni seconde source PID distante ne doit être
   inventé pour passer ces contrôles.
5. Pour un essai moteur ultérieur autorisé : image dédiée single-motor, choix
   local correct, lien/host sains, récupération explicite des sécurités, référence
   physique/calibration, limites conservatrices, `preflight verified`, arm,
   enable et cible nouvelle. Ces étapes ne sont pas effectuées dans ce lot.
   La politique de flash production continue de refuser ces images ; une
   procédure de bring-up distincte devra être explicitement revue/autorisée.
6. Ne pas utiliser les actions normales pour ouvrir des mouvements multicarte
   ou des poignets couplés. Leurs contraintes physiques, autorisations et
   séquencement coordonné exigent une validation séparée ; aucune activation
   globale n'est fournie par ce port.

Les transports, parsers, modèles de registres et contrôleurs ont été exécutés
sur PC, et les images ARM compilées/inspectées. Les premières vérifications
du lot 4 restent des vérifications matérielles désarmées. Aucun essai moteur
physique n'est déclaré sûr ou réussi par ce rapport.
