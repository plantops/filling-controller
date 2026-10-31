# SP01 field DO polarity correction

Scope: software/configuration evidence only. This record does not advance G8 and does not authorize machine actuator connection.

Observed/configuration finding:
- SP01 board DO is active-low.
- Field profile previously had no explicit DO inversion mask.
- Commit `7f98c76bda2878e34b3ff8705e1cf69265b3de46` sets `CONFIG_SP01_DO_INVERT_MASK=0xFF` in `firmware/esp32-s3/sdkconfig.defaults`.
- Logical OFF/idle therefore maps to physical HIGH; logical ON/active maps to physical LOW.

CI evidence for correction commit:
- push `fw #276`, run `34733315699`: PASS
- PR `fw #277`, run `34733317074`: PASS
- PR `edge-web #70`, run `34733317083`: PASS
- commit checks include `linux-amd64`, `esp32-s3`, `collector`, and `canonical-web`: PASS

What this proves:
- the corrected configuration compiles and passes the existing automated checks.

What this does not prove:
- physical DO polarity on the installed board;
- machine-channel mapping;
- safe physical state during reset/brownout/fault;
- actuator behavior;
- G8 or G9 completion.

Required local evidence before any machine-output authority:
1. Keep machine actuator wiring disconnected.
2. On an approved isolated dummy load or electrical test point, verify logical OFF gives the documented safe physical level for DO1..DO8.
3. Command one output at a time in a diagnostic/test context and verify only the matching channel changes to the active physical level, then returns to safe OFF.
4. Reboot/reset with actuator wiring still isolated and verify all physical outputs start/return to safe OFF.
5. Record board/spout ID, firmware SHA, test setup, measured levels/channel mapping, reset observation, operator, and any anomaly.

Formal gate status remains governed by `docs/GATE_EXECUTION_PLAN.md`. Machine actuator outputs remain isolated until formal G8 is complete and the documented G8-to-G9 transition is locally authorized.
