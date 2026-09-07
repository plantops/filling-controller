from __future__ import annotations

from dataclasses import dataclass

from .model import Commands, ControllerState, Estimates, FillStage, SignalValue


@dataclass(slots=True)
class Transition:
    previous: ControllerState
    current: ControllerState
    at: float


class SpoutController:
    """Reference implementation of SP01 state semantics; no hardware imports."""

    def __init__(self, recipe: dict):
        self.target = float(recipe["target_kg"])
        self.coarse_transition = float(recipe["coarse_transition_kg"])
        self.tolerance = float(recipe["tolerance_kg"])
        self.state = ControllerState.IDLE
        self.state_since = 0.0
        self.commands = Commands()
        self.cutoff_weight = 0.0
        self.cutoff_rate = 0.0
        self.cutoff_projected = 0.0
        self.fill_started_at: float | None = None

    def reset(self, now: float = 0.0) -> None:
        self.state = ControllerState.IDLE
        self.state_since = now
        self.commands = Commands()
        self.cutoff_weight = 0.0
        self.cutoff_rate = 0.0
        self.cutoff_projected = 0.0
        self.fill_started_at = None

    def _transition(self, state: ControllerState, now: float) -> Transition:
        previous = self.state
        self.state = state
        self.state_since = now
        return Transition(previous, state, now)

    @staticmethod
    def _bool(measurements: dict[str, SignalValue], tag: str) -> bool:
        return bool(measurements[tag].value)

    def tick(self, now: float, running: bool, measurements: dict[str, SignalValue], estimates: Estimates) -> Transition | None:
        c = Commands()
        transition = None

        if not running:
            self.commands = c
            if self.state != ControllerState.IDLE:
                transition = self._transition(ControllerState.IDLE, now)
            return transition

        if self.state == ControllerState.IDLE:
            transition = self._transition(ControllerState.WAIT_TRIGGER, now)
        elif self.state == ControllerState.WAIT_TRIGGER:
            ready = all(self._bool(measurements, tag) for tag in ("machine.running", "process.initiative", "product.available", "downstream.ready"))
            if ready:
                transition = self._transition(ControllerState.BAG_VERIFY, now)
        elif self.state == ControllerState.BAG_VERIFY:
            c.scanner_down = True
            c.bag_detect_air = True
            if self._bool(measurements, "bag.present"):
                transition = self._transition(ControllerState.TARE, now)
            elif now - self.state_since > 2.0:
                transition = self._transition(ControllerState.FAULT, now)
        elif self.state == ControllerState.TARE:
            c.scanner_down = True
            c.bag_detect_air = True
            if now - self.state_since >= 0.10:
                self.fill_started_at = now
                transition = self._transition(ControllerState.COARSE_FILL, now)
        elif self.state == ControllerState.COARSE_FILL:
            c.fill_stage = FillStage.COARSE
            c.filling_motor = True
            c.aeration = True
            if estimates.weight_filtered_kg >= self.coarse_transition:
                transition = self._transition(ControllerState.FINE_FILL, now)
            elif now - self.state_since > 12.0:
                transition = self._transition(ControllerState.FAULT, now)
        elif self.state == ControllerState.FINE_FILL:
            c.fill_stage = FillStage.FINE
            c.filling_motor = True
            c.aeration = True
            if estimates.projected_final_kg >= self.target:
                self.cutoff_weight = estimates.weight_filtered_kg
                self.cutoff_rate = estimates.fill_rate_kg_s
                self.cutoff_projected = estimates.projected_final_kg
                transition = self._transition(ControllerState.CUTOFF, now)
            elif now - self.state_since > 5.0:
                transition = self._transition(ControllerState.FAULT, now)
        elif self.state == ControllerState.CUTOFF:
            if now - self.state_since >= 0.05:
                transition = self._transition(ControllerState.SETTLING, now)
        elif self.state == ControllerState.SETTLING:
            if estimates.stable or now - self.state_since >= 1.5:
                transition = self._transition(ControllerState.WAIT_PUSH, now)
        elif self.state == ControllerState.WAIT_PUSH:
            if self._bool(measurements, "position.discharge_window"):
                transition = self._transition(ControllerState.PUSH_OFF, now)
            elif now - self.state_since > 6.0:
                transition = self._transition(ControllerState.FAULT, now)
        elif self.state == ControllerState.PUSH_OFF:
            c.push = True
            if now - self.state_since >= 0.40:
                transition = self._transition(ControllerState.COMPLETE, now)
        elif self.state == ControllerState.COMPLETE:
            if now - self.state_since >= 0.10:
                transition = self._transition(ControllerState.WAIT_TRIGGER, now)

        self.commands = c
        return transition
