from __future__ import annotations

import json
import sqlite3
from pathlib import Path

from .model import CycleSummary


class Recorder:
    def __init__(self, path: str | Path = "filling-controller.sqlite3"):
        self.conn = sqlite3.connect(str(path), check_same_thread=False)
        self.conn.execute("create table if not exists cycles (cycle_id integer primary key, completed_at real not null, payload text not null)")
        self.conn.execute("create table if not exists events (id integer primary key autoincrement, ts real not null, cycle_id integer not null, kind text not null, payload text not null)")
        self.conn.commit()

    def event(self, ts: float, cycle_id: int, kind: str, payload: dict) -> None:
        self.conn.execute("insert into events(ts, cycle_id, kind, payload) values (?, ?, ?, ?)", (ts, cycle_id, kind, json.dumps(payload, separators=(",", ":"))))
        self.conn.commit()

    def cycle(self, summary: CycleSummary) -> None:
        self.conn.execute("insert or replace into cycles(cycle_id, completed_at, payload) values (?, ?, ?)", (summary.cycle_id, summary.completed_at, json.dumps(summary.to_dict(), separators=(",", ":"))))
        self.conn.commit()

    def latest_cycles(self, limit: int = 50) -> list[dict]:
        rows = self.conn.execute("select payload from cycles order by cycle_id desc limit ?", (limit,)).fetchall()
        return [json.loads(row[0]) for row in rows]
