> **Évolution lot 6 :** les descriptions SIM v1/CRC8, RX SIM déconnectée et
> interdiction d'activation virtuelle ci-dessous sont historiques. Le contrat
> actuel et la boucle fermée PC sont dans [SIMULATION_LOT6](SIMULATION_LOT6.md)
> et le [guide court](SIMULATION_QUICKSTART.md). Les invariants HW restent
> désarmés ; UART réel/Gazebo/moteurs réels ne sont pas testés dans le lot 6.

# Lot 5 — robustesse désarmement, engagement UART4 et référentiel encodeur

Travail et validations PC les **8–9 octobre 2026**, sur le dépôt local après les lots
1–4. La demande jointe est la spécification ; aucun audit séparé trouvé parmi
les pièces jointes disponibles ou les fichiers du dépôt. Aucun AGENTS.md trouvé
à la racine, dans les sous-arbres concernés ou les répertoires parents consultés.
Les rapports antérieurs sont conservés avec un addendum daté.

## Périmètre maintenu

Une seule **STM32G474QET6**, huit moteurs locaux, deux axes de poignet inclus.
GNU Make reste maintenu via build.ps1 HW et build-sim.ps1 SIM. Aucun pin, mapping,
polarité, gain PID, borne mécanique, réglage électrique, .ioc, startup ou linker
n'est changé. MotorBackend_SetTarget reste le placeholder existant : le chemin
réel passe par Motor_Update et MotorBackend_ApplyOutput.

Les moteurs restent désarmés/désactivés au démarrage. ADC/OPAMP off par défaut,
PID présent mais non réglé et non activé, calibration historique bloquée à la
compilation. UART4 HW PC10/PC11, 115200 8N1, protobuf AA/longueur/payload ; le SIM
conserve son format AA/version/type/longueur/payload/CRC8. **RX SIM déconnectée**.
Aucun flash réel, énumération de sondes réelles, accès série, régénération CubeMX,
commit/push/reset/clean Git. Le flash est exercé uniquement avec l'exécutable
factice de tests/outputs ; son SN explicite et les artefacts canoniques restent exigés.

## Défauts et premières preuves

Les fixtures compilent les fonctions de production ; les mocks portent sur HAL,
RTOS et, pour les anciens tests safety/comms, le BSP. Les nouvelles fixtures BSP
compilent encoder_bsp.c lui-même. Pas de hook de test actif dans le firmware.

| Cas initial | Observation réellement obtenue avant correction | Après correction |
|---|---|---|
| Disarm public après INDEX activé et sortie non nulle | enabled=1, fault=0, assertion d'inhibition en échec | enabled=0, fault=1 ; reprise conditionnée aux quatre étapes explicites |
| STOP dans la première notification TX après extraction | fault=1, HAL calls=1 ; ancienne trame émise par la fixture | fault=1, HAL calls=0 ; tx_stale incrémenté |
| CPR=1000, quadrature=4, ratio=FLT_MAX | SetCalibration accepté, IsCalibrated vrai ; assertion de rejet en échec | proposition rejetée, aucune nouvelle calibration valide |
| Reset avec offset=100 sans mouvement | position suivante −0,15708 rad ; assertion de zéro en échec | zéro stable et offset dans le nouveau référentiel remis à zéro |
| raw=100, offset=INT64_MIN | copie du BSP initial vérifiée par SHA-256 : position −1,4488e16 au lieu d’un résultat positif ; assertion en échec (sans UBSan) | régression numérique exécutée, résultat fini positif, aucune soustraction signée des offsets |

Les premiers runners startup/safety/comms terminent avec code **1** à cause des
assertions voulues (processus C code 3). Le reset exécuté séparément termine avec
code **3**. Logs : build/validation/lot5/before-startup.log, before-safety.log,
before-comms.log et before-offset.log. Le BSP initial a été retrouvé dans une
sauvegarde du lot 2 : ses deux fichiers c/h ont exactement les SHA-256 relevés
au début du lot 5 (baseline-proof.txt). Cette copie inchangée est compilée dans
une fixture isolée sous build/validation/lot5/baseline ; l'offset donne −1,4488e16
et un code 3. Cette reproduction complémentaire a été exécutée après les corrections
sur la copie initiale vérifiée, pas en rétablissant le dépôt. Aucun diagnostic UBSan
n'est prétendu sur ce poste. Les tests sans sanitizer ne prouvent pas l'absence de toute UB.

Commande de reproduction complémentaire (le dernier appel doit échouer) :

~~~powershell
gcc -std=c11 -Wall -Wextra -Werror -I build/validation/lot5/baseline -I tests/startup/mocks build/validation/lot5/baseline/encoder_bsp.c build/validation/lot5/baseline/test_offset.c -o build/validation/lot5/baseline/offset.exe -lm
.\build\validation\lot5\baseline\offset.exe
~~~

## Contrat et preuve des transitions moteur

Motor_DRV8848_Disarm est une demande MANUAL à l'interlock global, puis un service
d'arrêt en tâche lorsque PRIMASK/IPSR le permettent. Elle coupe synchroniquement
SLEEP et les sorties, invalide activations, limites PWM, consignes, révisions et
PID préparés des huit moteurs. Sous masque ou en ISR, aucune attente HAL n'est
lancée ; les tâches assurent le nettoyage ultérieur.

Les primitives Stop/StopAll du driver sont des écritures de sortie zéro, distinctes
d'un désarmement persistant. Leur commentaire rend cette limite explicite.
L'interlock utilise InhibitLocked ; Motor_ServiceStop utilise exclusivement
**Motor_DRV8848_CleanupStoppedInternal**, jamais le désarmement public. Les anciens
callers Disarm dans Arm ont été retirés. Il n'y a donc pas de récursion
interlock → service → désarmement public → interlock.

Une propriété exclusive s_arm_in_progress couvre désormais **toute** transition
HAL, démarrage et nettoyage. Elle est acquise/libérée sous Motor_Lock et conservée
pendant les HAL et HAL_Delay hors verrou. L'epoch est capturée avant armement,
revérifiée entre ses six HAL, avant SLEEP haut et après le délai final. STOP
n'attend pas le propriétaire : il coupe immédiatement et change l'epoch.
L'armement interrompu garde la propriété jusqu'à son propre nettoyage complet.
Un ancien service ne peut nettoyer pendant cette transition, ni un armement
nouveau être accepté avant la fin d'un nettoyage.

Tous les retours des six starts/stops sont contrôlés. Le nettoyage tente les
six stops même si l'un échoue. Un échec de start ou de cleanup verrouille DRIVER.
Un nettoyage échoué ou momentanément exclu remet le travail d'arrêt en attente ;
récupération/armement restent refusés. Aucune attente sous PRIMASK et aucun mutex
ou spin de récupération. SLEEP peut avoir été haut **avant** un STOP pendant le
délai final, mais aucune continuation de cette transition ne le remet haut après STOP.

Reprise : **récupération conditionnelle → armement explicite → activation →
commande nouvelle**. Enable remet la limite à zéro ; aucun résultat PID ancien
ne redevient applicable, y compris après récupération et réarmement.

Fixtures : STOP et échec HAL partiel à chacun des six starts ; STOP dans le délai
final ; tentative Arm/Recover/Disarm à chacun des six stops ; répétitions de
Disarm ; cleanup en erreur et retries ; exclusion également testée sans défaut
préexistant ; absence de cleanup ancien après un Arm accepté ; désarmement après
préparation du PID réel ; huit moteurs activés puis désarmement public global.

## Engagement effectif UART4 et notifications

Un seul propriétaire TX UART4 conserve les trames complètes et le buffer statique
transmitting jusqu'à fin/abort. Extraction de queue et purge des notifications
précèdent la section critique d'engagement. Dans cette section : vérifier epoch,
préparer l'état de fin, appeler HAL_UART_Transmit_IT, accepter tx_active seulement
si HAL_OK, puis déverrouiller. Le **point d'engagement est l'enable TXEIE/TXFTIE
effectif de la HAL**, pas la mise en file ni une déclaration anticipée « en vol ».
STOP avant cette section invalide la trame ; STOP après est ordonné après cet
engagement et coupe immédiatement les moteurs locaux sans attendre TX.

HAL locale examinée : Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c,
HAL_UART_Transmit_IT lignes 1311–1370, AbortTransmit et UART_EndTransmit_IT.
Transmit_IT ne fait ni attente de périphérique, ni DMA, ni callback : seulement
état, pointeurs, choix ISR et enable d'interruption. **ATOMIC_SET_BIT contient bien
une boucle LDREX/STREX** (stm32g4xx.h:192), visible dans le désassemblage ; elle
n'est pas assimilée à une fonction sans boucle. La justification de la section
courte est l'absence d'interruption revenant entre les deux instructions sous
PRIMASK : aucune tâche/IRQ ne peut casser la réservation locale ; NMI/faults
aboutissent à la voie fatale sans retour, et aucun autre propriétaire UART
n'écrit le registre CR1/CR3 dans cet intervalle. Aucun polling matériel ou timeout
HAL n'est dans cette section. Cette preuve de sérialisation concerne le code
actuel en exécution normale, pas un debugger qui invaliderait une réservation,
un bus défaillant ou une nouvelle voie NMI revenante. Réauditer à toute mise à
jour HAL/exception/propriétaire UART. Sa durée effective n'a pas été mesurée sur carte.

La notification seule ne valide plus TX. Le callback doit correspondre au handle
unique, à une transmission active et à HAL READY avec TxXferCount=0. Le worker
exige tx_completed, avec deadline RTOS totale de 25 ms même en cas de réveil parasite.
Avant abort il retire l'état actif ; la HAL abort désactive TCIE/TXEIE/TXFTIE,
donc un ancien IRQ matériel pending ne peut terminer la nouvelle transmission.
Une notification tardive est purgée/ignorée, et un callback pendant le nouveau
BUSY ne l'achève pas. Abort reste hors verrou ; son échec compte une erreur et
met le propriétaire/buffer en quarantaine au lieu de le réutiliser.
Après erreur/timeout, silence RTOS de 101 ms conservé pour resynchronisation.

Tests déterministes : STOP après extraction, à la purge immédiatement avant
engagement ; IRQ STOP devenue pending après validation ou juste au start HAL,
délivrée au déverrouillage (donc après engagement), avec et sans récupération ;
échec HAL, timeout, notification/callback tardifs avant et pendant le transfert
suivant, nouvelle vraie fin, saturation, abort échoué et quarantaine. Les sections
atomiques sont observées par PRIMASK dans les mocks ; les injections utilisent
l'instrumentation GCC hôte et ne sont pas liées au firmware.

Diagnostics distingués : tx_stale (rejet avant engagement), tx_errors,
tx_timeouts, tx_complete, tx_full. Une trame engagée peut continuer après STOP ;
**aucun rappel des bits émis ni ALL_STOP distant garanti**. Aucun protocole nouveau
ni RX SIM n'est introduit.

## Encodeurs, conversions et changement de référence

Le produit CPR × quadrature × ratio et l'échelle effectivement stockée en float
sont validés : valeurs positives finies normales, pas d'overflow ou échelle
nulle/subnormale. Un ratio pris isolément fini ne suffit plus. Les différences
d'offset utilisent une magnitude unsigned avant conversion, sans overflow signé,
y compris INT64_MIN/MAX et une différence d'un seul compte près de ces bornes.
Les limites de conversion float/non-finis/sous-flux vers zéro sont vérifiées.
L'accumulation int64 est bornée avant addition ; un débordement conserve le
dernier compte représentable et rend la mesure invalide, sans wrap signé.

Un défaut numérique est sticky jusqu'au reset explicite, empêche une calibration
qui le masquerait et retourne NAN/valid=false, jamais un faux zéro exploitable.
Une erreur de conversion angulaire n'arrête pas la source brute ; une erreur
d'accumulation fige le dernier compte représentable. La calibration est un
contrat numérique, pas une validation mécanique ni un chargement automatique.

Reset retourne désormais bool : refuse un contrôleur enabled ; sinon fixe une
**nouvelle origine brute** au compteur matériel courant et remet l'offset à zéro
dans ce nouveau repère. CPR/ratio/sens sont conservés. ResetAll vérifie les huit
contrôleurs avant toute modification et refuse globalement si l'un est actif.
Aucun compteur matériel écrit ni timer redémarré ; PALM/LPTIM et les huit mappings
restent inchangés. Les callers de production actifs ne font aucun reset/calibration.

SetCalibration refuse atomiquement un contrôleur enabled ou en défaut numérique.
Une proposition invalide/refusée laisse l'ancienne calibration/mesure intacte.
Une modification acceptée ou un reset invalide le feedback contrôleur et sa
révision PID. Le backend HW lit une paire position/vitesse dans un snapshot
cohérent et livre le feedback dans le même domaine Motor_Lock : aucun ancien
échantillon préparé ne peut être livré après un changement de référentiel.
L'activation et le commit PID utilisent ce même verrou. Les sections BSP n'ont
aucune HAL start/stop ni attente ; leurs conversions double sont bornées mais
leur coût et leur latence d'IRQ demandent une mesure ultérieure.

Matrice BSP réelle : 18 combinaisons quadrature 1/2/4, deux sens, offsets −100/0/+100 ;
NaN/Inf, ratio nul/négatif/subnormal, CPR nul/UINT32_MAX, mauvais sens/quadrature,
produits extrêmes, offsets INT64_MIN/MAX, différence un compte aux extrêmes,
débordements accumulation positifs/négatifs et conversion non représentable ;
reset avec offset, calibration rejetée préservant la mesure, défaut sticky,
ResetAll, initialisation répétée et aucun redémarrage des huit encodeurs.
L'intégration réelle BSP → backend HW → feedback → interlock couvre l'invalidation,
le refus sur contrôleur actif, une modification après préparation PID, la perte
numérique active, la récupération refusée puis le reset et une mesure nouvelle.

## Commandes et résultats

Outils : ARM GCC **14.3.1**, GCC/G++ hôte MinGW **6.3.0**, Python **3.12.8**.
GNU Make MinGW **3.82.90** échoue ici avec INTERNAL: readdir: Invalid argument.
Le GNU Make **3.81** installé dans GnuWin32 fonctionne ; les wrappers et règles
n'ont pas été modifiés. Le lanceur exec habituel échoue au setup ; les mêmes
commandes sont réellement exécutées en processus locaux depuis Node.

PATH utilisé pour les validations finales :

~~~powershell
$env:PATH = 'C:\Program Files (x86)\GnuWin32\bin;C:\Users\cleme\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;C:\Users\cleme\Documents\Installation\MinGW\bin;' + $env:PATH
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-sim.ps1 -Rebuild
python tests/startup/run_tests.py
python tests/safety/run_tests.py
python tests/comms/run_tests.py
python tests/outputs/run_tests.py
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/outputs/verify_build.py
python tests/startup/test_build_modes.py
~~~

| Validation finale | Résultat |
|---|---|
| HW/SIM reconstructions GNU Make réelles, 68 unités chacune | code 0 |
| Startup, timebase/BSP/ADC + quatre scénarios numériques | 11 scénarios, code 0 |
| Safety anciennes suites async et HW | 24 × 2 scénarios, code 0 |
| Désarmement/transitions et intégration BSP réelle | deux nouveaux binaires, code 0 |
| Comms production parser/session/TX | 21 scénarios, code 0 |
| SIM bounds et encoding TX | 24 576 couples count/longueur, code 0 |
| Outputs/fatal/PWM | 8 groupes dont 176 états de sortie, code 0 |
| Flash factice exclusivement | 18 scénarios, code 0 |
| Quatre vérificateurs ELF HW/SIM | code 0, anciens invariants conservés/renforcés |
| Alternance HW → SIM → HW/Debug | 136 objets inchangés, alias HW conservé, mode invalide rejeté |

ELF par défaut : **Arm, Enable, SetCalibration/Reset/ResetAll absents**, aucun
HAL start PWM/HRTIM ni ADC/OPAMP. Les handlers forts FreeRTOS, HAL, UART4 et fatals
restent corrects. SIM n'a ni HAL_UART_Receive_IT ni parseurs RX raccordés ; aucun
consommateur LPUART1 actif ni UART polling. Le vérificateur startup exige désormais
également l'absence d'Enable et des modifications actives de calibration/reset.

Logs complets dans **build/validation/lot5/**. Les désassemblages HAL-TX-start et
TX-commit y sont conservés pour HW/SIM ; les contrôles fatal détaillés sont aussi
produits par la suite outputs dans build/validation/lot4/ comme auparavant.

**Skips explicites** : probes ASan+UBSan ET UBSan seul compilés/exécutables testés
indépendamment dans startup et safety ; GCC ne trouve pas les runtimes et aucun
clang n'est disponible. Les suites ordinaires passent sans sanitizer. Le codec
hôte HW Python reste sauté faute de google.protobuf ; le décodeur firmware C++
est effectivement testé. Aucun essai carte/oscilloscope ou simulateur distant.
Sur un poste avec HOST_CC et runtimes sanitizer disponibles, relancer les deux
runners startup/safety ci-dessus : ils exécutent automatiquement les régressions
numériques et l'intégration avec chacun des sanitizers utilisables.

## Mémoire et pile statiques

Tailles arm-none-eabi-size, en octets (avant = ELF présents au début du lot) :

| Mode | text avant → après | data avant → après | bss avant → après |
|---|---:|---:|---:|
| HW | 87 148 → 90 440 (+3 292) | 384 → 384 | 31 344 → 31 400 (+56) |
| SIM | 72 948 → 76 272 (+3 324) | 384 → 384 | 27 456 → 27 504 (+48) |

Les tableaux supplémentaires d'échelle/défaut coûtent 48 octets ; les trois
indicateurs TX et l'alignement expliquent le reste. Pas d'allocation dynamique
supplémentaire ni d'augmentation des piles de tâches.

Les fichiers before-stack.json, after-stack.json et stack-delta.json proviennent
de -fstack-usage, pas de mesures sur carte. Exemples de frames par fonction :

| Fonction | Avant | Après |
|---|---:|---:|
| Motor_Update | 160 | 160 |
| Motor_DRV8848_Arm | 24 | 24 |
| Motor_ServiceStop | 8 | 16 |
| MotorSafety_CompleteStopWork | 16 | 24 |
| UART4Transport_TxOnce | 32 | 48 |
| MotorBackend_RefreshFeedback HW | 32 | 64 |
| Encoder_BSP_SetCalibration | 24 | 64 |
| Encoder_BSP_GetRawCount | 80 | 32 + appel sample_locked (184) |
| Encoder_BSP_GetSnapshot | absent | 64 + appel sample_locked (184) |
| Encoder_BSP_Reset | 24 | 32 |

Ces frames ne sont pas des maxima de chemin : ajouter fonctions appelées,
conversions/libgcc double, contexte RTOS et exceptions. Les sections critiques
BSP/TX allongent certains chemins et doivent être chronométrées. Tailles des
tâches, heap de 16 KiB et linker/MSP inchangés ; aucune marge de pile en exécution
n'est revendiquée. Les doubles expliquent une partie de la croissance de code.

## Préservation et fichiers

État initial et SHA-256 dans build/validation/lot5/initial.json : 1 195 entrées
Git recensées, dont 1 150 fichiers présents et 45 suppressions préexistantes.
Le diff indexé initial reste identique ; son SHA-256 est
f355a5a457f8cec5defed294e65d0f4900bd1bd1b41aa62de029e98c58376894.
Aucun fichier initial présent disparu ; les suppressions préexistantes ne sont
pas restaurées. Comparaison finale : **1112 fichiers identiques, 38 modifiés dans le lot,
5 ajoutés, aucun disparu**. Comparaison détaillée dans preservation.json.
Cinq caches Windows apparus pendant l'exécution des outils dans un répertoire
littéral %SystemDrive% ont été retirés individuellement après vérification des
chemins, horodatages et de leur absence du relevé initial ; aucune suppression
récursive et aucun fichier initial concerné. Le relevé est dans
generated-cache-cleanup.json. git diff --check ne signale que l'espace final
préexistant dans Drivers/STM32G4xx_HAL_Driver/Inc/stm32g4xx_ll_crs.h:444,
fichier dont le SHA-256 initial est inchangé.

Modifications de production : encoder_bsp.c/.h, drv8848.c/.h, motor_lock.h,
motor_control.c (service), motor_safety.c/.h (retry cleanup), motor_backend_hw.c
(snapshot/delivery), uart4_transport.c. Les mappings, gains et protocoles restent
ceux du dépôt initial. Tests : runners startup/safety, mocks HAL/RTOS, fixtures
encoder/safety/comms ; ajout de test_disarm.c, test_encoder_integration.c,
test_encoder_numeric.c et sanitizers.py ; vérificateur startup renforcé.
README/MOTOR_STATUS décrivent maintenant la cible réelle. Guides calibration,
câblage, SIM, quick-test, testing/architecture/pipeline et documents racine
portent un avertissement daté. Les descriptions historiques sont conservées ;
le « démarrage rapide à jour » obsolète n'est plus une instruction applicable.

## Risques et étapes restantes

Aucune validation électrique, premier mouvement, protection courant calibrée,
réglage PID, watchdog ou arrêt matériel indépendant, homing, chargement de
paramètres ou RX SIM complet. Pas de garantie d'arrêt distant par UART.
Les compteurs 16 bits conservent la contrainte historique de moins d'un demi-tour
de compteur entre observations ; la lecture ne peut reconstruire des wraps
entiers perdus. Fraîcheur d'une position immobile ne prouve pas son câblage.
La conversion angulaire reste limitée par la précision float, même si le compte
brut et la différence entière sont protégés. Les longues coordonnées ne sont
pas une calibration physique admissible par simple validation numérique.
La preuve de concurrence est celle des fixtures et du code/ELF, sans mesure de
latence, de marge de pile, de signaux PWM ni d'inhibition SLEEP au banc.
Ce lot laisse intégralement fermée la voie au premier mouvement par défaut.

## Inventaire exact du lot

Fichiers préexistants adaptés :

- docs/COMMS_LOT3.md
- docs/OUTPUTS_LOT4.md
- docs/SAFETY_LOT2.md
- docs/STARTUP_LOT1.md
- fw/comm/uart4_transport.c
- fw/motors/motor_control/drv8848.c
- fw/motors/motor_control/drv8848.h
- fw/motors/motor_control/motor_lock.h
- tests/comms/mocks/stm32g4xx_hal.h
- tests/comms/mocks/task.h
- tests/comms/test_comms.c
- tests/safety/mocks/stm32g4xx_hal.h
- tests/safety/run_tests.py
- tests/safety/test_safety.c
- tests/startup/mocks/motor_control.h
- tests/startup/mocks/stm32g4xx_hal.h
- tests/startup/run_tests.py
- tests/startup/test_encoder.c
- tools/verify_startup_build.py
- FONCTIONNEMENT_FIRMWARE.MD
- MOTORS.md
- README.md
- docs/CALIBRATION.md
- docs/MOTOR_STATUS.md
- docs/QUICK_TEST.md
- docs/SIMULATION_GUIDE.md
- docs/TESTING.md
- docs/TEST_ARCHITECTURE.md
- docs/TEST_CLOSE_HAND.md
- docs/control_pipeline.md
- docs/sim_transport.md
- docs/wiring.md
- fw/bsp/encoder/encoder_bsp.c
- fw/bsp/encoder/encoder_bsp.h
- fw/motors/motor_control/motor_backend_hw.c
- fw/motors/motor_control/motor_control.c
- fw/motors/motor_control/motor_safety.c
- fw/motors/motor_control/motor_safety.h

Fichiers ajoutés :

- docs/ROBUSTNESS_LOT5.md
- tests/safety/test_disarm.c
- tests/safety/test_encoder_integration.c
- tests/startup/sanitizers.py
- tests/startup/test_encoder_numeric.c
