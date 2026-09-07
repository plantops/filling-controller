# Simulation Model

## Scope

The simulator is a control-engineering model, not CFD. It models the effects that materially affect filling control:

- gate opening and closing lag;
- coarse/fine gate position;
- cement flow response;
- transport delay from gate to bag;
- material in flight after cutoff;
- bag mass accumulation;
- load-cell noise and vibration;
- bag-detection delay;
- rotary/discharge timing;
- fault injection hooks.

## Gate model

The actual gate follows the requested gate with a first-order response:

```text
dg/dt = (command - actual) / gate_tau
```

## Flow model

Requested flow is proportional to gate opening while filling motor and product permissives are active. Actual gate flow follows requested flow with a first-order lag.

## Transport delay

Flow leaving the filling mechanism reaches the bag after a configurable delay. A delay queue models this transport. This naturally creates material-in-flight overshoot after the cutoff command.

## Load-cell model

Measured weight contains true bag mass plus offset, random noise, and additional vibration while the filling motor is active.

## Determinism

Every simulation run has a random seed. The seed and parameters must be stored with the cycle so a failed cycle can be replayed.

## Calibration path

Simulation parameters are placeholders until real observations exist. Replace them progressively with values identified from shadow-mode data:

1. coarse flow;
2. fine flow;
3. opening/closing response;
4. transport/inflight delay;
5. load-cell noise and lag;
6. actual filling and discharge windows.
