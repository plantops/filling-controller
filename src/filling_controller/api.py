from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
import os
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse

from .config import load_config
from .runtime import Runtime
from .storage import Recorder

PROFILE = Path(os.environ.get("FILLING_PROFILE", "spec/profiles/haver-rotary-pilot-sp01.yaml")).resolve()
STATIC = Path(__file__).resolve().parent / "static"

config = load_config(PROFILE)
runtime = Runtime(config, Recorder("filling-controller.sqlite3"))


async def runtime_loop() -> None:
    while True:
        runtime.step()
        await asyncio.sleep(runtime.simulation_dt)


@asynccontextmanager
async def lifespan(app: FastAPI):
    task = asyncio.create_task(runtime_loop())
    try:
        yield
    finally:
        task.cancel()


app = FastAPI(title="Filling Controller", version="0.1.0", lifespan=lifespan)


@app.get("/")
def index():
    return FileResponse(STATIC / "index.html")


@app.get("/api/state")
def state():
    return runtime.snapshot().to_dict()


@app.get("/api/config")
def get_config():
    return config.raw


@app.get("/api/channels")
def channels():
    snap = runtime.snapshot()
    return {tag: value.to_dict() for tag, value in snap.measurements.items()}


@app.get("/api/cycles")
def cycles(limit: int = 50):
    return runtime.recorder.latest_cycles(limit)


@app.post("/api/run/start")
def start():
    runtime.start()
    return {"running": True}


@app.post("/api/run/stop")
def stop():
    runtime.stop()
    return {"running": False}


@app.post("/api/run/reset")
def reset():
    runtime.reset()
    return {"running": False, "cycle_id": 1}


@app.post("/api/run/step")
def step():
    return runtime.step().to_dict()


@app.websocket("/ws/live")
async def live(ws: WebSocket):
    await ws.accept()
    telemetry_s = float(config.runtime["telemetry_ms"]) / 1000.0
    try:
        while True:
            await ws.send_json(runtime.snapshot().to_dict())
            await asyncio.sleep(telemetry_s)
    except WebSocketDisconnect:
        return


def main() -> None:
    import uvicorn

    uvicorn.run("filling_controller.api:app", host="0.0.0.0", port=8000, reload=False)
