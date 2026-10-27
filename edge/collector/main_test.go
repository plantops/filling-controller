package main

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"sync/atomic"
	"testing"
	"time"
)

func TestParseSources(t *testing.T) {
	sources, err := parseSources("SP02=http://10.0.0.2/api/state,SP01=http://10.0.0.1/api/state")
	if err != nil {
		t.Fatal(err)
	}
	if len(sources) != 2 || sources[0].ID != "SP01" || sources[1].ID != "SP02" {
		t.Fatalf("unexpected sources: %#v", sources)
	}
	if _, err := parseSources(""); err == nil {
		t.Fatal("expected empty source error")
	}
	if _, err := parseSources("SP01=ftp://bad"); err == nil {
		t.Fatal("expected scheme error")
	}
}

func TestNormalizeTelemetryLegacyAliases(t *testing.T) {
	got := normalizeTelemetry(map[string]any{
		"cycle":              float64(7),
		"do":                 float64(3),
		"broken_detected_us": float64(1234),
	})
	if got["schema_version"] != telemetrySchemaVersion {
		t.Fatalf("schema version missing: %#v", got)
	}
	if got["cycle_id"] != float64(7) || got["desired_do"] != float64(3) || got["broken_bag_detected_us"] != float64(1234) {
		t.Fatalf("legacy aliases not normalized: %#v", got)
	}
	if _, ok := got["commanded_do"]; ok {
		t.Fatalf("legacy desired output was incorrectly promoted to commanded output: %#v", got)
	}
	if _, ok := got["physical_do"]; ok {
		t.Fatalf("legacy desired output was incorrectly promoted to physical feedback: %#v", got)
	}
	for _, legacy := range []string{"cycle", "do", "broken_detected_us"} {
		if _, ok := got[legacy]; ok {
			t.Fatalf("legacy key %s leaked into canonical schema: %#v", legacy, got)
		}
	}
}

func TestCanonicalTelemetryWinsOverAliases(t *testing.T) {
	got := normalizeTelemetry(map[string]any{
		"cycle":                  float64(7),
		"cycle_id":               float64(8),
		"do":                     float64(3),
		"desired_do":             float64(251),
		"commanded_do":           float64(0),
		"broken_detected_us":     float64(1234),
		"broken_bag_detected_us": float64(5678),
	})
	if got["cycle_id"] != float64(8) || got["desired_do"] != float64(251) || got["commanded_do"] != float64(0) || got["broken_bag_detected_us"] != float64(5678) {
		t.Fatalf("canonical values did not win: %#v", got)
	}
}

func TestStoreTracksControllerChanges(t *testing.T) {
	s := NewStore(20)
	t0 := time.Unix(100, 0).UTC()
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0, Online: true, Data: map[string]any{
		"state": "COARSE_FILL", "fault": "NONE", "disposition": "UNDECIDED",
		"cycle_id": float64(1), "desired_do": float64(251), "commanded_do": float64(251),
		"broken_bag_detected_us": float64(0),
	}})
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0.Add(time.Second), Online: true, Data: map[string]any{
		"state": "REJECT_WAIT", "fault": "NONE", "disposition": "REJECT",
		"cycle_id": float64(1), "desired_do": float64(3), "commanded_do": float64(3),
		"broken_bag_detected_us": float64(900000),
	}})
	events := s.Events("SP01", 20)
	seen := map[string]bool{}
	for _, ev := range events {
		seen[ev.Kind] = true
	}
	for _, key := range []string{"state", "disposition", "desired_do", "commanded_do", "broken_bag_detected_us"} {
		if !seen[key] {
			t.Fatalf("missing tracked event %s: %#v", key, events)
		}
	}
}

// A missing observation must not be recorded as a controller transition.
// State changes during an offline interval are unknown, not observed events.
func TestStoreOfflineGapDoesNotInventProcessEvents(t *testing.T) {
	s := NewStore(20)
	t0 := time.Unix(200, 0).UTC()
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0, Online: true,
		Data: map[string]any{"state": "COARSE_FILL", "disposition": "UNDECIDED"}})
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0.Add(time.Second), Online: false,
		Error: "network timeout"})
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0.Add(2 * time.Second), Online: true,
		Data: map[string]any{"state": "REJECT_WAIT", "disposition": "REJECT"}})

	onlineChanges := 0
	for _, ev := range s.Events("SP01", 20) {
		switch ev.Kind {
		case "discovered":
		case "online":
			onlineChanges++
		default:
			t.Fatalf("unobserved process transition across offline gap: %#v", ev)
		}
	}
	if onlineChanges != 2 {
		t.Fatalf("expected offline and reconnect events, got %d", onlineChanges)
	}

	// Continuous online observations must still emit genuine process changes.
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0.Add(3 * time.Second), Online: true,
		Data: map[string]any{"state": "COMPLETE", "disposition": "REJECT"}})
	stateChanges := 0
	for _, ev := range s.Events("SP01", 20) {
		if ev.Kind == "state" {
			stateChanges++
			if ev.From != "REJECT_WAIT" || ev.To != "COMPLETE" {
				t.Fatalf("unexpected observed transition: %#v", ev)
			}
		}
	}
	if stateChanges != 1 {
		t.Fatalf("expected one observed state transition, got %d", stateChanges)
	}
}

func TestReadOnlyAPI(t *testing.T) {
	store := NewStore(10)
	store.Update(Snapshot{SpoutID: "SP01", ReceivedAt: time.Now().UTC(), Online: true, Data: map[string]any{"state": "WAIT_PERMISSIVE"}})
	app := &App{store: store}
	h := app.routes()

	for _, path := range []string{"/healthz", "/api/v1/spouts", "/api/v1/spouts/SP01", "/api/v1/spouts/SP01/events"} {
		r := httptest.NewRequest(http.MethodGet, path, nil)
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		if w.Code != http.StatusOK {
			t.Fatalf("%s returned %d: %s", path, w.Code, w.Body.String())
		}
	}

	r := httptest.NewRequest(http.MethodPost, "/api/v1/spouts/SP01", nil)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	if w.Code != http.StatusMethodNotAllowed {
		t.Fatalf("write route unexpectedly available: %d", w.Code)
	}
}

func TestPollOneRejectsHTTP200WithoutState(t *testing.T) {
	for _, invalid := range []string{
		"null", "{}", `{"state":null}`, `{"state":""}`,
		`{"state":"  "}`, `{"state":123}`,
	} {
		t.Run(invalid, func(t *testing.T) {
			var body atomic.Value
			body.Store(`{"state":"COARSE_FILL","disposition":"UNDECIDED"}`)
			server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
				w.Header().Set("Content-Type", "application/json")
				_, _ = fmt.Fprint(w, body.Load().(string))
			}))
			defer server.Close()

			store := NewStore(20)
			app := &App{store: store, client: server.Client()}
			src := Source{ID: "SP01", URL: server.URL}
			app.pollOne(context.Background(), src)

			body.Store(invalid)
			app.pollOne(context.Background(), src)
			snap, ok := store.Get("SP01")
			if !ok || snap.Online || snap.Error != "invalid telemetry: missing state" {
				t.Fatalf("invalid 200 body was accepted as process telemetry: %#v", snap)
			}

			body.Store(`{"state":"REJECT_WAIT","disposition":"REJECT"}`)
			app.pollOne(context.Background(), src)
			snap, ok = store.Get("SP01")
			if !ok || !snap.Online || snap.Data["state"] != "REJECT_WAIT" {
				t.Fatalf("valid telemetry did not recover: %#v", snap)
			}

			onlineChanges := 0
			for _, ev := range store.Events("SP01", 20) {
				switch ev.Kind {
				case "discovered":
				case "online":
					onlineChanges++
				default:
					t.Fatalf("invented process event during invalid-telemetry gap: %#v", ev)
				}
			}
			if onlineChanges != 2 {
				t.Fatalf("expected offline/reconnect events, got %d", onlineChanges)
			}
		})
	}
}
