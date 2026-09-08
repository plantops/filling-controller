from __future__ import annotations

from collections import deque
import math
import random

from .model import Commands, FillStage, PlantTruth, Quality, SignalValue, Source


class VirtualPacker:
    """Control-engineering model of one filling unit."""

    def __init__(self, cfg: dict):
        self.cfg = cfg
        self.rng = random.Random(int(cfg.get("random_seed", 1)))
        self.truth = PlantTruth()
        self._flow_delay: deque[float] = deque()
        self._bag_detect_accum = 0.0
        self.new_cycle()

    def new_cycle(self) -> None:
        self.truth = PlantTruth()
        self._bag_detect_accum = 0.0
        dt = 0.01
        delay_steps = max(1, math.ceil(float(self.cfg["transport_delay_s"]) / dt))
        self._flow_delay = deque([0.0] * delay_steps, maxlen=delay_steps)

    def tick(self, dt: float, commands: Commands) -> None:
        t = self.truth
        t.cycle_elapsed_s += dt

        if commands.bag_detect_air or commands.scanner_down:
            self._bag_detect_accum += dt
            if self._bag_detect_accum >= float(self.cfg["bag_detect_delay_s"]):
                t.bag_present = True
        else:
            self._bag_detect_accum = 0.0

        if commands.fill_stage == FillStage.COARSE:
            gate_command = 1.0
        elif commands.fill_stage == FillStage.FINE:
            gate_command = float(self.cfg["fine_gate_fraction"])
        else:
            gate_command = 0.0
        t.gate_command = gate_command

        gate_tau = max(0.001, float(self.cfg["gate_tau_s"]))
        t.gate_actual += (gate_command - t.gate_actual) * min(1.0, dt / gate_tau)

        flow_target = 0.0
        if commands.filling_motor and t.bag_present:
            flow_target = float(self.cfg["max_flow_kg_s"]) * max(0.0, t.gate_actual)

        flow_tau = max(0.001, float(self.cfg["flow_tau_s"]))
        t.flow_gate_kg_s += (flow_target - t.flow_gate_kg_s) * min(1.0, dt / flow_tau)

        delayed_flow = self._flow_delay.popleft()
        self._flow_delay.append(max(0.0, t.flow_gate_kg_s))
        t.flow_bag_kg_s = delayed_flow
        t.mass_kg += delayed_flow * dt

    def measurements(self, now: float, commands: Commands) -> dict[str, SignalValue]:
        t = self.truth
        vibration_std = float(self.cfg["vibration_noise_std_kg"]) if commands.filling_motor else 0.0
        noise = self.rng.gauss(0.0, float(self.cfg["sensor_noise_std_kg"]) + vibration_std)
        weight = t.mass_kg + float(self.cfg.get("zero_offset_kg", 0.0)) + noise
        discharge = float(self.cfg["discharge_window_start_s"]) <= t.cycle_elapsed_s <= float(self.cfg["discharge_window_end_s"])

        def sig(tag: str, value, unit: str | None = None) -> SignalValue:
            return SignalValue(tag, value, unit, now, Quality.SIMULATED, Source.SIM)

        return {
            "machine.running": sig("machine.running", True),
            "process.initiative": sig("process.initiative", True),
            "product.available": sig("product.available", True),
            "downstream.ready": sig("downstream.ready", True),
            "bag.present": sig("bag.present", t.bag_present),
            "position.discharge_window": sig("position.discharge_window", discharge),
            "weight.net": sig("weight.net", weight, "kg"),
        }
