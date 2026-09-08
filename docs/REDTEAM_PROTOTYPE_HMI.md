# Red-Team Review — ESP32/TLB Prototype + Embedded Web HMI

## Verdict status

**NOT APPROVED FOR LIVE ACTUATION.**

This document exists to attack the current prototype proposal, not defend it.

The proposed one-spout node is:

```text
ESP32-S3 + FreeRTOS
+ 8 isolated DI
+ 8 protected DO
+ isolated RS485
+ wired Ethernet
+ LAUMAS TLB weighing transmitter
+ embedded web HMI
```

One node controls one spout. Eight spouts would use eight independent nodes.

Red-team reviewers should assume the proposal is wrong until measurements and failure tests prove otherwise.

Review `docs/PROTOTYPE_HW.md` and `docs/HMI.md` together with this document.

## 1. Review objective

Try to kill the design before the machine does it for us.

Find:

- unsafe assumptions;
- hidden single points of failure;
- timing paths that are not actually bounded;
- unsuitable commodity hardware;
- EMI/grounding failures waiting to happen;
- incorrect I/O electrical assumptions;
- web/HMI paths that can interfere with control;
- bad update/recovery behavior;
- vendor lock disguised as an adapter;
- ways one broken spout can damage or stop the other seven;
- ways a technician can accidentally energize machinery;
- ways configuration can silently drift;
- claims that rely on brochure numbers rather than end-to-end measurements;
- complexity that should be deleted.

Do not accept "it should be fine" as evidence.

## 2. Immediate REJECT conditions

Reject live connection if any of these remains true:

1. output states during boot/reset/watchdog are not measured;
2. an ESP crash can leave a dosing valve energized indefinitely;
3. E-stop relies on ESP firmware, Ethernet, HMI, or Modbus;
4. field 24 V is connected directly to MCU GPIO;
5. coil current/inrush/thermal load is unknown;
6. filling motor power is switched directly by controller DO;
7. loss of TLB weight data can continue filling without a bounded abort;
8. a browser or HMI process is required to complete or stop a cycle;
9. web traffic can starve or delay the control task beyond its validated budget;
10. service I/O forcing is possible during automatic operation;
11. configuration writes can leave an unrecoverable or ambiguous state after power loss;
12. there is no tested physical rollback to the legacy SP01 controller;
13. the chosen ESP32 carrier has no credible evidence of surviving the actual cabinet environment;
14. RS485 grounding/termination/isolation is unresolved;
15. a watchdog reset can restart into active outputs;
16. firmware update can energize outputs or corrupt calibration/configuration;
17. one spout node can send commands to another spout's physical I/O by addressing mistake;
18. shared machine permissives have no defined fail behavior;
19. safety contactors/actuator power isolation are not independent of firmware;
20. the live pilot has no responsible commissioning authority and stop criteria.

## 3. Architecture attack

1. Why is ESP32-S3 needed at all? Could a simpler industrial controller or smart transmitter do the job with less lifecycle risk?
2. Are we replacing a 30-year proven controller with a more fragile software stack merely because it is interesting?
3. What exact capability justifies FreeRTOS rather than a simpler cyclic bare-metal loop?
4. Does dual-core add value, or only concurrency bugs?
5. Can the same node safely run on one core if the other is disabled? If not, why not?
6. Is the application genuinely hardware-neutral, or does the first implementation leak ESP-IDF assumptions everywhere?
7. Are TLB register addresses confined to one adapter?
8. Are GPIO/channel numbers confined to one adapter/configuration layer?
9. Can a simulated node and a real node pass the same conformance scenario?
10. Can another weighing transmitter replace TLB without modifying the spout FSM?
11. What happens when TLB is discontinued?
12. What happens when the selected ESP32 carrier is discontinued?
13. Is "one node per spout" actually independent, or are there hidden shared services required for normal operation?
14. Can seven spouts continue if SP04 repeatedly reboots?
15. Can one faulty node electrically disturb the shared network, power bus, RS485, or permissive wiring?

## 4. Legacy behavior attack

16. Has every legacy INPUT_0..INPUT_6 been physically traced to the actual machine terminal/device?
17. What exactly is legacy INPUT_4? Do not proceed until identified.
18. Has the missing legacy push-cylinder output been physically identified and numbered?
19. Is aeration really supposed to turn off exactly at fill cutoff, or does the legacy pneumatic sequence differ?
20. Are any outputs maintained mechanically/pneumatically after de-energization?
21. Are there hidden interlocks in relays/contactors that the pseudocode does not show?
22. Are there cam contacts or timing relays not represented in the source code?
23. Does the old controller exploit mechanical timing that the new state machine would accidentally defeat?
24. Is the scanner safe state actually UP? Verify physically.
25. What does the machine do if bag-detect pressure is marginal rather than clean ON/OFF?
26. Is there an existing permissive chain that must remain hardwired rather than reimplemented?

## 5. Throughput and timing attack

At 2,000 bags/h and eight spouts:

```text
one spout cycle ≈ 14.4 s
one machine bag interval ≈ 1.8 s
```

27. How much of 14.4 s is truly available for filling after bag acquisition, position, settling, and discharge?
28. What is the worst observed real cycle at >2,000 bags/h?
29. What is the maximum acceptable controller-cycle jitter?
30. What is measured worst-case FreeRTOS scheduling latency under maximum HMI/network load?
31. What is measured RS485 round-trip latency to TLB, not brochure conversion rate?
32. What is the maximum age of the weight sample actually used by cutoff logic?
33. How much delay is introduced by TLB digital filtering?
34. How much delay exists from ESP DO command to valve motion?
35. How much residual cement enters after the close command?
36. Is cutoff based on current weight, predicted final weight, TLB function, or a hybrid? Freeze ownership.
37. What happens if a Modbus reply arrives late exactly at cutoff?
38. What happens if one weight sample jumps/noises by 300 g?
39. How is stale weight distinguished from a real constant weight?
40. Is fine-fill rate measured over a window that introduces unacceptable phase lag?
41. Can the system meet target accuracy at both minimum and maximum material flow?

## 6. Weighing attack

42. Is TLB fast enough end-to-end after configured filtering and Modbus transport?
43. Is TLB resolution under actual machine vibration adequate, not bench-static adequate?
44. What is load-cell cable routing relative to motors, VFDs, solenoids, and rotary power?
45. Is shield termination defined at one or both ends according to the actual installation?
46. Is there a rotary/slip-ring path affecting the load-cell signal?
47. What is zero drift over temperature and one shift?
48. What is the calibration procedure after load-cell or mechanical work?
49. Who owns legal-metrology implications if applicable to packed product?
50. Can calibration be changed from the web HMI? If yes, is that acceptable?
51. Are TLB's local relay outputs unused, backup-only, or part of the design? Avoid ambiguous ownership.
52. Is there a hard maximum-weight cutoff independent of higher-level software?
53. What happens if ESP and TLB disagree about tare/zero state?

## 7. I/O hardware attack

54. What is the exact ESP32 carrier part number and revision?
55. Is it genuinely intended for industrial cabinet use or merely a development product with optocouplers?
56. What are DI input thresholds at 18 V, 24 V, and 30 V?
57. What is the isolation withstand rating?
58. Are all DI channels truly isolated from MCU ground, or grouped by common?
59. Are DO channels low-side sinking, high-side sourcing, relay, or something else?
60. Are machine solenoids wired compatible with that topology?
61. What is DO current rating at 50–60 °C cabinet temperature?
62. Is 500 mA/channel a continuous rating, peak rating, or marketing maximum?
63. What is the total board current/thermal limit with eight outputs on?
64. What happens on inductive load disconnect despite flyback protection?
65. Are there external fuses per output branch?
66. Can a shorted field coil destroy only one channel, the controller, or the whole 24 V bus?
67. Are outputs off while bootloader runs?
68. Are outputs off during brownout oscillation?
69. What happens if 24 V actuator supply collapses while control 24 V remains healthy?
70. What happens if control 24 V collapses while actuator 24 V remains present?
71. Is the motor contactor coil within output rating and suppression requirements?
72. Are interposing drivers needed even if current is nominally below rating for maintainability/isolation?

## 8. Power/EMC/environment attack

73. Cement plants are electrically noisy. Where are surge/ESD/EFT protections proven?
74. What is the PSU ride-through behavior during contactor switching?
75. Has brownout been injected repeatedly?
76. Is the 24 V supply shared with noisy solenoids?
77. Should controller and actuator branches use separate DC/DC isolation or only separate fuses?
78. Is grounding/star-point strategy documented?
79. Is the controller enclosure conductive and bonded appropriately?
80. What temperature is expected inside the packer cabinet?
81. What dust ingress is expected?
82. What vibration is expected on the rotating structure?
83. Are connectors screw terminals, pluggable terminals, RJ45, or headers that can loosen?
84. Is Ethernet magnetically isolated and mechanically secure?
85. Is Wi-Fi disabled in production unless explicitly needed?
86. Has radiated/conducted interference from nearby VFDs been tested?

## 9. RS485 attack

87. Which device is Modbus master?
88. Is TLB the only slave on the local spout bus?
89. What baud/parity/timeout/retry settings are validated?
90. What is the maximum safe retry count before declaring weight stale?
91. Can retries block the control task?
92. What happens with a permanently shorted A/B line?
93. What happens with reversed A/B?
94. What happens with duplicate slave addresses?
95. Is biasing present and located correctly?
96. Is termination present only at physical ends?
97. Is cable shield/reference strategy compatible with isolation?
98. Can an RS485 transient reset ESP or TLB?
99. Are communication errors counted and surfaced per cycle/shift?

## 10. FreeRTOS/firmware attack

100. Is the control task allocation-free after startup?
101. Can logging allocate/block inside high-priority execution?
102. Can JSON serialization run at a priority that affects control?
103. Can Ethernet interrupts create unacceptable jitter?
104. Are all waits bounded?
105. Is every state timeout explicit?
106. Can two tasks write the same output image?
107. Is physical output commit single-owner?
108. Is input image coherent for one FSM tick?
109. Can ISR code modify controller state directly?
110. Are monotonic timers used rather than wall clock?
111. What happens on 32-bit/64-bit timer wrap?
112. Is watchdog recovery tested while each output is active?
113. Is reboot reason persisted?
114. Is there a crash counter and boot-loop safe mode?
115. Can repeated crash/reboot energize a valve pulse each boot?
116. Is the firmware deterministic if NTP/time sync changes wall clock?
117. Is heap fragmentation measured during a multi-day HMI soak?
118. Does SD/flash logging ever block control?
119. Are flash writes prevented during timing-critical windows if necessary?
120. Is firmware built reproducibly and version-stamped?

## 11. HMI attack

121. Why is the web server on the controller at all? Prove its value exceeds its attack/complexity cost.
122. Can HMI be compiled out and controller behavior remain identical?
123. Can 20 browser clients degrade control timing?
124. What happens under a WebSocket reconnect storm?
125. What happens with malformed HTTP requests?
126. What happens with oversized requests?
127. Can a slow TCP client hold scarce resources indefinitely?
128. Are telemetry buffers bounded?
129. Does stale browser data visibly indicate staleness?
130. Can the browser show an output ON when the physical output is actually unknown?
131. Does Outer skin ever infer a physical state from a command without feedback?
132. Does Inner skin clearly distinguish command, measured feedback, and derived state?
133. Is timing data based on controller monotonic timestamps rather than browser arrival time?
134. Can replay be mistaken for live mode?
135. Is SIM/HARD/SHADOW mode unmistakable?
136. Can a user accidentally control SP02 while viewing SP01?
137. Is node identity visible on every privileged screen?
138. Can cached frontend assets mismatch firmware/API after OTA?

## 12. HMI security attack

139. Are write endpoints authenticated?
140. Are credentials unique per machine/site?
141. Are default credentials removed before production?
142. Is there any endpoint that exposes raw GPIO or arbitrary Modbus register access?
143. If yes, delete it or justify it.
144. Can CSRF or a malicious page on a laptop trigger commands?
145. Is TLS required on the machine LAN, or is network isolation considered sufficient? Decide explicitly.
146. Is session timeout enforced for engineer/service roles?
147. Are privileged actions audited locally and/or centrally?
148. Can a service user force outputs without physical service enable?
149. Does force mode automatically expire?
150. Does browser disconnect clear force mode?
151. Does controller reset clear force mode?
152. Are firmware images signed?
153. Is secure boot enabled/required?
154. Is flash encryption needed?
155. Can OTA be rolled back automatically after failed boot?
156. Can an attacker downgrade to an older vulnerable firmware?

## 13. Configuration attack

157. Is configuration schema-versioned?
158. Is configuration validated atomically before activation?
159. Are ranges defined for every timing/weight parameter?
160. Can target weight accidentally be configured as 500 kg?
161. Can push duration accidentally be configured as 60 s?
162. Can fill-position and push-position windows overlap illegally?
163. Can config writes be interrupted by power failure without corruption?
164. Is previous known-good configuration retained?
165. Is a factory-safe configuration available?
166. Are calibration parameters separated from ordinary recipe parameters?
167. Can SP03 load SP04's identity/configuration accidentally?
168. Is node/spout identity tied to hardware or only a writable config value?
169. Are config changes recorded with old/new values and actor?
170. Can local config drift from machine-level historian silently?

## 14. Multi-spout attack

171. What exactly is shared among eight spouts?
172. Which machine-wide signals are hardwired?
173. Which signals are network-distributed?
174. What happens if shared hopper permissive chatters?
175. What happens if downstream conveyor stops?
176. Are all eight spouts expected to abort immediately, finish current bags, or transition differently?
177. Can one node's output ever energize another node's actuator through wiring/common mistakes?
178. Are IP addresses enough to identify a spout, or is there a stronger identity scheme?
179. Can duplicate IPs cause unsafe behavior, or only HMI loss?
180. Is there a shared encoder? If so, how does its failure affect all eight nodes?
181. If position is local cam-based, how is timing consistency monitored across spouts?
182. Can the machine safely run 7/8 spouts after one node isolation?
183. Does mechanical balance/process behavior impose restrictions on running with missing spouts?
184. Who coordinates packer start/stop at machine level?
185. Is there an unnoticed need for a central arbiter after all?

## 15. Digital-twin/shadow attack

186. Does the simulator model actual pneumatic/material delay or only pretty animation?
187. Are load-cell noise and vibration modeled from real measurements?
188. Is simulated bag presence physically plausible?
189. Does shadow mode receive the exact same input image as the legacy/live controller?
190. Are shadow outputs timestamped but physically blocked by design?
191. Can shadow accidentally become live due to configuration error?
192. Can real and simulated signals be visually confused?
193. Are discrepancies between SIM and HARD quantified automatically?
194. Are replay tests deterministic?
195. Can a historical fault be reproduced from captured data?
196. Are conformance tests derived from the legacy machine's proven cycles, not invented timings?

## 16. Observability attack

197. Can we answer after any bad bag: "what inputs caused this output, at what time, using what weight sample and config version"?
198. Are digital transitions recorded as events rather than only periodic samples?
199. Are monotonic timestamps retained?
200. Is clock synchronization needed across eight nodes for machine-wide analysis?
201. If yes, what timestamp accuracy is actually required?
202. Is event storage bounded?
203. What happens when local storage fills?
204. Can logging failure affect control?
205. Is final weight measured after settling with a defined criterion?
206. Can we distinguish command state from physical device feedback when no feedback sensor exists?

## 17. Maintenance/lifecycle attack

207. Can a maintenance technician replace a node without a development laptop?
208. How is a replacement node assigned to SP03 safely?
209. Can config be restored from a controlled source?
210. What spare parts must be stocked?
211. What is the expected lifecycle of the ESP carrier versus the 30-year legacy controller?
212. What happens after Espressif SDK changes?
213. Are we willing to freeze a known-good toolchain for years?
214. Can firmware be built five years later?
215. Is there a golden firmware binary and source/tag?
216. Is there a bench jig for testing a repaired/replacement node?
217. Is there a hardware-in-loop test harness?
218. Is every field terminal labeled with semantic signal and channel?
219. Can a technician diagnose the node without internet access?
220. Is the HMI fully local/offline?

## 18. Cost attack

221. Is the proposed 9–15 M VND/spout prototype actually cheaper after engineering, panel work, testing, and spares?
222. Are we comparing against the correct commercial alternative, not a premium WTX-only strawman?
223. What does eight-spout total cost become after proper industrial I/O, enclosure, safety relays, Ethernet switch, commissioning, and spare stock?
224. How many engineering hours are required before first safe bag?
225. What failure rate would erase the hardware savings?
226. Does custom firmware create a single-person maintenance dependency?
227. Is a custom PCB actually economical at eight units, or should we keep modular DIN components?
228. Are there cheaper transmitters than TLB that still meet measured performance?
229. Are there better industrial ESP/MCU controllers with certifications/support that reduce risk enough to justify cost?

## 19. Mandatory bench tests

Before shadow mode:

- 10,000+ full FSM cycles with dummy loads;
- randomized input/fault injection;
- RS485 disconnect/reconnect in every state;
- TLB stale/frozen/noisy data injection;
- browser/network stress while measuring control jitter;
- repeated cold boot/brownout/watchdog resets with outputs instrumented;
- 24 V actuator-power loss while control power remains;
- control-power loss while actuator power remains;
- maximum simultaneous DO load thermal test;
- short/open field output test where safe;
- configuration power-loss test;
- firmware rollback/recovery test;
- service-force timeout/disconnect test;
- multi-client HMI soak;
- multi-day continuous run with heap/task/communication metrics.

Before live actuation:

- physical trace and signed I/O list;
- measured solenoid/contactor coil currents;
- verified existing safety chain;
- documented safe state for every actuator;
- shadow comparison against legacy cycles;
- measured weight latency and cutoff residual;
- electrical rollback plan;
- commissioning stop criteria.

## 20. Live-pilot stop criteria

Immediately revert SP01 to legacy control if any of the following occurs without an understood benign cause:

- unintended output pulse;
- controller reboot during normal cycle;
- weight-data staleness during fill;
- output remains energized after abort;
- missed E-stop/safety-chain behavior;
- repeated bag over/underweight beyond agreed limit;
- unexplained state transition;
- loss of rollback capability;
- network/HMI activity measurably disturbs control timing;
- cross-spout electrical or logical interference;
- cabinet temperature/output stage exceeds validated limit.

## 21. Required red-team verdict

Reviewers must return one of:

```text
REJECT
REWORK
BENCH-PILOTABLE
SHADOW-PILOTABLE
LIVE-SP01-PILOTABLE
PRODUCTION-CANDIDATE
```

A verdict must include:

- blocking findings;
- evidence required to close each blocker;
- unnecessary complexity to delete;
- assumptions still unproven;
- hardware purchase items that must wait;
- tests required before advancing one gate.

## 22. Current expected verdict

At this stage the expected verdict is:

```text
REWORK / BENCH-PILOTABLE ONLY
```

Reason: architecture is coherent enough to build a bench prototype, but the actual legacy wiring, actuator electrical loads, safe states, TLB end-to-end timing, carrier-board industrial robustness, and failure behavior have not yet been measured on the real machine.

That uncertainty is the work of the prototype. It must not be hidden by the simulator or by attractive HMI graphics.