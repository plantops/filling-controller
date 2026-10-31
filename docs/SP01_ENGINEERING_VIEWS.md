# SP01 Engineering Views — Archived Duplicate

This file is retained only so historical links do not break.

The authoritative engineering view pack is now:

- [`SP01_CANONICAL_VIEWS.md`](SP01_CANONICAL_VIEWS.md) — eleven core views V1..V11 plus commissioning extensions V12/V13.
- [`ENGINEERING_VIEW_INDEX.md`](ENGINEERING_VIEW_INDEX.md) — index, ownership and current review/evidence status.

Do **not** use older content from this file as controller truth. Earlier revisions pre-dated the executable `BagDisposition` / `RejectWait` path and contained stale statements such as “reject branch not yet implemented” and finite-window detector wording that no longer matches the current C++ controller.

Ground-truth precedence remains:

```text
1. measured installed-machine evidence
2. executable C++ controller + board adapter
3. frozen I/O / weighing / reject / topology contracts
4. SP01_CANONICAL_VIEWS.md
5. generated HMI/diagram projections
6. historical team proposals
```

Current bench boundary is recorded in the canonical pack and `evidence/SP01/2026-09-10/G2/RESULT.md`.
