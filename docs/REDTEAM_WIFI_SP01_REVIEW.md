# Red-Team Review Response — SP01 Wi-Fi Prototype v0.1

Reviewer response to `docs/REDTEAM_WIFI_SP01.md`.

Scope of evidence: branch `bootstrap/python-reference-runtime` as of commit
`docs: seed universal filling-controller contract`. All code findings below are read
directly from that source. No hardware, firmware, or RF measurement was available to
this review.

Final verdict: **REJECT** — see rationale at the end.

---

## 0. Meta-findings on the review document itself

```text
SEVERITY: BLOCKER
AREA: firmware
ASSUMPTION UNDER ATTACK: that this document reviews a reviewable artifact
FAILURE MODE: The document poses ~90 questions about ESP32-S3 FreeRTOS firmware,
  task priorities, heap stability, OTA, watchdogs, and HTTP endpoints. No ESP32
  firmware exists in the repository. The branch contains only a Python/FastAPI
  reference runtime.
EVIDENCE: Repository tree contains src/filling_controller/*.py and docs/ only.
  No firmware/, platformio.ini, CMakeLists.txt, or ESP-IDF component.
  adapters.py: HardAdapterStub raises RuntimeError("hard adapter is not implemented
  in bootstrap").
REQUIRED CHANGE: Questions 40-50 and 71-83 are unanswerable against present evidence.
  Either restate the document as scoped to the Python reference runtime, or hold it
  until firmware exists. As written it can be answered on paper without measurement.
```

```text
SEVERITY: HIGH
AREA: configuration
ASSUMPTION UNDER ATTACK: reviewer independence
FAILURE MODE: The document pre-declares its own outcome ("Expected current verdict
  remains: REWORK / BENCH-PILOTABLE"). A stated expected verdict biases the reviewer
  toward confirmation rather than attack.
EVIDENCE: docs/REDTEAM_WIFI_SP01.md, final section.
REQUIRED CHANGE: Remove the expected verdict from the review criteria.
```

---

## 1. Findings against the reference runtime

### F-01 Control loop shares an event loop with the network stack

```text
SEVERITY: BLOCKER
AREA: firmware
ASSUMPTION UNDER ATTACK: reject condition 6 — "the ESP web/network task can starve
  the control or TLB task"
FAILURE MODE: Control execution is coupled to network handler latency. Any blocking
  or long-running request handler delays the control tick by an unbounded amount.
EVIDENCE: api.py — runtime_loop() is scheduled with asyncio.create_task() on the same
  uvicorn event loop that serves HTTP routes and the /ws/live WebSocket.
  await asyncio.sleep(runtime.simulation_dt) sets a lower bound on the period, not a
  period.
REQUIRED CHANGE: Separate the control execution context from the network context, and
  declare a control period budget with measured worst-case jitter.
```

### F-02 Control time is decoupled from wall time; timeouts are unfalsifiable

```text
SEVERITY: BLOCKER
AREA: firmware
ASSUMPTION UNDER ATTACK: reject condition 1 — that timeouts are real
FAILURE MODE: The controller advances its clock by the nominal tick regardless of
  actual elapsed time. If the loop stalls, the controller does not observe the stall.
  Every FSM timeout is expressed in this synthetic clock, so a stalled runtime cannot
  time out. On real I/O this is safety-relevant: a stalled loop can hold an actuator
  command while believing little time has passed.
EVIDENCE: runtime.py — step() performs self.now += dt using the nominal dt from the
  profile, never measured elapsed time.
  controller.py — timeouts of 2.0 s (BAG_VERIFY), 12.0 s (COARSE_FILL), 5.0 s
  (FINE_FILL), 6.0 s (WAIT_PUSH) are all evaluated against self.now.
REQUIRED CHANGE: Drive the controller from a monotonic wall clock. Add tick-overrun
  detection with an explicit transition to safe state on overrun.
```

### F-03 Unauthenticated network write endpoints reach control directly

```text
SEVERITY: BLOCKER
AREA: security
ASSUMPTION UNDER ATTACK: reject condition 13 and question 74 — "is there any
  unauthenticated endpoint that can directly write control state? If yes, reject."
FAILURE MODE: Any client on the network can start, stop, reset, or single-step the
  control loop. /api/run/step advances the control loop on request, which is a
  network-to-control path rather than supervision. /api/run/stop during FINE_FILL
  clears all commands and forces IDLE. /api/run/reset is callable mid-cycle.
EVIDENCE: api.py — POST /api/run/start, /api/run/stop, /api/run/reset, /api/run/step.
  No authentication, no CSRF token, no allowed-state guard. main() binds
  host="0.0.0.0", port=8000.
REQUIRED CHANGE: Authenticate all writes independently of network association.
  Remove or gate /api/run/step. Apply state-boundary guards per question 65.
```

### F-04 Control execution waits on a synchronous history write

```text
SEVERITY: HIGH
AREA: firmware
ASSUMPTION UNDER ATTACK: question 59 — "does machine control ever wait for a history
  write?" Present answer: yes.
FAILURE MODE: A slow flash or SD commit blocks the control tick for the duration of
  the write.
EVIDENCE: runtime.py — recorder.event() and recorder.cycle() are called inline within
  step(). storage.py — every insert is followed by conn.commit().
REQUIRED CHANGE: Bounded queue between control and recorder; writer runs outside the
  control path; define behavior when the queue fills.
```

### F-05 Cycle records are overwritten silently after reset

```text
SEVERITY: HIGH
AREA: HMI
ASSUMPTION UNDER ATTACK: question 57 — "is synchronization idempotent after
  reconnect, or can cycles duplicate or disappear?"
FAILURE MODE: History is destroyed by a reset. Records cannot be attributed to a run
  or to a node.
EVIDENCE: storage.py — cycles table is keyed on cycle_id and written with
  "insert or replace". runtime.py — reset() sets self.cycle_id = 1.
  CycleSummary carries no wall-clock time, run id, or node id.
REQUIRED CHANGE: Composite key including run id and node id; append-only records;
  wall-clock timestamps alongside controller time.
```

### F-06 The HMI does not indicate stale or offline data

```text
SEVERITY: HIGH
AREA: HMI
ASSUMPTION UNDER ATTACK: questions 51 and 52 — stale data must not remain visually
  current.
FAILURE MODE: On WebSocket loss the last received values remain on screen with
  unchanged styling. An operator cannot distinguish live data from frozen data.
EVIDENCE: static/index.html — render() is invoked only from ws.onmessage;
  ws.onclose schedules a reconnect after 1000 ms with no visual state change.
  The displayed clock is s.timestamp, which is controller time, not arrival time.
  model.py defines Quality.STALE, which is not surfaced in the UI.
REQUIRED CHANGE: Explicit OFFLINE/STALE state in the UI, driven by a receive-side
  watchdog; per-channel quality display; visible age of last update.
```

### F-07 No local buffering during network loss

```text
SEVERITY: HIGH
AREA: HMI
ASSUMPTION UNDER ATTACK: reject condition 7 — "there is no local stale-data/event
  buffer during Wi-Fi loss"
FAILURE MODE: Telemetry is push-from-live-snapshot. During an outage nothing is
  buffered for later reconciliation; only completed cycle summaries reach SQLite.
  Questions 55 and 56 have no implementation to answer them.
EVIDENCE: api.py — /ws/live sends runtime.snapshot() on a timer with no sequence
  number, no backfill, and no client acknowledgement.
REQUIRED CHANGE: Sequenced local event buffer with a declared depth and a defined
  overflow policy; idempotent backfill on reconnect.
```

### F-08 FAULT is terminal and produces no diagnostic record

```text
SEVERITY: MEDIUM
AREA: firmware
ASSUMPTION UNDER ATTACK: that fault handling is observable and recoverable
FAILURE MODE: Once in FAULT with running=True, no branch matches and the controller
  remains there indefinitely. Outputs de-energize because commands are rebuilt each
  tick, but no fault reason is captured and the only exit is a network-initiated
  stop.
EVIDENCE: controller.py — tick() has no FAULT branch; the only path out is the
  "if not running" clause returning to IDLE.
REQUIRED CHANGE: Record a fault event with a reason code; define an explicit
  acknowledge-and-clear path that is local, not network-dependent.
```

### F-09 Physical safe state is nowhere defined

```text
SEVERITY: BLOCKER
AREA: rotating-installation
ASSUMPTION UNDER ATTACK: reject conditions 4 and 11, question 50
FAILURE MODE: Reject conditions about latched outputs and reset-time output behavior
  cannot be evaluated, because no document states the required de-energized physical
  behavior of each actuator: gate fail-closed, push valve fail-retracted, aeration
  fail-off, impeller fail-stopped.
EVIDENCE: The default Commands() object is the de-facto safe state in code. No
  specification of the corresponding physical behavior appears in
  ARCHITECTURE.md, PROTOTYPE_HW.md, SPECIFICATION.md, or the machine profile.
REQUIRED CHANGE: Specify per-actuator fail state in the machine profile and verify it
  against the wiring, including behavior during reset and boot.
```

### F-10 Acceptance tests carry no pass or fail thresholds

```text
SEVERITY: HIGH
AREA: RF
ASSUMPTION UNDER ATTACK: that the mandatory test list is decidable
FAILURE MODE: A test with no threshold cannot fail. The listed tests can all be
  performed and reported as passed without evidence.
EVIDENCE: docs/REDTEAM_WIFI_SP01.md, "Mandatory v0.1 RF/network tests" — no numeric
  criteria for control jitter, reconnect duration, RSSI margin, packet loss, or
  permissible difference in cutoff weight between Wi-Fi-enabled and Wi-Fi-disabled
  runs.
REQUIRED CHANGE: State numeric acceptance criteria per test before execution.
```

---

## 2. Gaps in the review document

The following are not asked anywhere in the 90 questions.

1. **Hardwired safety.** No question addresses E-stop, safety relays, guard
   interlocks, or a required performance level. For a rotary packer the safety
   function belongs outside the ESP32 entirely. This is the largest omission in the
   document.
2. **RS485 and TLB failure semantics.** Wi-Fi is declared non-critical by design; the
   RS485 link to the LAUMAS TLB is inside the cutoff path. The document contains no
   question on RS485 timeout during FINE_FILL, TLB reboot mid-cycle, or last-value
   retention on a dead link. This is a larger risk than the entire RF section.
3. **Stuck-sensor and plausibility checks.** Cutoff is driven by
   `projected_final_kg`. A frozen weight reading yields rate near zero and a
   projection near the current weight, so FINE_FILL continues to its timeout. No
   liveness or plausibility check on the weight channel is specified.
4. **Environment.** Cement dust ingress, enclosure IP rating, internal enclosure
   temperature on a rotating frame near the packer, and ESP32-S3 thermal limits are
   not addressed.
5. **Alternatives.** The slip-ring path is rejected in one line. ESP-NOW, sub-GHz
   links, optical and inductive coupling are not compared. If Wi-Fi proves marginal
   during the RF survey, no documented second option exists.
6. **The supervisory command seam.** PROTOTYPE_HW.md permits "validated
   non-time-critical commands" over the supervisory path. The boundary between that
   and control is undefined, which is the route by which reject condition 1 returns.
7. **Legal metrology.** [Unverified] A cement bag filling scale may be subject to
   legal-for-trade requirements and the weighing instrument may be type-approved and
   sealed. Replacing or shadowing the weighing controller may carry certification
   consequences. This has not been verified against Vietnamese regulation and is
   raised as a question for the responsible metrology authority, not as a finding.

---

## 3. Verdict

```text
REJECT
```

The rejection is not directed at the Wi-Fi topology. The separation argument is sound
and the framing — Wi-Fi must be non-critical enough that its unreliability cannot
affect local filling control — is the correct claim to test.

The rejection is directed at the review process and the present code:

- the document reviews firmware that does not exist (Meta-1);
- the code that does exist already contradicts reject conditions 1, 6, 7 and 13, and
  answers question 59 in the failing direction (F-01 to F-04);
- physical safe state is undefined, which makes several reject conditions
  undecidable (F-09);
- the mandatory tests have no thresholds (F-10).

Suggested path to `BENCH-PILOTABLE`: close F-01 through F-09, define per-actuator
safe state in the machine profile, attach numeric thresholds to every mandatory test,
then rewrite `REDTEAM_WIFI_SP01.md` against real firmware with the expected verdict
removed.

---

## 4. Confidence and limits of this review

- Findings F-01 to F-09 are read directly from source on
  `bootstrap/python-reference-runtime` and are verifiable by inspection.
- [Inference] The operational consequences attributed to these findings on ESP32-S3
  hardware are reasoned from the code and the stated architecture. They have not been
  measured on the target, and behavior on that target is not established by this
  review.
- [Unverified] No RF measurement, timing measurement, hardware inspection, or
  regulatory check was performed.
