# OLD_HW lot 4 — audit final et état de validation

Date : 2026-10-14. Branche conservée : `oldHardNewSoft`.
HEAD inchangé : `4c69750e65393f1a8f663749ebac1e2d70101101`.
Référence historique lue sans modification : `origin/simulation_pipeline`,
`15f4d4ec109a4a642b0be77d99fee446246e70cc`.

**Gate 0 logiciel validé ; Gates 1–3 non exécutés.** Les builds et tests natifs
ne qualifient aucune carte réelle. Aucun flash, reset matériel, connexion SWD
ou mouvement n'a été exécuté. Aucun commit/push/changement de branche.
Les modifications antérieures de l'utilisateur, notamment `.ioc` et
`.vscode/settings.json`, ainsi que les rapports lots 1–3, sont conservées
octet pour octet par comparaison à l'instantané de début du lot 4.

La procédure physique est [OLD_HW_BRINGUP.md](OLD_HW_BRINGUP.md). Elle maintient
Vm et moteurs débranchés jusqu'aux mesures électriques ; tout essai sous
puissance demande une autorisation distincte après les gates applicables.

## Architecture active et audit de bout en bout

Un même ELF RET6 sert à MASTER et SLAVE ; PC0 choisit leur rôle. Le chemin actif
est le C maintenu, pas une génération CubeMX :

```text
Reset_Handler / startup_stm32g474retx.s / linker RET6
  HAL_Init (tick HAL TIM6)
  BoardLink_Init (rôle PC0)
  OldHW_HoldOutputsLow
  OldHW_ClockConfig (HSI/PLL, 170 MHz nominal)
  OldHW_Init (GPIO, TIM1/4, encodeurs locaux, LPUART1, USART3)
  [OldHW_TimingInit uniquement dans image dédiée]
  App_Init -> moteurs/safety/PID/BSP, estimation, comms, runtime boardlink
  CreateTasks -> scheduler FreeRTOS
```

Le startup initialise `.data/.bss` et utilise `_estack=0x20018000`. Le linker
réserve 512 Kio Flash et 96 Kio SRAM système ; les 32 Kio CCM ne sont pas utilisés
comme SRAM contiguë. Pas de constructeur/startup qui arme les sorties.
Les handlers RTOS forts SVC/PendSV/SysTick viennent du port ARM_CM4F, HAL tick
de TIM6 ; SysTick ne double pas `HAL_IncTick`. TIM6 est réinitialisé par
`HAL_InitTick` après changement d'horloge. Les callbacks UART uniques
dispatchent par handle : LPUART1 pour host, USART3 pour boardlink ; UART4
historique n'est pas actif dans l'ELF OLD_HW.

| Exécution | Priorité / pile | Fonction et contraintes |
|---|---|---|
| ControlTask | 3 / 256 mots | App_Task puis Motor_Update toutes les 10 ms, temps réellement écoulé |
| SafetyTask | 5 / 256 mots | Santé, host/link, fautes et nettoyage STOP toutes les 20 ms |
| Guard | 3 / 384 mots | Surveillance de mouvement toutes les 20 ms, tous les moteurs activés locaux |
| Comms RX | 2 / 768 mots | Parser/protobuf/commande hors IRQ |
| Host TX | 1 / 384 mots | File bornée, IT UART, timeout 25 ms |
| Telemetry | 1 / 768 mots | Service périodique, sérialisation hors verrou, estimateur séparé |
| Boardlink | 2 / 768 mots statiques | Au plus 256 octets par service, sommeil 2 ms, nonce RNG hors verrou |
| Idle / timer daemon | 0 / 128 ; 2 / 256 mots statiques | Timer LED 500 ms ; allocations vérifiées au démarrage |
| THUMB EXTI | NVIC 3 | Pas d'API RTOS, traitement borné |
| TIM6 HAL | NVIC 4 | Tick HAL indépendant du tick RTOS |
| LPUART1 / USART3 | NVIC 6 | Sous plafond syscall FreeRTOS 5, callbacks courts |
| PendSV / SysTick | NVIC 15 | Ordonnancement kernel |

Les piles sont exprimées en mots de 4 octets. Les allocations dynamiques
échouées conduisent aux hooks/assert/fatal ; retour du scheduler = fatal.
Les modes de calibration historique, test poignets et drivers QET6 sont
inactifs dans les configurations livrées. Un commentaire historique de création
des tâches cite encore comms priorité 4 ; la création effective est priorité 2.

LPUART1 PA2/PA3 à 115200 8N1 reçoit le protocole host existant. Hello maintient
la session (timeout 2000 ms), jamais une permission moteur. Les intentions
ordinaires sont réservées au MASTER ; l'opérateur peut diagnostiquer, STOPper
et récupérer séparément les deux cartes. L'image single donne autorité locale
exclusive au canal sélectionné ; le routage de cibles intercartes est refusé
dans ce mode.

Le chemin moteur est : validation session/epoch/ownership → limites et retour
calibré frais → armement explicite et préflight → Motor_SetTargetAtEpoch →
référence limitée/PID moderne → MotorBackend_HW → Motor_L298N_ApplySigned.
L'armement, l'enable et la cible sont trois actes distincts. Pas de démarrage
depuis les diagnostics, le calibrage, le reset encodeur ou une récupération.
Les estimates Kalman n'alimentent pas le PID ; les interfaces current/force
ne remplacent pas une mesure absente.

Les alias `Motor_DRV8848_*` dans `drv8848.h` désignent le driver L298N dans
OLD_HW : aucun backend physique DRV8848/HRTIM lié. Les anciens wrappers de
consigne backend ne sont pas la voie de commande active. Le wrapper void
`Encoder_BSP_UpdateRemote` est volontairement sans effet : les retours distants
sont conservés dans un cache boardlink séparé et ne deviennent jamais des
mesures BSP locales. Aucun de ces wrappers n'annonce une réussite d'action
matérielle. Le faux succès réel de ResetAll est corrigé ci-dessous.

## Mapping et propriété des broches

Table extraite de `old_hw.c`, BSP encodeur et driver actifs ; `IN1/IN2` sont des
sorties GPIO push-pull, initialisées bas. L'ENA est GPIO bas tant que désarmé,
connecté à l'AF PWM seulement par le chemin autorisé. Pas de pull-up d'ENA
logiciel destiné à activer un pont. Directions/ENA inactifs : output push-pull,
sans pull. Encodeurs timers : AF push-pull avec pull-up, quadrature TI12,
polarités montantes configurées, prescaler DIV1, filtres IC1/IC2=4 ; THUMB en
entrée pull-up sur deux fronts EXTI. Les UART sont AF push-pull sans pull.
Les niveaux et filtres électriques doivent être mesurés.

| ID / moteur | Carte | ENA / PWM / AF | IN1 / IN2 | Encodeur A/B et ressource | État lot 4 |
|---|---|---|---|---|---|
| 0 THUMB | SLAVE | PA8 / TIM1 CH1 / AF6 | PB5 / PB4 | PB14/PB15, EXTI 14/15 | Acquisition logicielle ; mouvement bloqué, qualification=0 |
| 1 INDEX | SLAVE | PA9 / TIM1 CH2 / AF6 | PC10 / PC11 | PA0/PA1, TIM2 CH1/2 AF1, 32 bits | Acquisition ; image single dédiée préparée |
| 2 MIDDLE | SLAVE | PA10 / TIM1 CH3 / AF6 | PC12 / PA15 | PA6/PA7, TIM3 CH1/2 AF2, 16 bits | Acquisition ; aucun essai moteur autorisé |
| 3 RING | SLAVE | PA11 / TIM1 CH4 / AF6 | PB12 / PB13 | PC6/PC7, TIM8 CH1/2 AF4, 16 bits | Acquisition ; aucun essai moteur autorisé |
| 4 LITTLE | MASTER | PB6 / TIM4 CH1 / AF2 | PC8 / PC9 | PA6/PA7, TIM3 CH1/2 AF2, 16 bits | Acquisition ; image single dédiée préparée |
| 5 WRIST_X | MASTER | PB7 / TIM4 CH2 / AF2 | PC4 / PC5 | PA0/PA1, TIM2 CH1/2 AF1, 32 bits | Acquisition ; pas de commissioning poignets |
| 6 WRIST_Y | MASTER | PB8 / TIM4 CH3 / AF2 | PB0 / PB1 | PC6/PC7, TIM8 CH1/2 AF4, 16 bits | Acquisition ; pas de commissioning poignets |
| 7 PALM | SLAVE | PB9 / TIM4 CH4 / AF2 | PD2 / PC3 | PB2 AF3 / PC2 AF6, TIM20 CH1/2, 16 bits | Acquisition ; aucun essai moteur autorisé |

Tous les angles/vitesses commencent **non calibrés**, sans position mécanique
inventée. L'image ordinaire interdit tous les moteurs, y compris les deux
canaux pour lesquels une image spéciale a été compilée. Une image single n'est
pas une qualification physique du canal.

TIM2/3/8 sont partagés entre identifiants appartenant à **des cartes différentes**,
jamais deux propriétaires sur la même carte. TIM1 PWM concerne SLAVE seulement ;
TIM4 CH4 PALM sur SLAVE et CH1–3 sur MASTER. Clocks GPIO A/B/C/D, TIM1/8/20 APB2,
TIM2/3/4/6 APB1, SYSCFG/EXTI et UART sont préparées dans le chemin manuel.
Les IRQ encodeurs timers ne sont pas nécessaires au comptage matériel.
THUMB utilise EXTI15_10, W1C uniquement bits 14/15, sans effacer les autres
pending partagés. PA5 est LED, ou marqueur de timing dans l'image dédiée.
PC0 haut/pull-up = MASTER, bas = SLAVE. USART3 PB10/PB11 AF7 et LPUART1 PA2/PA3
AF12 n'entrent pas en conflit avec le mapping actif.

La vérification des AF et de la disponibilité LQFP64 s'appuie aussi sur la
[datasheet ST DS12288](https://www.st.com/resource/en/datasheet/stm32g474re.pdf),
tables pinout/AF, notamment les routes TIM20 et LPUART1. PA13/PA14 restent SWD.
PA15/PB4 de SLAVE rendent JTAG complet incompatible ; utiliser SWD seulement.
PB3/SWO déclaré dans `.ioc` n'est pas initialisé explicitement comme sortie
de diagnostic. Le chemin actif ne manipule pas un GPIOG hérité de QET6 ; NRST
reste une connexion physique de debug/reset, pas une sortie moteur.

Le `.ioc` historique est incomplet : ni les huit sorties ni les rôles/USART3
ni la totalité de l'ownership RTOS/tick ne peuvent être régénérés fidèlement.
**Autorité : C actif + startup RET6 + linker + tools/firmware.mk. Ne pas régénérer
aveuglément avec CubeMX.** Le fichier `.ioc` existant n'a pas été modifié.

## Corrections ciblées du lot 4

### Récupération concurrente avec STOP

Une récupération validée pouvait libérer le verrou pour nettoyer les moteurs,
puis effacer un STOP intervenu dans cet intervalle. Vérifier seulement les
causes présentes n'était pas suffisant ; un STOP répété peut rester idempotent
pour l'epoch. Ajout d'un ticket `{epoch, stop_serial}` capturé **au moment de la
validation** de la demande. Tout STOP admissible incrémente stop_serial, même
une cause déjà verrouillée. Le commit de récupération refuse si l'un des deux
a changé. Les voies opérateur, commissioning et SIM utilisent ce même ticket.
Les epochs de STOP répété restent idempotents pour éviter des tempêtes de
sessions ; ce changement n'autorise aucun mouvement.

Tests : STOP répété après capture du ticket, STOP injecté pendant la récupération,
échec restant fermé puis nouvelle récupération explicite, INDEX et LITTLE.

### Reset global des encodeurs

ResetAll ignorait les valeurs de retour individuelles et pouvait réussir
partiellement. Il vérifie désormais tous les canaux locaux, timers et capacité
de reset THUMB avant de modifier les références, puis propage tout échec.
Le contrôle THUMB permet le reset explicite d'un défaut de décodage lorsque le
canal est inactif, mais refuse une configuration/NVIC indisponible. Les tests
prouvent qu'un timer arrêté empêche la modification des autres comptes et
qu'une IRQ THUMB indisponible est refusée. Ce n'est pas une transaction garantie
face à une panne électrique asynchrone du périphérique ; un défaut matériel
ultérieur reste une cause d'invalidation/arrêt.

### Diagnostics utilisables avant calibrage et recovery

`Encoder_BSP_RawAvailable` sépare disponibilité des comptes bruts et validité
de la conversion mécanique. MotorDiagnostic flag 128 expose cette disponibilité ;
le moniteur affiche `unavailable` pour un compte non acquis, pas un faux zéro.
La validité brute distante est aussi retirée sur perte de lien ou retour vieux.

Le protocole intercartes passe **v1 → v2** pour ce bit `BL_RAW_VALID=16` ; une
ancienne version est refusée et arrête en sécurité. Les deux cartes doivent
être mises à jour ensemble avant qualification. Aucun numéro de champ ou
opération host existant n'est renuméroté.

Ajout host `ProsthesisMessage.link=5` / `LinkDiagnostic`, sélection diagnostic
ID 8 et commande `link`. Il donne rôle, sessions u64, epochs et STOP **séparés**,
âges heartbeat/écho, états worker/quarantaine/autorisation, séquences et compteurs
d'erreurs transport/overflow. Les vues moteur continuent de donner brut,
calibrage, validité/âge, courant indisponible, PWM signé appliqué et rejets ACK.
Pour les retours distants, PWM/enable sont des valeurs du dernier snapshot et
doivent être interprétés avec l'âge ; ils ne certifient pas l'état électrique.

La classe générée C++ historique reste inchangée : l'émission du seul nouveau
message utilise un writer varint borné, sans allocation, testé en décodant son
résultat réel avec protobuf Python. Charge maximale 123 octets, sous plafond
128 ; trame host 125 octets. Les anciennes classes host ignorent ce nouveau
champ inconnu ; le moniteur mis à jour sait le lire. Le test compare l'intégrité
du descripteur lot 3 après retrait du seul ajout via une empreinte golden
embarquée, sans dépendre d'un instantané local ignoré par Git. Ce décodage et
cette compatibilité passent aussi avec le runtime protobuf Python pur.

### Instrumentation facultative et outils

Image HW `TIMING_DIAGNOSTICS=1` séparée, suffixe `-timing`, absente des artefacts
ordinaires et interdite en SIM. Compteurs DWT bornés, maxima d'exécution et gaps
de Motor_Update, Safety, worker, THUMB et deux UART ; durée de nouveau STOP et
durée extérieure des verrous Motor_Lock imbriqués. PA5 sélectionnable 0..6 ;
aucune broche moteur réutilisée. Le point contrôle commence après App_Task :
il mesure Motor_Update, pas l'intégralité du routage d'intentions.
Le firmware normal ne contient pas les symboles/probes de timing.

Build/manifest portent LOT4 et imposent les flags sûrs du workflow ordinaire.
Le flash vérifie chemin canonique HW, ELF ARM, flags, manifeste, hashes et SN
ST-LINK explicite unique. Dry-run énumère seulement `-l`, sans connexion/reset.
Le cas une seule sonde est signalé. Commissioning/timing restent refusés ; la
procédure spéciale manuelle est séparée dans le guide, sans assouplir le script.
Le moniteur expose `status [ID]`, `raw [ID]`, `link` et les flags de qualification.
Il envoie STOP à sortie/timeout/ACK incertain ; il n'est pas passif.

## Démarrage distribué, STOP et absence de dépendance circulaire

Chaque carte démarre inhibée, avec rôle local et acquisition locale. Le worker
obtient une session RNG via HSI48, hors section critique ; échec/timeout/seed
invalide empêche le lien et verrouille LINK. Les attentes sont RTOS bornées,
pas une boucle sous PRIMASK. Le test natif du contrôleur injecte un nonce au
seul bord entropie : il **ne prouve pas** le RNG réel ; les tests transport
vérifient aussi son échec fermé.

Hello/heartbeat fonctionnent avec moteurs désarmés et défauts verrouillés.
Recovery exige un lien bidirectionnel frais, pas un pair déjà armé ou sans
défaut. Il efface uniquement les causes locales après nettoyage et contrôles
de retour requis ; l'armement exige ensuite les défauts du pair absents.
Le défaut DRIVER de startup peut être récupéré sur un pair de production
correctement initialisé. Le verrou de compilation opt-in=0 demeure permanent.
Il n'y a donc pas de cercle « armer pour établir le lien, établir pour armer ».

STOP invalide préparations/cibles, inhibe immédiatement le contrôleur/driver
local sous le même verrou et organise le nettoyage hors IRQ. La propagation
distribuée utilise sessions, epochs indépendants, ordonnancement de séquences,
CRC et refus des anciennes commandes. Une cause déjà présente reste verrouillée,
une nouvelle cause avance l'epoch. MANUAL/FEEDBACK/GUARD/etc. d'une carte ne
peuvent pas être effacés par la récupération de l'autre. Un ACK est une
acceptation protocolaire, jamais une preuve de mouvement ou de signal électrique.

Un reboot change la session et invalide l'autorité précédente sur le pair.
Le retour de communication ne reprend aucune cible, n'arme et n'enable rien.
File cible pleine, overflow RX, erreurs/rearm UART, timeout ACK/TX et trames
incompatibles conduisent à STOP/refus ; certains échecs HAL mettent en
quarantaine et nécessitent un reset autorisé. Le CRC protège contre les erreurs,
pas contre un équipement hostile ; le lien câblé est supposé de confiance.

Les nouveaux essais utilisent deux chargements indépendants du **même DLL**
compilé, dont les copies ont des SHA-256 identiques, et le véritable adapter
USART3, protocole, safety, commissioning, BSP, contrôleur/PID et driver.
Seuls registres, HAL, RTOS, horloge et entropie sont modélisés.
Ils couvrent trois séquences défaut/retard/STOP indépendant en image ordinaire ;
six séquences INDEX (recovery/préflight neuf, reboot, faute encodeur armé,
panne unidirectionnelle, overflow, perte host) ; une LITTLE ; et deux
combinaisons single + pair ordinaire désarmé. Le guard ne masque pas le timeout
host dans l'essai correspondant : canal enabled avec PWM nul. Ces essais ne
modélisent pas la latence réelle du CPU/fil ; le test de débit baud du lot 3
reste distinct et conservé.

## Sorties, retours et limites physiques

TIM1/TIM4 : PSC=0, ARR=8499, PWM1 actif haut, 20 kHz nominal à 170 MHz.
IN1/IN2 = haut/bas pour signe positif, bas/haut pour négatif. Zéro/inhibition
déconnecte l'AF, force ENA bas, désactive le canal et remet les directions bas.
La polarité logicielle ne prouve pas le sens mécanique réel. Saturation et
plafond single sont contrôlés ; changement de signe coupe avant modification
des directions et impose ≥2 ms neutres, sans consigne différée à réappliquer.
Armement ≤5 s, préflight lié à l'epoch et retour frais sont conservés.

Le fatal utilise le cutoff registres OLD_HW, sans HAL/handle/role, désactive
TIM1/4 seulement si leurs clocks sont présentes et force l'union des huit ENA
PA8..11/PB6..9 bas. Il fonctionne avant initialisation complète. **Les directions
ne sont pas garanties basses par le fatal seul** ; c'est ENA bas qui inhibe.
Avant la première instruction de mise en sécurité et durant reset, les niveaux
dépendent du matériel : jumpers ENA retirés, polarisation/mesures externes
indispensables. Aucun mock ne certifie un L298N sous charge.

Acquisition timer : différentiel signé modulo largeur réelle, accumulation
int64 contrôlée, horodatage HAL et vitesse selon dt réel. Le delta doit rester
inférieur à la demi-plage entre deux lectures ; wrap multiple ne peut pas être
reconstruit. Calibrage explicite CPR/quadrature/ratio/sens/offset, refus si
enabled/armed/transition. Trou d'échantillonnage >100 ms/nombre invalide/défaut
acquisition invalide le retour ; aucune valeur NaN n'est remplacée par zéro.
Un câble encodeur débranché mais immobile n'est pas détectable sûrement par
le seul compteur. Le guard (180 ms, 6 comptes) surveille le mouvement attendu,
pas le courant, et exige un mouvement restauré pour recovery après trip.

THUMB : EXTI borné, lecture A/B, validation Gray et compte signé ; transitions
diagonales/répétées ou deux pending coalescés détectés invalident et arrêtent.
Les autres pending EXTI sont préservés. **Un cycle complet manqué peut paraître
légal** ; on ne peut pas promettre que toute perte silencieuse sera détectée.
La priorité 3 ne protège pas contre PRIMASK. Tous les cinq builds ARM ont
THUMB_EXTI_MOTION_QUALIFIED=0. Quelques fixtures natives héritées mettent 1
pour exercer le code simulé de faute THUMB ; ce ne sont pas des images flashables
ou une qualification physique. La procédure demande compteur externe,
latences/charge/rafales et taux mécanique confirmé avant une revue séparée.

## Temps réel et budget de STOP

| Borne logicielle | Valeur |
|---|---|
| Heartbeat | 50 ms |
| Expiration heartbeat ou absence de progrès écho | strictement >150 ms ; test 150/151 ms conservé |
| Trame intercartes / âge de traitement | 25 ms |
| Cible en file avant émission | 50 ms |
| ACK cible | 100 ms depuis offre |
| TX USART3 | 15 ms |
| Retour moteur | 100 ms |
| Contrôle/PID | 10 ms nominal, délai excessif >50 ms arrête |
| Host Hello | 2000 ms |

Si Safety est effectivement planifié tous les 20 ms avec retard au plus 5 ms,
tick HAL vivant et chemin cutoff borné, retenir ~175 ms depuis la dernière
preuve valide. En ajoutant une dernière trame déjà en vol (≤86 octets,
~7,5 ms à 115200 8N1), le budget conservateur est **185 ms depuis la rupture**.
Ce lot ne mesure ni WCET, ni retard maximal de tâche, ni délai physique ENA.
Il ne transforme donc pas ce budget en garantie.

USART3 IRQ reçoit un octet, met en ring et réarme IT, sans décodage/allocation
ni attente. Erreur signale STOP ; abort/flush se font dans le worker, hors
PRIMASK. Host IT conserve sa validation/commit atomique et ses bornes de file.
L'abort host historique dans IRQ n'attend pas de DMA dans OLD_HW : handles DMA
non initialisés/nuls, pas de stream actif. La mise en route HAL IT TX se fait
sous verrou avec validation epoch ; elle n'est pas un transfert bloquant.
Le parser/CRC, snapshots, PID, feedback et cutoff utilisent des sections
Motor_Lock bornées en nombre d'opérations, mais leur durée cible reste à mesurer.
Un trafic IRQ soutenu, PRIMASK long ou une tâche qui bloque peut violer les
hypothèses de scheduling et fusionner des fronts THUMB.

La sonde DWT/PA5 permet les mesures de maxima/gaps et du verrou extérieur,
avec limites : compteur 32 bits ~25,26 s, pas de preuve pour interruptions
plus longues, overhead et pauses debugger, pas tous les masquages RTOS/HAL.
Les mesures sans armement ne qualifient pas automatiquement le commit PWM armé ;
compléter avec mesures externes de D sur image single et charge réelle.
IWDG/WWDG existent sur ce MCU mais aucun init/start n'est lié. Les option bytes
réels n'ont pas été lus : leur éventuelle configuration hardware reste à
vérifier sans les modifier. Aucun watchdog activé par ce lot. Un CPU bloqué,
scheduler arrêté ou tick HAL figé reste hors borne ; coupure indépendante requise.

## ADC, courant et Kalman

La référence historique `fw/bsp/adc_dma/adc_dma_bsp.c/.h` de la branche simulation
contient init vide/TODO et tableaux bruts statiques à zéro. Les commentaires
proposent ADC1 PA0..4/ADC2 et un shunt supposé 0,01 ohm, sans configuration
GPIO/DMA/ADC/opamp réalisant cette acquisition. Ce n'est pas une mesure.
La conversion historique force/courant ne constitue pas une calibration
électrique. De plus PA0/1 sont ici encodeur TIM2 et PA2/3 host LPUART1 : réutiliser
ce mapping spéculatif créerait des conflits.

Informations manquantes : schéma réel par moteur, pins ADC et polarité, shunts
et puissance, gain/amplificateur/offset, référence/tension ADC, filtre,
fréquence/instant d'échantillonnage par rapport au PWM, impédances/protections,
calibration physique et unités/relations courant-force. Aucun mapping ajouté.
Le BSP moderne OLD_HW retourne état INVALID et NaN, acquisition forcée à 0 ;
pas de `HAL_ADC_Start_DMA`/OPAMP dans les images livrées. **La protection de
surcharge par courant n'est pas opérationnelle.** Les seuils force existants
ne sont efficaces que si une vraie source valide alimente cette interface.
Kalman moderne conservé, testé, indépendant du PID et invalide sans mesures
appropriées ; aucune estimation n'est présentée comme capteur de sécurité.

## Builds, ELF, SRAM et piles

ARM GCC 14.3.1, Cortex-M4F hard-float, `-O0 -g3`, GNU Make 3.81
(`C:\Program Files (x86)\GnuWin32\bin\make.exe`). MinGW make 3.82 installé a
un défaut readdir sur ce workspace ; le wrapper choisit le Make fonctionnel.
Les quatre builds requis sont propres (`rebuild`), plus un cinquième de timing :

| Image / répertoire build | text | data | bss (`size`) | Résultat |
|---|---:|---:|---:|---|
| HW ordinaire | 119456 | 100 | 38048 | PASS, outputs définitivement inhibés |
| SIM | 71520 | 96 | 31280 | PASS, backend virtuel sans activation physique |
| HW-single-1-optin INDEX | 125184 | 100 | 38072 | PASS, isolée, THUMB=0 |
| HW-single-4-optin LITTLE | 125184 | 100 | 38072 | PASS, isolée, THUMB=0 |
| HW-timing, opt-in=0 | 120096 | 100 | 38176 | PASS, isolée et sans armement |

Octets ; `bss` de GNU size comprend ici la section NOLOAD heap/stack réservée
de 1536 octets. HW `.data+.bss` réelle avant cette réserve : 36612 octets ;
avec réserve, 38148 octets sur 98304, soit 60156 octets non affectés par le
linker. Cette marge n'est pas le heap FreeRTOS disponible. MSP minimum 1024,
heap newlib minimum 512 ; les piles RTOS et heap_4 de 16384 sont déjà en BSS.

Audit DWARF par GDB **fichier ELF seulement**, sans target : TCB 168 octets
(incluant `_reent` 76), Queue 80, Timer 44, tx_frame 140, daemon message 16.
Six tâches dynamiques = 2816 mots de pile + TCB + headers/alignement, ~12368
octets heap_4 ; file TX ~1208, timer LED ~56, file daemon ~248. Total prévu
~13880 ; ~2496 octets libres après sentinelle de fin, avant allocations newlib
paresseuses/fragmentation. Idle/timer/boardlink sont statiques. Les échecs
d'allocation restent fatals fermés ; marge à relever sur vrai target sous charge.

Extraits `.su` GCC (frame individuel, **pas un maximum de chaîne d'appels**) :

| Fonction | Octets | Pile du contexte |
|---|---:|---|
| ControlTask / SafetyTask / TelemetryTask | 24 chacune | 1024 / 1024 / 3072 |
| Motor_Update / Motor_RecoverFromFaultWithTicket | 160 / 40 | tâche appelante |
| HWTelemetry_Service → SendMotorDiagnostic | 528 + 672 | télémétrie 3072, plus sérialisation et sous-appels |
| SendLinkDiagnostic / SendCommandReply | 440 / 616 | télémétrie ou RX |
| HandleDeviceMessage / HandleProsthesisMessage | 336 / 736 | RX 3072 |
| Worker / service / feedback boardlink | 80 / 48 / 168 | 3072 statiques |
| LPUART1 / USART3 handler ; THUMB handler | 8 / 8 ; 24 | MSP, plus HAL/callees et contextes exception |

La marge apparente des tâches et l'absence de récursion dans les chemins
audités ne constituent pas une preuve de stack maximale, notamment C++ généré,
HAL et empilement FPU/IRQ imbriquées. `configCHECK_FOR_STACK_OVERFLOW=2` reste
actif ; mesurer high-water marks, MSP et heap minimum dans les étapes physiques.
Pas d'agrandissement arbitraire des piles ou de SRAM pour masquer un problème.

`tools/verify_startup_build.py` vérifie les quatre images et timing : vecteurs
forts, tick/ordre startup, handlers locaux, région RAM, absence DRV8848/HRTIM/
PWM HAL et watchdog init, présence des seules voies single autorisées, flags,
hashes manifests/alias et isolation timing. Résultats tailles/empreintes/frames
dans `build/validation/old-hw-lot4-elf.json`. HW SHA-256 final :
`a71c91faadf43af64542a249acb87649fb7f0ce46c60d554016d06435937ebee`.
Les images spéciales n'ont volontairement pas de manifeste de publication
ordinaire et ne remplacent pas Debug/HW. Aucun programmeur réel invoqué ;
les 24 essais flash utilisent exclusivement un faux exécutable.

## Validation réellement exécutée et reproductibilité

Tous les résultats finaux ci-dessous ont code de sortie 0. Les cinq builds ARM
n'émettent aucun warning/error ; le contrôle whitespace Git tenant compte des
fins de ligne CRLF passe. Host GCC/G++ MinGW
6.3.0 (32 bits), Python 3.12.8 (64 bits). Le harness DLL C natif évite de charger
une DLL 32 bits dans Python 64 bits. Protobuf 7.36.2 utilisé depuis
`build/lot2-tools`, sans installation globale pour ce lot.

| Commande / suite | Résultat final |
|---|---|
| tests/startup/run_tests.py | 31 scénarios PASS, HAL/RTOS modélisés |
| tests/startup/test_build_modes.py | 150 objets HW/SIM inchangés lors alternance ; alias HW conservé ; modes/ADC/opt-in non cadré et timing SIM refusés |
| tools/verify_startup_build.py | PASS pour les cinq ELF et interlock de build OLD_HW |
| tests/safety/run_tests.py | PASS, 56 lignes de scénarios : STOP, races, feedback, PID, drivers, nettoyage, sessions, commissioning |
| tests/comms/run_tests.py | PASS parser/protobuf/session/UART réel avec I/O simulées, bornes SIM, codec host et trois scénarios SIM ciblés |
| tests/outputs/run_tests.py | PASS fatal/assert/PWM, 24 scénarios flash mock |
| tests/patch7/run_tests.py | PASS commissioning et Kalman, 12 groupes affichés dont stabilité 100000 pas |
| tests/patch7/test_monitor.py | PASS commandes, nouveaux états lien/brut, framing, invalides et compatibilité schéma |
| tests/simulation/run_tests.py | PASS codec/host et 28 cas SIM, aucun port ni Gazebo |
| tests/old_hw/run_tests.py | 43 scénarios PASS |
| tests/old_hw/run_lot2_tests.py | 37 scénarios PASS |
| tests/old_hw/run_lot3_tests.py | 46 scénarios PASS (35 protocole dont ancien v1 refusé, 1 routing, 8 transport, 2 contrôleurs) |
| tests/old_hw/run_lot4_tests.py | 21 scénarios PASS (6 recovery/reset, 12 adapter double, timing, writer C++, décodage/compatibilité Python) |

Le comptage par groupe n'est pas un comptage de chaque assertion ; pas de total
artificiellement gonflé. Résultats détaillés OLD_HW dans chaque
`build/old-hw[-lotN]-tests/results.json`, commandes/stdout/stderr/exit Windows
enregistrés. Logs finaux `build/lot4-*.log`. Les artefacts build sont ignorés
par Git et doivent être régénérés sur une autre machine.

**SKIP :** ASan+UBSan et UBSan indisponibles dans les compilers/runtimes locaux,
confirmés par probes de compilation ; les variantes natives non sanitizées
passent. Aucun test physique, WCET, RNG réel, UART réel, flash réel ou Gazebo
réalisé. Aucune suite désactivée ou assertion affaiblie.

Échecs intermédiaires conservés dans l'historique de travail : fixture lot 3
ne compilait pas le diagnostic sans `l298n_config.h` (include corrigé) ; fixture
communications ne liait pas le nouvel accès brut BSP (mock de bord ajouté) ;
alternance HW/SIM lancée pendant la reconstruction SIM échouait légitimement
sur « Incomplete source rebuild » (relancée après fin des builds, PASS).
Une interrogation GDB du nom static `xHeapStructSize` n'était pas exposée dans
DWARF ; son alignement/header a été lu dans heap_4. Aucun crash natif ni
exception Windows `0xC0000005` observé dans cette validation lot 4. Ces incidents
ne sont pas présentés comme essais réussis.

Commandes PC des builds et vérifications, à lancer séquentiellement :

```powershell
$env:MAKE = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
$oldHwMake = $env:MAKE
.\build.ps1 -Mode OLD_HW -Rebuild
.\build.ps1 -Mode SIM -Rebuild
& $oldHwMake rebuild SIM_MODE=0 CURRENT_ACQUISITION=0 HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=1 OLD_HW_MOTION_OPT_IN=1 THUMB_EXTI_MOTION_QUALIFIED=0 TIMING_DIAGNOSTICS=0
& $oldHwMake rebuild SIM_MODE=0 CURRENT_ACQUISITION=0 HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=4 OLD_HW_MOTION_OPT_IN=1 THUMB_EXTI_MOTION_QUALIFIED=0 TIMING_DIAGNOSTICS=0
& $oldHwMake rebuild SIM_MODE=0 CURRENT_ACQUISITION=0 HW_SINGLE_MOTOR_TEST=0 OLD_HW_MOTION_OPT_IN=0 THUMB_EXTI_MOTION_QUALIFIED=0 TIMING_DIAGNOSTICS=1 TIMING_MARKER=3
python tests/startup/test_build_modes.py
python tools/verify_startup_build.py
```

Ensuite lancer chacune des suites Python de la table. Installer les dépendances
Python dans un environnement dédié sur un nouveau poste. Ne pas lancer comms
et simulation en concurrence : ils réutilisent `build/simulation`. Les essais
flash mock n'appellent jamais le CLI ST réel. La procédure de programmation
future, SN par SN, et l'autorisation requise sont dans le guide.

## Gates, défauts restants et verdict

| Gate | État au lot 4 | Preuve / manque |
|---|---|---|
| 0 — logiciel validé | **PASS** | Cinq builds, suites applicables, inhibition ordinaire, contrôles ELF/artefacts/flash ; pas de défaut logiciel critique connu non résolu dans le périmètre audité |
| 1 — logique matérielle | **NON EXÉCUTÉ** | Boot, clock, rôles, UART et sorties au reset réels à mesurer |
| 2 — sorties/sécurité électriques | **NON EXÉCUTÉ** | PWM/directions/fatal/STOP/encodeurs/latences/limites réels à mesurer par canal |
| 3 — un moteur sous puissance | **NON AUTORISÉ / NON EXÉCUTÉ** | Tous gates applicables et accord distinct, limites/courant/coupure externes requis |

Limites ouvertes : pinout du montage réel non confirmé, glitches au reset,
budget 185 ms conditionnel, durée PRIMASK/IRQ/stack/heap non mesurée, compteur
THUMB susceptible de perdre des cycles non détectables, absence de courant/
force matérielle validés et de watchdog activé. Ce sont des limites explicites
de qualification et de couverture physique, pas des mesures prétendument nulles.

Prochaine action : inspection A hors tension, puis autorisation de programmation
et étapes B/C logique seulement ; D mesure chaque canal retenu sans Vm. Gate 2
incomplet interdit de déclarer prêt un essai moteur alimenté.

| Catégorie demandée | Verdict |
|---|---|
| A — essais logiciels | **Prêt**, preuves ci-dessus |
| B — essais physiques logique seule | **Candidat préparé**, après inspection A et autorisation opérateur ; Gate 1 reste à passer |
| C — un moteur alimenté | **Non prêt**, Gate 2 non exécuté et absence d'autorisation séparée |
| D — plusieurs moteurs / main complète | **Non prêt**, qualifications moteurs/couplages/THUMB et protections manquantes |

## Inventaire exact des fichiers du lot 4

Comparaison avec l'instantané de début du lot, et avec HEAD pour les fichiers
auparavant inchangés. Les lots 1–3 restent non commités ; `git diff HEAD` inclut
donc leur travail aussi. Cet inventaire distingue les changements **de ce lot**.
Les fichiers antérieurs non retouchés sont décrits dans les trois rapports
précédents ; `.ioc`, settings, linker et cutoff fatal notamment sont préservés.

Modifiés (38) :

```text
Core/Src/main.c
Core/Src/old_hw.c
Core/Src/stm32g4xx_it.c
build.ps1
flash.ps1
fw/app/hw_commission.c
fw/app/hw_telemetry.c
fw/bsp/encoder/encoder_bsp.c
fw/bsp/encoder/encoder_bsp.h
fw/bsp/encoder/thumb_encoder.c
fw/bsp/encoder/thumb_encoder.h
fw/comm-stack/PyUART/protocol_messages_pb2.py
fw/comm-stack/protocol_messages.proto
fw/comm-stack/tx.cpp
fw/comm-stack/tx.h
fw/comm/board_link.h
fw/comm/board_link_runtime.c
fw/comm/board_protocol.c
fw/comm/board_protocol.h
fw/comm/sim_session.c
fw/motors/motor_control/motor_control.c
fw/motors/motor_control/motor_control.h
fw/motors/motor_control/motor_lock.h
fw/motors/motor_control/motor_safety.c
fw/motors/motor_control/motor_safety.h
tests/comms/test_comms.c
tests/old_hw/mocks/stm32g4xx_hal.h
tests/old_hw/run_lot3_tests.py
tests/old_hw/test_board_protocol.c
tests/old_hw/test_board_transport.c
tests/old_hw/test_old_hw.c
tests/outputs/test_flash.py
tests/patch7/test_monitor.py
tests/startup/test_build_modes.py
tools/build_artifacts.py
tools/firmware.mk
tools/hw_monitor.py
tools/verify_startup_build.py
```

Nouveaux (11) :

```text
docs/OLD_HW_LOT4.md
docs/OLD_HW_BRINGUP.md
fw/app/old_hw_timing.c
fw/motors/motor_control/old_hw_timing.h
tests/old_hw/run_lot4_tests.py
tests/old_hw/test_bringup_board.c
tests/old_hw/test_link_codec.cpp
tests/old_hw/test_link_codec.py
tests/old_hw/test_lot4_local.c
tests/old_hw/test_timing.c
tests/old_hw/test_two_bringup.c
```

Les traces, DLL/EXE de tests, ELF/MAP/LIST/SU, manifestes et instantané sont
uniquement des artefacts sous `build/` ou alias ignorés `Debug/`. Aucun fichier
de branche historique ni aucun rapport antérieur modifié pour rendre les
résultats cohérents artificiellement.
