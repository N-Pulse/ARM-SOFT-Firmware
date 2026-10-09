# N-Pulse — Changements du firmware et guide de prise en main

Comparaison du code fourni avec le commit `4178e15df62c1cdb3c61ac73d6e086e08bb00020` (`Modified code with first version implementation of PID closed-loop control`). L’archive de référence correspond à ce commit, après normalisation des fins de ligne. État examiné le 9 octobre 2026.

Ce document décrit le comportement du code examiné, les commandes à utiliser et les limites observées. Les chemins sont relatifs à la racine du dépôt. Il distingue les tests relancés sur PC des résultats de compilation enregistrés dans l’archive et des essais matériels restant à réaliser.

## 1. Ce que cette version permet réellement

La version apporte une architecture pour une carte unique, une commande des huit moteurs via un backend matériel DRV8848, une gestion plus stricte des défauts et des communications, ainsi qu’une simulation en boucle fermée utilisant le véritable PID C du firmware.

**La simulation PC est utilisable immédiatement avec Python et GCC. Le firmware HW reste désarmé par défaut et ne permet pas encore de faire tourner les moteurs à travers le parcours applicatif normal. L’intégration Gazebo contient un adaptateur, mais ne constitue pas encore un banc Gazebo validé et prêt à lancer.**

| Usage | État de cette version |
| --- | --- |
| Simulation native sur PC, sans carte | 26 scénarios exécutés avec succès lors de cette revue |
| Compilation ARM HW et SIM | Réussites enregistrées dans les résultats fournis ; non relancées ici, faute de compilateur ARM |
| Contrôles des ELF et alternance des builds | Réussites enregistrées ; ce sont des vérifications statiques, pas des essais sur carte |
| Carte avec firmware HW, sans mouvement | Code et script de programmation disponibles ; démarrage et UART physiques non vérifiés |
| Mouvement d’un moteur réel | Pas encore disponible par le parcours applicatif HW : calibration, armement et activation restent à intégrer et à valider |
| Carte exécutant le firmware SIM | Client série disponible ; trajet UART réel non testé ; le script de flash sécurisé actuel refuse les images SIM |
| Gazebo / ROS 2 | Adaptateur logiciel fourni et logique hôte testée sans ROS ; intégration réelle non testée |
| Mesure de courant / protection électrique | Acquisition brute expérimentale, désactivée par défaut ; conversion en ampères et protection électrique non implémentées |

Le PID n’est pas introduit par ce changement : `motor_pid.c`, ses en-têtes et sa configuration existaient déjà. L’apport principal porte sur son intégration, les conditions dans lesquelles ses sorties sont autorisées et la boucle de simulation qui l’exerce réellement. FreeRTOS et EmbeddedProto existaient également dans la référence.

## 2. Démarrer sur PC

Depuis la racine du dépôt, avec Python 3 et GCC accessibles dans le `PATH` :

```sh
python tools/run_simulation.py
```

Cette commande compile du C de production pour le PC, lance le firmware natif et le modèle Python, puis exécute les scénarios. Elle ne programme aucune carte, n’ouvre aucun port série et ne lance pas Gazebo. Elle produit notamment :

- `build/simulation/results.json` : résultats et mesures par scénario ;
- `build/simulation/trace.csv` : traces de la simulation ;
- les exécutables et journaux de compilation sous `build/`.

Les résultats sont régénérés lors des exécutions suivantes. Conserver une copie des traces si une comparaison entre versions est nécessaire.

Pour une vérification ciblée :

```sh
python tools/run_simulation.py --case closed_loop
python tools/run_simulation.py --case feedback_loss
python tools/run_simulation.py --case complete_cut
```

Le compilateur natif peut être choisi via `HOST_CC`. Les vérifications avec sanitizers dépendent du compilateur et de son runtime. L’option `--no-sanitizers` les désactive explicitement ; un résultat sans sanitizer n’est pas une validation mémoire équivalente.

## 3. Architecture et cible matérielle

### Une carte pilote les huit moteurs

La configuration CubeMX passe de la cible STM32G474RET6 de l’ancienne configuration à une STM32G474QET6 en LQFP128. Le routage des broches, les initialisations et les périphériques sont adaptés à cette cible.

La séparation logicielle reste reconnaissable : application et intentions, contrôle moteur/PID, backend, BSP et communications. En revanche, **le fonctionnement à deux cartes n’est plus le chemin actif**. `board_link.c` conserve une interface de compatibilité, mais tous les identifiants moteurs valides sont locaux ; les opérations de liaison entre cartes n’assurent plus une distribution maître/esclave.

Il ne faut donc pas réutiliser les procédures de câblage ou de déploiement de l’ancienne architecture à deux Nucleo sans les réexaminer. Les fichiers et commentaires historiques encore présents ne sont pas tous représentatifs du chemin actif.

### Répartition des moteurs

Les identifiants logiques restent ceux de `motor_map`. Les changements concernent notamment leur réalisation matérielle et la validation des retours.

| ID | Moteur | PWM du backend HW | Encodeur configuré |
| --- | --- | --- | --- |
| 0 | THUMB | HRTIM TC1 / TC2 | TIM1, PC0 / PC1 |
| 1 | INDEX | HRTIM TA2 / TA1 | TIM8, PA15 / PB8 |
| 2 | MIDDLE | HRTIM TB2 / TB1 | TIM2, PD3 / PD4 |
| 3 | RING | HRTIM TF1 / TF2 | TIM4, PB6 / PB7 |
| 4 | LITTLE | HRTIM TE2 / TE1 | TIM20, PE2 / PE3 |
| 5 | WRIST_X | TIM15 CH1 / CH2 | TIM5, PA0 / PA1 |
| 6 | WRIST_Y | TIM16 CH1 / TIM17 CH1 | TIM3, PA6 / PA4 |
| 7 | PALM | HRTIM TD2 / TD1 | LPTIM1, PB5 / PC2 |

Cette table décrit le code. Elle ne vaut pas confirmation électrique de la carte assemblée, du sens moteur ou du sens encodeur.

Les modules HAL/CMSIS ont été ajustés, avec ajout des pilotes ADC, HRTIM, LPTIM et OPAMP nécessaires. Le BSP Nucleo et une ancienne copie de FreeRTOS sous `Middlewares/Third_Party/FreeRTOS/Source/` sont retirés. Le noyau utilisé reste dans `FreeRTOS-Kernel`. Des fichiers de pilotes USART synchrones sont supprimés ; cela ne signifie pas que le pilote UART asynchrone a disparu.

Le nom historique `STM32G474RETX_FLASH.ld` est conservé, malgré l’adaptation du contenu à la cible QETx. La carte mémoire déclarée comporte 512 Kio de flash et 128 Kio de RAM. Ne pas déduire la cible du seul nom du fichier linker ou du fichier startup.

## 4. Démarrage, horloges et arrêts fatals

La base de temps HAL est portée par TIM6. SysTick appartient au port FreeRTOS GCC Cortex-M4F : les deux chemins sont séparés pour éviter un double tick HAL ou des gestionnaires concurrents.

Les tests vérifient le démarrage de la base de temps, les échecs d’initialisation, les priorités et les gestionnaires effectivement présents dans les ELF. Les contrôles de build vérifient aussi les vecteurs SVC, PendSV, SysTick et TIM6 ainsi que les appels attendus.

Un nouvel arrêt fatal dans `Core/Src/fatal_stop.c` coupe les sorties par accès aux registres. Il ne dépend pas de l’ordonnanceur, d’une allocation mémoire ni de handles HAL complètement initialisés. Les exceptions graves, assertions et hooks FreeRTOS de débordement de pile ou d’échec d’allocation y sont raccordés.

Les tests couvrent l’arrêt avant initialisation complète, pendant l’activité des sorties, avec interruptions déjà masquées et lors d’appels répétés. Ils vérifient le comportement sur des registres simulés ; les niveaux électriques réels restent à mesurer.

## 5. Contrôle moteur et gestion des défauts

### Backend matériel et sorties

Le backend actif utilise `drv8848.c` et `motor_outputs.c`. Les six paires HRTIM et les deux poignets sont traités explicitement. Les deux entrées d’un pont sont commandées comme des entrées indépendantes, et non comme une paire complémentaire implicite.

Les transitions de sens préparent les sorties à l’état inactif avant d’appliquer la nouvelle commande. L’arrêt et la désactivation forcent les sorties à zéro. Les tests couvrent les huit mappings, les commandes nulles, les petites valeurs, les extrêmes, les inversions, le freinage et les configurations HRTIM non prises en charge.

Le câblage SLEEP/FAULT reste une hypothèse matérielle à confirmer. Le code utilise notamment PG0/PG2/PG4/PC4 pour SLEEP et PG1/PG3/PD12/PD10 pour FAULT. Les documents de carte ne suffisent pas à confirmer que ces connexions existent sur chaque révision assemblée.

### Calcul PID et validation avant application

`Motor_Update` exploite le temps réellement écoulé. La préparation du calcul utilise des instantanés ; les parties longues ne sont pas exécutées sous un masque global d’interruptions. Avant d’appliquer une sortie, le code revérifie les révisions et l’époque de sécurité associées à la commande.

Un STOP ou un défaut apparu pendant un calcul doit ainsi empêcher l’application de son résultat devenu obsolète. La file d’intentions transporte également ce contexte de validité. Les tests injectent des arrêts à plusieurs étapes, y compris juste avant la validation finale d’une sortie ou d’une émission.

Les protections déjà présentes dans le PID, comme les bornes numériques et le traitement de valeurs non finies, sont conservées. Les paramètres du profil SIM sont distincts des paramètres HW : les résultats du modèle virtuel ne constituent pas un réglage des gains pour les moteurs réels.

### Défauts verrouillés et reprise explicite

Les causes comprennent l’arrêt manuel, le blocage détecté par le guard, la perte de feedback, un retard de contrôle, la force excessive, un défaut driver, un défaut PID et la perte de liaison.

La coupure logique et des sorties est immédiate dans le chemin de sécurité ; le nettoyage HAL qui ne doit pas s’effectuer sous interruption est différé. Un propriétaire de nettoyage empêche une reprise concurrente tant que cette opération n’est pas terminée.

La reprise nécessite de lever explicitement le défaut, d’armer, d’activer les moteurs et d’émettre une nouvelle consigne. Une ancienne commande en file ne doit pas relancer le mouvement. Le retour de la communication ou du feedback ne suffit pas à réactiver les sorties.

| Paramètre logiciel | Valeur par défaut | Interprétation |
| --- | --- | --- |
| Âge maximal du feedback | 100 ms | Feedback trop ancien : inhibition |
| Intervalle maximal du contrôle | 50 ms | Retard excessif : défaut |
| Période du guard | 20 ms | Surveillance distincte du calcul PID |
| Fenêtre de blocage HW | 180 ms | Seuil de déplacement de 6 comptes encodeur |
| Fenêtre de blocage SIM | 250 ms | Seuil de 0,001 rad, sortie supérieure à 0,5 % |
| Force logicielle, seuil souple | 1 200 mN | Réduction de commande lorsqu’un feedback de force valide existe |
| Force logicielle, seuil dur | 1 600 mN ; poignets HW 2 000 mN | Arrêt selon le profil |

Ces valeurs sont des choix logiciels, pas des limites mécaniques ou électriques validées. Les tâches restent dépendantes du microcontrôleur et de son ordonnanceur : il ne s’agit pas d’une chaîne de sécurité matérielle indépendante.

### Pourquoi la version HW ne fait pas encore tourner les moteurs

Les APIs d’armement, d’activation et de calibration sont présentes, mais aucun parcours applicatif HW normal ne les raccorde à une procédure complète utilisable par l’opérateur. Les vérifications d’ELF confirment notamment l’absence de chemins actifs d’armement et d’activation dans l’image HW par défaut.

Une intention EMG reçue ne remplace donc ni la calibration ni l’armement. Cette version ne doit pas être présentée comme une démonstration de mouvement réel prête à l’emploi. L’initialisation de certains périphériques a néanmoins lieu : « désarmé » ne signifie pas « aucune broche ni aucun timer configuré ».

## 6. Encodeurs et acquisition analogique

### Encodeurs

Le BSP démarre les encodeurs une seule fois après une initialisation réussie. Il accumule les différences de compteurs 16 bits dans une valeur 64 bits, avec traitement du bouclage. Une remise à zéro logique ne redémarre pas le timer.

La calibration est explicite : résolution, quadrature, réduction, direction et offset. Une conversion en radians invalide n’est plus remplacée par une hypothèse de conversion silencieuse. Une calibration ou une remise à zéro est refusée lorsqu’un moteur est actif ; une modification invalide le feedback précédent.

Des tests ciblent les produits numériques extrêmes, les offsets, les différences et accumulations 64 bits, les erreurs persistantes ainsi que le passage du vrai BSP au backend HW avec HAL simulée.

**Limite :** l’accumulation repose sur la différence modulaire entre deux lectures. Un déplacement trop grand entre ces lectures peut être interprété dans le mauvais sens ou perdre des tours. Les tests de bouclage ne garantissent pas la détection de toutes les pertes de tours à vitesse élevée ou après une longue interruption des lectures.

### ADC, DMA et OPAMP

L’acquisition de courant est désactivée par défaut : `N_PULSE_ENABLE_CURRENT_ACQUISITION=0`. Une compilation expérimentale peut activer le démarrage des OPAMP et des cinq ADC en DMA.

Le mapping brut corrigé est le suivant :

| Moteur | Échantillon utilisé | Entrée associée |
| --- | --- | --- |
| INDEX | ADC1, rang 1 | PA3 / OPAMP1 |
| MIDDLE | ADC2, rang 1 | PA7 / OPAMP2 |
| RING | ADC2, rang 2 | PB0 / OPAMP3 |
| LITTLE | ADC5, rang 1 | PD11 / OPAMP4 |
| THUMB | ADC3, rang 1 | PE13 / ADC3_IN3 |
| PALM | ADC4, rang 1 | PE14 |
| WRIST_X | ADC5, rang 2 | PC3 / OPAMP5 |
| WRIST_Y | ADC4, rang 3 | PD9 / OPAMP6 |

ADC4 rang 2, PE15, correspond à `Batt_lvl` et n’est pas utilisé comme courant moteur. ADC3 participe désormais au démarrage et au nettoyage en cas d’échec. Les tests injectent un échec à chaque démarrage ADC et vérifient le nettoyage des cinq flux DMA.

Les interruptions DMA de demi-transfert et de transfert terminé sont désactivées pour cette acquisition brute ; les données sont lues dans les buffers. La conversion `ADC_DMA_BSP_GetCurrentA` renvoie `NaN` : aucune mesure en ampères n’est revendiquée sans calibration.

**Limites :** `GetRaw` peut renvoyer zéro lorsque l’acquisition est inactive, sans fournir à lui seul une validité et une fraîcheur d’échantillon. L’acquisition expérimentale, les gains analogiques, les offsets et la polarité ne sont pas validés sur carte. Aucun retour HW de force calibré ni protection physique contre les surintensités n’est démontré.

## 7. Communications : UART4, protocole HW et protocole SIM

### Transport commun

UART4 sur PC10/PC11, à 115 200 bauds, 8N1, devient le chemin actif de communication. Le transport utilise le handle HAL prévu pour ce périphérique. Il ne faut pas supposer que le port USB/VCP de l’ancienne Nucleo donne accès à ces broches sur la nouvelle carte.

La réception interruptible alimente un buffer borné de 256 octets. La tâche de communication attend les notifications plutôt que de monopoliser le CPU dans une boucle active. La priorité du contrôle reste supérieure à celle des communications.

L’émission a un propriétaire unique et une file bornée de huit trames complètes. Les attentes de fin d’émission sont bornées et se font hors des sections critiques longues. Une erreur d’abandon HAL met le buffer persistant en quarantaine pour éviter sa réutilisation pendant un transfert potentiellement encore actif.

Les tests couvrent débordements, fragmentation, bruit, délais, erreurs HAL, callbacks tardifs, saturation et invalidation des messages après STOP. Les octets de debug bruts ne doivent plus être mélangés au flux protocolaire actif.

### Mode HW

Le protocole reste basé sur le protobuf existant, encapsulé par `0xAA`, une longueur sur un octet et un payload de 1 à 128 octets. Aucun CRC n’est ajouté à cette enveloppe HW. Le schéma protobuf et les bindings générés ne sont pas fonctionnellement modifiés.

La session exige un Hello compatible, puis surveille la présence du pair. Le délai de liaison est de 2 000 ms par défaut. Les intentions admises sont bornées et rattachées à une époque de session/sécurité. Une liaison perdue déclenche un défaut verrouillé.

Les cinq intentions publiques existantes sont OPEN, CLOSE, PINCH, ROTATE_R et ROTATE_L. Une intention interne supplémentaire ne constitue pas automatiquement une nouvelle commande du protocole.

`tools/uart4_hw_client.py` est un client de maintenance explicite : Hello périodique et, facultativement, envoi d’une action. Il ne calibre pas, n’arme pas et ne rétablit pas un moteur après défaut. Son affichage « envoyé » ne prouve pas qu’une action a été acceptée ou exécutée.

### Mode SIM

Le protocole SIM v2 utilise une enveloppe propre, un CRC16, une session, une séquence et des informations de temps. Les commandes portent sur la configuration, la récupération après défaut, l’armement, l’activation, les consignes, l’arrêt et les retours du modèle. Le firmware renvoie notamment état et sorties calculées.

Les identifiants moteurs sont explicites. Les champs de position, vitesse, force et effort appliqué sont accompagnés d’informations de validité : une donnée absente ne doit pas être interprétée comme une mesure nulle. Les paquets sont vérifiés avant d’appliquer les modifications ; les retours partiellement invalides sont rejetés sans application partielle.

Les deux protocoles sont choisis à la compilation. Il n’y a pas d’autodétection HW/SIM sur un même firmware. Un client protobuf HW ne remplace pas le client SIM v2.

La session et les séquences rejettent des commandes obsolètes pendant l’exécution ; ce n’est pas un mécanisme d’authentification, ni une protection persistante contre le rejeu après redémarrage.

## 8. Simulation : ce qui a changé et ce qu’elle prouve

### Boucle fermée avec le C de production

La simulation n’envoie plus seulement une consigne abstraite à un visualiseur. Le contrôle C calcule une sortie signée, le modèle l’applique et renvoie un état mesuré qui alimente le contrôle suivant.

Le firmware natif s’exécute dans un processus séparé. Le modèle de référence représente huit rotors indépendants, avec un pas de 10 ms et une dynamique simplifiée : `v += (0.2 * u - 4 * v) * 0.01`, puis `x += v * 0.01`, où `u` est une commande en pourcentage. Le profil SIM borne les positions à ±2 rad et la variation de référence à 1 rad/s.

Ce modèle est fictif. Il ne représente pas la cinématique de la main, les transmissions, les butées, les frottements réels, la batterie ou l’électronique de puissance. Une convergence sur ce modèle ne valide pas les gains sur le robot.

### Coupures et watchdog du modèle

Le protocole transporte des autorisations à durée limitée et des données fraîches. Le profil SIM prévoit notamment une sortie toutes les 20 ms, un état toutes les 40 ms, une session à 150 ms, un âge maximal d’échantillon de 50 ms et une autorisation maximale de 80 ms.

Le serveur de modèle `tools/sim_model_server.py` est un processus UDP local indépendant du client série, avec sa propre horloge et son watchdog de commande de 100 ms. Si le client se bloque ou meurt, le modèle doit inhiber l’effort sans attendre que le client exécute son propre code d’arrêt.

Après expiration d’une session, une requête de diagnostic de la même session et du même correspondant peut encore lire l’état. Elle ne remet pas `live` à vrai et ne réarme pas un mouvement expiré. Ce comportement est couvert par les tests hôte.

Il s’agit d’une indépendance entre processus logiciels sur PC, pas d’une protection contre la panne du PC entier.

### Essai manuel avec firmware SIM sur carte

Ce parcours nécessite un firmware SIM déjà programmé par une procédure adaptée et une liaison UART4 confirmée. Il n’a pas été testé physiquement dans les éléments examinés.

Prérequis Python pour l’accès série :

```sh
python -m pip install pyserial
```

Dans un premier terminal :

```sh
python tools/sim_model_server.py --port 48160
```

Dans un second terminal, remplacer `COM_CONFIRME` par le port identifié :

```sh
python tools/sim_client.py --port COM_CONFIRME --model-port 48160
```

Attendre le Hello ACK et vérifier `status`. Envoyer les commandes une par une, en contrôlant les réponses : `config`, `recover`, `arm`, puis, une fois les retours frais disponibles, `enable 0xff`. Une commande telle que `target 0 0.5 60` demande 0,5 rad pour THUMB avec une limite de sortie de 60 %. `stop` arrête ; `reconnect` ouvre une nouvelle session. Ne pas coller cette séquence sans observer l’état : une commande mise en file n’est pas encore un acquittement.

## 9. Gazebo et ROS 2

`tools/ros2_bridge/serial_bridge.py` est désormais un adaptateur d’effort SIM v2. Il utilise la logique commune de `tools/sim/` et demande explicitement :

- le port série ;
- les huit noms de joints, dans l’ordre des IDs moteurs 0 à 7 ;
- huit facteurs de conversion effort/par pourcentage ;
- le topic de commande d’effort ;
- un topic horodaté de l’effort réellement appliqué ;
- la durée du watchdog indépendant du contrôleur/modèle ;
- éventuellement huit topics de force.

Pour consulter les arguments :

```sh
python tools/ros2_bridge/serial_bridge.py --help
```

Le retour de l’effort appliqué ne doit pas être remplacé par une simple copie de la commande publiée. Les noms de joints, les unités et la fraîcheur des messages sont contrôlés. Les timestamps ROS sont convertis vers les délais utilisés par l’adaptateur.

**Ce qui manque encore :** le dépôt ne livre pas, dans ces changements, un plugin Gazebo complet assurant le watchdog et l’inhibition indépendante des efforts. Le paramètre `--model-watchdog-ms` exprime une exigence ; il ne crée pas ce mécanisme. La configuration du robot, du contrôleur d’effort, des topics et des facteurs de conversion reste à fournir ou à raccorder au projet Gazebo de l’équipe.

Aucun test réel ROS 2/Gazebo n’a été exécuté. Les pauses, resets et particularités de l’horloge simulée restent à vérifier. Il serait incorrect d’annoncer une intégration Gazebo terminée à partir des seuls tests Python de l’adaptateur.

## 10. Compiler et choisir la bonne image

### Prérequis

Pour le firmware ARM : GNU Make, la chaîne GNU Arm Embedded (`arm-none-eabi-gcc`, `g++`, `objcopy`, `objdump`, `nm`, `size`) et Python. Les scripts `.ps1` nécessitent PowerShell. STM32CubeProgrammer n’est nécessaire que pour le parcours de programmation.

Le build est piloté par le `Makefile` racine et `tools/firmware.mk`. `Debug/makefile` est maintenant un wrapper de compatibilité vers ce build. Les anciens fragments de build générés sous `Debug/` ne constituent plus la définition de référence des sources.

### Commandes

Sous PowerShell :

```powershell
.\build.ps1 -Mode HW -Rebuild
.\build-sim.ps1 -Rebuild
```

Avec GNU Make :

```sh
make SIM_MODE=0
make SIM_MODE=1
```

| Mode | ELF de référence | Copie de compatibilité |
| --- | --- | --- |
| HW | `build/HW/n-pulse.elf` | `Debug/N-PULSE FIRMWARE.elf` |
| SIM | `build/SIM/n-pulse.elf` | `Debug/SIM/N-PULSE FIRMWARE.elf` |

Les objets des deux configurations sont isolés. Le build SIM ne doit pas écraser l’alias HW. Les options de compilation sont enregistrées et un manifeste lie l’image à ses empreintes. Un mode autre que 0 ou 1 est refusé.

L’acquisition analogique expérimentale peut être compilée séparément :

```sh
make SIM_MODE=0 CURRENT_ACQUISITION=1
```

Elle produit une configuration distincte sous `build/HW-current/`. Ce n’est pas le mode recommandé pour une première prise en main ; sa compilation ne prouve pas le bon fonctionnement analogique. Le script de programmation fourni refuse cette configuration expérimentale.

### Programmation HW

Le nouveau `flash.ps1` exige un numéro de série ST-LINK exact et n’accepte que l’image HW canonique, avec manifeste et empreintes cohérents. Il vérifie le type ELF ARM et les options attendues, refuse SIM et les alias historiques, puis sélectionne une sonde sans ambiguïté.

Après compilation HW et identification du numéro de série :

```powershell
.\flash.ps1 -SerialNumber NUMERO_EXACT -DryRun
```

`-DryRun` énumère les sondes et vérifie la sélection, mais n’effectue ni connexion SWD de programmation, ni écriture, ni reset. Ce n’est donc pas une commande entièrement indépendante du matériel ou de CubeProgrammer. `-ProgrammerPath` permet d’indiquer explicitement son exécutable.

Lorsque la carte et le montage sont confirmés, la commande de programmation est :

```powershell
.\flash.ps1 -SerialNumber NUMERO_EXACT
```

Elle écrit, vérifie et redémarre la cible. Aucun flash réel n’a été effectué pour cette revue.

**Attention aux tâches VS Code existantes :** `.vscode/tasks.json` contient encore un appel direct à CubeProgrammer utilisant l’ancien alias et sans sélection sécurisée par numéro de série. La tâche « Build SIM + Flash » peut ainsi compiler SIM puis tenter de programmer l’alias HW. Ne pas utiliser ces tâches pour ce parcours ; elles restent à mettre en cohérence avec les scripts actuels.

### Diagnostic UART HW

Avec l’image HW, la liaison UART4 identifiée et l’alimentation moteur coupée :

```sh
python -m pip install pyserial protobuf
python tools/uart4_hw_client.py --port COM_CONFIRME --duration 10
```

Ce client permet d’entretenir un Hello et d’observer les réponses. À son arrêt, la session doit finir par déclencher le défaut LINK. La reprise après ce défaut n’est pas exposée par ce client HW. Cette procédure reste à confirmer sur UART physique.

## 11. Tests ajoutés et résultats

Les suites nouvelles compilent des parties du code C de production sur PC, avec des doubles HAL/BSP/RTOS lorsque nécessaire. Les tests d’intégration ELF complètent cette couverture. Ni l’un ni l’autre ne mesure directement le comportement électrique d’une carte.

| Suite / commande | Ce qu’elle couvre | Résultat de cette revue |
| --- | --- | --- |
| `python tests/startup/run_tests.py` | Timebase, encodeurs, calibration, bornes numériques, ADC inactif par défaut, mapping brut et nettoyage sur échec | 15 scénarios ciblés réussis ; contrôles numériques supplémentaires inclus |
| `python tests/safety/run_tests.py` | Défauts, STOP, reprise, calculs obsolètes, concurrence guard/contrôle, désarmement et intégration encodeur | Réussite ; 24 scénarios principaux, rejoués aussi avec le vrai backend HW et HAL/BSP simulés, plus tests complémentaires |
| `python tests/comms/run_tests.py` | Parser, session, files, saturation, émission interruptible, abort, callbacks tardifs et validation SIM | 21 scénarios communication réussis, matrice de 24 576 couples taille/compte, 100 000 octets de bruit et tests hôte |
| `python tests/outputs/run_tests.py` | Arrêts fatals, assertions, mappings PWM et script de flash dans une fixture | Huit groupes C réussis ; suite globale incomplète ici : `powershell.exe` absent pour le test du script |
| `python tools/run_simulation.py` | Boucle fermée C/Python et fautes de transport/modèle | 26 scénarios réussis |

Les tests de flash utilisent un faux exécutable de programmation et une image factice. Leurs succès ne signifient pas qu’une carte a été programmée. Sur Linux, la suite appelle actuellement `powershell.exe` explicitement : l’absence de cet exécutable fait échouer la suite, même si les tests C précédents passent.

### Scénarios SIM couverts

Les 26 scénarios couvrent : convergence en boucle fermée ; démarrage/reconnexion ; routage partiel ; perte de feedback ; perte firmware→hôte ; perte hôte→firmware ; coupure complète ; pause du processus hôte ; paquets malformés ; fragmentation ; débordement RX ; STOP sous saturation ; STOP pendant émission ; timeout TX ; expiration TX ; erreur HAL TX ; échec d’abandon TX ; échec de réarmement RX ; retard de contrôle et force ; perte de force ; guard SIM ; budget de latence ; feedback tardif ; perte de paquets ; bouclage de séquence/temps ; rejet atomique d’un feedback invalide.

Les tests hôte couvrent aussi le codec, le mapping, les champs partiels, les valeurs non finies, l’écrivain unique avec plusieurs producteurs et le serveur de modèle dans un processus distinct. Ils n’ouvrent pas de port série et ne lancent pas ROS.

### Mesures reproduites sur le modèle PC

| Mesure | Résultat |
| --- | --- |
| Erreur maximale finale du scénario `closed_loop` | Environ 0,000120 rad |
| Perte firmware→hôte | Inhibition locale en 100 ms ; distante en 120 ms |
| Perte hôte→firmware | Inhibition locale en 80 ms ; distante en 90 ms |
| Coupure des deux sens | Inhibition locale en 80 ms ; distante en 120 ms |
| Budget de transport testé | Environ 7 652 octets/s vers le firmware et 3 467 octets/s au retour, avec 10 ms de délai ajouté par sens |

« Locale » désigne ici le côté firmware, « distante » le côté hôte/modèle, conformément au scénario de test. Ces délais sont des résultats du banc logiciel et de ses pas de simulation ; ils ne sont pas des bornes garanties sur UART réel ni des temps d’arrêt mécanique.

### Builds et analyses ELF

Après avoir compilé les deux modes, les commandes disponibles sont :

```sh
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/outputs/verify_build.py
python tests/startup/test_build_modes.py
```

Ces scripts vérifient notamment les gestionnaires réellement liés, les chemins d’arrêt, les dépendances backend, l’absence de chemins physiques actifs dans SIM et l’état HW désarmé par défaut. Le test d’alternance vérifie l’isolation des objets et la conservation de l’alias HW.

Les résultats enregistrés dans l’archive indiquent des succès pour les builds HW/SIM, ces contrôles et l’alternance, avec 140 objets isolés inchangés. La compilation ARM du mode ADC expérimental est également enregistrée comme réussie.

| Image enregistrée | `text` | `data` | `bss` |
| --- | ---: | ---: | ---: |
| HW | 90 620 octets | 384 octets | 31 408 octets |
| SIM | 78 972 octets | 96 octets | 31 848 octets |

Ces chiffres viennent du build fourni, pas d’une reconstruction effectuée pendant cette revue. Le compilateur ARM n’était pas disponible dans l’environnement de revue. Ces tailles ne mesurent pas la marge effective des piles FreeRTOS en fonctionnement.

### Sanitizers et dépendances absentes

Dans l’environnement de revue, UBSan a pu être exécuté avec succès sur les chemins numériques/intégration concernés et le firmware/parser SIM natif. Le probe combiné ASan/UBSan n’a pas fonctionné ; aucune réussite ASan n’est revendiquée. Certaines suites annoncent encore un skip de leur vérification combinée.

Le codec Python HW optionnel a été sauté faute de `protobuf` dans cet environnement. Les résultats Windows fournis mentionnent eux aussi des sanitizers indisponibles et ce codec optionnel sauté. Ces absences ne doivent pas être comptées comme des tests réussis.

## 12. Travail restant et limites à connaître

| Priorité d’usage | Reste à faire | Conséquence actuelle |
| --- | --- | --- |
| Première carte | Confirmer révision, alimentation, masses, UART4, PWM, SLEEP/FAULT et brochage encodeurs sur le matériel assemblé | Les tables logicielles seules ne prouvent pas le câblage |
| Premier démarrage HW | Programmer une carte identifiée, vérifier clocks/ticks/RTOS, niveaux de sortie au boot, interruptions et UART sans puissance moteur | Aucun résultat physique n’est encore disponible |
| Premier moteur | Définir calibration, sens, course et plafond PWM ; intégrer une procédure explicite de récupération/armement/activation | Le parcours HW actuel ne permet pas une démonstration de mouvement normale |
| Protection électrique | Définir et vérifier la limitation électrique externe ; calibrer et qualifier la mesure de courant si elle doit être utilisée | Les seuils logiciels de force ne remplacent pas une protection de courant |
| Gazebo | Raccorder robot et contrôleur d’effort, fournir le watchdog indépendant et le retour d’effort appliqué, puis essayer les coupures | L’adaptateur seul ne suffit pas à livrer une simulation Gazebo complète |
| Outils de l’équipe | Mettre à jour les tâches VS Code et les procédures historiques encore incompatibles | Risque de choisir une mauvaise image ou un ancien parcours de flash |
| Qualité et portabilité | Exécuter ASan dans un environnement compatible, tester le codec protobuf HW, rendre les tests PowerShell portables si nécessaire | Couverture partielle selon la machine de développement |

Quelques points techniques méritent aussi un suivi :

- Les seuils de watchdog, les périodes RTOS, la latence UART et les marges CPU/pile doivent être mesurés sur STM32 sous charge réelle.
- `Motor_SetTargetAtEpoch` renvoie un booléen lié à la validité de l’époque/sécurité ; il ne faut pas l’interpréter comme une preuve universelle qu’une nouvelle consigne est devenue effective. Tout futur acquittement HW doit préciser cette sémantique.
- Les acquisitions analogiques brutes ont besoin d’un contrat explicite de validité/fraîcheur avant d’alimenter une protection.
- Les tests d’injection de concurrence utilisent un environnement contrôlé ; ils ne reproduisent pas toutes les interactions possibles du matériel et de FreeRTOS.
- Les sources legacy encore présentes, les commentaires et certains guides doivent être lus en tenant compte du build actif. La présence d’un ancien driver ou d’un ancien script ne signifie pas qu’il est utilisé.

## 13. Repères dans le dépôt

| Zone | Rôle dans cette version |
| --- | --- |
| `Core/`, fichier `.ioc`, linker | Cible, init périphériques, interruptions, timebase et arrêt fatal |
| `fw/app/` | Réception et routage des intentions avec contexte de sécurité |
| `fw/motors/motor_control/` | PID existant, contrôle, feedback, guard, défauts, backends et sorties |
| `fw/bsp/encoder/` | Comptage, accumulation et calibration encodeurs |
| `fw/bsp/adc_dma/` | Acquisition brute optionnelle et mapping analogique |
| `fw/comm/` et `fw/comm-stack/` | Transport UART4, parsers, sessions et compatibilité protobuf |
| `tools/sim/` | Codec SIM, pair hôte, modèle, watchdog et adaptation ROS |
| `tools/ros2_bridge/` | Entrée ROS 2 vers le transport SIM |
| `tests/` | Régressions natives, mocks, scénarios et contrôles d’ELF |
| `tools/firmware.mk`, `tools/build_artifacts.py` | Build maintenu, séparation des modes et publication des artefacts |

Pour approfondir une utilisation précise : `docs/SIMULATION_QUICKSTART.md`, `docs/sim_transport.md`, `docs/CALIBRATION.md`, `docs/wiring.md`, `docs/TESTING.md`, `MOTORS.md` et `FONCTIONNEMENT_FIRMWARE.MD`. Les comportements décrits ici ont été confrontés au code actuel ; les anciennes instructions doivent être vérifiées lorsqu’elles concernent les deux cartes, le flash ou le câblage.
