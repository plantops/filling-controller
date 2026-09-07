from __future__ import annotations

from typing import Protocol

from .model import Commands, SignalValue
from .simulator import VirtualPacker


class Adapter(Protocol):
    def tick(self, dt: float, commands: Commands) -> None: ...
    def read_all(self, now: float, commands: Commands) -> dict[str, SignalValue]: ...
    def health(self) -> dict: ...


class SimAdapter:
    def __init__(self, plant: VirtualPacker):
        self.plant = plant

    def tick(self, dt: float, commands: Commands) -> None:
        self.plant.tick(dt, commands)

    def read_all(self, now: float, commands: Commands) -> dict[str, SignalValue]:
        return self.plant.measurements(now, commands)

    def health(self) -> dict:
        return {"status": "good", "source": "sim"}


class HardAdapterStub:
    """Stable placeholder for future GPIO/Modbus/PLC implementations."""

    def tick(self, dt: float, commands: Commands) -> None:
        raise RuntimeError("hard adapter is not implemented in bootstrap")

    def read_all(self, now: float, commands: Commands) -> dict[str, SignalValue]:
        raise RuntimeError("hard adapter is not implemented in bootstrap")

    def health(self) -> dict:
        return {"status": "unavailable", "source": "hard"}
