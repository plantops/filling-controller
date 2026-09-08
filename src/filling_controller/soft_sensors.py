from __future__ import annotations

from collections import deque

from .model import Estimates


class SoftSensors:
    def __init__(self, cfg: dict):
        self.alpha = float(cfg["filter_alpha"])
        self.window_s = float(cfg["rate_window_ms"]) / 1000.0
        self.effective_delay_s = float(cfg["effective_delay_s"])
        self.residual_inflight_kg = float(cfg["residual_inflight_kg"])
        self.stable_band_kg = float(cfg["stable_band_kg"])
        self.stable_time_s = float(cfg["stable_time_s"])
        self.filtered: float | None = None
        self.history: deque[tuple[float, float]] = deque()
        self._stable_since: float | None = None

    def reset(self) -> None:
        self.filtered = None
        self.history.clear()
        self._stable_since = None

    def update(self, now: float, raw_weight: float) -> Estimates:
        if self.filtered is None:
            self.filtered = raw_weight
        else:
            self.filtered += self.alpha * (raw_weight - self.filtered)

        self.history.append((now, self.filtered))
        while len(self.history) > 2 and now - self.history[0][0] > self.window_s:
            self.history.popleft()

        rate = 0.0
        if len(self.history) >= 2:
            dt = self.history[-1][0] - self.history[0][0]
            if dt > 0:
                rate = (self.history[-1][1] - self.history[0][1]) / dt

        span = 0.0
        if self.history:
            values = [v for _, v in self.history]
            span = max(values) - min(values)

        if span <= self.stable_band_kg and abs(rate) < 0.08:
            if self._stable_since is None:
                self._stable_since = now
        else:
            self._stable_since = None
        stable = self._stable_since is not None and now - self._stable_since >= self.stable_time_s

        projected = self.filtered + max(rate, 0.0) * self.effective_delay_s + self.residual_inflight_kg
        return Estimates(self.filtered, rate, projected, stable)
