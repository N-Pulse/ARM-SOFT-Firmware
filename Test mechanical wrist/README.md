# Test Mechanical Wrist

Banc autonome à deux moteurs pour **NUCLEO-G474RE / STM32G474RET6** et un module
**L298N**. Ouvrir directement ce dossier dans VS Code. Sources, HAL/CMSIS, scripts
et tests sont locaux : le dossier peut être déplacé hors du dépôt parent.

## Matériel et paramètres inconnus

Deux moteurs identiques `1627.G0042 SXR 0626`, chacun avec réducteur
`16GPT 203:1 8000777`. Alimentation disponible : **12 V, limitée à 1 A au total**.
La tension nominale des moteurs et la référence, résolution, position, tension
et interface des encodeurs restent à identifier. Les limites mécaniques et la
relation entre angles des réducteurs et angles anatomiques sont inconnues.

**12 V disponibles ne signifie pas que ces moteurs acceptent 12 V.** Le PWM
limite le rapport cyclique, pas la tension des impulsions. Identifier la variante
du moteur avant alimentation. Ne pas alimenter un encodeur inconnu en 12 V ni
relier ses sorties au STM32 sans vérifier leurs niveaux électriques.

Le banc démarre désarmé. Le mouvement exige les deux calibrations, un zéro commun,
les deux PID, limites et safety. Aucun PPR ou paramètre physique n'est inventé.
Les compteurs bruts fonctionnent sans calibration. Les confirmations `ack wiring`
et `verify encoder` sont supprimées ; `arm`, `stop` et les protections restent.

## Configuration facile à modifier

**`bench_config.py`** regroupe les paramètres PC. `None` signifie « inconnu » ; un
profil incomplet est refusé avant envoi. Relancer Python après modification.
Les deux entrées `MOTORS[1]` et `MOTORS[2]` sont indépendantes.

| Champ | Valeur à renseigner |
| --- | --- |
| `ppr` | Cycles d'un canal par tour d'encodeur. Diviser par 4 si la fiche fournit les comptes après décodage x4. |
| `gear_ratio` | Tours moteur / tour de sortie : 203 nominal, à remplacer par le rapport exact lorsqu'il sera connu. |
| `encoder_sign` | `1` ou `-1`, indépendamment de la polarité moteur. |
| `encoder_location` | `"motor"` avant réducteur ou `"output"` après réducteur. |
| `limits` | `[MIN_DEG, MAX_DEG, STEP_DEG, VELOCITY_DEG_S, MOVE_MS]`. |
| `pid` | `[KP, KI, KD, I_LIMIT, TAU, PWM, TOLERANCE_DEG]`. |
| `safety` | `[DELTA, STALL_PWM, STALL_DEG, STALL_MS, SETTLE_DEG_S, HOLD_ERROR_DEG, HOLD_ERROR_MS]`. |

Utiliser des nombres justifiés par le montage ; DELTA et les durées en ms sont
des entiers. Les listes non renseignées restent `None`. On peut également saisir
directement les commandes de configuration dans la console.

**`firmware/wrist_config.h`** contient les paramètres nécessitant recompilation :
contrôle 10 ms, watchdog de communication 500 ms, limite technique PWM 25 %, délai
d'inversion et polarité électrique `WRIST_MOTOR1_POLARITY` / `WRIST_MOTOR2_POLARITY`.
Ces bornes logicielles ne sont pas des caractéristiques validées des moteurs.
Une commande positive doit augmenter la position encodeur calibrée correspondante.

## Pinout proposé

Le **L298N a deux ponts pour deux moteurs DC**, avec quatre bornes `OUT1..OUT4`.
Un bornier à trois positions peut correspondre à l'alimentation VS/GND/5V,
pas à trois sorties moteur. Vérifier le marquage du module réel.

| Fonction | STM32 | Connecteur Nucleo | Destination |
| --- | --- | --- | --- |
| PWM M1 | PB7, AF2 TIM4_CH2 | CN7-21 | ENA |
| Sens M1 + | PC4 GPIO | CN10-34 via SB33 ; autre accès CN10-35 via SB13 | IN1 |
| Sens M1 − | PC5 GPIO | CN10-6 via SB3 | IN2 |
| PWM M2 | PB6, AF2 TIM4_CH1 | CN10-17 / CN5-3 D10 | ENB |
| Sens M2 + | PC6 GPIO | CN10-4 | IN3 |
| Sens M2 − | PC8 GPIO | CN10-2 | IN4 |
| Encodeur M1 A | PA0, AF1 TIM2_CH1 | CN7-28 / A0 | A après vérification électrique |
| Encodeur M1 B | PA1, AF1 TIM2_CH2 | CN7-30 / A1 | B après vérification électrique |
| Encodeur M2 A | PA6, AF2 TIM3_CH1 | CN10-13 / CN5-5 D12 | A après vérification électrique |
| Encodeur M2 B | PA7, AF2 TIM3_CH2 | CN10-15 / CN5-4 D11 | B après vérification électrique |
| USB série PC | PA2/PA3 LPUART1 | Routage interne ST-LINK | USB ST-LINK de la Nucleo |
| Masse commune | GND | CN7-20 ou CN7-22 | Driver, alimentation et référence encodeurs |

- M1 entre OUT1/OUT2 ; M2 entre OUT3/OUT4. Retirer les cavaliers **ENA et ENB**.
- Positif de puissance vers VS du module, négatif vers GND. Alimenter la Nucleo
  par USB ST-LINK ; ne jamais connecter 12 V à ses broches logiques.
- Le cavalier régulateur/5 V dépend du module : vérifier son fonctionnement et
  ne pas connecter deux sources 5 V ensemble. L'alimentation des encodeurs reste
  à déterminer. Les entrées A/B n'ont pas de pull-up interne ; collecteur ouvert,
  différentiel ou tension incompatible peuvent nécessiter une interface externe.
- Vérifier les ponts de la révision réelle : SB17/SB23 ON pour VCP,
  SB12/SB20/SB18/SB22 OFF pour éviter les conflits VCP/direction ; SB33/SB3 ON
  pour les accès directs PC4/PC5 ci-dessus, SB21 OFF pour libérer PA0 du bouton.
  Ne pas utiliser de shield occupant ces broches.
- TIM2 est sur 32 bits, TIM3 sur 16 bits. Le comptage signé gère les débordements,
  avec strictement moins de 32768 comptes par échantillon sur M2 ; le demi-tour
  de compteur ambigu est refusé. Régler DELTA et la vitesse avec le PPR réel.

Sources du pinout : [ST UM2505, tableaux 15/16](https://www.st.com/resource/en/user_manual/um2505-getting-started-with-stm32g4-series-nucleo64-board-stmicroelectronics.pdf),
[STM32G474, fonctions alternatives](https://www.st.com/resource/en/datasheet/stm32g474re.pdf),
[L298, double pont](https://www.st.com/resource/en/datasheet/l298.pdf).
Le câblage et les signaux réels restent à tester.

## Compiler et programmer

Python **3.10+** et ARM GNU dans PATH. GNU Make et CubeIDE ne sont pas requis.
Depuis **ce dossier** :

```powershell
.\build.ps1 -Rebuild
```

Produit `build/wrist-bench.elf`, `.hex`, `.bin`. La compilation recompile toutes
les sources et ne programme rien. Variante : `python tools/build.py --rebuild`.

Alimentation moteur coupée, utiliser STM32CubeProgrammer ou le script suivant
en remplaçant le numéro ST-LINK :

```powershell
.\flash.ps1 -SerialNumber VOTRE_NUMERO_STLINK -DryRun
.\flash.ps1 -SerialNumber VOTRE_NUMERO_STLINK
```

Programmer exclusivement `build/wrist-bench.elf` pour ce client. L'ancien firmware
à un moteur n'est pas compatible.

## Installer et lancer le client

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe wrist_bench.py --list
.\.venv\Scripts\python.exe wrist_bench.py --port COM6 --csv wrist.csv --save-plot wrist.png
```

Remplacer COM6 par le port ST-LINK. Les graphiques s'ouvrent **par défaut** ;
`--no-plot` donne la console seule. Pour chacun des moteurs : position/consigne,
erreur, PID/PWM appliqué et vitesse. Les angles sont en degrés de **sortie de
réducteur**. Sans calibration, le premier graphique affiche les comptes bruts avec
l'unité correspondante. Aucun angle anatomique du poignet n'est affiché.

À la connexion, le client envoie `stop`, puis entretient automatiquement le
heartbeat. Aucune calibration, mise à zéro ou commande d'armement n'est automatique.
Fermer la fenêtre, `quit` ou Ctrl+C coupe les deux moteurs. Une erreur de protocole,
un timeout de commande ou une rupture série termine la session, sans reconnexion
ni réarmement automatiques. La liaison série est à 115200 bauds, 8N1.

## Commandes du bench

M vaut **1 ou 2**. Angles en **degrés de sortie de réducteur**, vitesses en degrés/s,
durées en ms, PWM en %. Les gains du banc d'origine en radians ne sont pas
directement interchangeables avec les gains en degrés de ce banc.

| Commande | Fonction |
| --- | --- |
| `pc_help` / `help` | Aide PC / firmware. |
| `status` / `encoder` / `fault` | Mesures, configuration et état des deux moteurs. |
| `show_config` | Affiche le profil du PC. |
| `calibrate_config` | Applique les deux calibrations du fichier si elles sont renseignées. |
| `calibrate M PPR 4 GEAR SIGN LOCATION` | Calibration individuelle ; SIGN = 1 ou -1 ; LOCATION = motor ou output. Invalide le zéro commun et les deux limites. |
| `zero` | Zéro commun après calibration, poignet placé manuellement au neutre. Invalide les limites. |
| `setup` | Applique PID, safety et limites du fichier, après `zero`, sans mouvement. |
| `limits M MIN MAX STEP VELOCITY MOVE_MS` | Intervalle de position, déplacement maximal par consigne, vitesse mesurée maximale, délai d'arrivée. |
| `pid M KP KI KD I_LIMIT TAU PWM TOLERANCE` | Gains %/degré, %/(degré·s), %·s/degré ; contribution intégrale max %, filtre dérivé s, plafond PWM %, tolérance degrés. |
| `safety M DELTA STALL_PWM STALL_DEG STALL_MS SETTLE_DEG_S HOLD_ERROR_DEG HOLD_ERROR_MS` | Anomalie de compteur, seuils de blocage, vitesse d'établissement et erreur prolongée de maintien. |
| `arm` | Arme les deux moteurs à PWM nul ; maintien à partir de la première consigne. |
| `move motor M DEG` | Cible absolue d'un moteur, conserve la consigne de l'autre. |
| `move motors DEG1 DEG2` | Deux cibles validées ensemble avant application. |
| `move flex DEG` | Coordonnée différentielle, moteurs en sens opposés. |
| `move rotation DEG` | Coordonnée commune, moteurs dans le même sens. |
| `move wrist FLEX ROTATION` | Combine les deux coordonnées. |
| `jog ...` | Mêmes formes que move, mais incréments relatifs aux consignes courantes ; aux positions mesurées si aucune consigne active. |
| `stop` / `disarm` | Coupe les deux sorties, désarme et libère le couple. |
| `!` / `estop` | Coupe les sorties et mémorise un défaut d'arrêt d'urgence. |
| `clear_fault` | Efface le défaut avec sorties désactivées ; zéro et limites à refaire. |
| `stream on HZ` / `stream off` | Télémétrie de 1 à 20 Hz, limitée aussi par le débit série. |
| `test step AXIS DELTA` | Échelon positif après armement explicite. |
| `test positive AXIS DELTA` / `test negative AXIS DELTA` | Échelon signé, DELTA strictement positif. |
| `test repeat AXIS DELTA COUNT` | 1..20 cibles alternées autour du point initial, puis retour au point initial. |
| `save fichier.png` | Exporte les graphiques de la session. |
| `quit` | Arrête les deux moteurs et ferme la connexion. |

AXIS vaut `flex`, `rotation`, `m1` ou `m2`. Les cibles alternées sont espacées de
`2 × DELTA`, toujours soumises aux limites individuelles. Les tests s'arrêtent
sur défaut, refus ou déconnexion sans réarmement automatique. Après réussite, le
maintien reste actif ; `stop` le libère.
La console reste accessible pendant un test. `stop`, `disarm` ou `!` pendant
un test l'annule par arrêt prioritaire et mémorise un défaut d'arrêt d'urgence.

## Coordonnées et maintien de position

```text
M1 = rotation + flex
M2 = rotation - flex
flex     = (M1 - M2) / 2
rotation = (M1 + M2) / 2
```

`flex = 10` signifie des cibles de réducteurs +10° et −10° si rotation vaut zéro,
**pas 10° de flexion anatomique**. Les signes flexion/extension et
pronation/supination seront identifiés sur le montage réel. Modifier un axe
conserve la consigne de l'autre. Les deux cibles sont appliquées ensemble, mais
deux PID indépendants ne garantissent pas des trajectoires parfaitement
synchronisées sous charge. Il n'y a pas de rampe de vitesse ni d'asservissement
croisé ; la vitesse mesurée est surveillée par les limites.

Après établissement pendant 100 ms, l'état passe à `holding`. Les PID restent
actifs, y compris dans la tolérance, pour soutenir une charge. Un défaut sur un
moteur coupe **les deux**. Le montage doit supporter une perte de couple sur arrêt
ou perte de lien. Ce maintien régule la position, pas un couple mesuré.

Un blocage exige une erreur hors tolérance et un PWM appliqué dépassant le seuil :
un moteur immobile à sa consigne n'est pas déclaré bloqué uniquement parce qu'il
soutient une charge. En maintien, une erreur supérieure à HOLD_ERROR_DEG pendant
HOLD_ERROR_MS coupe les deux moteurs. MOVE_MS limite l'arrivée initiale, pas le
maintien établi. Il n'y a **ni mesure de courant ni mesure de température** ; les
limites thermiques du maintien prolongé restent à déterminer matériellement.

## Ordre de mise en service

1. Alimentation moteur coupée : programmer, ouvrir le client, inspecter `status`
   et `encoder`.
2. Identifier l'encodeur et son alimentation/interface ; vérifier les signaux et
   comptes. Le fonctionnement d'un timer ne prouve pas la présence d'un encodeur.
3. Renseigner les calibrations, puis `calibrate_config`. Vérifier sens et échelle
   avec un déplacement connu ; aucune confirmation logicielle à saisir ensuite.
4. Placer manuellement au neutre, puis `zero`.
5. Renseigner les limites/PID/safety adaptés, puis `setup` et `status`.
6. Une fois tensions et montage vérifiés, alimenter, `arm`, puis une petite
   consigne choisie pour ce montage.
7. Terminer par `stop`. Un lien absent au-delà de 500 ms, une boucle bloquée
   au-delà de 50 ms ou une panne d'un timer encodeur coupe les deux canaux dans
   l'interruption SysTick indépendante de la boucle PID. `!` coupe dans l'IRQ UART.

La configuration est volatile et doit être refaite après reset. Une boucle de
position correcte exige le PPR ; le logiciel ne peut pas le déduire du marquage
du moteur ou du rapport de réducteur.

## CSV, analyse et tests

Le CSV contient l'état global et pour M1/M2 : cibles, positions, erreurs, vitesses,
PID/PWM, comptes entiers et drapeaux de calibration. Les mesures indisponibles
restent vides. La télémétrie PC est moins fréquente que le contrôle à 100 Hz.

```powershell
.\.venv\Scripts\python.exe wrist_bench.py --analyze wrist.csv --save-plot analyse.png
.\.venv\Scripts\python.exe tests/run_tests.py
```

L'analyse calcule par moteur montée 10–90 %, établissement, dépassement et erreurs.
Utiliser un CSV avec un seul échelon. Les mesures insuffisantes ou ambiguës donnent
`available: false` ou des métriques nulles.

Les tests demandent GCC ou Clang **natif** dans PATH. Ils exécutent contrôleur et
protocole C, pilote matériel avec registres simulés et client Python, dont
CSV/PNG et coupures sur défaut. Les JSON du protocole C sont vérifiés par le
parseur Python. Aucun test logiciel ne valide le câblage ou le mécanisme réels.
Les premiers essais physiques restent à effectuer.

Les HAL/CMSIS dans `vendor/Drivers` et le démarrage STM32 sont copiés du dépôt
parent avec leurs avis/licences. PID, encodeur et communication reprennent les
composants du banc à un moteur, adaptés ici. Les modifications futures du banc
parent ne sont pas automatiquement répercutées dans ce dossier.

Validation logicielle réalisée : compilation ARM depuis ce dossier et depuis
une copie autonome dans un autre chemin ; tests natifs contrôleur/protocole et
registres simulés ; 10 tests Python, incluant CSV et export PNG. Les courbes de
vérification utilisent des données synthétiques, sans moteur connecté.
