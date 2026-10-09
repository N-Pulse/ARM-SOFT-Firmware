> **Addendum du 9 octobre 2026 — lot 5 :** ce rapport conserve les résultats
> historiques. [ROBUSTNESS_LOT5](ROBUSTNESS_LOT5.md) précise désormais le contrat
> de désarmement public, la sérialisation HAL, l'engagement UART4 IT et le reset
> encodeur. « Aucun UART sous Motor_Lock » est remplacé par une exception bornée
> pour le démarrage IT effectif, sans attente ; l'ancienne vérification d'epoch
> avant ce démarrage ne suffisait pas. Le flash maintenu exige un SN explicite ;
> RX SIM reste déconnectée et aucun premier mouvement n'est activé.

# Lot 4 — coupure fatale, sorties PWM et cible de flash

État au 8 octobre 2026. Ce lot implémente des corrections logicielles ; aucune
carte n'a été connectée, programmée ou resetée par l'agent. Les moteurs restent
désarmés et l'ADC désactivé dans les configurations par défaut. La compilation
et les tests sur registres simulés ne prouvent ni le démarrage sur carte, ni
les niveaux électriques ou les formes d'onde réelles.

Les instructions disponibles, l'état Git et les rapports
[STARTUP_LOT1](STARTUP_LOT1.md), [SAFETY_LOT2](SAFETY_LOT2.md) et
[COMMS_LOT3](COMMS_LOT3.md) ont été lus. Aucun `AGENTS.md` applicable n'a été
trouvé. Les changements antérieurs, fichiers non suivis et modifications
indexées sont conservés. Aucun commit, push, reset Git ou nettoyage destructif
n'a été exécuté. Les reconstructions suppriment seulement les artefacts générés
de leur configuration, selon le helper GNU Make déjà utilisé.

## Constats vérifiés et coupure fatale

`Error_Handler`, HardFault, MemManage, BusFault, UsageFault et NMI bouclaient
sans inhibition matérielle. `configASSERT` masquait les interruptions puis
bouclait. Les hooks malloc/stack appelaient `Error_Handler`, mais leurs options
FreeRTOS étaient absentes : le noyau les désactivait par défaut. Le handler
par défaut des interruptions non implémentées bouclait également.

`fatal_stop.c` fournit maintenant une seule voie terminale. Elle masque les
interruptions, active uniquement les horloges GPIOC/G nécessaires, écrit les
BSRR pour abaisser PG0/PG2/PG4 et PC4, puis configure ces seules broches en
sorties push-pull, sans pull. Le latch bas précède le changement de mode. Les
autres bits GPIO et RCC sont préservés. Ce mapping SLEEP est celui du dépôt ;
aucune nouvelle association physique n'est introduite.

Après SLEEP bas, les périphériques dont l'horloge APB2 est active sont inhibés :
ODISR désactive les douze sorties HRTIM et les compteurs A..F sont arrêtés ;
MOE, les enables CCER utilisés et CEN sont abaissés sur TIM15/16/17, couvrant
les deux axes du poignet. Les adresses CMSIS fixes remplacent tout accès aux
handles HAL dans cette voie. Un périphérique à horloge désactivée n'est pas
touché. Le readback RCC et les barrières DSB n'utilisent aucune boucle de
polling. Il n'y a aucun appel HAL, RTOS, UART, mutex, allocation ou délai.

La cause est écrite **après** la coupure dans `g_n_pulse_fatal_cause`, puis
`N_PULSE_FatalHalt` ne revient jamais. La RAM est remise à zéro au reset ; ce
n'est pas un journal persistant ni une analyse complète des registres de fault.

| Valeur | Point d'entrée |
|---:|---|
| 1 | Error_Handler |
| 2..5 | HardFault, MemManage, BusFault, UsageFault |
| 6 | configASSERT |
| 7 | allocation FreeRTOS échouée |
| 8 | overflow de pile FreeRTOS |
| 9 | NMI |
| 10 | assert HAL, si USE_FULL_ASSERT est activé |
| 11 | interruption non implémentée / Default_Handler |

`configUSE_MALLOC_FAILED_HOOK=1` et `configCHECK_FOR_STACK_OVERFLOW=2` rendent
les hooks effectifs. Le diagnostic stack concerne les tâches aux points de
contrôle du noyau ; il ne protège pas à lui seul la pile MSP des exceptions.
Les faults désactivés peuvent escalader en HardFault : la cause indique le
handler exécuté, pas nécessairement l'origine électrique ou mémoire.

Cette voie C demande encore un CPU, une pile, des bus et des GPIO capables
d'exécuter les écritures. Une pile corrompue, un lockup, un bus bloqué, une
perte d'alimentation ou une panne externe peuvent empêcher la coupure. Aucun
arrêt matériel indépendant du CPU n'est prétendu. Les niveaux TIM après
désactivation peuvent dépendre de l'off-state configuré ; SLEEP bas reste
l'inhibition principale suivant le mapping actuel, à confirmer sur la carte.
Cette coupure est locale : aucun STOP distant UART/SIM n'est émis depuis une
voie fatale, et aucun arrêt du simulateur distant n'est garanti par ce lot.

## Références ST examinées

Référence applicable : [RM0440, révision 9](https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

- §28.3.4, table 232, pages 892–893 : DIV1/CKPSC=5 autorise les comparaisons
  ordinaires de 3 à 0xFFFD ; une comparaison supérieure à PER ne produit pas
  d'événement. L'exception zéro concerne CMP1/CMP3 et des sources précises.
- §28.3.7, pages 922–925 : distinguer l'arbitrage interne CMP4 > CMP3 > CMP2 >
  CMP1 > PER, RESET entre sources distinctes en haute résolution, et le cas
  CKPSC≥5 où RESET suivi de SET dans le même cycle peut retenir le dernier
  événement. « RESET gagne toujours » serait une simplification incorrecte.
- §28.5.28–31, §28.5.56–57 et §28.5.62 : SST/SRT logiciels non préchargés,
  suspension TxUDIS, transfert immédiat TxSWU, désactivation vers l'état idle.
- Chapitre 31, TIM15/16/17 : modes OC forcés, contrôle CCPC et preload CCR.

[AN4539, révision 5](https://www.st.com/resource/en/application_note/an4539-hrtim-cookbook-stmicroelectronics.pdf),
notamment le preload et le software update pages 16–17 et les exceptions
de rapport cyclique en annexe, complète ces règles. La
[fiche STM32G474xE, DS12288](https://www.st.com/resource/en/datasheet/stm32g474qe.pdf)
confirme les capacités mémoire du composant. Les règles ci-dessus sont des
faits documentaires ; leur application au banc reste à mesurer.

## Comportement logiciel des sorties

Le défaut initial était démontrable : écrire CMP2=0 ne signifie pas LOW.
De même, CMP=PER à 100 % introduisait des événements concurrents. Les deux
comparaisons étaient écrites successivement sans transaction cohérente et le
commentaire promettait une prévention de shoot-through non établie. Sur les
TIM classiques, multiplier par 7999 ne représentait pas les 8000 comptes
de la période ARR=7999.

Le driver conserve sa table de canaux et utilise `motor_outputs.c`. Les
pourcentages non finis passent toujours par l'interlock du lot 2 ; les sorties
directes restent refusées si le moteur n'est pas armé/autorisé. Les demandes
finies sont bornées à ±100 comme auparavant. Une configuration périphérique
incompatible provoque `Error_Handler`, donc la coupure, plutôt qu'une écriture
hors domaine. Les petits ordres quantifiés à LOW donnent désormais une
direction effective nulle au guard, évitant de déclarer un entraînement absent.

Dans le tableau, A/B désignent les deux **entrées MCU suivant le mapping
existant**, avec polarité active haute. Aucun sens mécanique n'est déduit.

| Demande | Entrée A attendue | Entrée B attendue |
|---|---|---|
| zéro / Stop | LOW | LOW |
| positive partielle représentable | PWM | LOW |
| négative partielle représentable | LOW | PWM |
| +100 % / saturation positive | HIGH constant | LOW |
| -100 % / saturation négative | LOW | HIGH constant |
| Brake, API historique | HIGH constant | HIGH constant |
| inversion de sens | passage des deux entrées à LOW pendant la préparation, puis nouvel état | même transaction |

Le nom Brake ne valide pas l'effet de freinage du montage externe. Son état
logique historique est conservé ; il n'a pas de nouveau caller actif. Aucun
chemin d'armement, d'activation UART ou de calibration n'a été ajouté.

Pour HRTIM A..F :

1. ODISR tient les deux sorties à l'idle configuré inactif, et le compteur de
   la paire est arrêté. Les autres compteurs ne sont pas modifiés.
2. PREEN est activé par le propriétaire logiciel des sorties. TxUDIS suspend
   les transferts pendant la préparation. Un premier TxSWU retire les anciens
   événements, puis SRT remet explicitement les deux latches à zéro.
3. Les deux comparaisons et sources sont préparées ensemble, puis transférées
   par un second TxSWU. À zéro, aucun SET n'est sélectionné et CMP1/CMP2 sont
   parqués à 3. À 100 %, SST force HIGH, PER maintient cet état, aucun RESET
   n'est sélectionné et la comparaison parquée vaut PER+1.
4. Le compteur est repositionné à zéro. Seuls ses bits CEN antérieurement
   actifs et les sorties antérieurement activées peuvent être restaurés ; la
   restauration des sorties est en plus réservée au driver déjà armé.

La période est lue dans PER, actuellement 7999, avec le DIV1 existant. Les
ordres produisant moins de 3 comptes sont traduits en LOW. La plage partielle
retenue est [3, PER−3] ; les valeurs très proches de 100 % sont bornées à
PER−3, puis 100 % utilise l'état constant. Cette quantification prudente est
un choix logiciel pour écarter les événements trop proches, sans calibration
ni durée de commutation électrique. Elle peut créer une discontinuité aux
extrêmes et doit être évaluée au banc.

Pour TIM15 et TIM16/17 : les compteurs concernés sont arrêtés, CCPC est
désactivé et les deux modes OC sont d'abord forcés inactifs. Les CCR utilisent
ARR+1, sont préchargés puis transférés par UG avant de sélectionner PWM1 ou
les états forcés LOW/HIGH. Aucun enable CCER/MOE n'est ajouté par cette
fonction. Les compteurs initialement arrêtés le restent. Les écritures entre
TIM16 et TIM17 sont ordonnées, mais ne promettent pas des fronts simultanés
entre deux périphériques indépendants.

Ces transactions restent sous `Motor_Lock` et ne contiennent que calculs et
écritures bornées, sans UART ni attente. La phase est réinitialisée lors des
mises à jour ; le début du premier PWM HRTIM peut attendre le prochain PER.
Des pauses/transitoires de mise à jour sont donc possibles. Aucun dead time,
temps de commutation, synchronisme inter-timers ou absence de shoot-through
électrique n'est revendiqué.

Au démarrage, `main.c` et le `.ioc` sélectionnent désormais aucune source
SET/RESET et des comparaisons à 3. Le preload initial Cube reste désactivé ;
le driver le prend explicitement en charge avant chaque transaction. Il ne
suffit pas de réactiver PREEN sans conserver les TxSWU du propriétaire.

## Flash à sélection explicite

`flash.ps1` ne parcourt plus les sondes pour les programmer. Le paramètre
`-SerialNumber` est requis et le résultat de `-l` doit contenir exactement
une occurrence de ce numéro, avec comparaison exacte. Un numéro absent,
un préfixe, une liste en erreur ou un numéro répété sont refusés. Si la version
du CLI répète le même SN, le script refuse prudemment la sélection ; aucune
déduplication silencieuse ne choisit une cible.

Seul `build/HW/n-pulse.elf` est accepté, avec `compile.flags` et le manifeste
généré par la publication GNU Make. Mode HW, ADC=0, absence de définition SIM,
SHA-256 de l'ELF et des flags, et en-tête ELF ARM sont contrôlés. Les chemins
SIM, alias Debug, substitutions ou manifests incohérents sont rejetés. Ce
manifeste vérifie la cohérence des artefacts générés ; ce n'est pas une
signature cryptographique contre une modification volontaire du manifeste.

Le mode normal prépare une commande unique avec `sn=<SN>`, fichier canonique,
écriture, vérification `-v` et reset `-rst`. Les codes de sortie et les lignes
`Error:` de l'outil sont contrôlés. `-DryRun` affiche exactement cible,
fichier et arguments, puis s'arrête après l'énumération, sans `-c`, `-w` ni
`-rst`. Le chemin du programmeur est configurable pour le banc simulé.

Exemple destiné à une utilisation ultérieure, **non exécuté avec l'outil réel** :

```powershell
.\flash.ps1 -SerialNumber NUMERO_EXACT -DryRun
```

Les tests utilisent une copie du script dans une fixture isolée et un petit
exécutable C qui ne fait qu'afficher des réponses et enregistrer les arguments.
Il ne charge aucune bibliothèque USB/SWD et ne lance aucun outil matériel.
Même l'énumération et les essais simulés de reset/connexion restent factices.

## Reconstructions et tests réellement exécutés

Outils locaux : ARM GCC 14.3.1 dans
`C:\Users\cleme\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin`,
GNU Make 3.82.90 et GCC hôte 6.3.0 dans
`C:\Users\cleme\Documents\Installation\MinGW\bin`, Python 3.12.8.

Depuis la racine du dépôt, commandes de validation finales :

```powershell
$env:PATH = 'C:\Users\cleme\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;C:\Users\cleme\Documents\Installation\MinGW\bin;' + $env:PATH
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
```

Les scripts restent des wrappers GNU Make, avec SIM_MODE=0 et SIM_MODE=1
respectivement. Une vraie reconstruction des 68 unités par configuration a
été effectuée. Les logs finaux se trouvent dans `build/validation/lot4/`.

| ELF canonique | text | data | bss |
|---|---:|---:|---:|
| `build/HW/n-pulse.elf` | 87 148 | 384 | 31 344 |
| `build/SIM/n-pulse.elf` | 72 948 | 384 | 27 456 |

| Validation | Résultat |
|---|---|
| HW / SIM, reconstruction complète | réussies, code de sortie 0 |
| Lot 1, production timebase/encodeurs/ADC | 7 scénarios réussis |
| Lot 2, production interlock/PID/guard/driver | 24 scénarios × 2 backends réussis |
| Lot 3, parseur/session/UART/interlock | 17 scénarios réussis |
| Bornes SIM du lot 3 | 24 576 couples count/longueur et régressions 52/86 réussis |
| Lot 4, coupure/ASSERT/PWM | 8 groupes réussis, dont 176 états de sortie sur les huit mappings |
| Flash avec programmeur simulé | 18 scénarios réussis, aucune commande matérielle |
| Vérificateurs ELF des quatre lots | réussis HW et SIM |
| Alternance HW → SIM → HW/Debug | 136 objets inchangés, alias HW préservé, mode invalide rejeté |

ASan/UBSan ne sont pas disponibles dans le compilateur hôte installé ; la
suite SIM s'exécute sans sanitizer et conserve cette limite explicite.
Le test optionnel du codec Python hôte HW reste sauté faute de protobuf
Python installé ; le parseur protobuf de production C++ est bien compilé et
testé. Aucun port série n'est ouvert.

Le désassemblage vérifie les vecteurs forts des faults, les appels effectifs
des hooks depuis `pvPortMalloc` et `vTaskSwitchContext`, ainsi que les assertions
du noyau vers la coupure. `N_PULSE_FatalCutoff` n'a aucun appel externe, UART,
RTOS ou instruction flottante. Ses adresses MMIO fixes et le masque PRIMASK
sont présents dans les deux ELF. Les désassemblages sont conservés dans
`build/validation/lot4/*-fatal-disassembly.txt`.

Les contrôles antérieurs d'absence d'armement/PWM start et d'acquisition
ADC/OPAMP au démarrage ne sont pas affaiblis. Les handlers FreeRTOS/HAL et
le chemin RX unique UART4 restent ceux des lots précédents, sans consommateur
LPUART1 actif. Les fixtures du lot 2 ont été adaptées pour exiger SET absent
et comparaison HRTIM parquée à 3, plutôt qu'une valeur zéro trompeuse.

L'unique warning des reconstructions est le `sDefaultTaskHandle` inutilisé
préexistant. `git diff --check` signale aussi un espace final préexistant dans
`stm32g4xx_ll_crs.h:444`, fichier inchangé par ce lot, laissé intact.
La comparaison avec la sauvegarde initiale retrouve les 1 137 fichiers
existants, sans disparition : 14 ont été modifiés dans ce lot, les 1 123
autres sont identiques. Treize fichiers ont été ajoutés et le diff indexé
initial est inchangé.

## Mémoire et régénération

Le linker conservé décrit FLASH=512 Ko et RAM=128 Ko depuis 0x20000000.
Le MSP initial des deux ELF vaut 0x20020000. Cette plage inclut l'alias CCM
0x20018000..0x2001FFFF, conforme au §2.4 du RM0440 et à la fiche du G474xE.
Aucune incohérence de carte mémoire n'est démontrée ; le linker n'a pas été
modifié. Son nom RETX reste historique, son contenu décrit QETx.

Après régénération CubeMX, conserver les nouveaux fichiers et les entrées
GNU Make, les routes fatales dans les zones USER CODE, les deux options hooks
et configASSERT, et le branchement de `Default_Handler` dans le startup.
Vérifier comparaisons 3, sources SET/RESET absentes à l'initialisation, DIV1,
PER=7999, polarités et mapping inchangés. Le `.ioc` ne remplace pas la
transaction de registres du driver. La régénération n'a pas été exécutée.
Relancer les quatre suites et les contrôles ELF après toute régénération.

## Fichiers du lot

Modifiés :

- `Core/Inc/FreeRTOSConfig.h`, `Core/Src/main.c`, `Core/Src/stm32g4xx_it.c`,
  `Core/Startup/startup_stm32g474retx.s` : routage fatal et hooks effectifs.
- `fw/motors/motor_control/drv8848.c` : transactions de sortie et direction
  effective après quantification.
- `N-PULSE FIRMWARE.ioc` : initialisation inactive et comparaisons valides.
- `flash.ps1`, `tools/build_artifacts.py`, `tools/firmware.mk` : cible unique,
  manifeste et inclusion des nouvelles unités ; chemins HW/SIM conservés.
- `tests/safety/mocks/stm32g4xx_hal.h`, `tests/safety/test_safety.c`,
  `tests/safety/run_tests.py`, `tests/comms/run_tests.py` : fixtures et compilation
  du nouveau module de production.
- `README.md` : lien vers le lot et nouvelle sélection de cible.

Ajoutés : `Core/Inc/fatal_stop.h`, `Core/Src/fatal_stop.c`,
`fw/motors/motor_control/motor_outputs.h/.c`, `tests/outputs/` (runners,
tests de production, mock CMSIS, faux programmeur, contrôles ELF) et ce rapport.

## Vérification sur carte, puissance moteurs déconnectée

Cette procédure est à exécuter ultérieurement par l'utilisateur. Elle n'a pas
été réalisée ici. Laisser tous les drivers SLEEP bas, contrôleurs désactivés
et alimentation de puissance physiquement déconnectée pendant les essais.

1. Confirmer sur schéma/révision de carte la continuité des quatre SLEEP et
   des seize entrées MCU vers les drivers, leur polarité et les niveaux
   logiques. Identifier la référence réelle des drivers, les éventuelles
   inversions/buffers, pulls et l'association des deux axes du poignet.
   Sans ces informations, maintenir l'inhibition ; ne pas attribuer un effet
   mécanique aux noms A/B, sens ou Brake.
2. Sur une cible identifiée et un firmware chargé par une méthode validée
   indépendamment, vérifier le boot normal : SLEEP mesurés bas, s_armed=false,
   moteurs disabled, HRTIM ODSR désactivé, TIM MOE/CCER inactifs, pas d'ADC.
   Refaire les vérifications tick et UART des rapports précédents.
3. Tester la coupure au debugger, en test séparé avant initialisation, après
   initialisation et avec PRIMASK déjà à 1. Provoquer les points d'entrée
   fatals dans un banc contrôlé, sans sortie de puissance ; observer SLEEP
   à l'oscilloscope, inhibition HRTIM/TIM et cause RAM, puis arrêt permanent
   du CPU. Vérifier également les hooks avec des fautes injectées dans un
   banc dédié. Ne pas lire de string de tâche depuis la voie stack corrompue.
4. Pour les PWM, utiliser un banc de debug distinct maintenant SLEEP bas et
   retenant les fonctions de production des sorties. Le banc peut démarrer
   **uniquement les compteurs et sorties logiques MCU** pour observation,
   sans appeler Arm/Enable, sans nouvelle commande UART d'activation, et
   sans modifier les gains/calibrations. Les binaires normaux n'offrent aucun
   démarrage automatique permettant cet essai.
5. Sonder simultanément les deux entrées de chaque paire au niveau MCU puis
   à l'entrée du driver. Essayer zéro, les ordres autour de 0,037/0,038 %
   sur HRTIM, une valeur partielle, 99,99 %, ±100 %, saturation, retour à zéro
   et alternance de signe. Pour les TIM, inclure un ordre inférieur à un
   compte. Relever période, niveaux constants, largeur minimale, premier
   cycle et transitoires des mises à jour. Les correspondances attendues
   sont celles du tableau logiciel, pas une preuve déjà acquise.
6. Capturer les inversions avec déclenchement sur les deux entrées et les
   SLEEP. Mesurer les intervalles réels, impulsions inattendues, pauses et
   éventuelle superposition. Examiner spécifiquement TIM15 et TIM16/17,
   où la simultanéité des fronts n'est pas promise. Ne conclure à une
   commutation sûre ou à une valeur de dead time qu'après validation du
   montage et des mesures ; aucune valeur n'est proposée dans ce lot.
7. Avec les sorties logiques du banc en cours, déclencher un arrêt interlock
   puis un fatal séparément. Les nouvelles commandes/cycles ne doivent pas
   rétablir de sortie après l'interlock ; après fatal, aucune exécution ne
   reprend. Consigner les mesures et laisser la puissance déconnectée.

Restent hors lot : validation électrique et formes d'onde sur carte,
calibrations, gains PID, protection par courant, transport SIM complet et
arrêt matériel indépendant du CPU. Les limites et politiques de session du
lot 3 restent applicables. Aucun réglage mécanique ou électrique n'est inventé.
