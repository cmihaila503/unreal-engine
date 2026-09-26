<!-- Text extracted 2026-09-19 from the PDF the user supplied (H:/3D FBX/PDF.pdf, 31 pages). Source of truth for the Police Vehicle AI work; ASCII diagrams lost their alignment in extraction - the PDF is authoritative where they matter. -->

MURDAR --- Specificația Sistemului Police
Vehicle AI
UE5 / Chaos Vehicles --- Source of Truth
Versiune: 1.0\
Status: Implementare phase-by-phase prin Claude Code\
Scope: Police Vehicle AI, urmăriri, tactici de oprire, condus de
urgență, coordonare între unități și World Police Events.
------------------------------------------------------------------------
0. SCOP ȘI PRINCIPII
Sistemul trebuie să permită poliției să conducă autonom mașini în lumea
MURDAR, să răspundă la crime reale sau generate în lume, să urmărească
suspecți, să evite obstacole, să aleagă tactici de oprire și să treacă
natural de la vehicul la AI pietonal.
Sistemul trebuie să funcționeze atât pentru: - urmărirea playerului; -
urmărirea unui NPC criminal; - evenimente polițienești independente de
player; - patrulare; - traffic stop; - interceptare; -
transport/returnare după incident.
Principiul central: \> Mașina de poliție este un vehicul Chaos real
controlat de AI, nu un obiect care se deplasează pe spline.
AI-ul produce intenții/input/control; Chaos Vehicle Physics rămâne
solverul fizic autoritar.
------------------------------------------------------------------------
1. REGULI OBLIGATORII PENTRU CLAUDE CODE
1. Citește întregul document înainte de implementare.
2. Execută fazele strict în ordine.
3. Nu trece la următoarea fază fără toate criteriile GATA.
4. Compilează după fiecare fază.
5. Nu inventa API-uri UE5.
6. În Phase 0 verifică exact versiunea UE5 și API-urile disponibile în
Engine Source.
7. Documentează verificarea în `Docs/POLICE_VEHICLE_AI_RECON.md`.
8. Nu modifica automat `.uasset` sau `.umap`.
9. Nu folosi Blueprint ca înlocuitor pentru arhitectura C++; Blueprint
este permis pentru wiring, animații, VFX și configurare.
10. Fără magic numbers.
11. Orice parametru de tuning trebuie să aibă owner unic, unitate,
default, min/max și motiv.
12. O singură sursă de adevăr pentru fiecare runtime value.
13. Physics thread nu citește direct UObject-uri; configurația necesară
trebuie copiată într-o structură sigură.
14. Nu folosi `AddImpulse` pentru forțe continue de control.
15. Nu modifica masa vehiculului pentru a repara comportamentul AI.
16. Nu crea polițiști lângă player doar pentru că Heat a crescut.
17. Poliția nu primește poziția actuală a suspectului dacă nu o cunoaște
legitim.
18. După pierderea contactului se folosește Last Known Position +
Search, nu poziția reală.
19. Sirenele și luminile sunt controlate de starea AI, nu de player.
20. Toate tacticile trebuie să poată eșua și să aibă recovery.
21. Un AI blocat trebuie să poată abandona planul curent și cere altă
strategie/unitate.
22. Nu presupune că toate drumurile sunt navigabile.
23. Nu presupune că toate unitățile au aceeași agresivitate.
24. Nu presupune că PIT-ul este mereu soluția.
25. Nu face poliția să se lipească de țintă; păstrează chase slots și
spacing.
26. Nu face toate unitățile să execute aceeași manevră simultan.
27. Nu permite coliziuni intenționate dacă evaluarea tactică nu
justifică manevra.
28. Nu folosi sistemul Police Response Director pentru steering
individual; acesta distribuie obiective și unități.
29. AI-ul pietonal și AI-ul vehiculului trebuie să poată preda controlul
între ele.
30. După fiecare fază: raport
IMPLEMENTED / TESTED / FAILED / BLOCKED / NEXT.
31. Dacă proiectul existent contrazice această arhitectură, nu rescrie
arbitrar proiectul: raportează conflictul și propune cea mai mică
modificare compatibilă.
------------------------------------------------------------------------
2. ARHITECTURA GENERALĂ
MURDAR WORLD
|
+-- Crime System
|
+-- Heat System
|
+-- Police Response Director
|
+-- Police World Event Director
|
+-- Suspect NPC
+-- Police Unit A
+-- Police Unit B
+-- Police Unit C
|
v
Police Vehicle AI
|
+-------+--------+
| | |
Routing Driving Tactics
| | |
Lanes Avoidance PIT
Roads Recovery Box-In
Intersections Roadblock
Intercept
Side Sweep
|
v
Chaos Vehicle Physics
|
v
Vehicle State
|
+------+------+
| |
Vehicle AI Pedestrian AI
| |
+------handoff+
------------------------------------------------------------------------
3. COMPONENTELE PRINCIPALE
Numele sunt propuneri și trebuie validate în Phase 0.
AMurdarPoliceVehiclePawn
UMurdarPoliceVehicleMovementComponent
UMurdarPoliceVehicleAIComponent
UMurdarPoliceEmergencyDrivingComponent
UMurdarPolicePursuitComponent
UMurdarPoliceTacticsComponent
UMurdarPoliceNavigationComponent
UMurdarPoliceObstacleAvoidanceComponent
UMurdarPoliceRecoveryComponent
UMurdarPoliceCoordinationComponent
UMurdarPoliceVehicleTelemetryComponent
UMurdarPoliceResponseDirector
UMurdarPoliceWorldEventDirector
UMurdarPoliceUnitManager
UMurdarPoliceSearchManager
UMurdarVehicleAIPathComponent
UMurdarVehicleLaneComponent
UMurdarVehicleTrafficInteractionComponent
Responsabilități:
PoliceVehicleAI
Orchestrator local. Nu deține singur toate sistemele.
EmergencyDriving
Gestionează: - sirenă; - lumini; - emergency priority; - stilul de
condus; - toleranța la risc; - viteza țintă.
Pursuit
Gestionează: - urmărirea; - target knowledge; - contact; - chase
slots; - pierderea contactului; - revenirea în pursuit.
Tactics
Alege și execută tacticile: - Follow; - Intercept; - PIT; - Box-In; -
Roadblock; - Side Sweep; - Multi-Unit Pin; - Ram/Aggressive Intercept.
Navigation
Transformă obiectivul într-o rută de drum și lane.
ObstacleAvoidance
Reacție locală la: - trafic; - mașini parcate; - pietoni; - obiecte; -
accidente; - vehicule oprite; - vehicule care schimbă banda.
Recovery
Detectează și rezolvă: - stuck; - dead end; - blocare; - coliziune; -
imposibilitate de rutare.
Coordination
Coordonează mai multe unități fără hive mind.
------------------------------------------------------------------------
4. DATELE DE CONFIGURARE
Toate profilele trebuie să fie data-driven.
Exemple:
PoliceDrivingProfile
- MaxCruiseSpeed
- EmergencySpeedMultiplier
- DesiredFollowDistance
- MinimumPursuitDistance
- MaximumRisk
- LaneChangeAggression
- OvertakeAggression
- BrakingConfidence
- CollisionRiskTolerance
- PITRiskTolerance
- RoadblockPreference
- BoxInPreference
- InterceptPreference
- RecoveryTimeout
Profile:
Police_Disciplined
Police_Normal
Police_Aggressive
Police_Elite
Police_Inexperienced
Police_Tactical
Personalitatea modifică preferințele, nu creează state machine-uri
separate.
------------------------------------------------------------------------
5. STAREA MAȘINII DE POLIȚIE
State model:
AVAILABLE
PATROLLING
RESPONDING
APPROACHING_TARGET
EMERGENCY_DRIVING
PURSUING
POSITIONING
ATTEMPTING_INTERCEPT
ATTEMPTING_PIT
BOXING_IN
ROADBLOCKING
STOP_CONFIRMED
SECURING_VEHICLE
OFFICERS_EXITING
FOOT_HANDOFF
SEARCHING
RETURNING
RECOVERY
DISABLED
Orice state trebuie să definească: - Entry; - Exit; - Allowed
interruption; - Timeout; - Failure; - Recovery; - Next valid states.
------------------------------------------------------------------------
6. EMERGENCY DRIVING
Când unitatea primește un obiectiv de urgență:
NORMAL DRIVING
|
v
EMERGENCY REQUEST
|
+--> SIREN ON
+--> LIGHTS ON
+--> EmergencyPriority ON
+--> DrivingProfile = Emergency
v
EMERGENCY DRIVING
La terminarea obiectivului:
EMERGENCY OFF
-> siren OFF
-> lights OFF
-> normal traffic behavior
Sirena trebuie pornită automat de AI atunci când condițiile o cer.
Civilian AI trebuie să poată reacționa la: - sirenă; - lumini; -
apropierea vehiculului de urgență.
------------------------------------------------------------------------
7. ROUTING ȘI ROAD NETWORK
AI-ul vehiculului nu navighează direct NavMesh-ul pietonal pentru
traseul principal.
Trebuie definit un strat rutier compatibil cu vehiculele:
Road
├── Lane
│ ├── Direction
│ ├── SpeedLimit
│ ├── Connections
│ └── AllowedVehicleTypes
└── Intersection
├── Incoming Lanes
├── Outgoing Lanes
├── Turn Options
└── Traffic Control
Dacă proiectul are deja sistem de trafic, acesta trebuie reutilizat.
Dacă există plugin/framework existent, Phase 0 trebuie să determine dacă
poate fi folosit.
Nu implementa un al doilea road system fără justificare.
------------------------------------------------------------------------
8. LANE FOLLOWING
AI-ul trebuie să urmărească: - centerline; - lane direction; -
curvature; - desired speed; - lead vehicle; - upcoming intersection.
Controlul trebuie să producă: - steering target; - acceleration
target; - brake target; - gear/reverse intent dacă este disponibil.
Nu se controlează direct transformul vehiculului.
------------------------------------------------------------------------
9. VITEZA
Viteza țintă trebuie calculată din:
SpeedLimit
Curvature
TrafficDensity
ObstacleDistance
IntersectionDistance
EmergencyState
TacticalState
VehicleCondition
DriverProfile
Exemplu conceptual:
TargetSpeed =
min(
RoadLimit,
CurvatureLimit,
TrafficLimit,
ObstacleLimit,
TacticalLimit
)
Nu folosi o valoare fixă de tip „police always drives 180 km/h".
------------------------------------------------------------------------
10. INTERSECȚII
AI-ul trebuie să detecteze din timp:
• semafor;
• stop;
• trafic transversal;
• vehicule care virează;
• pietoni;
• posibilitatea de coliziune.
În emergency mode poate traversa o intersecție cu prioritate crescută,
dar nu cu ignorarea completă a lumii.
Trebuie evaluat: - vizibilitate; - trafic; - viteză; - distanță de
frânare; - risc de coliziune.
------------------------------------------------------------------------
11. OBSTACLE AVOIDANCE
Niveluri:
A. Predictive
Detectează obiectul înainte de contact.
B. Local avoidance
Alege: - stânga; - dreapta; - frânare; - schimbare bandă; - oprire.
C. Re-route
Dacă obstacolul blochează drumul: - lane change; - alternative lane; -
alternative road.
D. Recovery
Dacă toate eșuează: - reverse; - reposition; - re-route; - request
assistance.
Nu face teleport.
------------------------------------------------------------------------
12. COLLISION RISK
Pentru fiecare obstacol relevant:
RelativePosition
RelativeVelocity
Distance
TimeToCollision
LaneOverlap
EscapeOptions
AI-ul trebuie să aleagă între:
BRAKE
STEER_LEFT
STEER_RIGHT
LANE_CHANGE
OVERTAKE
STOP
REROUTE
în funcție de risc.
------------------------------------------------------------------------
13. TRAFFIC INTERACTION
Poliția trebuie să poată trece prin trafic în timpul urmăririi.
Comportament:
FOLLOW LANE
|
+-- slow traffic -> overtake
|
+-- stopped traffic -> lane change
|
+-- blocked lanes -> reroute
|
+-- emergency corridor -> use available opening
Nu trebuie să forțeze fiecare mașină civilă să dispară.
Civilian traffic poate: - încetini; - schimba banda; - trage spre
margine; - opri temporar; - continua dacă nu are loc.
------------------------------------------------------------------------
14. ANTI-STUCK SYSTEM
Trebuie să fie sistem first-class.
Detectoare:
PositionDelta
Velocity
SteeringCommand
ThrottleCommand
ObstaclePersistence
RouteProgress
Exemplu:
MOVING
|
| insufficient progress
v
STUCK_SUSPECTED
|
+--> steering correction
|
+--> lane change
|
+--> reverse
|
+--> re-route
|
+--> alternative road
|
+--> abandon tactic
|
+--> request backup
|
v
RECOVERED / FAILED
Un timer de recovery trebuie configurabil.
------------------------------------------------------------------------
15. PURSUIT ARCHITECTURE
Pursuit-ul nu este doar:
DriveTo(Player)
Este:
PursuitTarget
TargetIdentity
TargetVehicleIdentity
KnowledgeConfidence
LastSeenLocation
LastSeenTime
LastKnownDirection
PredictedDirection
RoadContext
TargetSpeed
TargetHeading
Dacă targetul este vizibil: - actualizează contactul.
Dacă nu: - Last Known; - search; - predicted route; - radio information
legitimă.
------------------------------------------------------------------------
16. CHASE SLOTS
Pentru 2+ polițiști:
INTERCEPTOR
↓
[ TARGET VEHICLE ]
FOLLOW A FOLLOW B
Roluri:
PRIMARY
SECONDARY
INTERCEPTOR
BLOCKER
SUPPORT
Nu toate mașinile urmăresc exact aceeași traiectorie.
------------------------------------------------------------------------
17. DISTANȚA DE URMĂRIRE
Unitatea trebuie să păstreze o distanță tactică.
Factori: - viteză; - Heat; - profil; - trafic; - risc; - tactic; -
vehicul.
Too close: - crește riscul coliziunii; - reduce timpul de reacție.
Too far: - pierde presiunea; - crește riscul pierderii contactului.
------------------------------------------------------------------------
18. TACTICA 1 --- FOLLOW / SHADOW
Scop: - menținerea contactului; - așteptarea unei oportunități.
Nu încearcă oprirea imediată.
Preferată: - Heat redus; - trafic dens; - risc mare; - unitate prudentă.
------------------------------------------------------------------------
19. TACTICA 2 --- INTERCEPT
Unitatea nu urmărește direct ținta.
Calculează:
TargetLastKnown
TargetVelocity
TargetHeading
RoadNetwork
CandidateIntersections
ETA
Alege un punct înaintea țintei.
TARGET ------>
\
\ predicted path
\
X INTERCEPTOR
Dacă interceptarea eșuează: INTERCEPT -> FOLLOW.
------------------------------------------------------------------------
20. TACTICA 3 --- PIT
PIT este o manevră condiționată.
Precondiții: - viteză compatibilă; - unghi relativ; - spațiu; - risc
acceptabil; - vehicul capabil; - target vehicle state valid; - trafic
evaluat; - profil permite PIT; - Heat permite sau tactica este
autorizată.
Execuție:
ALIGN
↓
CLOSE_DISTANCE
↓
CONTACT_WINDOW
↓
PIT_EXECUTE
↓
TARGET_RESPONSE
↓
SUCCESS / FAIL / ABORT
După contact: - nu presupune că targetul s-a oprit; - recalculează
starea.
------------------------------------------------------------------------
21. PIT ABORT
PIT trebuie abandonat dacă: - trafic prea dens; - pietoni în zona de
risc; - unghi invalid; - viteză prea mare; - target schimbă brusc
direcția; - polițistul pierde poziția; - obstacol static; - altă unitate
ocupă zona; - risc de coliziune disproporționat.
Fallback: FOLLOW sau INTERCEPT.
------------------------------------------------------------------------
22. TACTICA 4 --- BOX-IN
Pentru minimum două unități:
POLICE
↓
+-------------+
| TARGET |
+-------------+
↑
POLICE
Unitățile primesc sloturi: - FRONT; - REAR; - LEFT/RIGHT dacă este
sigur.
Fiecare unitate ajustează poziția.
Nu se permite ocuparea aceluiași slot.
------------------------------------------------------------------------
23. TACTICA 5 --- ROADBLOCK
Response Director selectează: - intersecție; - drum îngust; - punct de
constrângere; - zonă cu puține alternative.
Unitatea se deplasează acolo prin road network.
La sosire: POSITIONING -> BLOCKING.
Dacă suspectul schimbă ruta: - roadblock devine invalid; - unitatea
poate trece în INTERCEPT.
------------------------------------------------------------------------
24. TACTICA 6 --- SIDE SWEEP
Unitatea încearcă să împingă/forțeze direcția țintei fără contact
distructiv inutil.
Evaluare: - lateral clearance; - obstacole; - trafic; - escape route; -
Heat; - agresivitate.
Dacă spațiul este insuficient: ABORT.
------------------------------------------------------------------------
25. TACTICA 7 --- MULTI-UNIT PIN
Pentru Heat ridicat:
POLICE
↓
POLICE -> TARGET <- POLICE
↑
POLICE
Response Director atribuie roluri.
Nu toate unitățile atacă simultan.
Unele: - urmăresc; - blochează; - interceptează; - mențin presiunea.
------------------------------------------------------------------------
26. AGRESIVITATEA ÎN FUNCȚIE DE HEAT
Heat influențează: - distanța de urmărire; - viteza; - frecvența
schimbării benzii; - frecvența interceptărilor; - PIT willingness; -
roadblock; - numărul de unități; - toleranța la risc; - timpul de
răspuns; - folosirea tacticilor avansate.
Dar:
Behavior =
Heat
+ PoliceProfile
+ CrimeSeverity
+ Traffic
+ VehicleCondition
+ AvailableUnits
+ RoadContext
+ OfficerKnowledge
Nu: Behavior = Heat.
------------------------------------------------------------------------
27. POLICE WORLD EVENTS
Acesta este unul dintre obiectivele principale ale sistemului.
Playerul nu trebuie să fie necesar pentru existența unei urmăriri.
Exemplu:
CRIMINAL COMMITS CRIME
↓
WITNESS / POLICE PERCEPTION
↓
CRIME EVENT
↓
POLICE RESPONSE DIRECTOR
↓
DISPATCH UNITS
↓
PURSUIT
↓
TACTICAL STOP
↓
ARREST / ESCAPE / SHOOTOUT / RECOVERY
------------------------------------------------------------------------
28. WORLD EVENT DIRECTOR
UMurdarPoliceWorldEventDirector trebuie să poată genera evenimente
precum:
• stolen vehicle pursuit;
• robbery getaway;
• armed suspect pursuit;
• violent offender pursuit;
• gang vehicle pursuit;
• traffic offender pursuit;
• convoy/intercept;
• police response to shooting.
Evenimentele trebuie să aibă: - tip; - severitate; - locație; -
suspect; - unități; - motiv; - lifetime; - spawn rules; - despawn
rules; - outcome.
------------------------------------------------------------------------
29. SPAWN REALIST
Nu spawnăm poliția la 20 m de player.
Unitățile trebuie să aibă: - origine; - distanță; - ETA; - road
access; - availability; - assignment.
Dacă playerul este departe: - eventul poate fi simulat la frecvență
redusă; - când playerul se apropie, devine fully simulated.
------------------------------------------------------------------------
30. EVENIMENT OBSERVABIL DE PLAYER
Exemplu:
Playerul merge normal.
La distanță:
SIREN
Playerul vede:
POLICE -----> CRIMINAL
Sistemul trebuie să permită:
1. poliția ajunge din spate;
2. suspectul schimbă banda;
3. poliția evită traficul;
4. o a doua unitate interceptează;
5. prima încearcă PIT;
6. suspectul pierde controlul;
7. două mașini îl blochează;
8. polițiștii ies;
9. suspectul trage;
10. poliția răspunde;
11. suspectul moare/se predă/fuge;
12. eventul se încheie.
Nu trebuie să existe o cutscene.
------------------------------------------------------------------------
31. VEHICLE → PEDESTRIAN HANDOFF
Când suspectul este oprit:
PURSUIT
↓
STOP_CONFIRMED
↓
VEHICLE_SECURED
↓
PED_AI ACTIVATED
↓
OFFICER AI EXIT
↓
ARREST / COMBAT / FLEE
Când suspectul fuge: - mașina rămâne în lume; - vehiculul devine obiect
separat de interes; - AI-ul pietonal continuă.
------------------------------------------------------------------------
32. POLICE VEHICLE → POLICE OFFICER HANDOFF
Driver și passenger officers sunt entități AI distincte.
Police Vehicle
├── Driver AI
└── Officer AI
La oprire:
Driver secures vehicle
Officer exits
Officer takes interaction/combat role
Driver may cover
Pentru mai multe unități: - unii ofițeri acoperă; - unul
negociază/arestează; - ceilalți mențin perimetrul.
------------------------------------------------------------------------
33. SHOOTOUT AFTER VEHICLE STOP
Dacă suspectul devine violent:
VEHICLE STOP
↓
SUSPECT THREAT
↓
OFFICER RESPONSE
↓
COMBAT AI
Police Vehicle AI nu controlează aim/shooting.
Face doar handoff către Combat AI.
------------------------------------------------------------------------
34. POLICE COORDINATION
Response Director poate comunica:
TARGET_SPOTTED
TARGET_LOST
LAST_KNOWN_LOCATION
INTERCEPT_REQUEST
ROADBLOCK_REQUEST
PIT_OPPORTUNITY
UNIT_DISABLED
BACKUP_REQUIRED
PURSUIT_ESCALATION
Mesajele trebuie să aibă: - Source; - Timestamp; - Confidence; -
Target; - Location; - Urgency.
Fără hive mind.
------------------------------------------------------------------------
35. UNITATE DEZACTIVATĂ
Dacă o mașină: - este distrusă; - rămâne blocată; - pierde motorul; -
este implicată într-un accident grav;
atunci:
UNIT_DISABLED
↓
ROLE_RELEASED
↓
REASSIGNMENT
Altă unitate poate deveni: - Primary; - Interceptor; - Blocker.
------------------------------------------------------------------------
36. PURSUIT ESCALATION
Exemplu:
1 unit
↓
target still escaping
↓
backup requested
↓
2 units
↓
target continues
↓
interceptor
↓
roadblock
↓
multi-unit containment
Nu crește unitățile instant.
Response Director folosește: - Heat; - crime severity; - available
units; - distance; - response ETA; - pursuit duration; - target threat.
------------------------------------------------------------------------
37. LOST PURSUIT
CONTACT LOST
↓
LAST KNOWN
↓
SEARCH AREA
↓
INTERSECTION PREDICTION
↓
RADIO INFORMATION
↓
TARGET FOUND?
/ \
YES NO
| |
RESUME SEARCH
|
EXPIRE
Dacă eventul expiră: - unitățile revin la patrulare; - sau primesc alt
task.
------------------------------------------------------------------------
38. SEARCH
Search manager distribuie: - centru; - radius; - search points; - road
segments; - intersections.
Unitățile nu caută toate același punct.
------------------------------------------------------------------------
39. RECOVERY DUPĂ ACCIDENT
Dacă poliția lovește ceva:
COLLISION
↓
VEHICLE STATE
↓
CAN DRIVE?
/ \
YES NO
| |
RECOVER DISABLED
Dacă poate continua: - stabilizează; - recalculare rută; - reia pursuit.
Dacă nu: - unit disabled; - backup.
------------------------------------------------------------------------
40. PARKING / RETURN TO PATROL
După terminarea unui event:
EVENT_COMPLETE
↓
RETURN_TO_PATROL
↓
ROUTE_TO_PATROL_ZONE
↓
PARK / PATROL
Nu despawn instant dacă playerul vede unitatea.
------------------------------------------------------------------------
41. LOD / PERFORMANCE
LOD 0 --- Full
Player nearby / active pursuit: - full physics; - full obstacle
avoidance; - full tactical decisions.
LOD 1 --- Nearby
• full movement;
• reduced decision frequency.
LOD 2 --- Distant
• simplified movement;
• lower AI frequency.
LOD 3 --- Background
• event simulation;
• no expensive collision/tactical calculation unless needed.
Dacă eventul intră în zona playerului: LOD -> Full.
------------------------------------------------------------------------
42. TELEMETRY
Developer telemetry trebuie să poată afișa:
Unit ID
State
Target
Target Confidence
Current Tactic
Desired Speed
Actual Speed
Current Lane
Next Road
Emergency Mode
Siren
Lights
Chase Role
Stuck Timer
Recovery State
Decision Reason
Exemplu:
UNIT 04
PURSUING
TACTIC: INTERCEPT
ROLE: PRIMARY
TARGET CONFIDENCE: 0.91
SPEED: 112 km/h
TARGET: 104 km/h
LANE: RIGHT
SIREN: ON
STUCK: 0.0s
------------------------------------------------------------------------
43. DECISION TRACE
Exemplu:
Decision:
ATTEMPT_PIT
Reasons:
Heat = HIGH
RelativeSpeed = ACCEPTABLE
Angle = VALID
TrafficRisk = LOW
OfficerProfile = AGGRESSIVE
Backup = AVAILABLE
RoadWidth = SUFFICIENT
Result:
PIT_ABORT
Reason:
TrafficRisk increased
------------------------------------------------------------------------
44. DEBUG COMMANDS
Trebuie prevăzute comenzi/configurații pentru:
Show Police AI
Show Pursuits
Show Road Network
Show Lanes
Show Chase Slots
Show Intercept Points
Show PIT Evaluation
Show Obstacles
Show Recovery
Show Police World Events
Show AI Decision Trace
API-urile exacte trebuie verificate în Phase 0.
------------------------------------------------------------------------
45. TEST MAPS
Nu testa doar în harta principală.
Creează manual în editor test scenarios/maps:
PoliceAI_Test_StraightRoad
PoliceAI_Test_Intersection
PoliceAI_Test_Traffic
PoliceAI_Test_PIT
PoliceAI_Test_BoxIn
PoliceAI_Test_Roadblock
PoliceAI_Test_Recovery
PoliceAI_Test_LostPursuit
PoliceAI_Test_WorldEvent
Nu automatiza modificarea .umap.
------------------------------------------------------------------------
46. TESTE AUTOMATE
PVA-T01 --- Lane Following
Poliția urmează banda fără drift excesiv.
PVA-T02 --- Speed Control
Viteza țintă respectă limitele și condițiile.
PVA-T03 --- Intersection
Poliția traversează corect o intersecție.
PVA-T04 --- Obstacle Detection
Detectează obstacolul înainte de coliziune.
PVA-T05 --- Obstacle Avoidance
Evită obstacolul fără teleport.
PVA-T06 --- Lane Change
Poate schimba banda dacă există spațiu.
PVA-T07 --- Re-route
Găsește rută alternativă când banda este blocată.
PVA-T08 --- Stuck Detection
Detectează lipsa progresului.
PVA-T09 --- Recovery
Revine pe traseu după stuck.
PVA-T10 --- Siren Activation
Emergency state -\> siren ON.
PVA-T11 --- Siren Deactivation
Emergency end -\> siren OFF.
PVA-T12 --- Pursuit
Unitatea menține contactul fără teleport.
PVA-T13 --- Chase Spacing
Unitățile păstrează sloturi distincte.
PVA-T14 --- Intercept
Interceptorul ajunge la punctul estimat.
PVA-T15 --- Intercept Failure
Dacă estimarea e invalidă, revine la follow.
PVA-T16 --- PIT Valid
PIT se execută doar în condiții valide.
PVA-T17 --- PIT Abort
PIT este abandonat când riscul devine invalid.
PVA-T18 --- Box-In
Două unități ocupă sloturi diferite.
PVA-T19 --- Roadblock
Unitatea ajunge la punctul de blocare.
PVA-T20 --- Side Sweep
Tactica este aleasă doar când există clearance.
PVA-T21 --- Heat Aggression
Heat crescut -\> tactici mai agresive.
PVA-T22 --- Profile Difference
Police_Normal și Police_Aggressive au decizii diferite.
PVA-T23 --- Lost Contact
Unitatea caută Last Known, nu poziția actuală.
PVA-T24 --- Search Distribution
Unitățile distribuie zona.
PVA-T25 --- Disabled Unit
Unitatea dezactivată este scoasă din rol.
PVA-T26 --- Role Reassignment
Altă unitate preia rolul.
PVA-T27 --- Vehicle/Ped Handoff
Suspectul poate trece din vehicle AI în pedestrian AI.
PVA-T28 --- Police Officer Handoff
Officer AI preia controlul după oprire.
PVA-T29 --- World Event
O urmărire poate exista fără player.
PVA-T30 --- World Event Observation
Playerul poate intra în raza eventului și vede simularea reală.
PVA-T31 --- World Event Completion
Eventul poate termina prin arrest/escape/disabled target.
PVA-T32 --- Spawn Distance
Unitățile nu apar arbitrar lângă player.
PVA-T33 --- Pursuit Escalation
Backup-ul este solicitat progresiv.
PVA-T34 --- Recovery After Collision
Poliția recuperează după accident.
PVA-T35 --- Impossible Road
AI-ul abandonează ruta imposibilă.
PVA-T36 --- Traffic Density
AI-ul reacționează diferit la trafic redus/ridicat.
PVA-T37 --- Multi-Unit Collision Avoidance
Unitățile nu se distrug între ele prin convergență stupidă.
PVA-T38 --- FPS Independence
Comportamentul nu depinde de frame rate.
PVA-T39 --- AI LOD
Costul scade când unitatea este distantă.
PVA-T40 --- Performance
Benchmark la 5/10/25/50 unități.
------------------------------------------------------------------------
47. SCENARII DE ACCEPTANȚĂ
A --- Patrulare
Poliția conduce normal, respectă road network și poate schimba banda.
B --- Traffic Stop
Unitatea vede o încălcare, pornește sirena/luminile și încearcă oprirea.
C --- Three Warnings
Playerul ignoră trei avertismente -\> pursuit.
D --- Pursuit Low Heat
Unitatea urmărește prudent, evitând traficul.
E --- Pursuit High Heat
Unitatea folosește intercept/PIT/backup mai frecvent.
F --- Criminal World Event
Playerul conduce normal și întâlnește o urmărire NPC-vs-police.
G --- PIT
Poliția execută un PIT valid, iar targetul poate scăpa dacă manevra
eșuează.
H --- Roadblock
O a doua unitate este trimisă înaintea suspectului și încearcă blocarea.
I --- Box-In
Două sau mai multe unități coordonează oprirea.
J --- Lost Pursuit
Suspectul dispare; poliția caută ultima locație cunoscută.
K --- Collision
Poliția lovește un vehicul; evită stuck permanent.
L --- Disabled Unit
Mașina nu mai poate continua; alta preia rolul.
M --- Vehicle-to-Foot
Suspectul abandonează mașina și fuge pe jos.
N --- Shootout
Suspectul devine violent; controlul trece la Combat AI.
O --- Player Observation
Playerul poate vedea întregul eveniment fără să fie targetul lui.
------------------------------------------------------------------------
48. PERFORMANCE BUDGET
Nu inventa limite finale înainte de benchmark.
Phase 13 trebuie să măsoare:
Vehicle AI CPU
Road Query CPU
Obstacle Avoidance CPU
Tactical Evaluation CPU
Physics CPU
Perception CPU
Coordination CPU
World Event CPU
Memory
Active Vehicles
Teste: - 5; - 10; - 25; - 50; - 100 vehicule AI.
Rezultatele reale stabilesc bugetul final.
------------------------------------------------------------------------
49. THREADING
Claude trebuie să documenteze în Phase 0: - ce rulează Game Thread; - ce
poate fi async; - ce acces la UObject este permis; - ce date trebuie
copiate; - ce sisteme Chaos rulează pe physics thread; - cum se transmit
comenzile AI -\> movement.
Nu presupune thread safety.
------------------------------------------------------------------------
50. INTEGRARE CU VEHICLE PHYSICS
Police Vehicle AI nu creează propriul solver.
Flux:
AI Decision
↓
Driving Intent
↓
Steering / Throttle / Brake / Gear Intent
↓
Murdar Vehicle Movement
↓
Chaos Vehicles
↓
Actual Vehicle Motion
↓
AI Telemetry
↓
Next Decision
AI-ul trebuie să reacționeze la comportamentul real al vehiculului.
Dacă vehiculul nu virează suficient: - AI nu presupune că virajul s-a
executat; - verifică actual heading/path progress; - ajustează.
------------------------------------------------------------------------
51. INTEGRARE CU AI-UL GENERAL
Police Vehicle AI este subordonat sistemului Police AI.
Police AI
|
+-- Intent: PURSUE
|
+-- Intent: STOP
|
+-- Intent: INTERCEPT
|
+-- Intent: RETURN
|
v
Police Vehicle AI
Vehicle AI nu decide singur dacă un suspect trebuie arestat.
El decide: - cum ajunge; - cum urmărește; - cum poziționează
vehiculul; - cum execută tactica; - când este imposibilă execuția.
------------------------------------------------------------------------
52. INTEGRARE CU CIVILIAN TRAFFIC AI
Poliția și traficul trebuie să comunice prin stimuli legitimi:
SIREN
LIGHTS
EMERGENCY VEHICLE
COLLISION
ROADBLOCK
Civilian AI poate: - încetini; - opri; - trage lateral; - schimba
banda; - evita accidentul.
Nu toate NPC-urile trebuie să facă exact același lucru.
------------------------------------------------------------------------
53. FAIL-SAFE PRINCIPLES
Dacă: - road path missing -\> reroute; - lane missing -\> road-level
fallback; - target missing -\> search; - target invalid -\> terminate
pursuit; - vehicle disabled -\> backup; - tactic invalid -\> fallback; -
obstacle impossible -\> recovery; - unit collision risk -\> disengage; -
world event invalid -\> cancel safely.
Niciun sistem nu trebuie să rămână într-un loop infinit.
------------------------------------------------------------------------
54. CALIBRATION
Nu modifica simultan: - steering; - braking; - aggression; - PIT
distance; - chase distance; - speed.
Regulă:
ONE PARAMETER
→ TEST DEPENDENCIES
→ FULL REGRESSION
→ LOG
Fișier:
Docs/POLICE_VEHICLE_AI_TUNING_LOG.md
Format:
Date:
Parameter:
Owner:
Old:
New:
Reason:
Tests:
Result:
Side Effects:
------------------------------------------------------------------------
55. DOCUMENTAȚIE CERUTĂ DE CLAUDE
La final trebuie să existe:
Docs/POLICE_VEHICLE_AI_RECON.md
Docs/POLICE_VEHICLE_AI_ARCHITECTURE.md
Docs/POLICE_VEHICLE_AI_EDITOR_TASKS.md
Docs/POLICE_VEHICLE_AI_TUNING_LOG.md
Docs/POLICE_VEHICLE_AI_TEST_REPORT.md
------------------------------------------------------------------------
56. FAZE DE IMPLEMENTARE
PHASE 0 --- UE5/API/PROJECT RECON
Verifică: - exact UE version; - Chaos Vehicles API; - AI framework; -
navigation; - road/traffic systems existente; - perception; - Gameplay
Message/Events dacă există; - threading; - physics integration.
GATA: POLICE_VEHICLE_AI_RECON.md complet și fără API presupus.
------------------------------------------------------------------------
PHASE 1 --- Vehicle AI Core
Implement: - AI component; - state model; - driving intent; - movement
interface; - telemetry.
GATA: mașina poate primi un destination și conduce printr-un traseu
valid.
------------------------------------------------------------------------
PHASE 2 --- Road/Lane Navigation
Implement: - road graph; - lanes; - intersections; - route queries; -
lane selection.
GATA: unitatea ajunge la destinații multiple.
------------------------------------------------------------------------
PHASE 3 --- Driving Control
Implement: - steering; - throttle; - brake; - speed target; - curvature
control.
GATA: condus stabil fără oscilații majore.
------------------------------------------------------------------------
PHASE 4 --- Obstacle Avoidance
Implement: - obstacle detection; - local avoidance; - lane change; -
braking; - reroute.
GATA: obstacolele nu produc stuck permanent.
------------------------------------------------------------------------
PHASE 5 --- Recovery
Implement: - stuck detection; - reverse; - re-route; - role
abandonment; - disabled handling.
GATA: PVA-T08/PVA-T09/PVA-T34/PVA-T35 PASS.
------------------------------------------------------------------------
PHASE 6 --- Emergency Driving
Implement: - siren; - lights; - emergency state; - emergency speed; -
traffic interaction.
GATA: PVA-T10/PVA-T11 PASS.
------------------------------------------------------------------------
PHASE 7 --- Basic Pursuit
Implement: - target; - contact; - follow; - chase distance; - target
prediction; - lost contact.
GATA: PVA-T12/PVA-T23 PASS.
------------------------------------------------------------------------
PHASE 8 --- Multi-Unit Coordination
Implement: - chase slots; - primary; - secondary; - interceptor; -
blocker; - reassignment.
GATA: PVA-T13/PVA-T25/PVA-T26 PASS.
------------------------------------------------------------------------
PHASE 9 --- Tactics
Implement: - follow; - intercept; - PIT; - box-in; - roadblock; - side
sweep; - multi-unit pin.
GATA: PVA-T14--PVA-T20 PASS.
------------------------------------------------------------------------
PHASE 10 --- Police Integration
Integrează: - Heat; - Crime; - Police Response Director; - Police AI; -
arrest; - combat handoff.
GATA: acceptance B--E, G--N PASS.
------------------------------------------------------------------------
PHASE 11 --- World Police Events
Implement: - event director; - suspect generation/assignment; - unit
dispatch; - event lifecycle; - offscreen simulation; - player
observation; - completion.
GATA: PVA-T29--PVA-T32 PASS.
------------------------------------------------------------------------
PHASE 12 --- Vehicle/Pedestrian Handoff
Implement: - officer exit; - suspect exit; - pedestrian AI; - combat
handoff; - arrest.
GATA: PVA-T27/PVA-T28 PASS.
------------------------------------------------------------------------
PHASE 13 --- LOD + Performance
Implement: - AI LOD; - event LOD; - reduced update rates; - pooling dacă
este justificat.
Benchmark: 5/10/25/50/100.
------------------------------------------------------------------------
PHASE 14 --- FULL REGRESSION
Rulează: - toate PVA tests; - toate testele Police AI; - toate testele
Enemy/Civilian AI relevante; - vehicle physics regression.
Nu considera sistemul GATA dacă o îmbunătățire a condusului strică
physics sau AI-ul pietonal.
------------------------------------------------------------------------
57. MANUAL EDITOR TASKS
Claude trebuie să documenteze, nu să automatizeze:
• Road Network;
• Lane setup;
• intersections;
• traffic lights;
• NavMesh;
• police vehicle Blueprint;
• siren audio;
• emergency lights;
• police officer sockets;
• driver seat;
• passenger seats;
• vehicle collision channels;
• AI Controller;
• Data Assets;
• spawn zones;
• police stations;
• world event zones;
• test maps.
------------------------------------------------------------------------
58. LISTA FINALĂ --- NU ESTE ACCEPTAT
  teleport vehicul\
  transform manipulation pentru condus\
  spline-only driving dacă există vehicle physics\
  poliție care știe poziția actuală după pierderea contactului\
  spawn lângă player doar pentru Heat\
  toate unitățile urmăresc aceeași traiectorie\
  PIT la orice viteză\
  PIT fără evaluarea riscului\
  sirenă controlată manual de player\
  AI blocat permanent\
  infinite recovery loops\
  magic numbers\
  API inventat\
  UObject access nesigur pe physics/async threads\
  .uasset/.umap automat editate\
  poliție cu aim/vehicle behavior identic indiferent de profil\
  world event care este doar cutscene\
  despawn vizibil după event\
  unități care apar instant fără ETA\
  vehicle AI care decide singur logica de arest\
  fiecare problemă rezolvată prin creșterea vitezei\
  fiecare problemă rezolvată prin creșterea agresivității.
------------------------------------------------------------------------
59. PRINCIPIUL FINAL
Sistemul trebuie să producă emergență:
CRIME
+
PERCEPTION
+
KNOWLEDGE
+
HEAT
+
POLICE PROFILE
+
ROAD NETWORK
+
TRAFFIC
+
VEHICLE STATE
+
AVAILABLE UNITS
+
TACTICAL OPPORTUNITY
=
POLICE BEHAVIOR
Nu trebuie să existe o singură secvență:
"POLICE CHASE PLAYER"
Trebuie să existe un sistem în care:
> poliția percepe, decide, conduce, reacționează, comunică, pierde
> contactul, caută, schimbă tactica, cere backup, execută opriri și
> poate termina sau pierde un incident.
Iar același sistem trebuie să poată produce o urmărire NPC-vs-police în
timp ce playerul își vede liniștit de drum.
------------------------------------------------------------------------
60. PRIMUL PROMPT PENTRU CLAUDE CODE
Citește integral:
Docs/MURDAR_POLICE_VEHICLE_AI_SPEC.md
Acest document este Source of Truth pentru Police Vehicle AI.
NU implementa Phase 1 încă.
Începe exclusiv cu PHASE 0 — UE5/API/PROJECT RECON.
1. Identifică exact versiunea UE5 a proiectului.
2. Inspectează structura proiectului.
3. Identifică sistemul actual de vehicle physics.
4. Inspectează Chaos Vehicles API disponibil în engine source.
5. Identifică exact clasele/API-urile disponibile pentru:
- vehicle movement
- AI Controller
- navigation
- road/lane systems
- perception
- gameplay messaging/events
- async/threading
- physics integration
6. Caută dacă proiectul are deja traffic/road/vehicle AI.
7. Nu crea un al doilea sistem dacă există unul reutilizabil.
8. Verifică toate clasele și funcțiile în engine source înainte să le propui.
9. Documentează headers, module, clase, funcții, semnături și limitele API.
10. Notează orice diferență față de spec.
11. Creează:
Docs/POLICE_VEHICLE_AI_RECON.md
12. Nu modifica .uasset/.umap.
13. Nu implementa Phase 1.
14. La final raportează:
- ENGINE VERSION
- EXISTING VEHICLE SYSTEM
- EXISTING AI SYSTEM
- EXISTING ROAD/TRAFFIC SYSTEM
- VERIFIED APIs
- UNKNOWN APIs
- CONFLICTS
- PROPOSED ARCHITECTURE
- FILES CREATED
- FILES MODIFIED
- TESTS RUN
- GATA / NOT GATA
Dacă editorul este deschis și împiedică compilarea, oprește-te și cere-mi să îl închid.
Nu inventa API-uri.
Nu presupune că un API UE5 din documentație generică există în versiunea instalată.
Nu trece la Phase 1 până când Phase 0 nu este complet și verificabil.