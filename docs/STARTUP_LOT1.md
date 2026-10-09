> **Addendum du 9 octobre 2026 — lot 5 :** ce rapport conserve les résultats
> historiques. [ROBUSTNESS_LOT5](ROBUSTNESS_LOT5.md) précise désormais le contrat
> de désarmement public, la sérialisation HAL, l'engagement UART4 IT et le reset
> encodeur. « Aucun UART sous Motor_Lock » est remplacé par une exception bornée
> pour le démarrage IT effectif, sans attente ; l'ancienne vérification d'epoch
> avant ce démarrage ne suffisait pas. Le flash maintenu exige un SN explicite ;
> RX SIM reste déconnectée et aucun premier mouvement n'est activé.

# Premier lot : compilation et démarrage désarmé

Validation sur PC le 8 octobre 2026. Aucun flash ni essai sur carte effectué.
Une compilation réussie ne démontre pas que la carte démarre correctement.

## Constats vérifiés dans les sources locales

Les cinq problèmes du ZIP étaient encore présents :

| Point | État avant ce lot | Correction |
| --- | --- | --- |
| Tick FreeRTOS | `SysTick_Handler` appelait seulement `HAL_IncTick`. Les macros associaient SVC/PendSV au port, mais pas SysTick. | Les trois exceptions sont fournies par le port GCC `ARM_CM4F`. SysTick appelle le tick RTOS, sans incrément HAL. |
| Timebase HAL | `HAL_InitTick` utilisait TIM6, sans callback fort pour incrémenter HAL. `MX_TIM6_Init` le reconfigurait ensuite avec PSC=79. Le MSP remettait aussi sa priorité à 0. | TIM6 est réservé à HAL. Handle, initialisation et callback sont dans le fichier timebase ; suppression de `MX_TIM6_Init`. |
| Encodeurs | Le backend HW démarrait le BSP puis le guard redémarrait les timers HAL avec une table différente. PALM/LPTIM1 manquait dans le guard. Les erreurs BSP étaient silencieuses. | Le BSP démarre les huit encodeurs une seule fois et remonte les erreurs. Le guard utilise son mapping et ses comptes bruts, y compris LPTIM1. |
| ADC/DMA | Acquisition continue lancée malgré un commentaire annonçant sa désactivation ; buffers de 1 à 3 valeurs et interruptions DMA de priorité 0. | Acquisition explicitement optionnelle et désactivée par défaut. Priorité 6 pour les IRQ ADC DMA. L'option expérimentale supprime HT/TC par échantillon et conserve les IRQ d'erreur DMA. |
| HW/SIM | `build.ps1` sélectionnait SIM ; objets partagés ; `SIM_MODE=0` était non vide donc activait SIM ; clean incomplet. | GNU Make valide exactement 0/1, applique les définitions à toutes les unités et isole les objets et flags. Reconstruction séquentielle complète, même avec `-j`. |

La compilation réelle utilise `Middlewares/Third_Party/FreeRTOS-Kernel`
**V10.5.1**, son port `portable/GCC/ARM_CM4F/port.c` et `heap_4.c`.
L'ancien arbre `FreeRTOS/Source` et CMSIS-RTOS ne sont pas compilés.
L'en-tête historique de FreeRTOSConfig mentionne encore 10.3.1.

Les règles maintenues sont `Makefile` et `tools/firmware.mk` ;
`Debug/makefile` est un relais compatible. Les anciens `subdir.mk` restent
présents, avec les modifications préexistantes, mais ne sont plus utilisés.
Le startup et le linker gardent leur nom historique `STM32G474RETX` ;
le `.ioc` cible déjà STM32G474QET6. Le `.cproject` CubeIDE reste historique.

## Horloges, interruptions et démarrage

`SystemClock_Config` utilise HSI 16 MHz, PLL M=1/N=20/R=2 :
SYSCLK/HCLK=160 MHz, APB1=160 MHz avec diviseur 1,
APB2=80 MHz avec diviseur 2. TIM6 reçoit donc 160 MHz :
PSC=159 et ARR=999 donnent 1 ms. Avant la PLL, HSI 16 MHz donne PSC=15.
La timebase tient également compte du doublement des clocks timers quand
APB1 est divisé. Ces valeurs sont déduites du code, pas mesurées sur carte.

TIM6 est configuré avant activation de son IRQ, avec effacement du flag
d'update généré par l'initialisation. Il est réinitialisé lors du changement
d'horloge. `HAL_TIM_PeriodElapsedCallback` incrémente HAL uniquement pour TIM6.
HAL reste à 1 kHz ; un changement de fréquence HAL non supporté est rejeté.

TIM6 a la priorité **4**, sans aucune API FreeRTOS dans son callback.
Le plafond RTOS est 5, SysTick/PendSV sont à 15. Cette priorité permet aux
timeouts HAL de progresser avant le scheduler même après création d'objets
RTOS, qui peut laisser BASEPRI à 5. Les IRQ DMA ADC sont à 6.
Les IRQ DMA UART n'ont pas été adaptées : la réception UART4 reste hors lot.

Le DRV8848 est initialisé désarmé avant le démarrage du BSP/backend.
Une erreur HAL d'encodeur ou de l'acquisition optionnelle empêche le démarrage
normal au lieu d'être ignorée. Les comptes accumulés 64 bits sont protégés
entre les tâches control/guard. Leur remise à zéro est logique, sans
redémarrer le timer LPTIM. Les conversions angulaires du BSP restent
indisponibles sans calibration explicite.

Le test encodeur en lecture seule utilise également le BSP et n'affiche que
des comptes bruts. Le mode de calibration historique est bloqué à la
compilation : son ancienne table et ses constantes ne sont pas validées pour
la nouvelle carte. Aucun armement automatique n'a été ajouté.

## Acquisition de courant : option expérimentale

`CURRENT_ACQUISITION=0` est la valeur par défaut des builds et des scripts.
`ADC_DMA_BSP_Init` ne démarre alors aucun ADC DMA ni OPAMP ; les GPIO et
périphériques CubeMX peuvent être initialisés sans lancer de conversions.
`GetRaw` retourne 0 sans échantillon disponible ; ce zéro n'est pas une mesure.
`GetCurrentA` reste un placeholder à 0. Aucune protection par courant
calibrée ou validée n'est fonctionnelle.

`CURRENT_ACQUISITION=1` active uniquement l'acquisition brute expérimentale
dans `build/HW-current` ou `build/SIM-current`. Elle ne remplace pas les
artefacts par défaut. Les IRQ HT/TC sont désactivées pendant le démarrage
protégé de chaque DMA pour éviter la saturation CPU ; les données sont lues
par polling. Les démarrages partiels sont arrêtés en cas d'erreur, remontée
à l'appelant. Aucun seuil, gain, rapport mécanique ou paramètre électrique
n'a été ajouté. Cette option n'a pas été testée sur carte.

## Compilation et résultats sur ce PC

Outils détectés :

- ARM GCC **14.3.1** : `C:\Users\cleme\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin\arm-none-eabi-gcc.exe`.
- GNU Make **3.82.90** : `C:\Users\cleme\Documents\Installation\MinGW\bin\mingw32-make.exe`.
- GNU Make 3.81 est aussi installé dans `C:\Program Files (x86)\GnuWin32\bin` ; le script utilise le Make disponible dans PATH, sinon ce fallback.
- Python 3 et GCC hôte MinGW sont disponibles. Les helpers de build demandent Python 3.9 ou ultérieur.

Commandes exécutées depuis la racine, via PowerShell :

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-sim.ps1 -Rebuild
python tests/startup/run_tests.py
python tools/verify_startup_build.py
python tests/startup/test_build_modes.py
```

Les deux reconstructions complètes se sont terminées avec le code **0** :
60 unités recompilées pour HW et 60 pour SIM, sans utilisation des anciens
objets de `Debug`. Les logs sont dans `build/validation/hw-build.log` et
`build/validation/sim-build.log`.

| Build | ELF canonique | text | data | bss |
| --- | --- | ---: | ---: | ---: |
| HW | `build/HW/n-pulse.elf` | 70 736 | 384 | 26 600 |
| SIM | `build/SIM/n-pulse.elf` | 74 972 | 384 | 26 928 |

Les tailles sont celles de `arm-none-eabi-size`, en octets ; elles ne
constituent pas une analyse des marges de pile à l'exécution.
Il reste deux warnings de code inutilisé en HW (`sDefaultTaskHandle`,
`apply_rate_limit`) et quatre en SIM (les mêmes, plus
`isr_send_intent_ack` et `s_max_velocity_rad_s`).

La copie compatible `Debug/N-PULSE FIRMWARE.elf` est **HW uniquement**.
SIM utilise `Debug/SIM/N-PULSE FIRMWARE.elf`. Le build SIM ne remplace
jamais la copie HW. Le script `flash.ps1` n'a pas été exécuté ni modifié ;
ses descriptions et son comportement multi-cartes restent historiques.
Le nettoyage touche seulement les fichiers générés de la configuration
sélectionnée sous `build/`, avec vérification du chemin résolu. Les anciens
objets et `.su` de Debug sont laissés intacts.

## Vérifications effectuées et limites

- **Tests hôte** : sept scénarios, compilant les routines de production avec
  HAL/RTOS simulés : timebase, erreur HAL, mapping des huit encodeurs,
  wrap 16 bits, reset logique LPTIM, absence de redémarrage, ADC désactivé,
  option ADC sans IRQ par échantillon et arrêt après erreur partielle.
- **ELF/map/disassembly** : handlers forts SVC/PendSV/SysTick issus du port
  ARM_CM4F ; vecteurs égaux aux adresses des handlers ; TIM6 IRQ vers HAL ;
  callback fort vers HAL_IncTick ; SysTick vers xTaskIncrementTick.
  Aucun HAL_IncTick depuis SysTick. Les fonctions d'armement, de démarrage
  PWM/HRTIM et d'acquisition ADC DMA ne sont pas retenues dans les ELF par défaut.
- **Builds** : alternance HW → SIM → HW via Debug sans recompilation ni
  changement des 120 objets ; copie HW préservée ; `SIM_MODE=2` rejeté.
  L'option ADC a aussi passé un contrôle ARM `-fsyntax-only -Wall -Wextra -Werror`.
- **Préservation** : comparaison avec les empreintes de l'état initial et
  comparaison identique de l'index Git. Aucune source préexistante hors lot
  ni fichier non suivi recensé n'a été supprimé ; aucun commit/push/reset.
- **Sur carte** : aucune validation du boot, des clocks effectives, du timing
  des tâches, des GPIO, des signaux encodeurs ou des DMA. UART4, le transport
  de simulation, les calibrations et le réglage PID restent à terminer dans
  d'autres lots. Le guard n'est pas une protection matériellement validée.

## Précautions CubeMX

Le `.ioc` réserve désormais la timebase SYS à TIM6, retire son activation
comme timer applicatif, et aligne les priorités TIM6/ADC/PendSV. Les broches
physiques et leurs associations ne sont pas modifiées.

Après toute régénération, vérifier et réappliquer les adaptations manuelles :
un seul `htim6` dans le fichier timebase, aucun `MX_TIM6_Init`, callback
HAL fort unique, `TICK_INT_PRIORITY=4` et aucun écrasement de priorité/enable
dans le MSP. Le port doit rester le seul propriétaire de SVC/PendSV/SysTick,
avec ses trois macros d'alias et sans wrapper CMSIS ou handler Cube concurrent.
CubeMX peut écraser le fichier timebase et des zones hors USER CODE.
Ne pas remplacer le relais Debug ni les règles maintenues par les anciens
Makefiles générés. Réexécuter les deux reconstructions, tests et vérifications
ELF avant tout essai. La régénération elle-même n'a pas été exécutée ici.

## Vérification sur carte, à exécuter par l'utilisateur

1. Déconnecter l'alimentation de puissance des moteurs. Identifier la seule
   STM32G474QET6 cible et charger manuellement l'ELF HW par la méthode validée
   pour cette carte ; ne pas utiliser le script historique multi-cartes.
2. Vérifier l'arrivée à `main`, puis l'absence de passage dans Error_Handler
   ou configASSERT. Contrôler `SystemCoreClock=160000000`, TIM6 PSC=159,
   ARR=999, IRQ priorité 4 ; après scheduler, SysTick LOAD=159999 et priorité 15.
3. En exécution continue, mesurer les ticks : HAL progresse avant et après
   scheduler ; HAL et RTOS avancent approximativement de 1000 sur une seconde.
   Vérifier les réveils de control (10 ms), safety/guard (20 ms) et telemetry.
4. Contrôler `s_armed=false`, tous les moteurs `enabled=false`, sorties à zéro,
   SLEEP à 0 selon le mapping actuel (PG0/PG2/PG4/PC4), sorties PWM/HRTIM
   non démarrées et aucune conversion ADC DMA active dans le build par défaut.
5. Faire évoluer prudemment les encodeurs sans alimentation moteur et observer
   les comptes bruts des huit entrées, notamment PALM/LPTIM1 et les wraps.
   Aucun appel supplémentaire à HAL_TIM_Encoder_Start ne doit venir du guard.
   Documenter les résultats et les marges de pile ; laisser les moteurs désarmés.

## Fichiers du lot

Modifiés : `Core/Inc/FreeRTOSConfig.h`, `Core/Inc/stm32g4xx_hal_conf.h`,
`Core/Src/main.c`, `Core/Src/stm32g4xx_it.c`,
`Core/Src/stm32g4xx_hal_timebase_tim.c`, `Core/Src/stm32g4xx_hal_msp.c`,
`fw/bsp/encoder/encoder_bsp.c`, `fw/bsp/encoder/encoder_bsp.h`,
`fw/bsp/adc_dma/adc_dma_bsp.c`, `fw/bsp/adc_dma/adc_dma_bsp.h`,
`fw/motors/motor_control/motor_control.c`,
`fw/motors/motor_control/motor_backend_hw.c`, `fw/app/app.c` (commentaire),
`Makefile`, `Debug/makefile`, `build.ps1`, `build-sim.ps1`,
`N-PULSE FIRMWARE.ioc`, `README.md`.

Ajoutés : `tools/firmware.mk`, `tools/build_artifacts.py`,
`tools/verify_startup_build.py`, `tests/startup/` (tests et mocks HAL/RTOS),
et ce document. Les artefacts de compilation sont générés et ignorés par Git.
