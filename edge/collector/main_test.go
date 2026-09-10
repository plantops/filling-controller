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

func TestStoreTracksControllerChanges(t *testing.T) {
	s := NewStore(10)
	t0 := time.Unix(100, 0).UTC()
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0, Online: true, Data: map[string]any{
		"state": "COARSE_FILL", "fault": "NONE", "disposition": "UNDECIDED", "do": float64(251),
	}})
	s.Update(Snapshot{SpoutID: "SP01", ReceivedAt: t0.Add(time.Second), Online: true, Data: map[string]any{
		"state": "REJECT_WAIT", "fault": "NONE", "disposition": "REJECT", "do": float64(3),
	}})
	events := s.Events("SP01", 20)
	seenState, seenDisposition, seenDO := false, false, false
	for _, ev := range events {
		switch ev.Kind {
		case "state":
			seenState = true
		case "disposition":
			seenDisposition = true
		case "do":
			seenDO = true
		}
	}
	if !seenState || !seenDisposition || !seenDO {
		t.Fatalf("missing tracked events: %#v", events)
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
