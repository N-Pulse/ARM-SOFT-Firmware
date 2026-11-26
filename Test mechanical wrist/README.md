# Test Mechanical Wrist

Banc à deux moteurs pour **NUCLEO-G474RE + L298N**, avec interface en français.
Flexion/extension : moteurs en **sens opposés**. Rotation : moteurs dans le **même sens**.

## Installation — une seule fois

Ouvrir un terminal dans ce dossier, avec **uv installé**. Python 3.10+ avec Tkinter est requis pour
l'interface graphique ; le mode `--console --no-plot` fonctionne sans Tkinter.

**Mêmes commandes sous Windows, macOS et Linux** :

```sh
uv venv .venv
uv pip install --python .venv -r requirements.txt
```

Si `.venv` existe déjà, passer directement à la commande d'installation des
dépendances : uv peut réutiliser cet environnement. L'option `--python .venv`
cible explicitement celui de ce dossier, même si un autre environnement est actif.
Les commandes de lancement ci-dessous utilisent directement son interpréteur ;
il n'est pas nécessaire d'activer le venv.
Voir la [documentation uv](https://docs.astral.sh/uv/pip/environments/).

Le STM32 doit utiliser le firmware de ce dossier. Compilation (ARM GNU requis) :
`.\.venv\Scripts\python.exe tools/build.py --rebuild` sous Windows,
ou `.venv/bin/python tools/build.py --rebuild` sous macOS / Linux.
Pour programmer sous Windows / PowerShell (puissance moteur coupée, USB ST-LINK branché) :

```powershell
& "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -l stlink
.\flash.ps1 -SerialNumber VOTRE_NUMERO_STLINK
```

Sous macOS / Linux, avec le CLI de CubeProgrammer dans `PATH` :

```sh
STM32_Programmer_CLI -l stlink
STM32_Programmer_CLI -c port=SWD sn=VOTRE_NUMERO_STLINK -w build/wrist-bench.elf -v -rst
```

Remplacer le numéro ST-LINK par celui affiché. Le flash écrit `build/wrist-bench.elf`,
vérifie l'écriture puis redémarre le STM32. Le script Windows propose aussi `-DryRun`.
Le nouveau cycle s'exécute sur le PC et ne nécessite pas de reflasher un banc déjà compatible.

## Utilisation quotidienne

Lancer depuis ce dossier, selon le système :

```powershell
.\.venv\Scripts\python.exe wrist_bench.py
```

```sh
# macOS / Linux
.venv/bin/python wrist_bench.py
```

1. Brancher l'USB ST-LINK, choisir son port et cliquer sur **Connecter**.
2. Puissance moteur coupée : vérifier le profil dans `bench_config.py`, puis **Charger le profil encodeurs**. Le PPR par défaut de **512 n'est pas confirmé** (voir ci-dessous).
3. Placer **manuellement** le poignet au neutre, puis **Définir le neutre + réglages**. Cela définit le zéro commun et applique les PID, protections et limites.
4. Une fois le montage et l'alimentation prêts, cliquer sur **Armer les moteurs**.
5. Saisir le nombre de cycles et les amplitudes **positives** de flexion et d'extension, puis **Démarrer le cycle**. Les virgules décimales sont acceptées pour les angles.

Un retour au neutre est effectué au départ, puis **un cycle** vaut :

**Flexion +A → Extension −B → Neutre → Rotation +180° → Neutre.**

Chaque étape attend l'arrivée des **deux moteurs**. Le compteur augmente après le dernier retour au neutre, jusqu'à N cycles. Un arrêt ou un défaut interrompt la séquence.

Les boutons **Flexion**, **Extension**, **Tourner** et **Retour au neutre** commandent des positions absolues : l'autre axe revient à zéro. Le retour au neutre utilise le zéro existant, sans recalibrer.

- **Arrêter / libérer les moteurs** annule l'opération et désarme, sans retour automatique au neutre. Pour repartir : armer explicitement, puis relancer ; le compteur repart de zéro.
- **ARRÊT D'URGENCE** mémorise un défaut. Après correction de sa cause : effacer le défaut, replacer manuellement au neutre, refaire le neutre + réglages, puis armer.
- Une déconnexion ou la fermeture de l'application coupe les sorties. Il n'y a pas de reprise automatique.

Les mesures sont enregistrées dans **`recordings/`** au format CSV. **Courbes** affiche les mesures ; **Exporter PNG** enregistre leur image.

## PPR inconnu : valeur à identifier

**Le PPR réel des encodeurs est inconnu.** `ppr = 512` est conservé comme
**valeur par défaut non confirmée** pour les deux moteurs. La calibration
accepte ce profil, mais ne mesure ni ne vérifie le PPR : adapter cette valeur
aux mesures ou à la fiche de l'encodeur.

Le champ `ppr` désigne les périodes d'**un seul canal** par tour d'encodeur, **avant** décodage ×4. Quelques hypothèses possibles à comparer aux mesures :

| PPR candidat | Comptes par tour d'encodeur après ×4 |
| ---: | ---: |
| 32 | 128 |
| 64 | 256 |
| 128 | 512 |
| 256 | 1 024 |
| 512 | 2 048 |
| 1 024 | 4 096 |
| 2 048 | 8 192 |
| 4 096 | 16 384 |

Ces **hypothèses ne sont pas des options confirmées du modèle installé** ; la liste n'est pas exhaustive. Une fiche annonçant 2048 comptes/tour **après ×4** correspond à `ppr = 512`.

Pour départager les valeurs, garder les moteurs désarmés et la puissance moteur coupée : relever les **comptes bruts** avant/après un déplacement manuel connu d'une sortie de réducteur (commande console `encoder`). Avec encodeur **avant** le réducteur :

```text
PPR = |variation des comptes| × 360 / (4 × rapport de réduction × |angle de sortie en degrés|)
```

Avec un encodeur **après** le réducteur (`encoder_location = "output"`), retirer le rapport de la formule. Vérifier chaque moteur et le sens de comptage ; le rapport 203 est nominal.

Faire varier la valeur candidate si nécessaire, puis comparer l'angle affiché à un angle physique connu. Si l'angle affiché est doublé, le PPR saisi est deux fois trop petit, à rapport et position d'encodeur corrects. **Ne pas utiliser le cycle 180° pour identifier le PPR.**

Après modification : relancer l'application, recharger le profil, refaire le neutre + réglages, puis armer. `MOTORS[1]` et `MOTORS[2]` sont indépendants.

## Réglages et repères

**Les limites, PID et autres paramètres restent modifiables dans `bench_config.py`.**

| Paramètre | Contenu, dans l'ordre de la liste |
| --- | --- |
| `limits` | Position min, position max, déplacement max par consigne (°), ancien champ de vitesse (conservé pour le protocole), délai d'arrivée (ms). |
| `pid` | `KP`, `KI`, `KD`, limite intégrale, filtre dérivé `TAU`, plafond PWM (%), tolérance de position (°). |
| `safety` | `DELTA`, `STALL_PWM`, `STALL_DEG`, `STALL_MS`, `SETTLE_DEG_S`, `HOLD_ERROR_DEG`, `HOLD_ERROR_MS` ; détails commentés dans le fichier. |
| Encodeurs | `ppr`, `gear_ratio`, `encoder_sign`, `encoder_location`, séparément pour M1 et M2. |
| PC / cycle | `PORT`, `BAUDRATE`, `TELEMETRY_HZ`, `PLOT_HISTORY_SECONDS`, `CYCLE_FLEXION_DEG`, `CYCLE_EXTENSION_DEG`. |
| Trajectoire | `MOTION_SPEED_DEG_S` (9°/s) et `MOTION_ACCEL_DEG_S2` (4,5°/s²), pour les cycles et commandes simples. |
| PWM maximal | 50 % sur les deux moteurs, au 6e emplacement de `pid` pour chaque moteur. |

`motor_config()` fournit les valeurs communes initiales ; `MOTORS[1]` et
`MOTORS[2]` peuvent ensuite être ajustés séparément. Les amplitudes de cycle
préremplies permettent aussi d'utiliser `cycle N` en console.

Après modification, **enregistrer et relancer l'application** : le fichier est
chargé une fois au démarrage. Pour appliquer uniquement PID / limites / protections,
désarmer puis utiliser `setup` en console ; dans l'interface, replacer manuellement
au neutre puis **Définir le neutre + réglages**. Si les paramètres encodeurs ont
changé, recharger aussi le profil encodeurs avant de refaire le neutre.
**Aucune recompilation ni reprogrammation du STM32 n'est nécessaire pour modifier
les encodeurs, PID, limites, protections ou amplitudes de cycle.** `BAUDRATE`
doit rester identique au débit du firmware (115200 par défaut) ; ce champ PC
ne modifie pas le débit côté STM32.

```text
M1 = rotation + flexion
M2 = rotation − flexion
```

**Angles en degrés de sortie des réducteurs**, y compris les 180° : vérifier les sens et la correspondance avec le poignet réel. Les PID maintiennent la position après arrivée.

Pour les cycles et commandes simples, le PC envoie une seule commande par trajet :
`move wrist FLEX ROTATION VITESSE ACCELERATION` (°, °/s, °/s²). Reflasher la carte
et relancer le client pour utiliser cette commande. Le STM32 calcule la rampe
à chaque cycle de contrôle (10 ms). La télémétrie distingue la consigne instantanée
`target_deg` de la destination `goal_deg`. L'arrivée exige la fin de la rampe et
la stabilisation des deux moteurs. Les deux références
moteur suivent ensemble le même trajet, à 9°/s au maximum par défaut. Cette vitesse règle la rampe de consigne. La vitesse mesurée
reste disponible pour le PID, la détection de stabilisation et les courbes.
Le firmware borne le PWM à 50 % pour les deux moteurs dans les deux sens.
Un trajet de 180° prend environ 22 s de rampe, plus l'attente finale de position.
Un trajet prévu trop long pour le délai moteur est refusé avant mouvement.
Les commandes avancées `move`, `jog` et `test` restent des consignes directes.

Les cibles M1 = +180°, M2 = −180° correspondent à une **flexion/extension**.
Pour une **rotation +180°**, les cibles sont M1 = M2 = +180° : utiliser **Tourner**
ou `rotation 180`.

### Profil PID de départ et marge de mouvement

Le profil initial proposé pour les deux moteurs est **approximatif, à régler sur
le montage** :

```python
"pid": [15.0, 1.0, 0.2, 2.0, 0.02, max_pwm_percent, 1.0]
#       KP   KI   KD   Imax  TAU   PWM  tolérance
```

Le profil d’essai utilise `Kp = 15` pour les deux moteurs,
soit 15 % de PWM par degré d’erreur avant saturation.
`Kd = 0.2` freine selon la vitesse mesurée ; `TAU = 0.02` filtre cette dérivée
sur 20 ms. Avec `Ki = 1`, une erreur persistante de 0,5° ajoute 0,5 point
de PWM par seconde, avec une contribution intégrale limitée à ±2 points.
Le profil plafonne la PWM à **50 % sur les deux moteurs**, avec une
tolérance de **±1°**. Ces gains et ces plafonds ne sont pas des caractéristiques
validées des moteurs.

Les paramètres `M1_MAX_PWM_PERCENT` et `M2_MAX_PWM_PERCENT` règlent les plafonds
du profil PC. La carte refuse les valeurs supérieures à la borne du moteur
correspondant (50 % pour les deux moteurs). Le PWM est un rapport cyclique,
pas une limitation de courant. Une courbe centrée autour de zéro peut afficher
automatiquement une échelle ±0,05 % sans que cela représente le plafond configuré.

La plage logicielle **−200° à +200°** laisse 20° autour de la cible de rotation
à +180° ; elle ne valide pas la course mécanique. Le délai de `stall` est passé
de **500 à 1500 ms** : avec une erreur hors tolérance et un PWM appliqué d'au moins
2 %, un déplacement net inférieur à 0,1° sur cet intervalle déclenche toujours
l'arrêt des deux moteurs. Le délai d'arrivée est défini
dans `limits` ; les watchdogs du firmware restent actifs.

Pour ajuster, après vérification du PPR et du sens des encodeurs, utiliser de
petits échelons adaptés au montage et observer position, erreur et PWM :

1. Isoler l'effet de P avec `Ki = Kd = 0`. Essayer `Kp = 1`, puis `2`, puis `5`
   si nécessaire, une valeur à la fois. Si la réponse oscille, réduire Kp.
2. Si le mouvement dépasse la cible ou oscille, réintroduire D progressivement :
   `Kd = 0.05`, puis `0.1`, puis `0.2`, avec le filtre de 20 ms. Ce sont des valeurs
   d'essai, pas une garantie de stabilité.
3. Si le retour encodeur reste immobile alors que le PWM est déjà au plafond,
   augmenter Kp ne fournit pas plus de commande. Vérifier le retour encodeur,
   le driver, l'alimentation et la mécanique avant de modifier le plafond PWM.

Relancer l'application et appliquer les nouveaux réglages après chaque changement
du fichier, comme expliqué ci-dessus. Commencer par les mouvements simples avant
le cycle complet.

À la connexion, le client peut écarter une première ligne partielle si la lecture
commence au milieu d'une trame. Le statut complet est ensuite vérifié. Une trame
malformée commençant comme un objet JSON, un mauvais firmware ou toute corruption
après synchronisation restent bloquants ; le message d'erreur inclut la raison
et le début de la trame pour faciliter le diagnostic.

### Diagnostic des défauts

Le contrôle vérifie les vitesses toutes les 10 ms ; le CSV et les courbes sont
moins fréquents. Après un arrêt, la vitesse courante peut déjà être revenue à
zéro : le graphique ne montre donc pas forcément la mesure déclenchante.

Le firmware mémorise désormais le premier échantillon fautif **avant** de couper
les sorties : instant, intervalle de mesure, vitesse, limite, position, cible,
PWM appliqué, comptes bruts et présence d'une consigne active. Ce diagnostic
reste disponible jusqu'à **Effacer le défaut**, même après retour à l'arrêt.
Il est affiché dans le journal et enregistré dans les colonnes CSV `fault_*`.
Pour un `stall`, le journal indique la cible, la position, l'erreur et le critère
de progression chargé dans la carte (par exemple 0,1° sur 1,5 s). Une vitesse
instantanée non nulle n'empêche pas ce défaut si la progression reste insuffisante.
**Cette fonction nécessite de compiler et reflasher le nouveau firmware**, puis
de relancer l'application et de refaire la préparation. Avec un ancien firmware,
le client reste compatible mais indique que la mesure au déclenchement est indisponible.

À la connexion, le journal affiche aussi les paramètres de rampe chargés par
le client. Après modification des fichiers Python, fermer puis relancer l'application :
une fenêtre déjà ouverte conserve son ancien code et son ancien profil.

Avant la mise sous puissance, faire valider les tensions, courants, encodeurs,
filtres PWM et limites mécaniques adaptés au montage. Le logiciel ne mesure ni
courant ni température.

## Console et vérification logicielle

```powershell
.\.venv\Scripts\python.exe wrist_bench.py --list
.\.venv\Scripts\python.exe wrist_bench.py --console --port COM8 --no-plot
```

```sh
# macOS / Linux : exemple de port, à remplacer par celui affiché par --list
.venv/bin/python wrist_bench.py --list
.venv/bin/python wrist_bench.py --console --port /dev/ttyACM0 --no-plot
```

Utiliser le port réellement affiché par `--list` : par exemple `COM8` sous
Windows, `/dev/ttyACM0` sous Linux ou `/dev/cu.usbmodem…` sous macOS.

Préparation dans `wrist>` : `calibrate_config`, placement manuel au neutre, `zero`, `setup`, puis `arm`. Exemples de mouvements :

```text
cycle 10 20 15
neutre
flexion 10
extension 10
rotation -90
stop
```

`cycle 10 20 15` = 10 cycles, flexion +20°, extension −15°, rotation +180° (exemple uniquement). Attendre la fin avant une autre commande ; `stop` et `!` restent disponibles. `pc_help` affiche l'aide ; `move`, `jog` et `test` restent disponibles. En console, retirer `--no-plot` pour afficher les courbes.

Tests sans matériel :

```powershell
.\.venv\Scripts\python.exe tests/run_tests.py
```

```sh
# macOS / Linux
.venv/bin/python tests/run_tests.py
```

GCC ou Clang natif requis pour les tests C. Les simulations vérifient le cycle, les retours au neutre, les limites et les interruptions ; elles ne valident pas le montage physique.
