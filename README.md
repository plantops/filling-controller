# Filling Controller — Python Simulator / Reference

> **Branch status:** `py-sim` is the preserved Python digital twin, replay and conformance-reference implementation. It is **not** the candidate production controller for live SP01 hardware.
>
> Current hardware/FW direction lives on `main` and `fw-sp01-v0.1`.

Universal, digital-twin-first bag filling reference runtime.

The permanent product is the machine behavior contract, not a specific implementation language. A machine profile, semantic signal model, state semantics, strategies, adapters, events, and conformance scenarios define behavior.

## Role of this branch

Use `py-sim` for:

- simulated plant physics;
- replay and fault reproduction;
- algorithm experiments;
- soft-sensor development;
- prototype HMI work;
- scenario generation;
- conformance/oracle comparison with embedded firmware.

Do **not** use this branch as evidence that ESP32 timing, watchdogs, local I/O, TLB/RS485, Wi-Fi isolation or live machine safety behavior have been validated.

The design evolved after the bootstrap: production SP01 control is now targeted at ESP32-S3 with ESP-IDF/C++/FreeRTOS, local 24 V I/O and LAUMAS TLB485. See the current `main` README and `docs/EVOLUTION.md` there.

## Architectural rules

1. Control logic never imports GPIO, HX711, Modbus, PLC, or simulator implementations directly.
2. All I/O is addressed by semantic signal names.
3. Plant truth, sensor measurements, and controller estimates are separate domains.
4. State transitions are deterministic and observable within the reference model.
5. Simulation and hardware implementations should converge through common semantic interfaces and conformance scenarios.
6. Hardware migration is channel-by-channel, with shadow comparison before authority transfer.
7. Machine/vendor differences belong in profiles, capabilities, strategies, and adapters.
8. Alternate runtimes must pass the same conformance scenarios before behavioral equivalence is claimed.

## Branch contents

- `docs/ARCHITECTURE.md` — layering and portability rules
- `docs/SPECIFICATION.md` — language-neutral packer model
- `docs/ADAPTERS.md` — sim/hard/shadow adapter contract
- `docs/SIMULATION.md` — filling physics model
- `spec/profiles/haver-rotary-pilot-sp01.yaml` — simulator profile
- `src/filling_controller/` — Python reference runtime
- `tests/test_cycle.py` — deterministic simulated-cycle test seed

## Run

Python 3.14 is the reference CI runtime. The package currently supports Python 3.12+.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[dev]'
filling-controller
```

On Windows PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e ".[dev]"
filling-controller
```

Open `http://127.0.0.1:8000`.

If port 8000 is occupied:

```powershell
python -m uvicorn filling_controller.api:app --host 127.0.0.1 --port 8010
```

## Select another profile

The runtime defaults to:

```text
spec/profiles/haver-rotary-pilot-sp01.yaml
```

Override it with:

```bash
FILLING_PROFILE=/path/to/profile.yaml filling-controller
```

## Test

```bash
pytest -q
```

The reference simulator remains useful for predictive-cutoff development and for generating deterministic scenarios to compare against the embedded SP01 controller.