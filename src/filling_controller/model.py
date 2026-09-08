from __future__ import annotations

from dataclasses import asdict, dataclass, field
from enum import StrEnum
from typing import Any


class Source(StrEnum):
    SIM = "sim"
    HARD = "hard"
    DERIVED = "derived"
    REPLAY = "replay"
    MANUAL = "manual"


class Quality(StrEnum):
    GOOD = "good"
    STALE = "stale"
    BAD = "bad"
    UNKNOWN = "unknown"
    SIMULATED = "simulated"


class FillStage(StrEnum):
    OFF = "off"
    COARSE = "coarse"
    FINE = "fine"


class ControllerState(StrEnum):
    IDLE = "idle"
    WAIT_TRIGGER = "wait_trigger"
    BAG_VERIFY = "bag_verify"
    TARE = "tare"
    COARSE_FILL = "coarse_fill"
    FINE_FILL = "fine_fill"
    CUTOFF = "cutoff"
    SETTLING = "settling"
    WAIT_PUSH = "wait_push"
    PUSH_OFF = "push_off"
    COMPLETE = "complete"
    FAULT = "fault"


@dataclass(slots=True)
class SignalValue:
    tag: str
    value: Any
    unit: str | None
    timestamp: float
    quality: Quality
    source: Source

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["quality"] = self.quality.value
        data["source"] = self.source.value
        return data


@dataclass(slots=True)
class Commands:
    scanner_down: bool = False
    bag_detect_air: bool = False
    fill_stage: FillStage = FillStage.OFF
    filling_motor: bool = False
    aeration: bool = False
    push: bool = False

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["fill_stage"] = self.fill_stage.value
        return data


@dataclass(slots=True)
class PlantTruth:
    mass_kg: float = 0.0
    gate_command: float = 0.0
    gate_actual: float = 0.0
    flow_gate_kg_s: float = 0.0
    flow_bag_kg_s: float = 0.0
    cycle_elapsed_s: float = 0.0
    bag_present: bool = False


@dataclass(slots=True)
class Estimates:
    weight_filtered_kg: float = 0.0
    fill_rate_kg_s: float = 0.0
    projected_final_kg: float = 0.0
    stable: bool = False


@dataclass(slots=True)
class CycleSummary:
    cycle_id: int
    target_kg: float
    started_at: float
    completed_at: float
    fill_total_s: float
    weight_at_cutoff_kg: float
    final_weight_kg: float
    final_error_kg: float
    estimated_rate_at_cutoff_kg_s: float
    projected_at_cutoff_kg: float
    result: str = "complete"

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass(slots=True)
class Snapshot:
    timestamp: float
    state: ControllerState
    cycle_id: int
    running: bool
    truth: PlantTruth
    estimates: Estimates
    measurements: dict[str, SignalValue] = field(default_factory=dict)
    commands: Commands = field(default_factory=Commands)

    def to_dict(self) -> dict[str, Any]:
        return {
            "timestamp": self.timestamp,
            "state": self.state.value,
            "cycle_id": self.cycle_id,
            "running": self.running,
            "truth": asdict(self.truth),
            "estimates": asdict(self.estimates),
            "measurements": {k: v.to_dict() for k, v in self.measurements.items()},
            "commands": self.commands.to_dict(),
        }
