# OLD_HW — Lot 1 : base STM32G474RET6 désarmée

Rapport du 14 octobre 2026. Branche de travail : `oldHardNewSoft`, base
`4c69750`. Référence historique consultée exclusivement par `git show` :
`origin/simulation_pipeline`, commit `15f4d4ec109a4a642b0be77d99fee446246e70cc`.
Le code prime sur les documents Markdown en cas de divergence.

## Résultat et limites

La base RET6/LQFP64 compile avec la vraie chaîne ARM, en HW et en SIM.
Le même ELF HW détecte MASTER/SLAVE par PC0. Les sorties physiques sont
**inhibées en permanence**, y compris dans l'ELF SIM : aucun démarrage PWM,
aucun armement, aucun freinage alimenté, aucune calibration en mouvement.
Le logiciel de contrôle moderne, ses sécurités et sa simulation sont conservés.

**THUMB reste explicitement indisponible en acquisition physique.** L'ancien
code tente un encodeur quadrature TIM15 sur PB14/PB15. Le header ST
`Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g474xx.h`, macro
`IS_TIM_ENCODER_INTERFACE_INSTANCE`, exclut TIM15. Ce timer n'est donc ni
initialisé ni démarré comme encodeur. Ses deux broches restent des entrées
avec pull-up. Les snapshots/calibrations THUMB sont refusés, sans mesure fictive.
La documentation officielle distingue également TIM15 des timers quadrature
dans la [datasheet ST DS12288, section 3.24.3](https://www.st.com/resource/en/datasheet/stm32g474vb.pdf).

La coupure L298N est implémentée au niveau logiciel et testée sur un modèle
de registres. **Aucune validation électrique sur carte n'a été effectuée.**
Le firmware conserve donc l'interdiction permanente de mouvement.

## Sources historiques auditées

Comparaison de `Core/Src/main.c`, `stm32g4xx_it.c`, `stm32g4xx_hal_msp.c`,
`fw/comm/board_link.c`, `fw/motors/motor_l298n/motor_l298n.c`,
`fw/bsp/encoder/encoder_bsp.c`, `docs/wiring.md`, `Makefile`, `Debug/makefile`,
`build.ps1`, linker historique et BSP Nucleo (`stm32g4xx_nucleo.h`).
Les modes de calibration, démarrages PWM et beacons UART historiques n'ont pas
été réintroduits. Les fichiers modernes et leurs tests ont servi de base pour
les tâches, le temps HAL, les verrous STOP, la validation et les calculs numériques.

## Mapping récupéré dans le C historique

Les broches ENA restent en GPIO push-pull bas dans ce lot, même lorsque leur
timer PWM est préparé. Les fonctions alternatives PWM ci-dessous décrivent le
câblage à utiliser ultérieurement ; elles ne sont pas sélectionnées au démarrage.

| Rôle | Moteur | ENA / PWM prévu | IN1 | IN2 | Encodeur A / B | État lot 1 |
|---|---|---|---|---|---|---|
| MASTER | WRIST_X | PB7 / TIM4_CH2 AF2 | PC4 | PC5 | PA0 / PA1, TIM2 AF1 | Compteur 32 bits, non calibré |
| MASTER | WRIST_Y | PB8 / TIM4_CH3 AF2 | PB0 | PB1 | PC6 / PC7, TIM8 AF4 | Compteur 16 bits, non calibré |
| MASTER | LITTLE | PB6 / TIM4_CH1 AF2 | PC8 | PC9 | PA6 / PA7, TIM3 AF2 | Compteur 16 bits, non calibré |
| SLAVE | THUMB | PA8 / TIM1_CH1 AF6 | PB5 | PB4 | PB14 / PB15, TIM15 AF1 historique | Quadrature indisponible |
| SLAVE | INDEX | PA9 / TIM1_CH2 AF6 | PC10 | PC11 | PA0 / PA1, TIM2 AF1 | Compteur 32 bits, non calibré |
| SLAVE | MIDDLE | PA10 / TIM1_CH3 AF6 | PC12 | PA15 | PA6 / PA7, TIM3 AF2 | Compteur 16 bits, non calibré |
| SLAVE | RING | PA11 / TIM1_CH4 AF6 | PB12 | PB13 | PC6 / PC7, TIM8 AF4 | Compteur 16 bits, non calibré |
| SLAVE | PALM | PB9 / TIM4_CH4 AF2 | PD2 | PC3 | PB2 AF3 / PC2 AF6, TIM20 | Compteur 16 bits, non calibré |

PC0 est une entrée avec pull-up : HIGH = MASTER, GND = SLAVE. Le rôle est
échantillonné une seule fois, avant les ressources moteur. Les timers encodeurs
partagés ne sont démarrés que pour leurs moteurs locaux : trois encodeurs sur
MASTER, quatre sur SLAVE, THUMB exclu. Aucun feedback distant n'est inventé.

Host : LPUART1, PA2 TX / PA3 RX, AF12, 115200 bauds, 8N1, **sur les deux
cartes**, conformément à l'initialisation COM1 du `main.c` historique. Le
Markdown historique indiquant USART2 et un host seulement MASTER est dépassé.
Inter-cartes : USART3, PB10 TX / PB11 RX, AF7, 115200 bauds, 8N1.
La LED de vie utilise PA5, jamais PB10 qui est désormais USART3_TX.

## Initialisation et build

Ordre actif : `HAL_Init` → détection PC0 → maintien ENA/IN1/IN2 bas → horloge →
configuration OLD_HW → `App_Init` → création des tâches modernes → scheduler.
Les échecs HAL sont propagés à `Error_Handler`, qui utilise la coupure fatale.

L'horloge reproduit le C historique : HSI 16 MHz, PLL M=4/N=85/R=2,
SYSCLK/HCLK/PCLK1/PCLK2 = 170 MHz, voltage scaling BOOST, Flash latency 4.
Aucun HSE/LSE n'est nécessaire à cette configuration. TIM1/TIM4 ont PSC=0,
ARR=8499, soit une préparation 20 kHz ; CCR=0, CCER désactivé, CEN désactivé,
MOE/AOE TIM1 désactivés. Aucun `HAL_TIM_PWM_Start` n'est lié dans les ELF.

SysTick, SVC et PendSV restent fournis par le port FreeRTOS GCC/ARM_CM4F.
PendSV/SysTick utilisent la priorité noyau 15. TIM6 reste l'unique source du
tick HAL 1 ms, priorité 4, sans API RTOS dans son interruption. La configuration
moderne réinitialise proprement TIM6 après le changement d'horloge ; à 170 MHz,
PSC=169 et ARR=999. Le plafond RTOS reste 5 ; LPUART1 utilise la priorité 6.
Son IRQ est activée par le transport moderne après préparation des objets/tâches.
USART3 est configuré mais son IRQ, sa réception de commandes et ses tâches sont
désactivées. Un IRQ inattendu USART3/UART4 mène au handler fatal par défaut.

Le startup `startup_stm32g474retx.s`, le FPU Cortex-M4, `STM32G474xx` et la
taille Flash 512 KiB sont conservés. Le linker FLASH, utilisé par Make, nomme
désormais RET6. RAM/stack sont bornées aux **96 KiB de SRAM système**
(SRAM1 80 KiB + SRAM2 16 KiB), `_estack=0x20018000`. Les 32 KiB CCM restent
réservés ; aucune dépendance à leur alias ni placement DMA en CCM n'est requis.
Le linker RAM, non utilisé par ce workflow, reçoit la même limite.
La répartition SRAM est décrite dans la [datasheet ST, section 3.5](https://www.st.com/resource/en/datasheet/stm32g474vb.pdf).

Make/PowerShell restent les points d'entrée. `-Mode OLD_HW` est le défaut,
`-Mode HW` demeure un alias et `-Mode SIM` conserve la simulation virtuelle.
Les dossiers restent `build/HW` et `build/SIM`, avec copies séparées dans Debug.
Les flags `N_PULSE_OLD_HW_LOT1=1` / `N_PULSE_TARGET_RET6=1` et les manifests
identifient explicitement RET6, le lot et l'inhibition physique. Un build ARM
sans ces marqueurs est refusé, notamment en cas de flags générés obsolètes.
`CURRENT_ACQUISITION=1` et `HW_SINGLE_MOTOR_TEST=1` sont refusés pour ce lot.

HRTIM, LPTIM et OPAMP sont retirés des modules HAL actifs OLD_HW. Les drivers
DRV8848 et `motor_outputs.c` ne participent plus au build ARM ; leur source reste
présente pour référence et tests de non-régression. Aucun GPIO E/G du QET6 n'est
utilisé par l'initialisation active.

Le Make MinGW 3.82.90 installé échoue ici avec `INTERNAL: readdir: Invalid
argument`. `build.ps1` préfère le GNU Make 3.81 déjà installé dans GnuWin32 et
accepte `-MakePath` ou `$env:MAKE` pour sélectionner explicitement un exécutable.
Aucun outil ni système de build nouveau n'a été installé.

## CubeMX et code manuel

Le `.ioc` fourni par l'utilisateur n'a pas été modifié ni régénéré. Il décrit
RET6/LQFP64, le modèle Nucleo, PA2/PA3 LPUART1, PA5 LED, le debug, des broches
d'oscillateurs, TIM6 timebase et les paramètres PLL. Il ne décrit pas entièrement
les moteurs, encodeurs, le strap et le lien inter-cartes. Sa priorité TIM6=15
diffère du correctif moderne actif à 4 ; son wrapper CMSIS-RTOS/defaultTask
n'est pas la séquence de démarrage active.

`Core/Src/old_hw.c` configure manuellement PC0, ENA/IN1/IN2, TIM1/TIM4,
TIM2/TIM3/TIM8/TIM20, les entrées THUMB passives, LPUART1, USART3 et leurs
clocks/IRQ. Les routines générées du nouveau matériel restent dans les sources,
exclues par le préprocesseur. Une régénération CubeMX ne constitue pas un build
OLD_HW validé et doit préserver ces modifications manuelles.

## Sécurité, fonctions préservées et accommodations

PID, moteur/feedback, garde, verrous STOP, estimateur Kalman, parser, Protobuf,
commissioning, interfaces courant, simulation et tests sont conservés.
Les encodeurs physiques démarrent sans calibration mécanique implicite : les
comptes bruts sont disponibles, les positions/vitesses calibrées restent invalides.
Les fonctions de calibration/reset modernes restent dans les sources mais ne
sont pas accessibles via le commissioning de production dans ce lot.

L'acquisition courant reste désactivée. Les interfaces ADC existantes retournent
un état invalide/inactif et `NAN` pour le courant ; aucun capteur manquant n'est
assimilé à zéro ampère. Les mappings ADC/OPAMP du nouveau matériel ne sont pas
initialisés et restent à déterminer pour les anciennes cartes.

Accommodations explicites : `drv8848.h` adapte temporairement les noms d'API des
appelants modernes vers des symboles `Motor_L298N_*`. L'implémentation
`motor_l298n_lot1.c` est un interlock permanent, pas le driver historique mobile.
`Arm` retourne false ; le statut driver bloque l'activation et la récupération,
et `MOTOR_STOP_DRIVER` est verrouillé au démarrage. Une sortie non nulle ou une
commande inter-cartes est rejetée et comptée. `CleanupStoppedInternal` vérifie
les registres/GPIO bas au lieu de retourner un succès aveugle. `Brake` ne produit
aucun courant et ne fournit pas de maintien mécanique.

La coupure fatale OLD_HW n'utilise ni HAL ni handles : elle force l'union des
ENA des deux cartes (PA8–PA11, PB6–PB9) en GPIO bas, puis neutralise TIM1/TIM4
uniquement si leurs clocks sont actives. Elle fonctionne sans rôle connu ni
initialisation réussie. Le code de coupure QET6 est conservé mais inactif.
Les niveaux réels, les éventuels jumpers ENA, l'état au reset avant le logiciel
et le comportement du pont doivent encore être vérifiés sur carte.

Diagnostics : `g_old_hw_role`, `g_old_hw_rejected_commands`,
`g_old_hw_encoder_unavailable_mask` (bit THUMB sur SLAVE). Les flags Protobuf
existants 0–5 sont conservés ; bit 16 = MASTER, bit 17 = SLAVE,
bit 18 = interlock lot 1, bit 19 = encodeur sélectionné indisponible/non local.
Un snapshot absent est initialisé avant sérialisation pour éviter de lire des
données de pile pour un moteur distant. Les champs calibrés restent `NAN`.
La simulation conserve huit moteurs virtuels, indépendamment du rôle physique ;
elle ne fournit aucun feedback aux moteurs physiques.

## Commandes et résultats réellement exécutés

Depuis la racine du dépôt, sans matériel connecté par les tests :

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Mode OLD_HW -Rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Mode SIM -Rebuild
python tools/verify_startup_build.py
$env:MAKE = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
python tests/startup/test_build_modes.py
python tests/old_hw/run_tests.py
python tests/startup/run_tests.py
python tests/safety/run_tests.py
python tests/comms/run_tests.py
python tests/outputs/run_tests.py
python tests/patch7/run_tests.py
python tests/simulation/run_tests.py
arm-none-eabi-nm -u build/HW/n-pulse.elf
arm-none-eabi-nm -u build/SIM/n-pulse.elf
git diff --check
```

Environnement : ARM GCC 14.3.1 (GNU Tools for STM32), GNU Make 3.81,
GCC hôte MinGW 6.3.0, Python 3.12.8. Les logs de cette exécution sont conservés
dans `build/*tests.log`, `old-hw-build.log`, `old-hw-sim-build.log`,
`old-hw-linked-checks.log` et `old-hw-build-modes.log`.

| Validation | Résultat |
|---|---|
| Clean build ARM HW | PASS, text=99712, data=100, bss=31144 octets, sans warning de compilation |
| Clean build ARM SIM | PASS, text=71360, data=96, bss=31272 octets, sans warning de compilation |
| ELF/startup/symboles | PASS : vecteurs forts, propriété des ticks, ordre PC0/sorties, LPUART1, absence d'armement/démarrage PWM, marqueurs/manifests RET6 |
| Symboles non résolus | Aucun dans les deux ELF (`nm -u`) |
| Alternance HW → SIM → HW/Debug | PASS : 140 objets isolés inchangés, copie HW préservée ; modes et options dangereux refusés |
| Tests OLD_HW | PASS : 43 scénarios, deux rôles et chaque échec HAL d'initialisation/start injecté ; broches, compteurs, refus d'armement et coupure fatale |
| Startup existant | PASS : 31 scénarios hôte, tick, encodeurs/arithmétique et ADC |
| Safety existant | PASS : contrôleur, backend, STOP/disarm, concurrence/cleanup, commissioning et intégration BSP |
| Communications existantes | PASS : 24 scénarios, bornes SIM et scénarios de saturation/STOP/timeout |
| Outputs existants | PASS : registres et driver historiques ; 20 scénarios du script flash avec exécutable programmeur factice uniquement |
| Patch7 existant | PASS : commissioning et Kalman |
| Simulation native | PASS : boucle fermée, défauts, transport et validation de protocole sur PC |
| Espaces/conflits de patch | `git diff --check` PASS |

Les sanitizers ASan/UBSan ont été tentés par les suites et sont **indisponibles**
sur la chaîne hôte locale ; les tests ordinaires ont passé. Le codec Python
host optionnel est **SKIPPED** faute de `google.protobuf`. Aucun test en échec
n'a été supprimé ou désactivé. Une première vérification d'alternance lancée
pendant la recompilation détectait logiquement des objets modifiés ; elle a
été relancée après les builds et a passé sans modification de son exigence.

Les tests hôte utilisent HAL/RTOS/registres modélisés. La compilation et
l'inspection ELF ne démontrent pas le fonctionnement électrique, le baudrate
réel, la précision d'horloge, les compteurs réels ou le scheduler sur carte.
Aucun flash, connexion à un programmeur réel, reset debugger ou mouvement
n'a été exécuté.

## Inventaire des changements

Ajoutés :

- `Core/Inc/old_hw.h`, `Core/Inc/old_hw_cutoff.h`, `Core/Src/old_hw.c`.
- `fw/motors/motor_l298n/motor_l298n_lot1.c`.
- `tests/old_hw/run_tests.py`, `tests/old_hw/test_old_hw.c`.
- `tests/old_hw/mocks/main.h`, `stm32g4xx.h`, `stm32g4xx_hal.h`.
- `docs/OLD_HW_LOT1.md`.

Modifiés :

- `Core/Inc/main.h`, `stm32g4xx_hal_conf.h`, `stm32g4xx_it.h`.
- `Core/Src/main.c`, `stm32g4xx_hal_msp.c`, `stm32g4xx_it.c`, `fatal_stop.c`.
- `STM32G474RETX_FLASH.ld`, `STM32G474RETX_RAM.ld`, `build.ps1`.
- `fw/app/hw_telemetry.c`.
- `fw/bsp/encoder/encoder_bsp.c`, `fw/bsp/adc_dma/adc_dma_bsp.c`.
- `fw/comm/board_link.c`, `board_link.h`, `board_uart_compat.c`, `uart4_transport.c`.
- `fw/motors/motor_control/drv8848.h`, `motor_backend_hw.c`.
- `tools/firmware.mk`, `tools/build_artifacts.py`, `tools/verify_startup_build.py`.
- `tests/startup/test_build_modes.py`.

Aucun fichier supprimé. Les modifications préexistantes de
`N-PULSE FIRMWARE.ioc` et `.vscode/settings.json` ont été conservées, sans édition
par ce lot. Aucun checkout de `closed-loop`/`simulation_pipeline`, merge, reset,
rebase, commit ou push n'a été réalisé.

## Dépendances précises pour le lot 2

La branche est **prête pour le développement du lot 2** sur cette base compilable
et désarmée. Elle n'est pas prête à activer les moteurs.

1. Résoudre THUMB : valider une acquisition quadrature compatible avec PB14/PB15
   ou un changement de câblage vers un timer compatible. Ne pas contourner la
   limitation TIM15 ni déclarer sa mesure valide.
2. Implémenter le backend L298N moderne en conservant les epochs/verrous STOP,
   la propriété exclusive des transitions et le fail-closed. Remplacer
   l'adaptateur temporaire sans importer les modes open-loop/homing historiques.
3. Vérifier électriquement ENA/IN1/IN2 et la coupure (démarrage, reset, fatal,
   STOP, inversion, erreurs HAL partielles) avant de lever l'interlock lot 1.
4. Définir le mapping et la stratégie de protection courant de l'ancien
   matériel. L'absence actuelle de mesure doit rester explicite ; ne pas
   réactiver les ADC/OPAMP du QET6 ni interpréter l'absence comme zéro courant.
5. Fournir/valider les calibrations mécaniques des encodeurs et leur sens,
   avec un protocole d'autorisation explicite ; aucune calibration au boot.
6. Le lot 3 devra apporter le transport moteur/feedback inter-cartes moderne
   et ses défauts de lien/STOP distribués. USART3 préparé dans ce lot ne fournit
   encore aucune de ces garanties.
