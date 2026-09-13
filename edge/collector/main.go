package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

const telemetrySchemaVersion = 1

var trackedTelemetryKeys = []string{
	"mode",
	"state",
	"fault",
	"disposition",
	"cycle_id",
	"di",
	"desired_do",
	"commanded_do",
	"broken_bag_detected_us",
}

type Source struct {
	ID  string
	URL string
}

type Snapshot struct {
	SpoutID    string         `json:"spout_id"`
	ReceivedAt time.Time      `json:"received_at"`
	Online     bool           `json:"online"`
	Error      string         `json:"error,omitempty"`
	Data       map[string]any `json:"data,omitempty"`
}

type Event struct {
	SpoutID string    `json:"spout_id"`
	At      time.Time `json:"at"`
	Kind    string    `json:"kind"`
	From    any       `json:"from,omitempty"`
	To      any       `json:"to,omitempty"`
}

type Store struct {
	mu        sync.RWMutex
	latest    map[string]Snapshot
	events    map[string][]Event
	listeners map[chan struct{}]struct{}
	maxEvents int
}

func NewStore(maxEvents int) *Store {
	return &Store{
		latest:    make(map[string]Snapshot),
		events:    make(map[string][]Event),
		listeners: make(map[chan struct{}]struct{}),
		maxEvents: maxEvents,
	}
}

func trackedValue(data map[string]any, key string) any {
	if data == nil {
		return nil
	}
	return data[key]
}

func equalJSONValue(a, b any) bool {
	ab, _ := json.Marshal(a)
	bb, _ := json.Marshal(b)
	return string(ab) == string(bb)
}

// normalizeTelemetry freezes the collector-facing schema while the embedded
// /api/state endpoint migrates from legacy field names. Canonical keys win;
// aliases are accepted only as read compatibility and are not re-emitted.
func normalizeTelemetry(data map[string]any) map[string]any {
	if data == nil {
		return nil
	}
	out := make(map[string]any, len(data)+1)
	for k, v := range data {
		out[k] = v
	}
	out["schema_version"] = telemetrySchemaVersion
	if _, ok := out["cycle_id"]; !ok {
		if v, exists := out["cycle"]; exists {
			out["cycle_id"] = v
		}
	}
	if _, ok := out["broken_bag_detected_us"]; !ok {
		if v, exists := out["broken_detected_us"]; exists {
			out["broken_bag_detected_us"] = v
		}
	}
	if _, ok := out["commanded_do"]; !ok {
		if v, exists := out["do"]; exists {
			out["commanded_do"] = v
		}
	}
	if _, ok := out["desired_do"]; !ok {
		if v, exists := out["do"]; exists {
			out["desired_do"] = v
		}
	}
	delete(out, "cycle")
	delete(out, "broken_detected_us")
	delete(out, "do")
	return out
}

func (s *Store) Update(next Snapshot) {
	next.Data = normalizeTelemetry(next.Data)
	s.mu.Lock()
	prev, hadPrev := s.latest[next.SpoutID]
	s.latest[next.SpoutID] = next

	if hadPrev {
		if prev.Online != next.Online {
			s.appendEventLocked(Event{SpoutID: next.SpoutID, At: next.ReceivedAt, Kind: "online", From: prev.Online, To: next.Online})
		}
		for _, key := range trackedTelemetryKeys {
			a := trackedValue(prev.Data, key)
			b := trackedValue(next.Data, key)
			if !equalJSONValue(a, b) {
				s.appendEventLocked(Event{SpoutID: next.SpoutID, At: next.ReceivedAt, Kind: key, From: a, To: b})
			}
		}
	} else {
		s.appendEventLocked(Event{SpoutID: next.SpoutID, At: next.ReceivedAt, Kind: "discovered", To: next.Online})
	}

	for ch := range s.listeners {
		select {
		case ch <- struct{}{}:
		default:
		}
	}
	s.mu.Unlock()
}

func (s *Store) appendEventLocked(ev Event) {
	list := append(s.events[ev.SpoutID], ev)
	if len(list) > s.maxEvents {
		list = append([]Event(nil), list[len(list)-s.maxEvents:]...)
	}
	s.events[ev.SpoutID] = list
}

func (s *Store) List() []Snapshot {
	s.mu.RLock()
	out := make([]Snapshot, 0, len(s.latest))
	for _, snap := range s.latest {
		out = append(out, snap)
	}
	s.mu.RUnlock()
	sort.Slice(out, func(i, j int) bool { return out[i].SpoutID < out[j].SpoutID })
	return out
}

func (s *Store) Get(id string) (Snapshot, bool) {
	s.mu.RLock()
	v, ok := s.latest[id]
	s.mu.RUnlock()
	return v, ok
}

func (s *Store) Events(id string, limit int) []Event {
	s.mu.RLock()
	all := s.events[id]
	if limit <= 0 || limit > len(all) {
		limit = len(all)
	}
	out := append([]Event(nil), all[len(all)-limit:]...)
	s.mu.RUnlock()
	return out
}

func (s *Store) Subscribe() (<-chan struct{}, func()) {
	ch := make(chan struct{}, 1)
	s.mu.Lock()
	s.listeners[ch] = struct{}{}
	s.mu.Unlock()
	return ch, func() {
		s.mu.Lock()
		delete(s.listeners, ch)
		close(ch)
		s.mu.Unlock()
	}
}

type App struct {
	store      *Store
	client     *http.Client
	sources    []Source
	pollEvery  time.Duration
	corsOrigin string
}

func parseSources(raw string) ([]Source, error) {
	if strings.TrimSpace(raw) == "" {
		return nil, errors.New("SP01_SPOUTS is empty")
	}
	seen := map[string]bool{}
	var out []Source
	for _, item := range strings.Split(raw, ",") {
		parts := strings.SplitN(strings.TrimSpace(item), "=", 2)
		if len(parts) != 2 {
			return nil, fmt.Errorf("invalid source %q; expected SP01=http://host/api/state", item)
		}
		id := strings.ToUpper(strings.TrimSpace(parts[0]))
		url := strings.TrimSpace(parts[1])
		if id == "" || url == "" || seen[id] {
			return nil, fmt.Errorf("invalid/duplicate source %q", item)
		}
		if !strings.HasPrefix(url, "http://") && !strings.HasPrefix(url, "https://") {
			return nil, fmt.Errorf("source %s must use http:// or https://", id)
		}
		seen[id] = true
		out = append(out, Source{ID: id, URL: url})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].ID < out[j].ID })
	return out, nil
}

func envDurationMS(name string, fallback int) time.Duration {
	raw := strings.TrimSpace(os.Getenv(name))
	if raw == "" {
		return time.Duration(fallback) * time.Millisecond
	}
	v, err := strconv.Atoi(raw)
	if err != nil || v < 20 {
		log.Fatalf("%s must be integer >= 20 ms", name)
	}
	return time.Duration(v) * time.Millisecond
}

func (a *App) pollOne(ctx context.Context, src Source) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, src.URL, nil)
	if err != nil {
		return
	}
	req.Header.Set("Accept", "application/json")
	resp, err := a.client.Do(req)
	now := time.Now().UTC()
	if err != nil {
		a.store.Update(Snapshot{SpoutID: src.ID, ReceivedAt: now, Online: false, Error: err.Error()})
		return
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		a.store.Update(Snapshot{SpoutID: src.ID, ReceivedAt: now, Online: false, Error: resp.Status})
		return
	}
	var data map[string]any
	dec := json.NewDecoder(http.MaxBytesReader(nil, resp.Body, 64<<10))
	if err := dec.Decode(&data); err != nil {
		a.store.Update(Snapshot{SpoutID: src.ID, ReceivedAt: now, Online: false, Error: "invalid JSON: " + err.Error()})
		return
	}
	a.store.Update(Snapshot{SpoutID: src.ID, ReceivedAt: now, Online: true, Data: data})
}

func (a *App) pollLoop(ctx context.Context) {
	poll := func() {
		var wg sync.WaitGroup
		for _, src := range a.sources {
			src := src
			wg.Add(1)
			go func() {
				defer wg.Done()
				a.pollOne(ctx, src)
			}()
		}
		wg.Wait()
	}
	poll()
	t := time.NewTicker(a.pollEvery)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-t.C:
			poll()
		}
	}
}

func (a *App) headers(w http.ResponseWriter) {
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	if a.corsOrigin != "" {
		w.Header().Set("Access-Control-Allow-Origin", a.corsOrigin)
		w.Header().Set("Vary", "Origin")
	}
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}

func (a *App) handleHealth(w http.ResponseWriter, _ *http.Request) {
	a.headers(w)
	writeJSON(w, http.StatusOK, map[string]any{"ok": true, "spouts": len(a.sources), "schema_version": telemetrySchemaVersion, "time": time.Now().UTC()})
}

func (a *App) handleSpouts(w http.ResponseWriter, r *http.Request) {
	a.headers(w)
	path := strings.TrimPrefix(r.URL.Path, "/api/v1/spouts")
	path = strings.Trim(path, "/")
	if path == "" {
		writeJSON(w, http.StatusOK, a.store.List())
		return
	}
	parts := strings.Split(path, "/")
	id := strings.ToUpper(parts[0])
	if len(parts) == 2 && parts[1] == "events" {
		limit, _ := strconv.Atoi(r.URL.Query().Get("limit"))
		if limit <= 0 {
			limit = 200
		}
		if limit > 1000 {
			limit = 1000
		}
		writeJSON(w, http.StatusOK, a.store.Events(id, limit))
		return
	}
	if len(parts) != 1 {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "not found"})
		return
	}
	snap, ok := a.store.Get(id)
	if !ok {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "unknown spout"})
		return
	}
	writeJSON(w, http.StatusOK, snap)
}

func (a *App) handleLive(w http.ResponseWriter, r *http.Request) {
	a.headers(w)
	flusher, ok := w.(http.Flusher)
	if !ok {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "stream unsupported"})
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("X-Accel-Buffering", "no")

	changes, cancel := a.store.Subscribe()
	defer cancel()
	keepalive := time.NewTicker(15 * time.Second)
	defer keepalive.Stop()

	send := func() bool {
		payload, err := json.Marshal(a.store.List())
		if err != nil {
			return false
		}
		if _, err := fmt.Fprintf(w, "event: snapshot\ndata: %s\n\n", payload); err != nil {
			return false
		}
		flusher.Flush()
		return true
	}
	if !send() {
		return
	}
	for {
		select {
		case <-r.Context().Done():
			return
		case <-changes:
			if !send() {
				return
			}
		case <-keepalive.C:
			if _, err := fmt.Fprint(w, ": keepalive\n\n"); err != nil {
				return
			}
			flusher.Flush()
		}
	}
}

func (a *App) routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", a.handleHealth)
	mux.HandleFunc("GET /api/v1/spouts", a.handleSpouts)
	mux.HandleFunc("GET /api/v1/spouts/", a.handleSpouts)
	mux.HandleFunc("GET /api/v1/live", a.handleLive)
	return mux
}

func main() {
	sources, err := parseSources(os.Getenv("SP01_SPOUTS"))
	if err != nil {
		log.Fatal(err)
	}
	listen := os.Getenv("LISTEN_ADDR")
	if listen == "" {
		listen = ":8080"
	}
	pollEvery := envDurationMS("POLL_MS", 250)
	app := &App{
		store: NewStore(2000),
		client: &http.Client{
			Timeout: 2 * time.Second,
		},
		sources:    sources,
		pollEvery:  pollEvery,
		corsOrigin: strings.TrimSpace(os.Getenv("CORS_ORIGIN")),
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go app.pollLoop(ctx)

	server := &http.Server{
		Addr:              listen,
		Handler:           app.routes(),
		ReadHeaderTimeout: 5 * time.Second,
		IdleTimeout:       60 * time.Second,
	}
	log.Printf("SP01 collector listen=%s spouts=%d poll=%s schema=%d read-only=true", listen, len(sources), pollEvery, telemetrySchemaVersion)
	log.Fatal(server.ListenAndServe())
}
