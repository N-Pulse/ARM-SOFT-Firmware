# Lot 6 — boucle fermée SIM exécutable

> Historical PWM protocol and validation report. For the current direction/speed
> output and velocity-feedback conversion, see [current SIM protocol](SIM_COMMUNICATION_CURRENT.md).
> The old convergence results below do not validate the new velocity interface.

État au 9 octobre 2026. Niveau obtenu : **tests logiciels PC avec le vrai code C,
et compilation/inspection ELF STM32 HW et SIM**. Aucun flash, aucune énumération
de sonde, aucun port série physique, aucun essai ROS2/Gazebo ou moteur réel.
Le [guide court](SIMULATION_QUICKSTART.md) donne la commande de reproduction.

## Résultat et périmètre

`python tools/run_simulation.py` compile et exécute le contrôleur, le PID,
la safety, le guard SIM, le backend SIM, la session, les codecs et le worker UART4
de production. Python fournit le client et le modèle, jamais un PID ou une safety
remplaçant le C. Les substitutions portent sur HAL, les frontières RTOS,
l'ordonnancement et les horloges. L'exécutable C est un processus séparé relié
au banc par stdin/stdout ; aucun matériel n'est nécessaire.

Une nouvelle consigne explicite produit une sortie PID signée, une évolution du
modèle, puis des mesures renvoyées au contrôleur C. Les 26 scénarios passent.
Les huit objectifs `0.30 + 0.05*ID` rad convergent après 5 s : erreur maximale
**0,00011994 rad**, critère <0,02 rad. STOP annule l'actionnement ; la position
reste proche de la position atteinte, sans retour à zéro ni vitesse forcée à zéro.
Après récupération/armement/activation, aucune ancienne consigne ne reprend.

| Niveau | PC natif | Compilé STM32 | UART réel | Gazebo/ROS2 | Moteurs réels |
|---|---|---|---|---|---|
| Boucle SIM, pertes et STOP | 26 scénarios exécutés | SIM oui, ELF inspecté | Non | Non | Non |
| Client, mapping effort, écrivain unique | Code Python et transports virtuels testés | Sans objet | Non | Imports et logique pure seulement | Non |
| Invariants HW et lots 1–5 | Régressions avec frontières matérielles simulées | HW oui, ELF inspecté | Non | Sans objet | Non |

Les tests natifs ne mesurent ni CPU STM32, ni IRQ réelles, ni durée sous PRIMASK,
ni marges de pile/heap en fonctionnement. Les chiffres de latence ci-dessous
sont ceux du temps virtuel du banc, pas des garanties matérielles.

## État initial et préservation

Avant modification : lecture des rapports STARTUP_LOT1, SAFETY_LOT2, COMMS_LOT3,
OUTPUTS_LOT4 et ROBUSTNESS_LOT5 ; suites startup/safety/comms/outputs et contrôle
startup ELF exécutés avec succès. Le lot 5 avait effectivement coupé RX SIM,
et le contrôle commun exigeait encore l'état/calibration du backend physique.
Réactiver uniquement RX aurait laissé une boucle inutilisable.

Le dépôt était déjà fortement modifié, indexé et non suivi. L'inventaire initial,
les empreintes de fichiers existants, le diff indexé binaire et les mesures avant
sont conservés sous `build/validation/lot6/`. Aucun commit/push/reset/clean,
aucune régénération CubeMX. Les changements de ce lot ne touchent pas `Core/`,
le BSP encodeur, les mappings électriques, `motor_pid_config.c`, le linker,
la configuration ADC, `.ioc` ou le script flash. Les anciennes suppressions
restent celles de l'état initial. Les artefacts sont reconstruits dans les
répertoires HW/SIM déjà séparés.

Contrôle final : 1200 entrées inventoriées, aucun fichier initial supplémentaire
manquant ; les 45 entrées déjà absentes le restent. 28 fichiers existants modifiés
dans le périmètre du lot, plus les nouveaux fichiers. Diff indexé binaire identique
(SHA256 f355a5a457f8cec5defed294e65d0f4900bd1bd1b41aa62de029e98c58376894).
`git diff --check` signale seulement un espace final préexistant dans
`stm32g4xx_ll_crs.h:444`, fichier dont l’empreinte est inchangée.
Voir [bilan des vérifications](../build/validation/lot6/final-summary.json) et
[préservation](../build/validation/lot6/preservation-final.json).

## Séparation HW/SIM et chemin réel

La petite interface `motor_backend_state.h` couvre initialisation des entrées,
état armé/FAULT/transition, disponibilité du feedback et inhibition/nettoyage.
En HW elle appelle les préconditions et mécanismes du lot 5 : driver physique,
encodeur calibré, verrou/générations et nettoyage différé conservés.
En SIM elle dépend uniquement de `SimSession`. Aucune calibration HW fictive.
Le driver physique `drv8848.c` est exclu du profil SIM.

Le chemin partagé reste `Motor_SetTargetAtEpoch → Motor_Update → MotorPID_Update
→ MotorBackend_ApplyOutput`. Révisions, epoch d'arrêt, calcul du temps écoulé,
rejet des non-finis, fraîcheur, retard ControlTask et récupération conditionnelle
restent communs. Le backend SIM transporte **la sortie PID**, jamais la référence
comme position à imposer par un second contrôleur. Les paramètres du modèle
sont nommés SIM et ne modifient pas les gains HW.

Au boot HW : désarmé, huit contrôleurs désactivés, ADC/OPAMP off ; pas de caller
d'armement/activation/calibration active, ni démarrage PWM/HRTIM.
Au boot SIM : session absente, non configuré, non armé, huit contrôleurs désactivés.
L'activation virtuelle est accessible uniquement dans SIM. Les vérificateurs ELF
cherchent ce chemin et interdisent les fonctions physiques pertinentes en SIM
(DRV init/écriture/inhibition, encodeurs physiques, writers de paires, démarrages
HAL). Le banc piège aussi tout appel physique par abort. Le code fatal peut
conserver une coupure directe vers LOW ; il ne dispose pas d'un chemin d'activation.

Le format HW et le protocole protobuf restent inchangés. Le build sélectionne
un seul parser : HW ou SIM v2, jamais une auto-détection. RX SIM utilise le même
UART4, ISR octet, ring et CommsTask ; parser en tâche uniquement. Aucun autre
consommateur RX, aucun polling HAL bloquant, aucun LPUART1 ajouté.

## Modèle et coordonnées fictives

Huit coordonnées **rotor indépendantes**, IDs 0..7, position en rad et vitesse
en rad/s. Les IDs 5/6 sont deux rotors de test, pas deux angles articulaires
Gazebo et pas une preuve du mécanisme différentiel de poignet.

Modèle `tools/sim/model.py`, pas Euler fixe 10 ms, initialement x=v=0 :
`v += (0.2*u - 4*v)*0.01`, puis `x += v*0.01` ; `u` est le pourcentage
signé réellement appliqué. STOP pose u=0, désactive le run, et laisse amortir v.
Aucune consigne n'est recopiée dans x. Profil PID SIM seulement : Kp=40, Ki=0,
Kd=4, filtre dérivé 0,02 s, dt maximal 0,05 s, limite intégrale 10 ; rampe de
référence 1 rad/s, cible bornée à ±2 rad. Ces valeurs servent ce modèle fictif.

Le guard SIM utilise la position mesurée et le pourcentage **réellement appliqué
rapporté par le modèle**, frais. À partir de 0,5 %, absence de progrès de 0,001 rad
pendant 250 ms : défaut global sticky GUARD. Une récupération nécessite le
progrès observé requis ; aucun compte encodeur physique n'est consulté.
La force est optionnelle et déclarée par axe : seuils SIM soft 1200 mN
(réduction à 25 %) et hard 1600 mN. Ce ne sont pas des protections de courant HW.

## Contrat unique SIM v2

Sources de référence : `fw/comm/sim_protocol.*`, `sim_session.*`,
`motor_sim_config.h` et `tools/sim/protocol.py`. Entiers little endian,
flottants IEEE754 binary32 little endian. Refus des NaN/Inf, subnormaux,
dépassements et conversions hôte sous-débordant en zéro. Les zéros de champs
absents sont canoniques (+0 binaire) et ne constituent aucune mesure valide.

Trame : `A5 5A | version=02 | type u8 | longueur payload u8 | payload | CRC16 LE`.
CRC16 CCITT, polynomial 0x1021, initial 0xFFFF, couvre version/type/longueur/payload.
Payload ≤144 octets, trame ≤151 ; payload applicatif commence toujours par
`session u64, sequence u32, stamp u32` (16 octets). Pas de compatibilité SIM v1
(CRC8), ni de traduction automatique depuis HW. Les helpers v1 conservés pour
leurs tests de bornes ne sont plus raccordés à RX SIM. L'ancien client
`serial_emg_client.py` délègue au client SIM v2 manuel.

Les structures suivantes décrivent le corps **après** les 16 octets communs :

| Type | Direction | Corps |
|---|---|---|
| 0x01 HELLO | Hôte → C | Vide ; sequence=0, stamp=0, session non nulle |
| 0x02 HEARTBEAT | Hôte → C | Vide |
| 0x03 CONFIG | Hôte → C | model u16=1, axes u8=0xFF, force_mask u8 |
| 0x04 RECOVER | Hôte → C | Vide ; récupération conditionnelle, sans activation |
| 0x05 ARM | Hôte → C | Vide ; armement virtuel explicite |
| 0x06 ENABLE | Hôte → C | mask u8 non nul, axes à activer |
| 0x07 TARGET | Hôte → C | count u8 (1..8), puis count × (ID u8, cible f32 rad, limite u8 0..100 %) |
| 0x08 STOP | Hôte → C | Vide ; arrêt global immédiat |
| 0x09 FEEDBACK | Hôte → C | run u32, sample_age u16 ms, count u8, puis count × entrée ci-dessous |
| 0x81 HELLO_ACK / 0x82 STATUS | C → Hôte | run u32, epoch u32, causes u32, enabled_mask u8, armed u8, configured u8, ack_type u8, ack_seq u32, result u8 |
| 0x83 OUTPUT | C → Hôte | run u32, epoch u32, enabled_mask u8, huit i16 dans l'ordre ID 0..7 |
| 0x08 STOP | C → Hôte | run u32, causes u32 |

Entrée FEEDBACK (14 octets) : `ID u8, fields u8, position f32, velocity f32,
force u16, applied i16`. Bits fields : position=1, vitesse=2, force=4,
applied=8 ; masque non nul, aucun autre bit. Position/vitesse bornées à ±1000
(unités rad/rad/s), force en mN 0..65535, applied et OUTPUT en **centièmes de
pourcent signé**, -10000..10000. Toute sortie d'un axe désactivé vaut zéro.
La limite TARGET borne le module de sortie PID ; elle n'est pas une vitesse.
Tous les IDs sont 0..7 et uniques par paquet. Ordre d'arrivée arbitraire, placement
par ID. Le count compte les entrées réellement présentes, jamais tous les noms
d'une configuration. Un FEEDBACK partiel ne met à jour que ses champs présents.
FORCE est refusé si l'axe ne la déclare pas dans CONFIG.

Chaque paquet est entièrement validé avant application. Un feedback numérique
malformé reçu pendant armement inhibe globalement ; CRC/version/longueur de trame
invalides sont rejetés, puis les échéances de fraîcheur continuent indépendamment.
Les paquets refusés ne renouvellent pas la liaison, sauf commandes structurellement
valides dont une précondition d'action échoue : elles répondent result=0 tout en
prouvant un trafic de session récent. result=1 n'est jamais une preuve de
mouvement ou de livraison au modèle.

### Sessions, séquences, ancienneté et reprise

La session u64 doit être strictement supérieure au high-water accepté depuis
le boot du firmware. Un HELLO identique de session encore live peut répéter
l'ACK ; il ne renouvelle pas la liaison et ne réarme rien. Après perte ou nouvelle
connexion, utiliser une nouvelle session supérieure. CONFIG est accepté une fois
par session, inactif. Changement de session : inhibition, invalidation des mesures,
configuration et armement ; défaut LINK si une session existait.

Le high-water est **volatile** : aucune protection de replay persistante entre
cold boots, aucune authentification. CRC n'assure pas l'identité. Le client utilise
`time.time_ns()`, puis `max(ancienne+1, time.time_ns())` au reconnect ; conserver
une horloge/session supérieure si l'hôte redémarre pendant que le firmware reste
allumé. Une horloge hôte revenue en arrière peut nécessiter correction explicite.
Ce contrat traite les anciennes trames dans une session d'exécution, pas un
attaquant capable de fabriquer du trafic neuf.

Chaque direction a sa séquence u32 globale. Accepter seulement un delta modulo
2^32 dans 1..0x7FFFFFFF ; doublons, anciens et demi-tour sont refusés. Wrap de
séquence et de HAL ms testé. Le run u32 change à l'armement et à l'inhibition,
et le feedback doit porter le run courant. Il ne wrappe pas : à UINT32_MAX,
nouvel armement refusé, nouvelle session supérieure nécessaire. OUTPUT doit
correspondre au run explicitement autorisé côté hôte et à l'epoch observée.
Seul un ACK d'ARM explicitement demandé peut autoriser le modèle.

Hôte → C, stamp reprend le dernier tick firmware reçu : âge ≤80 ms à l'application,
et âge de réception ≤80 ms, génération RX et epoch toujours actuelles. Le timestamp
de mesure est conservateur : `stamp - sample_age`, sample_age ≤50 ms. Une mesure
structurellement recevable peut déjà être trop ancienne pour activer : position,
applied et force déclarée doivent chacun avoir ≤100 ms. Heartbeat ne rafraîchit
aucune mesure. C → hôte : ancre fixe tick/local prise au Hello ACK reçu en ≤80 ms,
âge accepté de -20 à +80 ms (jitter/skew borné), jamais recalée par STATUS.
Une dérive d'horloge dépassant cette fenêtre provoque un refus sûr ; pas de
synchronisation d'horloge à long terme démontrée sur UART réel.

Séquence manuelle : HELLO/ACK → CONFIG/ACK → mesures fraîches des huit axes →
RECOVER/ACK si nécessaire → ARM/ACK → **nouvelles mesures du nouveau run** →
ENABLE/ACK → **nouveau TARGET**. ARM exige position et applied frais pour les
huit axes, plus force déclarée ; ENABLE prévalide tous les axes demandés.
Activation sans consigne nouvelle laisse la sortie nulle. STOP invalide
activations/consignes/PID préparés, run et sorties en attente. RECOVER ne réarme
jamais ; les conditions de chaque cause doivent être revenues à l'état permis.
Une restauration de trafic, un Hello, un feedback ou STATUS ne réactive rien.

Causes (bitmask) : MANUAL=1, GUARD=2, FEEDBACK=4, CONTROL_DELAY=8, FORCE=16,
DRIVER=32, PID=64, LINK=128 ; toutes inhibent globalement et restent sticky.

### RX, TX, saturation et watchdogs

Parser incrémental borné, resynchronisation par suffixes, délais interoctets
15 ms et trame entière 50 ms avec les timestamps originaux des octets.
Une génération/epoch nouvelle invalide le candidat. Overflow et erreur UART
inhibent immédiatement, indépendamment du parser et des queues ; réarmement
RX en tâche, jamais une remise en service de l'actionnement.

Le worker TX UART4 du lot 5 conserve engagement HAL IT sous le même verrou
que STOP, notification/attente RTOS hors verrou, timeout 25 ms, abort hors verrou
et quarantaine du buffer si abort échoue. Après échec, silence local supplémentaire
101 ms (paramètre transport conservé). En SIM, trois mailboxes bornées et
remplaçables : dernière OUTPUT, dernier STATUS/ACK, STOP urgent. STOP prend la
priorité sur les autres et la queue générique saturée, supprime OUTPUT en attente.
Epoch vérifiée à l'offre et à l'engagement ; OUTPUT de plus de 40 ms abandonnée.
Le TX SIM ne consomme pas la queue générique HW. Aucun backlog illimité.

Un envoi déjà engagé peut finir avant STOP. Une file pleine/offre acceptée,
un ACK écrasé ou un STOP mis en attente ne prouvent jamais la livraison.
Les séquences sortantes peuvent avoir des trous et l'ordre de priorité peut faire
rejeter un ancien STATUS : c'est sûr, mais impose de surveiller les ACK.
Envoyer une seule commande manuelle en attente et attendre son ACK ; en cas
ambigu, STOP/reconnexion et séquence explicite complète. Pas de retry aveugle
ARM/TARGET. Le client a huit commandes bornées, un feedback remplaçable,
au plus trois trames complètes prêtes ; **un seul thread écrit** le port.

| Élément | Période / échéance |
|---|---|
| ControlTask / safety / guard | 10 / 20 / 20 ms |
| Sorties et batch feedback hôte | 20 ms (50 Hz) |
| STATUS et heartbeat hôte | 40 ms (25 Hz) |
| Fraîcheur position/applied/force déclarée | 100 ms |
| Retard de contrôle autorisé | 50 ms |
| Session firmware/hôte/modèle UDP | 150 ms |
| Commande modèle / endpoint hôte | 100 ms |
| Lease paquet / âge capture hôte maximal | 80 / 50 ms |

L'endpoint hôte expire sa permission sur perte de OUTPUT même si STATUS continue.
Le modèle possède **son propre watchdog 100 ms**, sans exécution de l'endpoint :
commande à zéro, run révoqué, inertie conservée. Le serveur UDP séparé possède
aussi son horloge/session 150 ms et refuse APPLY d'un run expiré. Il continue si
le processus client/UART cesse d'exécuter ; une trame tardive ne le réarme pas.
Pour un modèle externe, ce watchdog doit vivre dans le modèle/contrôleur,
pas seulement dans le bridge qui pourrait mourir avec la liaison.

Budget UART4 inchangé : PC10 TX/PC11 RX, **115200 8N1**, donc 11520 octets/s
**par direction** en full duplex. FEEDBACK huit axes=142 octets, heartbeat=23 ;
50×142 +25×23 =7675 octets/s (66,62 %). OUTPUT=48 et STATUS=44 ;
50×48 +25×44 =3500 octets/s (30,38 %). Le plus long batch prend 12,33 ms,
OUTPUT 4,17 ms, STOP 31 octets 2,69 ms. Les commandes/ACK ponctuels consomment
le budget restant ; les fréquences/plafonds empêchent une croissance permanente.
Ne pas sommer les deux directions comme un lien half duplex.

## Tests exécutés et traces

Environnement : Windows, Python 3.12.8, GCC natif MinGW 6.3, ARM GCC 14.3.1,
GNU Make 3.81. L'ancien MinGW Make produisait INTERNAL readdir ; GNU Make 3.81
fonctionne avec les mêmes règles et wrappers.

```powershell
.\build.ps1 -Rebuild
.\build-sim.ps1 -Rebuild
python tests/startup/run_tests.py
python tests/safety/run_tests.py
python tests/comms/run_tests.py
python tests/outputs/run_tests.py
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/outputs/verify_build.py
python tests/startup/test_build_modes.py
python tools/run_simulation.py
```

Deux reconstructions complètes, 70 unités par profil. Suites lots 1–5 réussies :
11 contrôles startup, 24 scénarios safety asynchrones et 24 avec backend HW,
désarmement/armement interrompu/nettoyage/BSP intégrés, 21 scénarios communication
HW dont races TX, 24576 couples de bornes legacy SIM, huit groupes sorties
(176 états) et 18 scénarios flash **faux programmeur uniquement**. Les invariants
obsolètes « RX SIM coupée / Motor_Enable absent SIM » sont remplacés par
réception/activation virtuelle présentes et impossibilité des starts physiques.
Les quatre vérificateurs ELF passent pour les deux profils. Alternance
HW → SIM → HW/Debug : 140 objets inchangés, alias HW préservé, mode invalide rejeté.

Nouveaux tests : parser C de production, toutes longueurs u8, matrice de capacité,
100000 octets bruités, sentinelles, délais/epochs/wraps ; codec Python/non-finis,
imports sans ROS/port, mapping par ID et feedback partiel/horodaté ; trois
producteurs et un écrivain fake serial ; serveur modèle dans un vrai processus
OS sur UDP localhost, perte du client puis inertie/watchdog et rejet d'APPLY tardif.

26 scénarios fermés : démarrage/reconnexion, activation/convergence/STOP/reprise,
routage partiel, perte feedback avec heartbeat vivant, chacune des directions,
coupure totale, pause du processus hôte, paquets malformés et anciens,
fragmentation/consecutifs/bruit/deadlines, erreur UART/overflow/réarmement échoué,
STOP avec queue pleine, STOP en vol à l'engagement HAL, timeout/erreur TX,
abort échoué/quarantaine, expiration OUTPUT, retard ControlTask,
force excessive/manquante, guard SIM, débit soutenu, feedback tardif,
pertes périodiques, wraps et sept rejets atomiques de feedback.

| Injection virtuelle | Résultat PC |
|---|---|
| Coupure C → hôte seule | Inhibition locale 100 ms, distante 120 ms |
| Coupure hôte → C seule | Locale 80 ms, distante 90 ms |
| Coupure totale | Locale 80 ms, distante 120 ms |
| Endpoint hôte ne s'exécute plus | Modèle expire seul, actionnement nul |
| +10 ms chaque direction pendant 10 s | Convergence ; 7652,41 / 3466,80 octets/s observés, au plus 4 / 2 trames virtuelles en attente |
| Perte d'un feedback sur quatre +10 ms chaque direction, 6 s | Convergence <0,02 rad sans défaut |
| Feedback retardé de 110 ms, heartbeat sans retard ajouté | Défaut FEEDBACK, arrêt, liaison encore live |
| Queue générique pleine (8) + TX suspendu 200 ms | Remplacement borné, STOP prioritaire, vieille OUTPUT sans réactivation |

Latences mesurées aux pas virtuels de 10 ms, avec serialization 8N1 et files
indépendantes par direction. Les scénarios assertent les bornes locales ≤240 ms
et distantes ≤180 ms ; elles couvrent ce banc, pas tout ordonnancement RTOS.
Le lien virtuel délivre les trames après leur temps de sérialisation ; les tests
fragmentation/ISR complètent ce modèle, sans émuler l'UART cycle par cycle.

Trace [CSV](../build/simulation/trace.csv) : scénario, temps, session, run, ID,
consigne, référence rampée, position reçue et modèle, vitesse, sortie PID,
actionnement appliqué, enabled/armed, causes, âge position, état watchdog.
[Récapitulatif JSON](../build/simulation/results.json) et
[log final](../build/validation/lot6/simulation-final.log).
Ces fichiers sont générés, remplacés à chaque lancement et non une capture UART.

**Skips :** ASan+UBSan et UBSan seul ne compilent/exécutent pas avec les runtimes
locaux ; sondes et raisons dans `build/simulation/probe-*.log` et les logs safety.
Les scénarios sans sanitizer passent ; aucun succès sanitizer n'est revendiqué.
Le test optionnel du codec hôte **HW** est sauté car Python protobuf absent ;
les tests du décodeur HW C++ de production passent. Aucun essai UART, ROS2,
Gazebo, charges mécaniques, oscilloscope ou temps réel : environnement/matériel
non utilisé dans ce lot.

## Mémoire et pile statique

Mesures `arm-none-eabi-size` avant/après sur ELF des mêmes profils et outil.

| Profil | text avant → après | data avant → après | bss avant → après | Flash text+data | RAM data+bss |
|---|---|---|---|---|---|
| HW | 90440 → 90620 (+180) | 384 → 384 | 31400 → 31408 (+8) | +180 | +8 |
| SIM | 76272 → 78972 (+2700) | 384 → 96 (-288) | 27504 → 31848 (+4344) | +2412 | +4056 |

Le profil SIM embarque désormais RX/parser/session et mailboxes ; le tableau
rapporte la taille liée totale, pas une attribution instruction par instruction.
Les 8 octets HW supplémentaires sont les diagnostics TX remplacement/expiration.
Les archives `before-stack.json`, `after-stack.json` et `stack-delta.json`
sous `build/validation/lot6/` contiennent les fichiers .su/comparaisons complets.

| Cadre .su (octets, par fonction) | HW avant → après | SIM avant → après |
|---|---|---|
| Motor_Update | 160 → 160 | 160 → 160 |
| Motor_Enable | 32 → 32 | 32 → 32 |
| MotorSafety_Poll / RequestStopCauseLocked | 24 / 32 inchangés | 24 / 32 inchangés |
| Motor_RecoverFromFault / UART4Transport_TxOnce | 24 / 48 inchangés | 24 / 48 inchangés |
| MotorGuard_Poll | 48 → 48 | 48 → 40 |
| Motor_OnPositionFeedback (wrapper) | 40 → 24 | 40 → 24 |
| Nouveau Motor_OnPositionFeedbackAtTime | — → 32 | — → 32 |
| HAL_UART_RxCpltCallback | 64 → 64 | 16 → 64 |
| Nouveau SimSession_OnFrame / validate | Absent ELF | 136 / 80 |
| Nouveau SimSession_Service / SimParser_Feed | Absent ELF | 352 / 48 |
| Encoder BSP sample_locked | 184 → 184 | Objet 184, absent ELF SIM |

Un wrapper plus petit ajoute un appel : ne pas assimiler baisse de cadre à baisse
de profondeur cumulée. .su ne compte pas à lui seul graphes d'appels, libc/libm,
FPU/exceptions, nesting, heap RTOS ou marge effective. Control/safety gardent
256 mots chacun, comms 768, TX 384, guard 384 ; heap RTOS 16 KiB inchangé.
La tâche SIM spécifique historique est supprimée ; service par les tâches
existantes. Aucun dimensionnement de pile matérielle n'est déclaré suffisant.
Les conversions double sous verrou du BSP HW ajoutées au lot 5 restent présentes
et doivent être chronométrées sur carte, avec le chemin HAL IT/exclusive retry.

## Procédure STM32 en mode SIM — fournie, non exécutée

1. Compiler `.\build-sim.ps1 -Rebuild` et exécuter les quatre contrôles ELF.
   L'artefact SIM est `build/SIM/n-pulse.elf`. Le chargement ultérieur nécessite
   une procédure SIM autorisée séparément : `flash.ps1` refuse SIM et reste
   limité à `build/HW/n-pulse.elf`, manifeste cohérent et SN exact.
2. Avec une carte **déjà chargée en SIM**, vérifier son identité/mode et l'UART4
   115200 8N1, matériel moteur maintenu sans puissance. Installer pyserial
   uniquement pour ce chemin : `python -m pip install pyserial`.
3. Terminal A : `python tools/sim_model_server.py --port 48160`.
   Ce processus est le modèle/watchdog indépendant ; le laisser vivre lors
   d'une pause/fermeture du client pour observer l'expiration.
4. Terminal B : `python tools/sim_client.py --port COM_SIM_CONFIRME --model-port 48160`.
   Le port est fourni explicitement, jamais énuméré. L'ouverture envoie seulement
   Hello ; aucune configuration, activation ou cible automatique.
5. À l'invite : `status` (live), `config`, attendre ACK et feedback frais ;
   `recover` si défaut, puis `arm`, attendre ACK/nouveau feedback au moins 80 ms,
   `enable 0xff`, attendre ACK, `target 0 0.5 60`. Une commande à la fois,
   contrôler `status` et les sorties/positions côté modèle.
6. `stop` : vérifier actionnement nul et inertie ; récupérer/réarmer/activer
   sans nouvelle cible doit rester nul. `reconnect` exige reconfiguration et
   toute la séquence explicite ; aucune reprise automatique.
7. Tester chaque direction et couper/figer le client en conservant le serveur :
   mesurer arrêt firmware et modèle, RTOS/IRQ/heap/piles. Les résultats PC ne
   remplacent pas ces mesures. Aucun de ces gestes matériels n'a été exécuté ici.

## Adaptateur ROS2/Gazebo — contrat, non exécuté

`tools/ros2_bridge/serial_bridge.py` importe ROS seulement dans main.
Il exige un contrôleur **d'effort** existant, huit noms de coordonnées d'actionneurs
ordonnés IDs 0..7, huit échelles positives effort/par-pourcent, un topic de
commande Float64MultiArray et un topic JointState d'effort **réellement appliqué**.
Aucun fallback position, aucune cinématique de poignet ou calibration inventée.
Les échelles/coordonnées doivent provenir du modèle fourni ; aucune valeur physique
n'est proposée ici. Exemple de forme à compléter avec les données du modèle :

```text
python tools/ros2_bridge/serial_bridge.py --port <port_SIM> --joint-names <8_noms_ID0_a_ID7> --effort-per-percent <8_echelles> --effort-topic <topic_commande_effort> --applied-effort-topic <topic_effort_applique> --model-watchdog-ms <delai_1_a_100>
```

`/joint_states` apporte noms/position/vitesse, placés par ID ; seuls les noms
et champs effectivement présents partent en feedback. L'effort appliqué est
converti avec les mêmes échelles, jamais déduit de l'effort demandé.
Headers source non nuls et âge ROS 0..50 ms exigés, convertis vers l'horloge
monotone hôte ; timestamp du batch = plus ancien champ inclus. Champs manquants
n'ont pas de valid bit. Force optionnelle : `--force-topics` avec huit topics
WrenchStamped ordonnés par ID ; norme du vecteur force en N convertie en mN,
avec source horodatée et CONFIG force_mask explicite.

`/n_pulse/sim_control` (std_msgs/String) reçoit les commandes manuelles
`config [mask]`, `recover`, `arm`, `enable mask`, `target ID rad limite`,
`stop`. STOP publie **huit efforts nuls**, jamais huit positions nulles.
Le modèle/contrôleur doit posséder son propre watchdog ≤100 ms. L'argument
`--model-watchdog-ms` déclare cette exigence ; il ne prouve pas sa configuration.
La mort du bridge peut empêcher la dernière publication zéro, d'où le watchdog
extérieur obligatoire. Un contrôleur de position seul ne convient pas à ce test
PID ; maintien de position et annulation d'effort seraient des modes différents.
Aucun modèle ni environnement ROS2/Gazebo n'étant fourni/exécuté, seule la logique
pure de mapping/import a été testée sur PC.

## Prérequis concrets du prochain lot HW

- Révision exacte de carte QET6, schéma et nomenclature correspondants ; références
  réelles des huit drivers, brochage et polarités, modes d'arrêt/coast/brake.
- Table vérifiée ID 0..7 ↔ moteur ↔ PWM/timer/canal/pin ↔ encodeur A/B/timer ;
  coordonnées de poignet et modèle différentiel si utilisé.
- SLEEP/FAULT : pins, polarités, pull-ups, partage, propagation et réaction à
  l'arrêt ; état sans alimentation et ordre d'alimentation.
- CPR encodeur, définition PPR/CPR et facteur quadrature, rapports mécaniques,
  sens positifs, offsets persistants et gestion des wraps/comptes manqués.
- Courses autorisées, repères mécaniques et procédure de mise à zéro autorisée
  avec capteurs/données ; aucune recherche de butée automatique dans ce lot.
- Chaîne de mesure courant réelle : shunt/gain/offset, ADC/référence/plage,
  filtrage/débit/saturation, unités, calibration et protection indépendante.

Checklist **désarmée**, sans puissance moteurs, à préparer/exécuter sur carte :

- [ ] Vérifier boot, ADC/OPAMP off, huit activations false et SLEEP LOW ; aucune
      calibration, armement, homing ou commande de mouvement.
- [ ] Mesurer ticks HAL/RTOS, périodes et jitter control/safety/guard/comms,
      timeout TX et comportement si une tâche manque son échéance.
- [ ] Chronométrer chaque région PRIMASK : BSP double, snapshots/locks imbriqués,
      parser/session SIM le cas échéant et engagement HAL IT/LDREX-STREX lot 5.
- [ ] Mesurer high-water de chaque pile, MSP avec exceptions/FPU, heap RTOS et
      échecs d'allocation ; confronter aux graphes d'appels et fichiers .su.
- [ ] Lire comptes bruts des huit encodeurs à la main/source de test, vérifier
      ordre/sens/quadrature/wrap/reset et absence de calibration/mouvement induit.
- [ ] Injecter FAULT sans puissance ; observer latch/arrêt/SLEEP, récupération
      refusée tant que la cause persiste et aucune reprise automatique.
- [ ] Observer au scope niveaux SLEEP/FAULT/PWM et signaux timers/paires sans
      puissance moteurs, y compris boot/fatal/STOP ; toute excitation de test
      exige une procédure explicite distincte, pas un armement implicite.

Armement HW, homing, réglage PID physique et protection courant seront traités
avec ces données et une procédure de test explicite. Aucun de ces chemins n'est
ajouté au démarrage ou aux handlers HW par le lot 6.
