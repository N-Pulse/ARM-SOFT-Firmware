# Calibration des moteurs de la main — Guide complet

Ce guide explique, **de zéro**, comment tester et calibrer les 8 moteurs de la
main en boucle fermée (closed-loop encodeur). Quelqu'un qui n'a jamais touché
au projet doit pouvoir le reproduire en suivant ce document.

---

## 1. À quoi ça sert

Chaque moteur (doigt / paume / poignet) entraîne une articulation via un
réducteur. On veut connaître, **pour chaque moteur**, la position « ouvert » et
« fermé » réelles. Le mode calibration :

- pilote un moteur **en boucle fermée** avec son encodeur (pas de devinette au
  temps comme avant) ;
- fait un **auto-homing** : le moteur va se caler tout seul sur ses 2 butées
  mécaniques, mesure la **course réelle** et le **sens de câblage** ;
- permet d'ouvrir/fermer chaque moteur à la demande pour vérifier.

Ce mode **remplace** la pipeline normale (proto / `CLOSE_HAND`). C'est un outil
de bring-up. Pour revenir au fonctionnement normal, voir §8.

---

## 2. Prérequis

| Outil | Détail |
|-------|--------|
| STM32CubeProgrammer | installé (chemin par défaut, utilisé par `flash.ps1`) |
| Toolchain ARM GCC | déjà configurée pour `build.ps1` |
| Python 3 | avec `pyserial` : `pip install pyserial` |
| Matériel | les 2 cartes (motherboard + daughterboard), moteurs + encodeurs câblés, alims moteur + logique + **GND commun** |

Câblage des pins moteur/encodeur : voir [docs/wiring.md](wiring.md).

### Répartition des moteurs (rôle décidé par le strap `PC0`)

Le **même binaire** tourne sur les 2 cartes. Le strap `PC0` décide du rôle :

| Carte | `PC0` | Rôle | Moteurs locaux |
|-------|-------|------|----------------|
| Motherboard | **flottant** (laisser libre) | master | WRIST_X, WRIST_Y, LITTLE |
| Daughterboard | **relié à GND** | slave | INDEX, MIDDLE, RING, THUMB, PALM |

Chaque carte ne pilote QUE ses moteurs locaux. Chaque carte a son propre câble
USB ST-LINK = son propre port COM.

---

## 3. Activer le mode calibration

Ouvrir `Core/Src/main.c`, section `USER CODE BEGIN Includes` (vers le haut du
fichier). Il y a un bloc de toggle :

```c
/* #define WRIST_ENCODER_TEST */
#define MOTOR_CALIB_MODE
```

Pour la calibration : `WRIST_ENCODER_TEST` **commenté**, `MOTOR_CALIB_MODE`
**décommenté** (c'est l'état par défaut actuel). Une seule ligne active à la fois.

---

## 4. Compiler et flasher les 2 cartes

Dans un terminal PowerShell, à la racine `ARM-SOFT-Firmware/` :

```powershell
.\build.ps1      # compile -> Debug\N-PULSE FIRMWARE.elf
.\flash.ps1      # detecte TOUTES les sondes ST-LINK et flashe chaque carte
```

`flash.ps1` flashe automatiquement **toutes les Nucleo branchées** (même
binaire sur chacune). Pas besoin de débrancher. Il affiche les numéros de série
détectés puis `[1/2] OK`, `[2/2] OK`.

> Si une seule carte est branchée, il ne flashe que celle-là.

---

## 5. Trouver les ports COM

Brancher les 2 cartes en USB, puis :

```powershell
python -m serial.tools.list_ports -v
```

Repérer les 2 entrées « STMicroelectronics STLink Virtual COM Port (COMxx) ».
Noter les 2 numéros. (Aussi visible dans Gestionnaire de périphériques →
Ports (COM & LPT).)

Tu ne sais pas a priori quel COM est le master vs le slave : tu le verras à
l'écran (le master liste WRIST_X/WRIST_Y/LITTLE, le slave liste les doigts).

---

## 6. Lancer l'outil de calibration

Le script est `fw/comm-stack/PyUART/motor_calib.py`. Il faut **une instance
par carte** (une par COM), dans 2 terminaux séparés :

```powershell
# Terminal 1
python .\fw\comm-stack\PyUART\motor_calib.py --com COM6

# Terminal 2 (autre carte)
python .\fw\comm-stack\PyUART\motor_calib.py --com COM8
```

À la connexion, la liste des moteurs locaux de la carte s'affiche (commande
`?`). Tu tapes une commande + **Entrée**.

---

## 7. Procédure de calibration (par moteur)

Workflow recommandé, **automatique** :

```
?            liste les moteurs locaux + leur index
<index>      sélectionne un moteur (ex: 1 pour INDEX)
H            AUTO-HOME : le moteur cale ses 2 butées tout seul,
             mesure la course et le sens de câblage
o            va à la butée OUVERTE (re-zéro auto : open = 0.0°)
c            va à la butée FERMÉE  -> affiche la course réelle
x            si o ferme et c ouvre (inversé) -> corrige (une fois)
d            dump des courses mesurées de tous les moteurs locaux
```

Exemple concret (terminal slave, on calibre l'index) :

```
?            -> CAL [1] INDEX ... (et les autres)
1            -> CAL sel=INDEX
H            -> CAL INDEX HOMING...
                CAL INDEX butee1 cnt=-21 -> phase2
                CAL INDEX HOMED course=17.0 deg
o            -> CAL INDEX OPEN (butee, re-zero -> 0.0 deg)
c            -> CAL INDEX CLOSE (butee) course=17.0 deg
o            -> CAL INDEX OPEN (butee, re-zero -> 0.0 deg)
c            -> CAL INDEX CLOSE (butee) course=17.0 deg   (stable = fiable)
```

Refaire pour chaque moteur (sélection par index puis `H`, ou `A` pour
auto-homer **tous** les moteurs locaux d'un coup), sur **les 2 terminaux**
(master + slave). À la fin, `d` sur chaque terminal donne les courses à
reporter.

### Comment marche l'auto-home (`H`)

1. pousse dans un sens jusqu'à ce que l'encodeur n'avance plus (~0,35 s) =
   **butée 1** ;
2. mesure au passage le **sens de câblage** (corrige automatiquement « ça
   ouvre quand je veux fermer ») ;
3. pousse dans l'autre sens jusqu'à la **butée 2** ;
4. calcule la **course** et mémorise les 2 butées.

`o` / `c` refont ensuite un **calage physique** sur la butée (exactement comme
`H`, pas une visée de position) → identique à chaque fois, robuste à la dérive
de l'encodeur incrémental. À chaque `o`, l'encodeur est **remis à 0** sur la
butée ouverte : « ouvert » = toujours 0,0°, et `c` affiche la vraie course
sans dérive cumulée.

### Le poignet (terminal master)

Le poignet = 2 moteurs couplés (WRIST_X + WRIST_Y). En plus du `H`/`o`/`c`
par moteur, commandes couplées :

```
r / l     rotation droite / gauche (les 2 moteurs même sens)
f / F     flexion / extension      (les 2 moteurs sens opposé)
```

---

## 8. Référence complète des commandes

Tapées dans le terminal `motor_calib.py` (lettre + Entrée) :

| Cmd | Effet |
|-----|-------|
| `?` | liste les moteurs locaux : index, nom, position, sens, état |
| `0`..`7` | sélectionne un moteur par index (s'il est local à cette carte) |
| `H` | auto-home du moteur sélectionné (cale les 2 butées) |
| `A` | auto-home de **tous** les moteurs locaux |
| `o` | va à la butée OUVERTE (re-zéro encodeur : open = 0°) |
| `c` | va à la butée FERMÉE (affiche la course) |
| `x` | échange open/close du moteur (si `o`/`c` inversés) |
| `+` / `-` | jog fin ±3° (boucle fermée, réglage manuel) |
| `g<deg>` | va à un angle absolu (ex: `g30`, `g-15`) |
| `z` | définit la position actuelle comme 0° |
| `s` / `S` | stop le moteur sélectionné / tous |
| `i` / `i<n>` | inverse le sens de câblage (secours si « BLOQUE ») |
| `d` | dump des courses mesurées (à reporter dans `motor_map.c`) |
| `r` / `l` | poignet : rotation droite / gauche (master) |
| `f` / `F` | poignet : flexion / extension (master) |
| `q` | quitte le script (le firmware continue) |

---

## 9. Dépannage

| Symptôme | Cause / action |
|----------|----------------|
| `?` ne liste pas le moteur attendu | il est sur l'autre carte → utilise l'autre terminal |
| `o` ferme / `c` ouvre | open/close inversés → tape `x` une fois pour ce moteur |
| `CAL X BLOQUE/MAUVAIS SENS` | sens de câblage : tape `i` (ou `i<index>`) puis relance |
| le moteur ne bouge pas du tout | câblage L298N (PWM/IN1/IN2) ou alim moteur — voir wiring.md |
| `course` instable entre cycles (>2°) | jeu mécanique / glissement ; vérifier accouplement encodeur-arbre |
| course très petite (ex: 16°) | normal, c'est la vraie course de ce doigt (réducteur) |
| aucun port COM | mauvais câble USB (data, pas charge) / driver ST-LINK absent |
| `local=0` côté master | `PC0` pas flottant (carte vue comme slave) |
| rien ne s'affiche | mauvais COM, ou pas flashé, ou mode pas activé (§3) |

Sécurités intégrées : recul nul mais coupure du courant dès le calage (pas de
forçage continu), timeout 10 s par phase si l'encodeur est mort.

---

## 10. Revenir au fonctionnement normal

Dans `Core/Src/main.c` (§3), commenter `MOTOR_CALIB_MODE` :

```c
/* #define MOTOR_CALIB_MODE */
```

Puis `.\build.ps1` + `.\flash.ps1`. La pipeline proto normale
(`CLOSE_HAND`, etc.) reprend.

> Le closed-loop n'existe pour l'instant **que** dans ce mode calibration.
> Intégrer le closed-loop dans la pipeline normale (pour que `CLOSE_HAND`
> utilise les encodeurs) est une étape ultérieure séparée.
