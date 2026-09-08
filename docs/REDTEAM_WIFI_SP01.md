# Red-Team Review — SP01 Wi-Fi Prototype v0.1

## Status

**AUTHORITATIVE NETWORK REVIEW FOR v0.1.**

This supplements `REDTEAM_PROTOTYPE_HMI.md` and supersedes its earlier wired-Ethernet assumptions for the live rotating SP01 topology.

Current v0.1 network:

```text
stationary:
  laptop/browser
      |
  dedicated AP/router
      ))) 2.4 GHz Wi-Fi, supervisory only

rotating:
  ESP32-S3 SP01
      |
  local RS485 -> TLB
      |
  local DI/DO -> machine
```

No Ethernet is carried through ordinary carbon brushes/slip rings.

The AP/router must be treated as disposable from the controller's point of view: unplugging it must not change the local filling state machine or safe output behavior.

## Immediate REJECT conditions

Reject live SP01 actuation if any of these is true:

1. any cutoff, interlock, timeout, output transition, or safe abort waits for Wi-Fi;
2. the AP/router is required for booting into a safe operational controller state;
3. configuration needed to finish the current cycle exists only on the router/HMI;
4. loss of Wi-Fi can leave an output forced or latched indefinitely;
5. browser disconnect can alter physical output state except by an explicitly validated expiring service-force policy;
6. the ESP web/network task can starve the control or TLB task;
7. there is no local stale-data/event buffer during Wi-Fi loss;
8. RF quality has not been tested through a complete 360° rotation in the intended enclosure/location;
9. the design assumes a PCB antenna will work inside/behind steel without measurement;
10. the AP/router shares office/guest traffic during the pilot without an explicit reason;
11. router reboot causes controller reboot, watchdog, brownout, or output disturbance;
12. OTA/update can occur during active filling;
13. a malicious/accidental browser request can directly write GPIO or raw Modbus registers;
14. a network outage is incorrectly treated as a process/safety signal;
15. there is no tested rollback to the legacy SP01 controller.

## Architecture attacks

1. Why is Wi-Fi present at all? Prove it is only for observability/configuration and not hidden control coupling.
2. Can SP01 complete an entire shift with the AP/router turned off after configuration?
3. Does any FSM state call a network API, wait on a WebSocket, DNS, NTP, DHCP, or router response?
4. Does the controller retain all current recipe/state/config locally?
5. Can the controller cold-boot safely if the AP/router is unavailable?
6. Is SP01 a peer node, or is the implementation accidentally making it a future master?
7. Can future SP02..SP08 be added without changing SP01 controller semantics?
8. If the router is later uplinked to the plant LAN, does that create any new write path into control?
9. Is the AP/router replaceable without firmware changes beyond network credentials/IP configuration?
10. Is the router doing anything that belongs in the controller?

## RF attacks

11. Where exactly is the SP01 antenna mounted relative to steel cabinets, motor housings and the rotating frame?
12. What does RSSI look like over 0–360° rotor position?
13. Are there deep fades at repeatable angles?
14. What happens when a person, bag stream, maintenance platform or metal guard changes the RF path?
15. What happens with nearby motors/contactors/VFDs energized?
16. Is 2.4 GHz channel utilization measured, not guessed?
17. Is the AP channel fixed or automatically changing unexpectedly?
18. Does the AP support a stable dedicated SSID without band steering surprises?
19. Does antenna orientation rotate relative to the AP in a way that causes polarization loss?
20. Is an external antenna/pigtail required for acceptable link margin?
21. Can vibration loosen antenna connectors?
22. Can dust/cement ingress degrade the antenna or connector?
23. Is AP placement near/above the packer actually physically possible?
24. Does one AP cover the full circumference, or is a second stationary AP required?
25. If two APs are later used, can roaming/reassociation storms affect ESP CPU timing?

## Router/AP failure attacks

26. Unplug the AP during `BAG_VERIFY`: what changes locally?
27. Unplug it during `COARSE_FILL`.
28. Unplug it during `FINE_FILL`.
29. Unplug it at predicted cutoff.
30. Unplug it during `SETTLING`.
31. Unplug it during `WAIT_PUSH`.
32. Reboot it repeatedly for 10 minutes while the controller runs dummy cycles.
33. Change DHCP availability while SP01 is active.
34. Remove DNS/NTP/Internet completely.
35. Flood the AP with unrelated traffic in a controlled test.
36. Connect/disconnect multiple engineering laptops repeatedly.
37. Change AP channel and force reconnect.
38. Power-cycle AP and laptop together while SP01 continues autonomous operation.
39. Verify zero unintended output transitions in every case.

## ESP network-stack attacks

40. Is Wi-Fi reconnect handled only by a lower-priority task/event handler?
41. Can reconnect loops allocate memory without bound?
42. Is heap usage stable over days of connect/disconnect cycles?
43. Can HTTP/WebSocket clients exhaust sockets or buffers?
44. Are telemetry queues bounded?
45. Can JSON generation delay control execution?
46. Are network interrupts/task priorities measured under stress?
47. Does disabling Wi-Fi entirely leave control timing statistically unchanged within the defined budget?
48. Does Wi-Fi radio activity cause measurable power-rail noise or resets?
49. Are watchdogs configured so a stuck web task does not reset an otherwise healthy control task unnecessarily?
50. If a global watchdog reset is unavoidable, are physical outputs guaranteed safe throughout reset/boot?

## HMI/data attacks

51. Does the browser clearly display `OFFLINE/STALE` when telemetry stops?
52. Can stale weight or I/O data remain visually green/current?
53. Are process timestamps generated by the controller rather than browser arrival time?
54. Can a Wi-Fi outage create a fake gap in the local bag-cycle record?
55. How many cycles/events can be buffered locally?
56. What happens when the buffer fills?
57. Is synchronization idempotent after reconnect, or can cycles duplicate/disappear?
58. Can HMI history distinguish `network data missing` from `process event did not occur`?
59. Does machine control ever wait for a history write/upload?
60. Can the HMI command the wrong spout because of stale identity/IP/browser tabs?

## Configuration attacks

61. Are network credentials/config separate from filling recipe/config?
62. Can a bad Wi-Fi configuration brick local control or only HMI connectivity?
63. Is last-known-good controller config retained locally?
64. Are configuration writes atomic?
65. Are new fill parameters applied only at an allowed state boundary?
66. What happens if Wi-Fi drops halfway through a config request?
67. Does the client receive a config version/hash only after successful persistence?
68. Can two browser sessions race conflicting configuration writes?
69. Are all privileged writes audited locally even if central logging is offline?
70. Can a router DHCP/IP change cause a node identity mistake?

## Security attacks

71. Is the prototype SSID dedicated and protected with strong credentials?
72. Are default router credentials changed?
73. Are write endpoints authenticated independently of merely joining Wi-Fi?
74. Is there any unauthenticated `/gpio`, `/modbus`, `/force`, `/raw` endpoint? If yes, reject.
75. Can CSRF or a malicious local webpage trigger a command?
76. Are service-force commands bounded and auto-clearing?
77. Does browser disconnect clear service force?
78. Does Wi-Fi loss clear service force?
79. Is a physical service-enable required for I/O forcing?
80. Can OTA be initiated only when the machine/node is in a validated safe service state?
81. Is firmware integrity verified before boot?
82. Can an older vulnerable firmware be installed accidentally?
83. If AP/router gains a plant-LAN uplink later, what firewall rules prevent broad access to controller writes?

## Rotating-machine attacks

84. Is all control power/local I/O physically on the rotating assembly so no control-data slip ring is required?
85. Are any supposedly local signals actually stationary and therefore still need a rotary interface?
86. Does the load-cell cable remain entirely local to the rotating spout/TLB?
87. Does the node share rotating 24 V with noisy solenoids/motors?
88. What voltage dips occur during actuation and radio transmit bursts?
89. Does rotation/vibration affect terminal, antenna, SD/flash or RS485 connections?
90. Is the enclosure/antenna positioned where maintenance can safely access it?

## Mandatory v0.1 RF/network tests

Before live SP01 actuation:

- run 10,000+ dummy FSM cycles with Wi-Fi continuously active;
- repeat with Wi-Fi completely disabled and compare control jitter;
- power-cycle AP/router during every FSM state;
- run a full 360° RF survey at representative rotor speed;
- record RSSI, disconnects, reconnect duration and packet loss versus angle;
- run nearby normal electrical loads during RF survey;
- open multiple browser/WebSocket sessions;
- generate controlled reconnect storms;
- interrupt config writes;
- verify local cycle/event buffering through a prolonged network outage;
- verify no output pulse occurs because of AP/router restart;
- prove SP01 can cold-boot into a safe local state without AP/router;
- prove the current filling cycle never depends on HMI/network acknowledgements.

## Required review output

For every finding:

```text
SEVERITY: BLOCKER | HIGH | MEDIUM | LOW
AREA: RF | router | firmware | HMI | configuration | security | rotating-installation
ASSUMPTION UNDER ATTACK:
FAILURE MODE:
EVIDENCE:
REQUIRED CHANGE:
```

Final verdict must be one of:

```text
REJECT
REWORK
BENCH-PILOTABLE
SHADOW-PILOTABLE
LIVE-SP01-PILOTABLE
```

Expected current verdict remains:

```text
REWORK / BENCH-PILOTABLE
```

The purpose of SP01 v0.1 is to obtain evidence, not to prove beforehand that Wi-Fi is reliable. The stronger claim is narrower and testable: **Wi-Fi must be non-critical enough that its unreliability cannot affect local filling control.**
