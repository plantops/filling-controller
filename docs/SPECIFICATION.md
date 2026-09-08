# Packer Specification

## Goal

A `PackerSpec` describes machine behavior without depending on Python, Go, Rust, C, or C++.

## Composition

A packer consists of:

```text
Packer
|- CommonMachine
|  |- product supply
|  |- rotor / position system (optional)
|  |- bag supply (optional)
|  `- discharge system
`- FillingUnit x N
```

A single-spout inline machine has one `FillingUnit`. A rotary packer may have many filling units and shared common-machine services.

## Capabilities

Profiles declare capabilities instead of using vendor-specific subclasses.

Examples:

- topology: `rotary`, `inline`
- weighing mode: `gross`, `net`, `loss_in_weight`
- filling mechanism: `impeller`, `gravity`, `auger`, `air`
- stages: `coarse`, `fine`, `dribble`
- position source: `encoder`, `cam`, `none`
- discharge: `pneumatic_push`, `conveyor`, `gravity`

## State semantics

The bootstrap SP01 sequence is:

```text
IDLE
-> WAIT_TRIGGER
-> BAG_VERIFY
-> TARE
-> COARSE_FILL
-> FINE_FILL
-> CUTOFF
-> SETTLING
-> WAIT_PUSH
-> PUSH_OFF
-> COMPLETE
-> WAIT_TRIGGER
```

Every state has entry outputs, exit conditions, timeout behavior, and fault behavior.

The Python bootstrap implements these semantics directly. A later milestone moves state definitions to a validated language-neutral statechart format and adds conformance vectors so alternate runtimes can prove equivalent behavior.

## Filling strategy

The first strategy is coarse/fine filling with projected cutoff:

```text
projected_final = filtered_weight
                + positive_fill_rate * effective_delay
                + residual_inflight
```

The strategy requests cutoff when `projected_final >= target`.

The initial profile parameters are simulation values only. They must be replaced with measured values from the physical packer.

## Portability rule

No profile may contain host-language expressions, callbacks, or imports. Configuration must use data and a deliberately small set of portable strategy identifiers and parameters.
