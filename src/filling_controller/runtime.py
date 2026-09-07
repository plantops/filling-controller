from __future__ import annotations

from .adapters import SimAdapter
from .config import Config
from .controller import SpoutController
from .model import ControllerState, CycleSummary, Estimates, Snapshot
from .simulator import VirtualPacker
from .soft_sensors import SoftSensors
from .storage import Recorder


class Runtime:
    def __init__(self, config: Config, recorder: Recorder | None = None):
        self.config = config
        self.plant = VirtualPacker(config.simulation)
        self.adapter = SimAdapter(self.plant)
        self.controller = SpoutController(config.recipe)
        self.soft = SoftSensors(config.strategy)
        self.recorder = recorder or Recorder()
        self.running = False
        self.now = 0.0
        self.cycle_id = 1
        self.cycle_started_at = 0.0
        self._controller_accum = 0.0
        self._last_estimates = Estimates()
        self._last_measurements = self.adapter.read_all(0.0, self.controller.commands)
        self._last_snapshot: Snapshot | None = None

    @property
    def simulation_dt(self) -> float:
        return float(self.config.runtime["simulation_tick_ms"]) / 1000.0

    @property
    def controller_dt(self) -> float:
        return float(self.config.runtime["controller_tick_ms"]) / 1000.0

    def start(self) -> None:
        self.running = True

    def stop(self) -> None:
        self.running = False

    def reset(self) -> None:
        self.running = False
        self.now = 0.0
        self.cycle_id = 1
        self.cycle_started_at = 0.0
        self._controller_accum = 0.0
        self.plant.new_cycle()
        self.soft.reset()
        self.controller.reset(0.0)

    def step(self, dt: float | None = None) -> Snapshot:
        dt = self.simulation_dt if dt is None else dt
        self.now += dt
        self.adapter.tick(dt, self.controller.commands)
        measurements = self.adapter.read_all(self.now, self.controller.commands)
        estimates = self.soft.update(self.now, float(measurements["weight.net"].value))
        self._last_estimates = estimates
        self._last_measurements = measurements

        self._controller_accum += dt
        if self._controller_accum + 1e-12 >= self.controller_dt:
            self._controller_accum = 0.0
            transition = self.controller.tick(self.now, self.running, measurements, estimates)
            if transition:
                self.recorder.event(self.now, self.cycle_id, "state_transition", {"from": transition.previous.value, "to": transition.current.value})
                if transition.current == ControllerState.COARSE_FILL:
                    self.cycle_started_at = self.now
                if transition.current == ControllerState.COMPLETE:
                    final = estimates.weight_filtered_kg
                    self.recorder.cycle(CycleSummary(
                        cycle_id=self.cycle_id,
                        target_kg=self.controller.target,
                        started_at=self.cycle_started_at,
                        completed_at=self.now,
                        fill_total_s=max(0.0, self.now - self.cycle_started_at),
                        weight_at_cutoff_kg=self.controller.cutoff_weight,
                        final_weight_kg=final,
                        final_error_kg=final - self.controller.target,
                        estimated_rate_at_cutoff_kg_s=self.controller.cutoff_rate,
                        projected_at_cutoff_kg=self.controller.cutoff_projected,
                    ))
                if transition.previous == ControllerState.COMPLETE and transition.current == ControllerState.WAIT_TRIGGER:
                    self.cycle_id += 1
                    self.plant.new_cycle()
                    self.soft.reset()

        self._last_snapshot = Snapshot(self.now, self.controller.state, self.cycle_id, self.running, self.plant.truth, estimates, measurements, self.controller.commands)
        return self._last_snapshot

    def snapshot(self) -> Snapshot:
        if self._last_snapshot is None:
            return self.step(0.0)
        return self._last_snapshot

    def run_for(self, seconds: float) -> Snapshot:
        end = self.now + seconds
        snapshot = self.snapshot()
        while self.now < end:
            snapshot = self.step()
        return snapshot
