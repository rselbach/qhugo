package lsp

import (
	"encoding/json"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"
)

// Manager manages LSP clients and document synchronization
type Manager struct {
	clients    map[string]*Client // key: server name
	clientsMu  sync.RWMutex
	
	documents  map[string]*DocumentInfo // key: document URI
	docMu      sync.RWMutex
	
	config     *Config
	configPath string
	workspaceRoot string
	
	// Debounce timers for document changes
	debounceTimers map[string]*time.Timer
	debounceMu     sync.Mutex
	debounceDelay  time.Duration
	
	// Callback for UI updates
	onDiagnostics func(uri string, diagnostics []Diagnostic)
	onLog         func(message string)
	onHover       func(uri string, line, char int, hover *Hover)
}

// DocumentInfo tracks document state across all LSP clients
type DocumentInfo struct {
	URI        string
	LanguageID string
	Content    string
	Version    int
	Open       bool
}

// ServerConfig configuration for a single LSP server
type ServerConfig struct {
	Name        string            `json:"name"`
	Command     string            `json:"command"`
	Args        []string          `json:"args,omitempty"`
	Environment map[string]string `json:"env,omitempty"`
	Languages   []string          `json:"languages,omitempty"` // file extensions or language IDs
	Enabled     bool              `json:"enabled"`
	RootURI     string            `json:"rootUri,omitempty"`     // override workspace root
}

// Config LSP configuration
type Config struct {
	Enabled  bool            `json:"enabled"`
	Servers  []ServerConfig  `json:"servers"`
	Debounce int             `json:"debounceMs,omitempty"` // debounce delay in ms, default 500
}

// DefaultConfig returns default LSP configuration
func DefaultConfig() *Config {
	return &Config{
		Enabled: false,
		Servers: []ServerConfig{
			{
				Name:      "languagetool",
				Command:   "languagetool-lsp",
				Args:      []string{},
				Languages: []string{"markdown", "md", "txt"},
				Enabled:   false, // disabled by default
			},
			{
				Name:      "marksman",
				Command:   "marksman",
				Args:      []string{},
				Languages: []string{"markdown", "md"},
				Enabled:   false,
			},
		},
		Debounce: 500,
	}
}

// NewManager creates a new LSP manager
func NewManager(configPath string, onDiagnostics, onLog func(uri string, diagnostics []Diagnostic), onHoverFunc func(uri string, line, char int, hover *Hover)) *Manager {
	return &Manager{
		clients:        make(map[string]*Client),
		documents:      make(map[string]*DocumentInfo),
		configPath:     configPath,
		debounceTimers: make(map[string]*time.Timer),
		debounceDelay:  500 * time.Millisecond,
		onDiagnostics:  onDiagnostics,
		onLog:          func(msg string) { onLog("", nil) }, // Adapt log callback
		onHover:        onHoverFunc,
	}
}

// LoadConfig loads LSP configuration from file
func (m *Manager) LoadConfig() error {
	dlog("[LSP Manager] Loading config from: %s", m.configPath)
	data, err := os.ReadFile(m.configPath)
	if err != nil {
		if os.IsNotExist(err) {
			dlog("[LSP Manager] Config file not found, using defaults")
			m.config = DefaultConfig()
			return m.SaveConfig()
		}
		return err
	}

	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		dlog("[LSP Manager] Failed to parse config: %v", err)
		return err
	}

	m.config = &config
	dlog("[LSP Manager] Config loaded: enabled=%v, servers=%d", config.Enabled, len(config.Servers))
	
	// Set debounce delay
	if config.Debounce > 0 {
		m.debounceDelay = time.Duration(config.Debounce) * time.Millisecond
	}

	return nil
}

// SaveConfig saves LSP configuration to file
func (m *Manager) SaveConfig() error {
	if err := os.MkdirAll(filepath.Dir(m.configPath), 0755); err != nil {
		return err
	}

	data, err := json.MarshalIndent(m.config, "", "  ")
	if err != nil {
		return err
	}

	return os.WriteFile(m.configPath, data, 0600)
}

// IsEnabled returns whether LSP is enabled
func (m *Manager) IsEnabled() bool {
	if m.config == nil {
		return false
	}
	return m.config.Enabled
}

// SetEnabled enables/disables LSP
func (m *Manager) SetEnabled(enabled bool) error {
	m.config.Enabled = enabled
	if err := m.SaveConfig(); err != nil {
		return err
	}

	if enabled {
		// StartClients is a no-op until the workspace root is set;
		// SetWorkspaceRoot will call it again once the root arrives.
		return m.StartClients()
	}
	m.StopClients()
	return nil
}

// StartClients starts all enabled LSP clients
func (m *Manager) StartClients() error {
	if m.config == nil || !m.config.Enabled {
		return nil
	}
	
	// Don't start if workspace root is not set
	if m.workspaceRoot == "" {
		dlog("[LSP Manager] Not starting clients - workspace root not set")
		return nil
	}

	for _, server := range m.config.Servers {
		if !server.Enabled {
			continue
		}

		if _, exists := m.clients[server.Name]; exists {
			continue // already running
		}

		clientConfig := ClientConfig{
			Command:     server.Command,
			Args:        server.Args,
			Environment: server.Environment,
			RootURI:     server.RootURI,
		}

		// Use project root if not specified
		if clientConfig.RootURI == "" {
			clientConfig.RootURI = m.getWorkspaceRoot()
		}

		client, err := NewClient(clientConfig, m.handleDiagnostics, m.handleLog)
		if err != nil {
			log.Printf("Failed to start LSP client %s: %v", server.Name, err)
			continue
		}

		// Initialize the client
		if _, err := client.Initialize(clientConfig.RootURI); err != nil {
			log.Printf("Failed to initialize LSP client %s: %v", server.Name, err)
			client.Close()
			continue
		}

		m.clientsMu.Lock()
		m.clients[server.Name] = client
		m.clientsMu.Unlock()

		log.Printf("Started LSP client: %s", server.Name)
	}

	return nil
}

// StopClients stops all LSP clients
func (m *Manager) StopClients() {
	m.clientsMu.Lock()
	defer m.clientsMu.Unlock()

	for name, client := range m.clients {
		if err := client.Close(); err != nil {
			log.Printf("Error closing LSP client %s: %v", name, err)
		}
		delete(m.clients, name)
	}
}

// StopClient stops a specific LSP client
func (m *Manager) StopClient(name string) error {
	m.clientsMu.Lock()
	defer m.clientsMu.Unlock()

	client, exists := m.clients[name]
	if !exists {
		return nil
	}

	if err := client.Close(); err != nil {
		return err
	}

	delete(m.clients, name)
	return nil
}

// DocumentOpened notifies all LSP clients that a document was opened
func (m *Manager) DocumentOpened(uri, languageID, content string) {
	m.docMu.Lock()
	m.documents[uri] = &DocumentInfo{
		URI:        uri,
		LanguageID: languageID,
		Content:    content,
		Version:    1,
		Open:       true,
	}
	m.docMu.Unlock()

	// Notify all applicable clients
	m.clientsMu.RLock()
	clients := make(map[string]*Client)
	for name, client := range m.clients {
		clients[name] = client
	}
	m.clientsMu.RUnlock()

	for _, client := range clients {
		go func(c *Client) {
			if err := c.DidOpen(uri, languageID, content); err != nil {
				log.Printf("Error sending didOpen: %v", err)
			}
		}(client)
	}
}

// DocumentChanged notifies all LSP clients of document changes (debounced)
func (m *Manager) DocumentChanged(uri, content string) {
	dlog("[LSP Manager] DocumentChanged called for %s (content length: %d)", uri, len(content))
	
	// Update document info
	m.docMu.Lock()
	doc, exists := m.documents[uri]
	if !exists {
		doc = &DocumentInfo{URI: uri, Version: 0}
		m.documents[uri] = doc
		dlog("[LSP Manager] Created new document entry for %s", uri)
	}
	doc.Content = content
	doc.Version++
	doc.Open = true
	m.docMu.Unlock()

	// Debounce the notification
	m.debounceMu.Lock()
	if timer, exists := m.debounceTimers[uri]; exists {
		timer.Stop()
		dlog("[LSP Manager] Stopped existing debounce timer for %s", uri)
	}
	
	dlog("[LSP Manager] Starting debounce timer for %s (delay: %v)", uri, m.debounceDelay)
	m.debounceTimers[uri] = time.AfterFunc(m.debounceDelay, func() {
		dlog("[LSP Manager] Debounce timer fired for %s", uri)
		m.sendDocumentChange(uri, content)
		
		m.debounceMu.Lock()
		delete(m.debounceTimers, uri)
		m.debounceMu.Unlock()
	})
	m.debounceMu.Unlock()
}

// sendDocumentChange sends the actual didChange notification
func (m *Manager) sendDocumentChange(uri, content string) {
	dlog("[LSP Manager] sendDocumentChange called for %s", uri)
	m.clientsMu.RLock()
	clientCount := len(m.clients)
	clients := make(map[string]*Client)
	for name, client := range m.clients {
		clients[name] = client
	}
	m.clientsMu.RUnlock()
	dlog("[LSP Manager] Sending didChange to %d clients", clientCount)

	for _, client := range clients {
		go func(c *Client) {
			if err := c.DidChange(uri, content); err != nil {
				log.Printf("Error sending didChange: %v", err)
			}
		}(client)
	}
}

// DocumentClosed notifies all LSP clients that a document was closed
func (m *Manager) DocumentClosed(uri string) {
	m.docMu.Lock()
	delete(m.documents, uri)
	m.docMu.Unlock()

	// Cancel any pending debounce timer
	m.debounceMu.Lock()
	if timer, exists := m.debounceTimers[uri]; exists {
		timer.Stop()
		delete(m.debounceTimers, uri)
	}
	m.debounceMu.Unlock()

	// Notify all clients
	m.clientsMu.RLock()
	clients := make(map[string]*Client)
	for name, client := range m.clients {
		clients[name] = client
	}
	m.clientsMu.RUnlock()

	for _, client := range clients {
		go func(c *Client) {
			if err := c.DidClose(uri); err != nil {
				log.Printf("Error sending didClose: %v", err)
			}
		}(client)
	}
}

// Hover requests hover information from all LSP clients
func (m *Manager) Hover(uri string, line, character int) {
	m.clientsMu.RLock()
	clients := make(map[string]*Client)
	for name, client := range m.clients {
		clients[name] = client
	}
	m.clientsMu.RUnlock()

	for _, client := range clients {
		go func(c *Client) {
			hover, err := c.Hover(uri, line, character)
			if err != nil {
				return
			}
			if hover != nil && m.onHover != nil {
				m.onHover(uri, line, character, hover)
			}
		}(client)
	}
}

// GetDiagnostics returns current diagnostics for a document
func (m *Manager) GetDiagnostics(uri string) []Diagnostic {
	// Diagnostics are received asynchronously via callbacks
	// The UI should maintain its own cache
	return nil
}

// GetServers returns list of configured servers
func (m *Manager) GetServers() []ServerConfig {
	if m.config == nil {
		return nil
	}
	return m.config.Servers
}

// AddServer adds a new LSP server configuration
func (m *Manager) AddServer(server ServerConfig) error {
	m.config.Servers = append(m.config.Servers, server)
	return m.SaveConfig()
}

// RemoveServer removes an LSP server configuration
func (m *Manager) RemoveServer(name string) error {
	// Stop the client if running
	m.StopClient(name)

	// Remove from config
	newServers := make([]ServerConfig, 0, len(m.config.Servers))
	for _, s := range m.config.Servers {
		if s.Name != name {
			newServers = append(newServers, s)
		}
	}
	m.config.Servers = newServers

	return m.SaveConfig()
}

// handleDiagnostics processes diagnostics from LSP servers
func (m *Manager) handleDiagnostics(uri string, diagnostics []Diagnostic) {
	dlog("[LSP Manager] handleDiagnostics called for %s with %d diagnostics", uri, len(diagnostics))
	if m.onDiagnostics != nil {
		dlog("[LSP Manager] Calling onDiagnostics callback")
		m.onDiagnostics(uri, diagnostics)
	} else {
		dlog("[LSP Manager] onDiagnostics callback is nil!")
	}
}

// handleLog processes log messages from LSP servers
func (m *Manager) handleLog(message string) {
	log.Printf("[LSP] %s", message)
}

// getWorkspaceRoot returns the current workspace root URI
func (m *Manager) getWorkspaceRoot() string {
	if m.workspaceRoot != "" {
		return m.workspaceRoot
	}
	// For now, return empty (LSP server will use current directory)
	return ""
}

// SetWorkspaceRoot sets the workspace root for LSP clients
func (m *Manager) SetWorkspaceRoot(root string) {
	dlog("[LSP Manager] SetWorkspaceRoot called: %s", root)
	m.workspaceRoot = root
	
	// If LSP is enabled and no clients are running, start them now
	if m.IsEnabled() && len(m.clients) == 0 {
		dlog("[LSP Manager] Auto-starting clients after workspace root set")
		m.StartClients()
	}
}

// GetDocumentVersion returns the current version of a document
func (m *Manager) GetDocumentVersion(uri string) int {
	m.docMu.RLock()
	defer m.docMu.RUnlock()
	
	if doc, exists := m.documents[uri]; exists {
		return doc.Version
	}
	return 0
}
