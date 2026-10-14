# OLD_HW — Lot 2 : L298N, encodeurs et contrôle moderne

Rapport du 14 octobre 2026, branche `oldHardNewSoft`, HEAD `4c69750`.
Les modifications non commitées du lot 1 ont été conservées comme point de
départ. Un état de comparaison est enregistré dans `build/lot2-baseline`.
`docs/OLD_HW_LOT1.md`, le `.ioc` fourni et `.vscode/settings.json` sont inchangés
par rapport au début du lot 2. Aucun changement de branche, commit, push,
merge, reset, rebase, cherry-pick, flash ou reset matériel n'a été effectué.

## État livré

Le backend L298N fonctionnel remplace le driver temporaire du lot 1. Il utilise
les sorties signées du PID moderne, les verrous STOP, les epochs, les règles de
feedback et le commissioning existants. Les algorithmes de `Motor_Update`,
PID, sécurité, garde, validation du feedback, Kalman et estimation n'ont pas
été réécrits. Le pilotage historique par durée et ses timers FreeRTOS ne sont
plus dans le module L298N actif.

**Les builds ordinaires HW et SIM restent incapables d'activer un moteur
physique.** Leur ELF ne contient ni `Motor_L298N_Arm` ni `program_locked`, qui
programme le PWM non nul. SIM conserve les huit moteurs virtuels et n'utilise
aucun encodeur physique. Le même binaire HW démarre sur MASTER ou SLAVE selon
PC0, comme au lot 1.

La capacité de mouvement existe uniquement dans une image dédiée :
`HW_SINGLE_MOTOR_TEST=1`, un identifiant de doigt autorisé, et
`OLD_HW_MOTION_OPT_IN=1`. Même cette image démarre désarmée, toutes sorties
basses. Un acquittement opérateur explicite, une calibration, du feedback local
frais, une session valide et une commande d'armement restent nécessaires.

THUMB possède désormais une acquisition logicielle PB14/PB15. **Son entraînement
reste bloqué par défaut, même dans l'image de commissioning**, par
`THUMB_EXTI_MOTION_QUALIFIED=0`. Ce verrou doit rester en place jusqu'à mesure
de la latence et des pertes EXTI sur le STM32 sous charge réelle. Les tests
hôte utilisent explicitement la valeur 1 pour vérifier le chemin logiciel,
sans aucun accès à un moteur.

## Driver L298N et comportement PWM

Le mapping du lot 1 est réutilisé sans importer les remappings ultérieurs de
la branche `presentation`. TIM1 prépare les quatre PWM SLAVE THUMB/INDEX/
MIDDLE/RING. TIM4 prépare LITTLE/WRIST_X/WRIST_Y sur MASTER et PALM sur SLAVE.
Seuls les moteurs locaux peuvent atteindre le driver ; un identifiant distant
ou invalide est refusé sans activer ses broches.

L'horloge reste à 170 MHz, PSC=0 et ARR=8499 : période de 8500 ticks, 20 kHz
nominal. Le signe positif utilise IN1=1/IN2=0, le signe négatif IN1=0/IN2=1.
La direction mécanique réelle doit être vérifiée avec la calibration de
l'encodeur ; aucune inversion globale issue de l'ancien contrôle open-loop
n'est appliquée automatiquement.

| Situation | Registres et niveaux demandés |
|---|---|
| Boot, désarmement, STOP | ENA GPIO push-pull bas ; IN1=IN2=0 ; CCR=0 ; CCER/CEN désactivés ; MOE/AOE TIM1 désactivés |
| PWM nul, y compris sous un tick | ENA reste en GPIO bas, aucune commutation vers l'AF PWM, IN1=IN2=0 |
| Commande positive/négative | ENA d'abord forcé bas ; direction préparée ; CCR immédiat ; transfert UG/CNT=0 ; CCER/CEN et MOE TIM1 ; AF6 TIM1 ou AF2 TIM4 sélectionnée en dernier |
| 100 % dans la primitive de registres | CCR=8500=ARR+1, PWM1 constamment actif ; pas de saturation à ARR qui créerait un creux |
| Commissioning réel | Magnitude limitée au plafond opérateur, lui-même borné par la configuration de build (5 % par défaut, maximum logiciel 10 %) |
| Inversion | ENA bas et entrées à zéro ; attente non bloquante ; aucune commande différée stockée |
| Freinage demandé | Arrêt en roue libre seulement, sans ENA actif ni freinage alimenté |

Le délai d'inversion est `N_PULSE_L298N_REVERSE_BLANK_MS=2` ticks HAL. Une reprise
exige une **nouvelle** commande PID après ce délai ; la commande qui a demandé
l'inversion n'est pas rejouée. La phase du tick rend la durée physique minimale
supérieure à environ 1 ms ; la boucle PID à 10 ms allonge généralement cette
coupure. Cette marge logicielle ne constitue pas une qualification électrique.

Les deux directions sont écrites en une seule commande BSRR lorsqu'elles
partagent un port. Les voies complémentaires ne sont jamais activées. Les
écritures de compare sont immédiates pendant qu'ENA est déconnecté du timer.
Le driver vérifie clocks, PSC/ARR, mode PWM, sélection AF, CCR attendu,
CCER/CEN/MOE et GPIO de direction. Une incohérence empêche un succès et
verrouille un STOP driver. `Motor_L298N_ApplySigned` distingue `INHIBITED`,
`ZERO`, `BLANKED`, `APPLIED` et `FAULT`. La télémétrie OLD_HW présente le PWM
effectivement programmé, qui peut être nul pendant la coupure d'inversion ou
la quantification, au lieu d'afficher uniquement la proposition PID.

L'armement n'active aucun canal : il établit seulement une permission logique.
La transition conserve un propriétaire et vérifie l'epoch avant sa validation
finale. STOP pendant cette transition annule l'armement ; le nettoyage refuse
de concurrencer un propriétaire actif. Un réappel de l'initialisation sur un
driver déjà initialisé provoque un STOP et ne conserve jamais une sortie vive.
Une durée d'armement maximale de **5000 ms**, y compris sans mouvement, est
contrôlée par le driver et la supervision moderne. L'expiration désarme et
exige une récupération opérateur. Le contrôle/safety doit évidemment continuer
à s'exécuter ; ces bornes sont des garanties logicielles, pas un watchdog matériel.

Les API historiques `Motor_DRV8848_*` des appelants modernes restent adaptées
vers `Motor_L298N_*` dans le header existant. Le code DRV8848 n'est pas lié dans
les ELF OLD_HW. Les primitives `Stop`/`StopAll` du driver mettent les sorties à
zéro ; les arrêts persistants passent par `Motor_Disable`, `Motor_StopAll` ou
`Disarm`, conformément au contrat moderne, qui invalide aussi commandes et epochs.

## Coupure, défauts et limitations électriques

La coupure fatale du lot 1 est conservée : sans HAL, handles ni rôle connu,
elle force l'union des ENA (PA8–PA11 et PB6–PB9) en GPIO bas et neutralise
TIM1/TIM4 uniquement si leurs clocks sont actives. Les tests couvrent avant
initialisation, état actif et clocks partiellement activées. La cause fatale
interdit aussi toute nouvelle activation si la fonction de coupure retourne
dans un test ; les handlers de production terminent dans `FatalHalt`.

STOP, défaut, désarmement et invalidation du feedback désactivent immédiatement
les sorties dans la même section protégée que les états de contrôle. Le PID
est calculé hors du verrou ; sa validation d'epoch/révision et les vérifications
du driver empêchent une proposition ancienne de réactiver une sortie arrêtée.
Aucune file de commandes matérielles, auto-stop historique, calibration en
mouvement ou homing automatique n'a été ajouté.

Le L298N n'a pas de nFAULT raccordé dans ce mapping. `FaultActive` représente
donc l'état logiciel/registres, les erreurs d'initialisation, la cause fatale
et la limite de temps ; il ne certifie ni la température ni l'état électrique
du pont. La mesure de courant reste **indisponible**, avec états ADC invalides
et courant `NAN`, sans réactiver ADC/OPAMP du QET6. Il n'existe ici aucune
protection de surcharge ou de blocage fondée sur un courant mesuré. La garde
moderne surveille le déplacement de l'encodeur. Kalman reste disponible dans
les sources et testé, sans être imposé au feedback PID.

La [datasheet ST L298, table 5 et section 5](https://www.st.com/resource/en/datasheet/l298.pdf)
décrit ENA bas comme inhibition/roue libre, et les deux entrées égales avec ENA
haut comme freinage. Les temps de commutation indiqués sont obtenus dans des
conditions de test spécifiques ; ils ne valident pas le montage inductif réel.
Le lot 2 n'a mesuré aucun de ces comportements sur carte.

## État des huit encodeurs

| Rôle | Moteur | Acquisition active | Largeur / validité |
|---|---|---|---|
| MASTER | WRIST_X | TIM2, PA0/PA1 AF1 | 32 bits, accumulation moderne int64 |
| MASTER | WRIST_Y | TIM8, PC6/PC7 AF4 | 16 bits, wrap signé |
| MASTER | LITTLE | TIM3, PA6/PA7 AF2 | 16 bits, wrap signé |
| SLAVE | INDEX | TIM2, PA0/PA1 AF1 | 32 bits, accumulation moderne int64 |
| SLAVE | MIDDLE | TIM3, PA6/PA7 AF2 | 16 bits, wrap signé |
| SLAVE | RING | TIM8, PC6/PC7 AF4 | 16 bits, wrap signé |
| SLAVE | PALM | TIM20, PB2 AF3 / PC2 AF6 | 16 bits, wrap signé |
| SLAVE | THUMB | PB14/PB15, EXTI montant et descendant | Décodage logiciel int64, acquisition implémentée ; entraînement non qualifié |

Ce tableau décrit le code, pas des essais physiques réussis. Chaque timer
physique n'est démarré qu'une fois et pour son propriétaire local. Clocks,
CEN, SMS et les deux entrées CCER sont contrôlés avant d'accepter le feedback.
Une interruption des lectures dépassant le timeout moderne de 100 ms rend
l'accumulation incertaine et invalide le retour, au lieu d'interpréter un
compteur 16 bits potentiellement enroulé comme une mesure fraîche.

Les timestamps, calculs de position/vitesse, validation numérique, offset,
calibration et limites d'overflow modernes sont conservés. Aucune conversion
mécanique n'est installée au boot. Les comptes bruts ont une origine locale ;
ils ne prouvent pas une position mécanique zéro. Les changements de calibration
et resets OLD_HW sont refusés si le contrôleur est activé, le driver armé ou
une transition en cours. Les défauts de décodage/comptage restent bloquants
jusqu'au reset explicite, désarmé, puis à une nouvelle référence/validation.
Les moteurs distants ne reçoivent aucun feedback inventé.

## Investigation historique THUMB/TIM15

Référence principale : `origin/simulation_pipeline` à
`15f4d4ec109a4a642b0be77d99fee446246e70cc`, consultée uniquement par `git show`.
Son `encoder_bsp.c` et ses helpers de calibration dans `main.c` utilisent
TIM15 comme les autres encodeurs, sans démontrer un compteur fonctionnel.
La réussite de compilation n'est pas une preuve d'acquisition.

L'examen de `git log --all` a trouvé le commit
`7482ea0879eca5b0c0b317a7e7b7df61deae174c` du 20 mai 2026, présent dans
`presentation`/`origin/presentation`, mais absent de la référence
`simulation_pipeline`. Son message rapporte une observation physique : les
niveaux PB14/PB15 changent en tournant THUMB à la main, mais le compteur TIM15
reste nul. Son patch ajoute une table quadrature EXTI dans le chemin de
calibration de `main.c` et un dispatch partagé avec PC13. Il ne corrige pas le
BSP de référence, et ne fournit pas de trace de qualification complète du
décodeur, de sa latence ou du contrôle moderne. Aucun cherry-pick n'a été fait.

Le message historique attribue également le problème à un retour HAL_ERROR.
Dans le HAL actuellement présent, `HAL_TIM_Encoder_Init` vérifie l'instance
par `assert_param(IS_TIM_ENCODER_INTERFACE_INSTANCE(...))` ; avec les asserts
désactivées, une instance non supportée peut néanmoins atteindre HAL_OK.
Le défaut matériel ne doit donc pas être déduit du seul code de retour.

Le header CMSIS ST exclut TIM15 de `IS_TIM_ENCODER_INTERFACE_INSTANCE` et la
[datasheet STM32G474, section 3.24.3](https://www.st.com/resource/en/datasheet/stm32g474vb.pdf)
distingue les timers quadrature des timers TIM15/16/17 à compteur montant.
Les extraits officiels consultés du [RM0440, section 31.7.3](https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
montrent que TIM15 possède un contrôleur esclave ; sa présence ne suffit pas
à établir une interface quadrature. Le téléchargement complet du manuel a
échoué (limite du lecteur web puis délais réseau) ; l'audit utilise les extraits
ST accessibles et la confirmation croisée datasheet/CMSIS/HAL. TIM15 n'est
jamais initialisé ni démarré en encodeur dans la solution livrée.

### Solution adoptée et gestion des erreurs

`quadrature_decoder.c` est une table Gray à 16 transitions, A en bit 1 et B
en bit 0 : 00→01→11→10→00 compte positivement, l'ordre inverse négativement.
Le compteur signé int64 refuse tout overflow, conserve la dernière valeur
représentable et signale un défaut. Les diagnostics uint32 saturent aussi.

`thumb_encoder.c` capture les deux niveaux dans un seul IDR. Il ne modifie que
les pending EXTI14/15, selon leur sémantique W1C, et laisse les autres lignes
partagées au dispatch HAL. Sur MASTER, il ne prend pas possession de ces lignes.
Le handler ne contient ni attente, ni allocation, ni API RTOS. La priorité 3
évite le masquage BASEPRI de FreeRTOS ; PRIMASK sérialise lectures 64 bits,
calibration, reset et interruptions. Les autres callbacks de cette IRQ partagée
doivent respecter l'absence d'API RTOS à cette priorité ; aucun bouton BSP n'est
activé dans le startup OLD_HW actuel.

La validité THUMB exige aussi que son IRQ NVIC reste activée. Une IRQ désactivée
invalide le feedback et bloque le reset de référence ; celui-ci refuse également
une configuration GPIO/EXTI incohérente. Une acquisition inactive ne peut donc
pas rafraîchir artificiellement une position figée.

Une diagonale, un état répété malgré un edge, un désaccord pending/niveaux ou
les deux lignes pending simultanément signifient que l'ordre des transitions
est inconnu. Le décodeur ne reconstruit pas de mouvement supposé : il verrouille
le défaut, fige le compte fiable et invalide le feedback. Si THUMB est activé,
cette invalidation appelle immédiatement le STOP moderne depuis l'IRQ, sans
RTOS. Un rebond légal aller/retour s'annule ; un rebond agrégé provoque un
défaut. Du bruit reproduisant une séquence Gray légale reste indistinguable
d'un déplacement : la qualité des signaux doit être vérifiée physiquement.

Diagnostics supplémentaires : `g_thumb_encoder_invalid_transitions`, masque
`g_old_hw_encoder_unavailable_mask`, et flag télémétrie 19 pour un feedback
sélectionné indisponible/non calibré. Les bits 16/17 indiquent MASTER/SLAVE,
18 l'interdiction de mouvement du build et 20 l'acquittement de preflight valide.

### Fréquence et viabilité sous charge

Le C historique donne 12 PPR par canal, quadrature ×4 et réducteur 61:1.
La [fiche fabricant MOONS du modèle DCU10025P12-S104/RS10-12/PG10C](https://www.moons.com.cn/p/dcu10025-series-coreless-brushed-dc-motors/dcu10025p12-s104%26rs10-12%26pg10c-0064-000004611185000352)
indique 206 tr/min de sortie à vide, réduction 61 et encodeur 12 lignes.
Sous l'hypothèse que cette variante correspond au moteur monté, à 12 V :

```text
vitesse rotor ≈ 206 × 61 = 12 566 tr/min
fréquence totale des edges ≈ 12 566 / 60 × 12 × 4 = 10 053 edges/s
intervalle moyen ≈ 99,5 µs ; ≈ 16 910 cycles CPU à 170 MHz
marge d'essai proposée ×2 : ≈ 20,1 kedges/s, intervalle ≈ 49,7 µs
```

L'estimation historique à 15 tours/s d'arbre encodeur sous-estime sensiblement
cette variante. Ce calcul est une estimation à vide, pas une limite absolue
de survitesse ni la preuve du SKU réellement installé. L'ISR est bornée et
les tests vérifient deux millions de transitions ordonnées, mais ils ne
mesurent pas le STM32. La durée maximale des régions PRIMASK, du handler et
de ses chemins STOP doit être mesurée avec UART, RTOS, PID et faults actifs.
Un intervalle de masquage voisin de 100 µs peut agréger les edges. Le défaut
serait détecté, mais l'acquisition deviendrait indisponible.

La solution est donc viable à investiguer sans recâblage, pas encore qualifiée
pour entraîner THUMB. Garder `THUMB_EXTI_MOTION_QUALIFIED=0` jusqu'à ces mesures,
la confirmation du moteur et des signaux. Si le budget ne peut pas être tenu,
il faudra réaffecter/rerouter THUMB vers les canaux 1/2 d'un timer quadrature
compatible après revue complète du mapping RET6 ; contourner TIM15 n'est pas
une solution matérielle valide.

## Commissioning et conditions préalables au mouvement

L'image dédiée conserve les restrictions modernes : un seul doigt indépendant
(IDs 0–4), qui doit être local. MASTER peut donc commissionner LITTLE avec ID 4 ;
SLAVE peut sélectionner THUMB/INDEX/MIDDLE/RING, avec le verrou THUMB additionnel.
Les poignets couplés et PALM ne sont pas ouverts au commissioning individuel
par ce lot. Leur primitive de registres est testée sur le modèle hôte.

Le protocole reçoit l'extension additive `OUTPUT_PREFLIGHT=11`, sans changement
des numéros des champs existants ni des opérations 1–10. Les bindings C++ et
Python sont mis à jour ensemble ; un test vérifie le décodage C++ d'un message
produit avec les champs existants. `hw_monitor.py` accepte uniquement la forme
explicite `preflight verified`. Pour cette opération, le booléen existant
`stationary_reference_verified` porte l'attestation opérateur. Ce n'est pas
une mesure électrique réalisée par le firmware.

Conditions cumulatives, à respecter avant tout futur essai physique :

1. Vérifier alimentation, niveaux encodeurs, câblage ENA/IN1/IN2, absence de
   jumper forçant ENA actif, diodes/charge du pont et moyen de coupure indépendant.
   Mesurer ENA au boot/reset, au zéro, à l'inversion, au STOP et au fatal.
2. Valider l'état roue libre, les délais d'inversion et les limites mécaniques/
   PWM avec le matériel réel et une alimentation limitée. Aucun courant n'est
   mesuré par ce firmware ; ne pas supposer une protection de surcharge.
3. Pour THUMB, qualifier séparément l'acquisition EXTI sous la charge maximale
   et seulement alors envisager `THUMB_EXTI_MOTION_QUALIFIED=1` dans une image dédiée.
4. Configurer des limites conservatrices via `limits`, calibrer l'encodeur à
   partir d'une référence physique vérifiée, vérifier les comptes et le sens,
   obtenir du feedback frais et une session Hello saine.
5. Récupérer explicitement tout STOP, puis fournir `preflight verified`, `arm`,
   `enable` et une **nouvelle** commande `target`. Les paramètres/calibrations
   n'arment jamais. Les plafonds runtime commencent indisponibles.

L'acquittement est associé à l'epoch STOP courante. STOP/récupération exigent
donc un nouvel acquittement et un nouvel armement. La session, le feedback,
la garde, la borne de contrôle et la durée d'armement restent supervisés.
Les images dédiées sont isolées dans `build/HW-single-*-optin` et ne remplacent
ni `build/HW` ni l'alias Debug matériel. **Elles ne sont pas déclarées
électriquement sûres et n'ont pas été flashées.**

## Validations réellement exécutées

Depuis la racine, commandes exactes :

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

Chaînes : ARM GCC 14.3.1, GNU Make GnuWin32 3.81, GCC/G++ hôte MinGW 6.3.0,
Python 3.12.8. `protobuf` 7.36.2 a été installé uniquement dans
`build/lot2-tools` pour exécuter les tests de codec/monitor ; aucun environnement
Python global du projet n'a été modifié. La lecture documentaire a également
utilisé pypdf 6.20.1 dans ce dossier temporaire.

| Vérification | Résultat effectif |
|---|---|
| Clean ARM HW par défaut | PASS, exit 0 ; text=104840, data=100, bss=31320 octets |
| Clean ARM SIM | PASS, exit 0 ; text=71360, data=96, bss=31272 octets |
| Clean ARM commissioning INDEX et LITTLE | PASS, exit 0 chacun ; text=111808, data=100, bss=31344 octets chacun |
| Compilation des quatre images | Aucun warning/error de compilation dans les logs finaux |
| Linker/symboles/vecteurs | PASS ; aucun symbole non résolu ; SysTick/SVC/PendSV port RTOS et TIM6 HAL conservés ; EXTI THUMB fort en HW seulement |
| Inhibition des images ordinaires | PASS ; absence d'Arm et de la primitive PWM non nulle ; absence des périphériques/driver QET6 actifs |
| Images de commissioning | PASS ; vrai chemin L298N lié avec vérifications preflight/feedback/epoch ; THUMB non qualifié dans les flags |
| Alternance HW/SIM/HW-Debug | PASS ; 144 objets isolés inchangés, alias HW préservé, options dangereuses non cadrées refusées |
| Suite OLD_HW conservée | PASS ; 43 scénarios, deux rôles et chaque échec HAL startup injecté, wraps 16/32 bits, boot désarmé et fatal avant/après initialisation |
| Suite lot 2 dédiée | PASS ; 37 scénarios, toutes sorties de processus à 0 : 16 transitions Gray, 2M edges, overflow, rebonds/pertes, intégration BSP/PID/guard/STOP, PWM, inversion, saturation, concurrence, stale/faults, réinitialisation, deadline, wrap temporel, IRQ THUMB désactivée, qualification THUMB refusée et protocole C++ |
| Startup existant | PASS ; 31 scénarios hôte ordinaires |
| Safety existant | PASS ; suites async/HW, disarm, cleanup, commissioning et BSP ; contrôles anciens conservés comme non-régression |
| Communications existantes | PASS ; 24 scénarios de production, bornes SIM, saturation/STOP/timeout et codec HW Python |
| Outputs existants | PASS ; tests registres historiques et 20 scénarios flash avec programmeur factice uniquement |
| Patch 7 et monitor | PASS ; commissioning/Kalman, onze commandes explicites et attestation preflight |
| Simulation native | PASS ; 27 scénarios PC, sans ROS/Gazebo ni port série réel |
| Espaces/conflits | `git diff --check` PASS, exit 0 |

Les logs sont dans `build/lot2-*.log`. La nouvelle suite conserve les commandes,
stdout/stderr et codes Windows décimaux/hexadécimaux dans
`build/old-hw-lot2-tests/results.json` : **45 commandes finales, toutes exit 0
(0x00000000)**, incluant compilations et 37 scénarios exécutés.

Pendant le développement, deux assertions de nouvelle fixture ont retourné
exit 3 (0x00000003), pas un accès mémoire Windows : le modèle différant BSRR
perdait une écriture sur un même port, puis le test attendait un PWM non nul
dès un pas PID de 0,01 %, inférieur à un tick. Les écritures de direction ont
été regroupées atomiquement ; la fixture vérifie maintenant le zéro quantifié
et laisse le PID produire un pas représentable. Aucun test n'a été désactivé.
Aucun crash de type 0xC0000005 n'a été observé dans les exécutions finales.

ASan/UBSan restent indisponibles : les probes échouent à l'édition de liens
(exit 1, bibliothèques `-lasan`/`-lubsan` absentes), sans exécutable sanitizer
validé. Cela est distinct d'un passage de tests instrumentés ou d'un crash
du firmware. Les tests ordinaires passent ; la suite dédiée n'est pas présentée
comme exécutée sous sanitizer.

Aucun encodeur, moteur, signal PWM, courant, débit IRQ, scheduler ou cutoff
n'a été testé sur matériel. Les registres, périphériques et IRQ des suites
hôte sont modélisés. Les quatre builds ARM sont de vraies compilations et
inspections binaires, pas des exécutions sur STM32.

## Fichiers modifiés, ajoutés et retirés depuis le lot 1

Les wrappers du lot 1 emploient désormais `N_PULSE_OLD_HW`, avec le marqueur
de milestone `N_PULSE_OLD_HW_LOT2` dans le build. Ce renommage explique les
changements de plusieurs fichiers Core/comm ; il n'ajoute pas de plateforme
matérielle générique. L'inventaire détaillé avant rédaction de ce rapport est
enregistré dans `build/lot2-inventory.json`.

Modifiés :

- `Core/Inc/main.h`, `old_hw.h`, `stm32g4xx_hal_conf.h`, `stm32g4xx_it.h`.
- `Core/Src/main.c`, `old_hw.c`, `fatal_stop.c`, `stm32g4xx_hal_msp.c`, `stm32g4xx_it.c`.
- `build.ps1`.
- `fw/app/hw_commission.c`, `hw_commission.h`, `hw_telemetry.c`.
- `fw/bsp/encoder/encoder_bsp.c`.
- `fw/comm/board_link.c`, `board_link.h`, `board_uart_compat.c`, `uart4_transport.c`.
- `fw/comm-stack/protocol_messages.proto`, `protocol_messages.h`, `PyUART/protocol_messages_pb2.py`.
- `fw/motors/motor_control/drv8848.h`, `motor_backend_hw.c`.
- `fw/motors/motor_l298n/motor_l298n.c`, `motor_l298n.h`.
- `tests/old_hw/run_tests.py`, `test_old_hw.c`, `mocks/stm32g4xx_hal.h`.
- `tests/startup/test_build_modes.py`, `tests/patch7/test_monitor.py`.
- `tools/firmware.mk`, `build_artifacts.py`, `verify_startup_build.py`, `hw_monitor.py`.

Ajoutés :

- `fw/bsp/encoder/quadrature_decoder.c`, `quadrature_decoder.h`, `thumb_encoder.c`, `thumb_encoder.h`.
- `fw/motors/motor_l298n/l298n_config.h`.
- `tests/old_hw/run_lot2_tests.py`, `test_l298n.c`, `test_quadrature.c`, `test_preflight_protocol.cpp`.
- `docs/OLD_HW_LOT2.md`.

Retiré : `fw/motors/motor_l298n/motor_l298n_lot1.c`, remplacé par le module
L298N fonctionnel. Aucun algorithme moderne PID/safety/guard/Kalman ni test
existant n'a été supprimé.

## Bilan et dépendances pour le lot 3

**Le lot 2 logiciel est terminé et permet de commencer le lot 3**, en gardant
le build par défaut désarmé. Cela n'autorise aucun essai moteur physique.
Les blocages de validation matérielle restent : mesures des sorties/coupures,
référence/sens des encodeurs, identification exacte du moteur et qualification
temps réel THUMB. Les protections courant ne sont pas disponibles.

Le lot 3 doit fournir un transport USART3 moderne pour commandes, feedback et
STOP distribués : ownership MASTER/SLAVE, fraîcheur, sessions, epochs propres
aux cartes, validation/ordre des messages et gestion de perte de lien. Ne pas
confondre les compteurs locaux de deux cartes ni réutiliser des epochs d'une
session précédente. Le réarmement distant doit exiger la disponibilité et
l'autorisation du backend local, sans lever implicitement ses verrous.

USART3 PB10/PB11 est toujours préparé mais son IRQ et sa réception de commandes
moteur restent désactivées. Les API inter-cartes refusent les demandes de
mouvement. LPUART1 reste disponible pour diagnostic sur chacune des cartes.
Les poignets couplés, la coordination multicarte et les qualifications
électriques nécessitent leur propre validation ; aucune de ces garanties
n'est déclarée acquise par ce lot.
