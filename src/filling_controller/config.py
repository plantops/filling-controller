from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml


@dataclass(slots=True)
class Config:
    raw: dict[str, Any]

    @property
    def recipe(self) -> dict[str, Any]:
        return self.raw["recipe"]

    @property
    def strategy(self) -> dict[str, Any]:
        return self.raw["strategy"]

    @property
    def simulation(self) -> dict[str, Any]:
        return self.raw["simulation"]

    @property
    def runtime(self) -> dict[str, Any]:
        return self.raw["runtime"]


def load_config(path: str | Path) -> Config:
    with Path(path).open("r", encoding="utf-8") as fh:
        return Config(yaml.safe_load(fh))
