package main

import (
	"net/http"
	"net/http/httptest"
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
	if got["cycle_id"] != float64(7) || got["commanded_do"] != float64(3) || got["desired_do"] != float64(3) || got["broken_bag_detected_us"] != float64(1234) {
		t.Fatalf("legacy aliases not normalized: %#v", got)
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
