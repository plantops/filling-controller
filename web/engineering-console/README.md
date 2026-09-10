# SP01 Engineering Console

Single static HTML application that renders all 13 canonical SP01 engineering views. It is intended for Cloudflare static hosting and can also be mirrored on the plant LAN.

The browser consumes only the read-only edge collector API:

```text
GET /api/v1/spouts
GET /api/v1/spouts/<SPxx>
GET /api/v1/spouts/<SPxx>/events
GET /api/v1/live   (SSE)
```

No POST/PUT/DELETE control route exists in this FE.

## Cloudflare Pages

Publish directory:

```text
web/engineering-console
```

No build command is required.

The API origin can be selected at runtime by opening the page with:

```text
?api=https://<collector-api-host>
```

or by clicking `API`; the chosen value is kept in browser local storage.

For cross-origin operation set the collector's `CORS_ORIGIN` to the exact FE origin. Prefer exposing the collector through an authenticated outbound tunnel/reverse proxy rather than opening a plant inbound port.

## View rule

The UI follows `docs/SP01_CANONICAL_VIEWS.md`. Missing runtime values are rendered as unavailable. It must not synthesize rotor angle, detector thresholds, physical DO feedback or other pseudo-live values that the source telemetry does not provide.

## Control boundary

Cloudflare, the browser and the collector are supervisory. Local ESP32/TLB/I/O remain the process authority. Loss of FE, SSE, collector, WAN or tunnel must not participate in filling cutoff, broken-bag fill shutdown or 210°/355° eject timing.
