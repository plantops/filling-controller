from filling_controller.config import Config
from filling_controller.runtime import Runtime
from filling_controller.storage import Recorder


def profile() -> Config:
    return Config({
        "runtime": {"simulation_tick_ms": 10, "controller_tick_ms": 20, "telemetry_ms": 100},
        "recipe": {"target_kg": 50.0, "coarse_transition_kg": 42.0, "tolerance_kg": 0.2},
        "strategy": {"filter_alpha": 0.25, "rate_window_ms": 300, "effective_delay_s": 0.28, "residual_inflight_kg": 0.12, "stable_band_kg": 0.03, "stable_time_s": 0.35},
        "simulation": {"random_seed": 7, "cycle_time_s": 14.4, "bag_detect_delay_s": 0.15, "discharge_window_start_s": 9.5, "discharge_window_end_s": 12.5, "gate_tau_s": 0.08, "flow_tau_s": 0.12, "transport_delay_s": 0.18, "max_flow_kg_s": 8.0, "fine_gate_fraction": 0.30, "sensor_noise_std_kg": 0.015, "vibration_noise_std_kg": 0.040, "zero_offset_kg": 0.0},
    })


def test_simulated_cycle_completes():
    runtime = Runtime(profile(), Recorder(":memory:"))
    runtime.start()
    runtime.run_for(14.0)
    cycles = runtime.recorder.latest_cycles()
    assert cycles, "expected at least one completed bag"
    bag = cycles[-1]
    assert 49.0 <= bag["final_weight_kg"] <= 51.0
    assert bag["weight_at_cutoff_kg"] < bag["final_weight_kg"]
