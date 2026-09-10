# SP01 Plant Edge Collector

Read-only supervisory collector for SP01..SP08. It polls each local controller HTTP telemetry endpoint, aligns the latest snapshots, keeps a small in-memory event ring, and exposes a browser-facing REST/SSE API.

It is deliberately outside the real-time control loop:

```text
SP01..SP08 local controller authority
        |
        | telemetry only
        v
plant edge collector
        |
        +-> local engineering FE
        `-> optional Cloudflare Tunnel -> Cloudflare-hosted FE
```

No actuator, calibration or recipe-write endpoint exists in this collector.

## Run

```bash
cd edge/collector

export SP01_SPOUTS='SP01=http://192.168.10.101/api/state,SP02=http://192.168.10.102/api/state'
export LISTEN_ADDR=':8080'
export POLL_MS='250'
# Set this to the exact Cloudflare FE origin if the FE is cross-origin.
export CORS_ORIGIN='https://sp01.example.com'

go run .
```

For a local same-origin reverse proxy, leave `CORS_ORIGIN` empty.

## API

```text
GET /healthz
GET /api/v1/spouts
GET /api/v1/spouts/SP01
GET /api/v1/spouts/SP01/events?limit=200
GET /api/v1/live                    text/event-stream
```

The collector accepts the current ESP `api/state` shape as an opaque JSON object under `data`, so the firmware telemetry contract can evolve without moving process authority into the collector.

Tracked event changes currently include `mode`, `state`, `fault`, `disposition`, `cycle`, `di`, `do` and `broken_detected_us` when those fields exist.

## Deployment boundary

Recommended plant deployment is a small local Linux node/container with outbound-only Cloudflare Tunnel when remote access is needed. The static FE may be hosted on Cloudflare and mirrored locally. Loss of the collector, tunnel, WAN or browser must not affect local SP01 filling, cutoff, broken-bag shutdown or eject timing.

Before G8/G9, authentication/TLS and the exact plant network exposure must be reviewed. This collector intentionally has no write/control route, reducing the commissioning attack surface.
