# OLD_HW — procédure de vérification physique progressive

Date : 2026-10-14. Branche : `oldHardNewSoft`. Lire aussi [le rapport lot 4](OLD_HW_LOT4.md).

**Aucune opération physique décrite ici n'a été exécutée pendant le lot 4.**
Gate 0 concerne le logiciel. Gates 1, 2 et 3 restent NON EXÉCUTÉES. Le firmware
ordinaire interdit définitivement l'activation des moteurs ; une récupération
de défaut ne retire pas cette interdiction binaire. Aucun essai sous puissance
n'est autorisé par ce document.

## Règles communes et fiche de mesure

Un opérateur responsable valide chaque étape et consigne : identité des cartes,
SN ST-LINK et ports COM associés, strap PC0, SHA-256 ELF et `compile.flags`,
date/version du banc, câblage photographié, instruments et bandes passantes,
conditions de charge, résultats et captures. Ne pas confondre SN de la sonde et
identité du microcontrôleur ; vérifier leur association physique.

Pour B, C et D : **débrancher physiquement Vm des L298N et les fils des moteurs**.
Un réglage logiciel à zéro ou l'arrêt d'une alimentation ne remplace pas ce
débranchement. Vérifier au multimètre l'absence de tension Vm, y compris par
retour d'alimentation depuis USB, régulateur/jumper de module ou autre carte.
La logique STM32/L298N/encodeur peut être alimentée seulement après vérification
des tensions et des masses. Ne jamais injecter un signal dans un circuit éteint.

À toute anomalie : saisir `stop` sur les deux consoles si accessibles, utiliser
la coupure électrique indépendante, débrancher les sources concernées et ne
pas réarmer. L'absence d'acquittement ne prouve pas que le STOP est reçu.
Sur une image de mesure sans Vm, couper la logique en cas de défaut persistant.
Sur un futur banc sous puissance, couper **Vm en premier**. Un STOP logiciel,
le debugger et le timeout UART ne remplacent pas cette coupure physique.

Tout échec bloque le passage à l'étape suivante. Une reprise demande une cause
identifiée, un banc à nouveau vérifié et de nouvelles mesures. Aucun homing,
calibrage automatique, contournement du guard ou restauration d'ancienne cible.

## A — inspection hors tension

**Prérequis :** toutes les alimentations et USB débranchés, énergie stockée
évacuée. Instruments : schéma/netlist du montage réel, loupe et multimètre.

1. Confirmer sur chaque composant **STM32G474RET6, LQFP64** ; ne pas supposer
   que la documentation d'une carte QET6 décrit les pistes de ce montage.
2. Contrôler par continuité les huit lignes ENA/PWM, seize directions et seize
   entrées encodeur avec la table du rapport. Contrôler les conducteurs
   indépendamment de leur couleur et des étiquettes moteur.
3. PC0 : MASTER = niveau haut (pull-up interne), SLAVE = liaison à GND.
   Aucun strap vers une tension non compatible ; aucun changement sous tension.
4. USART3 : PB10 TX d'une carte vers PB11 RX de l'autre, et inversement ; GND
   commun. Host : PA2 TX / PA3 RX, LPUART1, adaptateur UART aux niveaux vérifiés.
   Ce sont des signaux logiques, pas une interface RS-232 à tensions bipolaires.
5. Retirer les jumpers ENA qui imposent un niveau haut sur les modules L298N.
   Vérifier aussi les jumpers de régulateur : logique et Vm ne doivent pas se
   réalimenter mutuellement. Contrôler la vraie topologie du module utilisé.
6. Vérifier alimentation, type de sortie (push-pull/open-collector), résistances,
   masse et amplitude des encodeurs. La tolérance éventuelle d'une broche ne
   justifie pas une injection 5 V sans revue électrique de toutes les conditions.
7. SWD : PA13/PA14 et NRST accessibles, masse de sonde raccordée, debugger en
   **SWD uniquement**. PA15 et PB4 sont des directions moteur sur SLAVE et
   entrent en conflit avec un usage JTAG complet. PB3/SWO n'est pas une sortie
   de mesure explicitement initialisée par le chemin actif.
8. Préparer une coupure d'urgence indépendante du logiciel pour la future
   alimentation moteur. Vérifier sa fonction hors énergie dangereuse.

**PASS :** identité, toutes les connexions/niveaux, séparation logique/Vm et
coupure documentés et acceptés. **FAIL :** composant, piste, jumper, tension ou
retour d'alimentation incertain : maintenir hors tension. Cette étape prouve
une inspection du montage ; elle ne qualifie ni le PWM ni le logiciel en cours
d'exécution.

## B — alimentation logique uniquement

**Prérequis :** A passé ; Vm et moteurs physiquement débranchés ; autorisation
explicite de l'opérateur avant toute programmation ou reset. Contrôler Vm=0 V
avant et après branchement USB et logique. Préparer l'oscilloscope sur ENA et
directions pour capturer le reset, y compris avant l'exécution du premier code C.

### Préparer les artefacts sur PC

Depuis la racine du dépôt, sans connexion matérielle nécessaire :

```powershell
.\build.ps1 -Mode OLD_HW -Rebuild
.\build.ps1 -Mode SIM -Rebuild
Get-FileHash -Algorithm SHA256 -LiteralPath build\HW\n-pulse.elf
Get-FileHash -Algorithm SHA256 -LiteralPath build\HW\compile.flags
Get-Content -LiteralPath build\HW\build-manifest.json
```

L'image physique ordinaire est `build/HW/n-pulse.elf` ; le même fichier sert aux
deux cartes. `Debug/N-PULSE FIRMWARE.elf` est un alias de même empreinte ; le
flasheur n'accepte que le chemin canonique. SIM est réservé aux tests virtuels.
Le manifeste doit indiquer `STM32G474RET6`, `OLD_HW_LOT4`, `HW` et
`physical_outputs=permanently_disabled` avec les deux empreintes correctes.
Dans les flags : OLD_HW=1, TARGET_RET6=1, DUAL_BOARD_LINK=1, CURRENT_ACQUISITION=0,
HW_SINGLE_MOTOR_TEST=0, OLD_HW_MOTION_OPT_IN=0, THUMB_EXTI_MOTION_QUALIFIED=0,
TIMING_DIAGNOSTICS=0 ; pas de `MOTOR_BACKEND_SIM`.

### Programmer chaque carte séparément, uniquement après autorisation

Remplacer les variables suivantes par les **SN relevés sur les sondes et
associés aux cartes**. Les commandes actives ci-dessous sont des instructions
pour une opération future autorisée ; elles n'ont pas été exécutées au lot 4.

```powershell
$masterProbe = 'SN_MASTER_VERIFIE'
$slaveProbe = 'SN_SLAVE_VERIFIE'
.\flash.ps1 -SerialNumber $masterProbe -DryRun
.\flash.ps1 -SerialNumber $slaveProbe -DryRun
```

Le dry-run valide fichier/flags/hash et énumère avec `-l` seulement. Il ne
connecte pas SWD et ne vérifie donc pas le silicium ciblé. Un seul ST-LINK
détecté est signalé ; le SN absent est refusé. Aucun choix automatique de la
deuxième carte et aucune boucle sur toutes les sondes. Une fois l'association
sonde/carte et l'identité RET6 physiquement confirmées, l'opérateur peut
autoriser puis lancer **une commande à la fois** :

```powershell
.\flash.ps1 -SerialNumber $masterProbe
.\flash.ps1 -SerialNumber $slaveProbe
```

Ces deux dernières commandes programment, vérifient et resetent leur seule
cible. Ne jamais les exécuter dans une campagne logicielle automatique.

### Observer le démarrage

Alimenter d'abord une carte puis l'autre. Ouvrir une console par UART host réel :

```powershell
python tools/hw_monitor.py --port COM_MASTER --motor 4 --link
python tools/hw_monitor.py --port COM_SLAVE --motor 1 --link
```

Remplacer les noms COM par ceux du banc. Installer/vérifier `protobuf` et
`pyserial` dans l'environnement Python du poste ; l'import du moniteur seul
n'ouvre aucun port. Le moniteur envoie Hello toutes les 500 ms. **Ce n'est pas
un instrument passif : `quit`, expiration de `--duration`, perte de diagnostic
ou ACK incertain envoient STOP.** Une console fermée peut donc faire arrêter
l'autre carte ; c'est attendu. Il n'y a aucun armement automatique.

Attendre les diagnostics : rôle correct, compteurs de séquences progressant,
worker lancé, sessions non nulles et différentes. Au démarrage, les défauts
DRIVER/LINK peuvent être verrouillés ; Hello ne les efface pas. Avant la seconde
carte, lien non frais et moteurs inhibés sont attendus. Utiliser `status 4`
sur MASTER, `status 1` sur SLAVE, puis `link` pour revenir au lien. Sur image
ordinaire, `status ID`/`raw ID` permettent aussi les autres IDs ; sur une image
single, seul l'ID choisi et la vue `link` sont sélectionnables.

Vérifier avec les instruments : ENA bas et IN1/IN2 bas après la prise en charge
logicielle, aucune impulsion dangereuse au reset, horloge nominale 170 MHz et
tick HAL/RTOS qui progressent. Avec debugger autorisé, consulter
`SystemCoreClock`, `HAL_GetTick`, les compteurs de tâches et les high-water marks
des piles ; une LED ou un timestamp UART seul ne suffit pas à prouver l'ensemble
des tâches. Ne pas arrêter le CPU en présence de puissance moteur.

**PASS B :** boot des deux cartes, rôles/diagnostics/clocks/tâches confirmés et
sorties inactives mesurées. **FAIL :** mauvais rôle, glitch ENA, reset en boucle,
tick figé, erreur d'initialisation : STOP/coupure logique, analyser avant reprise.
B ne prouve pas le comportement électrique en armement ni la capacité à arrêter
un moteur alimenté. Gate 1 nécessite aussi la communication physique de C.

## C — communication physique à deux cartes

**Prérequis :** B passé, deux consoles actives, Vm=0 V et moteurs débranchés,
autorisation explicite des débranchements/reset de test. Instruments : analyseur
logique USART3 et ENA, oscilloscope, captures des deux diagnostics `link`.

1. Vérifier PB10/PB11 à **115200, 8N1**, dans les deux directions. Les deux
   firmwares doivent parler **board protocol v2**. Ne pas mélanger un ancien
   lot 3 v1 avec v2 : le refus et STOP seraient attendus.
2. Attendre `established=True`, `link_fresh=True`, heartbeat et echo ages
   régulièrement faibles (timeout strictement `>150 ms`). Sessions indépendantes,
   epochs indépendants ; ils ne doivent pas nécessairement être égaux.
3. Sur chaque carte, attendre un diagnostic récent, saisir `link-recover`,
   attendre son ACK `OK`, puis observer les deux STOP locaux. S'il reste une
   cause indépendante, traiter sa cause et récupérer **sur la carte concernée**.
   La récupération du pair ne l'efface pas. Réessayer seulement après observation
   d'un état neuf, jamais en boucle aveugle. Tous les moteurs restent désarmés.
4. Sur les images ordinaires, le défaut initial DRIVER peut être récupéré après
   nettoyage et lien frais ; le verrou binaire de mouvement reste permanent.
   `link-authorize` sur chacune sert à tester l'autorisation distribuée une fois
   les STOP absents. Il ne remplace aucun calibrage, préflight ou armement.
5. Observer `raw ID` : un encodeur local démarré peut avoir un compte brut valide
   sans calibrage, alors que angle/vitesse/courant restent `unavailable`.
   Les IDs distants portent `source=remote` et un âge : c'est une copie reçue,
   pas une acquisition locale. Une valeur mise en cache n'est pas une mesure
   instantanée ; sur perte de lien/âge >100 ms les validités sont retirées.
6. Saisir `stop` sur MASTER, observer MANUAL local et LINK sur le pair ; répéter
   dans l'autre sens après récupération explicite. Récupérer seulement le pair
   et confirmer que MANUAL sur la carte d'origine persiste. Après récupération
   de celle-ci, confirmer encore `enabled=False`, `armed=False`, aucune cible
   reprise et ENA toujours bas.
7. Débrancher USART3, puis tester séparément chaque fil TX→RX pour panne
   unidirectionnelle. Mesurer depuis la coupure jusqu'aux diagnostics/STOP des
   deux cartes et, lors de D, jusqu'à ENA réellement bas. Le manque d'écho doit
   faire expirer aussi l'émetteur qui reçoit encore des données. Rebrancher :
   aucun réarmement implicite, nouvelle récupération requise.
8. Reseter une seule carte, puis l'autre lors d'un essai distinct. Observer
   changement de session, invalidation des états précédents et STOP du pair.
   Répéter démarrage tardif, carte absente et fermeture/perte d'une console host.
   Le délai host est 2000 ms, distinct du délai intercartes.
9. Sous trafic host/USART3 et signaux encodeurs à la charge maximale du banc,
   surveiller malformed/overflows/rejected/transport_errors. Toute injection de
   bruit/overflow doit être faite par un générateur maîtrisé, Vm débranché ; elle
   ne doit pas être ajoutée au moniteur normal. Une erreur peut verrouiller ou
   mettre le transport en quarantaine ; un reset explicitement autorisé peut
   être requis. Le retour de trafic n'est pas une permission de mouvement.

**PASS C :** échanges réellement bidirectionnels, recovery réalisable sans
mouvement, STOP indépendants préservés, reboot/perte unidirectionnelle détectés
et aucune reprise de cible ; captures conservées. **FAIL :** lien frais malgré
une panne durable, récupération qui efface le STOP de l'autre, sortie active ou
séquence inexpliquée : STOP/coupure. B+C permettent Gate 1, uniquement après
constat physique. Avec sorties déjà inactives, C seul ne mesure pas le délai
jusqu'à extinction d'un PWM actif ; cette mesure appartient à D.

## D — mesures PWM, arrêt et encodeurs, toujours sans Vm

**Prérequis :** A–C passés, revue du banc et autorisation explicite des images
spéciales/reset/debugger, Vm vérifié 0 V et fils moteurs débranchés. Oscilloscope
sur ENA, IN1, IN2 du canal choisi, analyseur USART3, générateur quadrature aux
niveaux compatibles et compteur matériel indépendant. Les autres canaux sont
observés pour vérifier leur inhibition. Ne jamais brancher un générateur en
parallèle d'un encodeur push-pull ; isoler la source remplacée.

### D1 — acquisition seule et limites d'observation

Sur l'image ordinaire, entraîner manuellement un moteur non alimenté ou injecter
une séquence A/B vérifiée. Contrôler chaque moteur sur sa carte propriétaire,
deux sens, retour à zéro du déplacement relatif, wrap 16 bits (TIM3/8/20), wrap
32 bits (TIM2) avec générateur et accumulation signée. Établir CPR/lignes par
tour, quadrature et rapport de réduction réellement mesurés avant toute
conversion radian. L'absence de calibrage doit rester explicite.

Comparer les comptes logiciels à un compteur externe, pas seulement à la forme
des deux bits lue par le STM32. Vérifier les taux maximum et interruptions
réels. La différence entre échantillons TIM doit rester inférieure à la
demi-plage du compteur ; des tours entiers perdus sont sinon ambigus. Un
encodeur débranché et immobile ne peut pas être identifié sûrement par ces
deux entrées seules. Un échantillon frais n'atteste pas la continuité électrique.
Le guard détecte un manque de mouvement attendu (180 ms, 6 comptes), pas le
courant ni chaque défaut de câblage.

### D2 — image de timing sans activation moteur

Construire séparément (commande PC seulement) :

```powershell
$oldHwMake = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
& $oldHwMake rebuild SIM_MODE=0 CURRENT_ACQUISITION=0 HW_SINGLE_MOTOR_TEST=0 OLD_HW_MOTION_OPT_IN=0 THUMB_EXTI_MOTION_QUALIFIED=0 TIMING_DIAGNOSTICS=1 TIMING_MARKER=3
```

Résultat : `build/HW-timing/n-pulse.elf`, jamais copié dans Debug/HW et refusé
par `flash.ps1`. Le marqueur PA5 remplace temporairement le clignotement LED.
IDs de marqueur : 0 Motor_Update après App_Task, 1 Safety, 2 service boardlink, 3 THUMB EXTI,
4 IRQ host, 5 IRQ USART3, 6 application d'un nouveau STOP. Le firmware ordinaire
ne contient pas ces sondes. Changer le marqueur dans un build dédié distinct,
relever son empreinte ; ne pas comparer des captures de binaires différents
sans le consigner.

Sous debugger autorisé, lire sans modifier les états moteur :
`g_old_hw_timing_enabled`, `g_old_hw_timing[0..6]` (`count`, `max_cycles`,
`max_gap_cycles`) et `g_old_hw_primask_max_cycles` / `g_old_hw_primask_count`.
Conversion nominale : cycles/170 = µs, à confirmer avec l'horloge mesurée.
Le compteur DWT 32 bits reboucle en ~25,26 s ; mesurer des événements rapprochés,
sur essais courts répétés et sous charge réelle. Les différences ne prouvent
pas un maximum si aucun événement n'arrive pendant plus d'un tour. Éviter les
halts debugger pendant les acquisitions ; ils peuvent rendre les durées
ininterprétables. Le coût des sondes est inclus et perturbe légèrement le banc.

Mesurer largeur/espacement PA5 et latence d'une transition A/B ou USART jusqu'au
début du marqueur IRQ. Le maximum PRIMASK couvre les sections utilisant
`Motor_Lock`, pas les masquages internes RTOS/HAL ni le temps avant la prise du
verrou. Mesurer aussi latence externe→IRQ, jitter Safety/contrôle et gaps du
worker. Les compteurs de durée seuls ne mesurent pas la latence d'entrée IRQ.

Charge représentative : contrôle 100 Hz, Safety 20 ms, deux UART bidirectionnels,
télémétrie/commandes bornées, quatre autres encodeurs SLAVE et THUMB à débit max,
erreurs RX, congestion TX et STOP. Mesurer aussi les démarrages, changements
d'état et récupérations où les sections critiques peuvent être plus longues.

Pour THUMB, PB14/PB15, générer Gray A/B dans les deux sens, phases proches,
inversions, rafales et transitions illégales. Si l'hypothèse mécanique historique
12 lignes/tour ×4 ×61 ×206 tr/min est confirmée, elle représente **10052,8 fronts/s**
(~99,5 µs entre fronts). Essayer au moins ce débit et une marge ×2 (~20106 fronts/s),
puis le vrai maximum documenté du matériel. Ce calcul est conditionnel, pas une
spécification vérifiée du moteur monté. Contrôler zéro perte/compte parasite
face au compteur externe, IRQ bornées, pending EXTI autres lignes préservés,
et invalidation/STOP sur les transitions détectées comme illégales.

**Limite fondamentale :** EXTI peut fusionner des fronts pendant un masquage.
Un cycle Gray complet perdu peut paraître légal ; le décodeur ne peut pas
garantir de détecter toutes les pertes. Conserver
`THUMB_EXTI_MOTION_QUALIFIED=0` pour toutes ces images. Un futur changement de
ce flag requiert une qualification séparée et une nouvelle revue, pas seulement
un test natif vert.

### D3 — image single pour mesurer les sorties logiques

Le seul périmètre préparé ici est INDEX (ID 1, SLAVE) **ou** LITTLE (ID 4,
MASTER), un canal à la fois. Les limites numériques compilées (5 % PWM,
pas ≤0,1 rad et armement ≤5 s) sont des plafonds de confinement logiciel,
**pas des valeurs de puissance ou course qualifiées**.

```powershell
& $oldHwMake rebuild SIM_MODE=0 CURRENT_ACQUISITION=0 HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=1 OLD_HW_MOTION_OPT_IN=1 THUMB_EXTI_MOTION_QUALIFIED=0 TIMING_DIAGNOSTICS=0
# Essai séparé LITTLE : même commande avec HW_TEST_MOTOR=4.
```

Artefacts isolés `build/HW-single-1-optin/n-pulse.elf` et
`build/HW-single-4-optin/n-pulse.elf`. Chaque image peut tourner sur les deux
cartes : le strap décide du rôle et seul le propriétaire du moteur choisi peut
armer. Un pair avec image ordinaire désarmée fonctionne aussi, à condition
d'utiliser la même version board protocol v2. Aucun besoin d'armer le pair pour
obtenir un lien sain ou récupérer ses défauts.

**Programmation spéciale :** `flash.ps1` doit refuser ces images et l'image de
timing. Après autorisation spécifique et vérification Vm=0 V, utiliser une
session manuelle séparée ; ne pas copier ces fichiers sur le chemin canonique.
Avant toute connexion : archiver ELF, flags, SHA-256 ; vérifier RET6/OLD_HW/DUAL,
CURRENT=0, THUMB=0, absence de SIM, ID unique et opt-in conforme au type d'image.
Le timing sans single doit avoir opt-in=0. Énumérer `-l`, vérifier SN unique et
association physique RET6, une sonde/carte sélectionnée à la fois. Exemple
**à adapter et à exécuter seulement après cette autorisation** :

```powershell
$cubeCli = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$benchElf = (Resolve-Path 'build\HW-single-1-optin\n-pulse.elf').Path
$benchProbe = 'SN_CARTE_VERIFIEE'
Get-FileHash -LiteralPath $benchElf -Algorithm SHA256
Get-Content -LiteralPath build\HW-single-1-optin\compile.flags
& $cubeCli -l
# Opération active, future, après accord opérateur :
& $cubeCli -c port=SWD "sn=$benchProbe" freq=4000 -w $benchElf -v -rst
```

Contrôler le résultat de programmation/vérification et l'identité affichée ;
en cas d'ambiguïté, ne pas programmer. Cette voie manuelle ne bénéficie pas des
contrôles automatiques du script ordinaire : la revue des flags/hash/SN est donc
obligatoire. Le manifeste de publication ordinaire est volontairement absent
des répertoires spéciaux. Ne pas écrire d'option bytes ou activer de watchdog.

Sur chaque console, assurer Hello/lien frais puis `link-recover` local après
diagnostic récent. Sur la carte du canal choisi, entrer les paramètres
**mesurés et acceptés par l'opérateur**, en remplaçant les symboles :

```text
limits MIN_RAD MAX_RAD PAS_RAD PWM_MAX VITESSE_REFERENCE_RAD_S
calibrate CPR QUADRATURE RAPPORT_REDUCTION SENS OFFSET_BRUT
zero
status
recover
preflight verified
arm
enable
target POSITION_RAD PWM_PERCENT
stop
```

Les symboles sont intentionnellement non exécutables : aucune calibration ou
limite mécanique universelle n'est fournie. `zero` remet le compte brut **et
l'offset de calibration** à zéro ; il ne découvre pas une origine mécanique.
Ne l'utiliser qu'à une référence vérifiée ; l'omettre si l'offset fourni doit
être conservé. Vérifier position et sens avec une référence indépendante avant
d'attester. Attendre chaque ACK et les
mesures fraîches après calibrage/reset ; ne pas coller la séquence en bloc.
Refaire `link-recover` sur le pair si une cause locale indépendante persiste.
Ne pas attester `preflight verified` avant la revue effective du **banc logique
isolé**, tensions, canaux inactifs, mesures de boot et référence encodeur.
Cette attestation ne prétend pas que Gate 2 ou les essais sous puissance sont
déjà validés : elle autorise seulement cet essai électrique sans Vm. Tout STOP
change l'epoch et invalide l'attestation ; récupération, préflight et armement
doivent ensuite être explicites. La calibration/limites ne sont pas persistées.

Mesures à consigner, une capture par condition :

| Condition | Observation attendue |
|---|---|
| Reset/boot avant armement | ENA non actif ; après init, directions basses ; mesurer aussi la fenêtre avant C |
| Arm/enable sans nouvelle cible | Aucune ancienne cible reprise, PWM nul |
| Cible égale à la mesure stabilisée | Sortie à zéro, ENA bas, directions basses |
| Faible écart positif dans les limites | IN1 haut/IN2 bas, PWM actif haut nominal 20 kHz, plafond accepté respecté |
| Faible écart négatif dans les limites | IN1 bas/IN2 haut, même contrôle de plafond |
| Changement de signe | ENA déconnecté de l'AF et bas avant directions ; intervalle neutre ≥2 ms |
| STOP, faute encodeur, perte host/lien, reset | ENA retombe et reste bas ; aucune reprise automatique |
| Fatal logiciel autorisé sur banc isolé | ENA forcé bas sans HAL ni rôle ; directions ne sont pas garanties remises à zéro par le fatal seul |

Pour obtenir deux signes sans cible hors course, fixer une référence encodeur
mesurée à l'intérieur des limites autorisées, puis choisir deux cibles de part
et d'autre de cette référence (pas à moins de 0 rad pour ces doigts). Le PID
calcule le PWM : le champ PWM fourni est un plafond, pas un générateur PWM brut.
Avec encodeur immobile, le guard doit arrêter au bout de son délai ; effectuer
des captures courtes, ne pas désactiver ce guard. Une mesure d'arrêt prolongée
demande une quadrature cohérente injectée, sans fabriquer un signal valide sur
un futur montage sous puissance. Tester séparément la saturation plafonnée,
le retour à zéro et les autres sept sorties qui doivent rester inactives.

Pour fatal, une **nouvelle autorisation** de manipulation debugger est requise
sur ce banc sans Vm : revue du point d'entrée `N_PULSE_FatalCutoff` et de sa
cause, déclenchement contrôlé, capture ENA, puis reset explicitement autorisé.
Ne pas exécuter un appel debugger improvisé pendant un essai moteur alimenté.
Pour reset/perte de câble, capturer l'instant physique de l'événement et ENA
sur les deux cartes. Le budget de 185 ms nécessite tick HAL vivant, Safety
planifié avec retard ≤5 ms et absence de masquage excessif. Un résultat supérieur
invalide cette hypothèse de banc ; ne pas présenter 185 ms comme garantie CPU
bloqué. Sans watchdog actif, un blocage CPU reste hors de cette borne.

**PASS D / Gate 2 applicable au canal :** captures de toutes les conditions,
pins/PWM/polarité/blanking corrects, STOP/fatal et pertes de communication
mesurés sous charge, acquisition externe concordante, piles/heap et limites
du canal revus. Documenter séparément les canaux non mesurés ; un essai INDEX
ne qualifie ni LITTLE, ni les poignets, ni la main. THUMB reste non qualifié
pour mouvement même si l'acquisition initiale fonctionne.
**FAIL :** perte de fronts inexpliquée, dépassement de délais/plafonds, canal
non choisi actif ou défaut non verrouillé : STOP/coupure, aucune puissance.
D ne prouve pas la capacité de coupure électrique du L298N sous charge, le
couple, l'absence de surintensité ni la sécurité mécanique.

## E — future mise en service d'un seul moteur sous puissance

**NON AUTORISÉE / NON EXÉCUTÉE dans ce lot.** Elle exige tous les gates antérieurs
applicables passés sur le banc réel et une autorisation distincte de l'opérateur
pour **un moteur local INDEX ou LITTLE**, un seul canal, montage mécaniquement
découplé, zone de déplacement libre et coupure Vm indépendante testée.

Avant cette autorisation, compléter par écrit : modèle moteur/réducteur,
charge et course sûres, butées, référence/calibration signée, tension Vm et
limitation de courant externe mesurées, seuils/délais d'arrêt acceptés,
plafond PWM réellement sûr, durée d'impulsion, température maximale, méthode
de fixation et scénario de coupure. Ces valeurs sont **à déterminer par revue
électrique et mécanique**, elles ne se déduisent pas du plafond logiciel de 5 %.
Prévoir mesure de courant indépendante, alimentation limitée, oscilloscope
ENA/directions/Vm/courant et opérateur immédiatement disponible à la coupure.

Le firmware actuel ne mesure aucun courant OLD_HW et ne fournit pas de
protection de surcharge fondée sur le courant. Le guard et les limites PID
ne la remplacent pas. Aucun dépassement de FORCE ne sera détecté sans vraie
mesure de force alimentant cette interface. Ne pas déclarer cette protection
opérationnelle. Toute simulation d'encodeur utilisée en D doit être retirée
et remplacée par le vrai capteur vérifié avant un essai sous puissance.

Séquence future : vérifier le banc hors Vm, récupérer/configurer explicitement,
contrôler les sorties inactives, faire autoriser l'application limitée de Vm,
effectuer une seule impulsion ciblée approuvée, STOP/coupure Vm et inspection.
Les commandes restent celles de D avec paramètres approuvés ; aucun script
d'activation automatique. Toute anomalie de sens, courant, course, retour
capteur, ACK ou délai entraîne STOP/coupure immédiate et verrouille la reprise.

**PASS E / Gate 3 :** seulement après mesures acceptées du moteur et archivage
de l'autorisation/fiche d'essai. Cela ne qualifie pas un autre moteur ni la
main complète. Les poignets couplés, la main à plusieurs moteurs et THUMB
nécessitent leurs propres validations et autorisations. Aucun de ces usages
n'est prêt ou autorisé par le lot 4.
