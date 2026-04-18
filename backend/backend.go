package main

/*
#include <stdlib.h>
#include <string.h>

// Forward declarations for C callbacks
typedef void (*lspDiagnosticCallback)(const char* uri, const char* jsonDiagnostics);
typedef void (*lspHoverCallback)(const char* uri, int line, int character, const char* contents);
typedef void (*lspLogCallback)(const char* message);

// Global callback pointers - marked as extern so they're defined in lspclient.cpp
extern lspDiagnosticCallback g_diagnosticCallback;
extern lspHoverCallback g_hoverCallback;
extern lspLogCallback g_logCallback;

// C helper functions for callbacks - declared here, defined in lspclient.cpp
extern void setDiagnosticCallback(lspDiagnosticCallback cb);
extern void setHoverCallback(lspHoverCallback cb);
extern void setLogCallback(lspLogCallback cb);
extern void callDiagnosticCallback(const char* uri, const char* jsonDiagnostics);
extern void callHoverCallback(const char* uri, int line, int character, const char* contents);
extern void callLogCallback(const char* message);
*/
import "C"

import (
	"encoding/json"
	"fmt"
	"image"
	_ "image/gif"
	"image/jpeg"
	_ "image/png"
	"log"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"unsafe"

	"github.com/akhenakh/qhugo/backend/lsp"
	"golang.org/x/image/draw"
)

// Config represents the qhugo configuration file
type Config struct {
	Sites   []string `json:"sites"`
	Current string   `json:"current"`
}

// getConfigDir returns the path to the qhugo config directory
func getConfigDir() string {
	home, err := os.UserHomeDir()
	if err != nil {
		return ""
	}
	return filepath.Join(home, ".config", "qhugo")
}

// getConfigPath returns the full path to the config file
func getConfigPath() string {
	return filepath.Join(getConfigDir(), "config.json")
}

// ensureConfigDir creates the config directory if it doesn't exist
func ensureConfigDir() error {
	configDir := getConfigDir()
	return os.MkdirAll(configDir, 0755)
}

// LoadConfig loads the configuration from the config file
func LoadConfig() (*Config, error) {
	configPath := getConfigPath()

	// If config file doesn't exist, return empty config
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		return &Config{
			Sites:   []string{},
			Current: "",
		}, nil
	}

	data, err := os.ReadFile(configPath)
	if err != nil {
		return nil, err
	}

	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		return nil, err
	}

	return &config, nil
}

// SaveConfig saves the configuration to the config file
func SaveConfig(config *Config) error {
	if err := ensureConfigDir(); err != nil {
		return err
	}

	configPath := getConfigPath()
	data, err := json.MarshalIndent(config, "", "  ")
	if err != nil {
		return err
	}

	return os.WriteFile(configPath, data, 0644)
}

//export GetConfigDir
func GetConfigDir() *C.char {
	return C.CString(getConfigDir())
}

//export LoadConfigCurrent
func LoadConfigCurrent() *C.char {
	config, err := LoadConfig()
	if err != nil {
		return C.CString("")
	}
	return C.CString(config.Current)
}

//export LoadConfigSites
func LoadConfigSites() *C.char {
	config, err := LoadConfig()
	if err != nil {
		return C.CString("")
	}

	data, err := json.Marshal(config.Sites)
	if err != nil {
		return C.CString("")
	}
	return C.CString(string(data))
}

//export AddSiteAndSetCurrent
func AddSiteAndSetCurrent(sitePath *C.char) int {
	site := C.GoString(sitePath)

	config, err := LoadConfig()
	if err != nil {
		config = &Config{
			Sites:   []string{},
			Current: "",
		}
	}

	// Check if site already exists in the list
	exists := false
	for _, s := range config.Sites {
		if s == site {
			exists = true
			break
		}
	}

	// Add to sites list if not exists
	if !exists {
		config.Sites = append(config.Sites, site)
	}

	// Set as current
	config.Current = site

	if err := SaveConfig(config); err != nil {
		return 0
	}
	return 1
}

var hugoCmd *exec.Cmd

//export InitBackend
func InitBackend() {
}

//export ReadFileContent
func ReadFileContent(path *C.char) *C.char {
	b, err := os.ReadFile(C.GoString(path))
	if err != nil {
		return C.CString("")
	}
	return C.CString(string(b))
}

//export SaveFileContent
func SaveFileContent(path *C.char, content *C.char) int {
	if err := atomicWriteFile(C.GoString(path), []byte(C.GoString(content)), 0644); err != nil {
		log.Println(err)
		return 0
	}
	return 1
}

// atomicWriteFile writes data to a sibling temp file and renames it into
// place, so a crash mid-save can't truncate the user's post.
func atomicWriteFile(path string, data []byte, perm os.FileMode) error {
	dir := filepath.Dir(path)
	tmp, err := os.CreateTemp(dir, ".qhugo-save-*")
	if err != nil {
		return err
	}
	tmpName := tmp.Name()
	cleanup := func() { os.Remove(tmpName) }

	if _, err := tmp.Write(data); err != nil {
		tmp.Close()
		cleanup()
		return err
	}
	if err := tmp.Chmod(perm); err != nil {
		tmp.Close()
		cleanup()
		return err
	}
	if err := tmp.Sync(); err != nil {
		tmp.Close()
		cleanup()
		return err
	}
	if err := tmp.Close(); err != nil {
		cleanup()
		return err
	}
	if err := os.Rename(tmpName, path); err != nil {
		cleanup()
		return err
	}
	return nil
}

//export FreeString
func FreeString(str *C.char) {
	C.free(unsafe.Pointer(str))
}

func getFreePort() (int, error) {
	addr, err := net.ResolveTCPAddr("tcp", "localhost:0")
	if err != nil {
		return 0, err
	}
	l, err := net.ListenTCP("tcp", addr)
	if err != nil {
		return 0, err
	}
	defer l.Close() // Closes the listener, freeing the port for Hugo
	return l.Addr().(*net.TCPAddr).Port, nil
}

//export StartHugo
func StartHugo(repoC *C.char) int {
	repo := C.GoString(repoC)
	if hugoCmd != nil && hugoCmd.Process != nil {
		log.Println("Killing existing Hugo server")
		hugoCmd.Process.Kill()
		hugoCmd = nil
	}

	port, err := getFreePort()
	if err != nil {
		log.Println("Error finding free port, falling back to 1313:", err)
		port = 1313
	}

	log.Printf("Starting Hugo server in %s on port %d", repo, port)

	// Launch hugo in background with live reload enabled (default)
	// --bind 127.0.0.1 ensures it binds to localhost
	// -D includes drafts
	hugoCmd = exec.Command("hugo", "server",
		"-s", repo,
		"-p", fmt.Sprintf("%d", port),
		"-D",
		"--bind", "127.0.0.1",
		"--noHTTPCache", // Prevent HTTP caching issues
	)

	// Capture output for debugging
	hugoCmd.Stdout = os.Stdout
	hugoCmd.Stderr = os.Stderr

	if err := hugoCmd.Start(); err != nil {
		log.Println("Error starting Hugo:", err)
		return 0
	}

	log.Printf("Hugo server started with PID %d on port %d", hugoCmd.Process.Pid, port)
	return port
}

//export StopHugo
func StopHugo() {
	if hugoCmd != nil && hugoCmd.Process != nil {
		hugoCmd.Process.Kill()
		hugoCmd = nil
	}
}

// CatmullRom resize operation to scale down to blog-acceptable size
func resizeImage(srcPath, dstPath string) error {
	file, err := os.Open(srcPath)
	if err != nil {
		return err
	}
	defer file.Close()

	img, _, err := image.Decode(file)
	if err != nil {
		return err
	}

	bounds := img.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()

	if width > 1200 {
		ratio := float64(height) / float64(width)
		newWidth := 1200
		newHeight := int(float64(newWidth) * ratio)

		dst := image.NewRGBA(image.Rect(0, 0, newWidth, newHeight))
		draw.CatmullRom.Scale(dst, dst.Bounds(), img, bounds, draw.Over, nil)
		img = dst
	}

	out, err := os.Create(dstPath)
	if err != nil {
		return err
	}
	defer out.Close()

	return jpeg.Encode(out, img, &jpeg.Options{Quality: 85})
}

//export ProcessImage
func ProcessImage(srcC, repoC, docC *C.char) *C.char {
	src := C.GoString(srcC)
	repo := C.GoString(repoC)
	doc := C.GoString(docC)

	docName := filepath.Base(doc)
	docPrefix := strings.TrimSuffix(docName, filepath.Ext(docName))

	imgDir := filepath.Join(repo, "static", "img")
	os.MkdirAll(imgDir, 0755)

	srcName := filepath.Base(src)
	dstName := fmt.Sprintf("%s-%s", docPrefix, srcName)
	dstName = strings.TrimSuffix(dstName, filepath.Ext(dstName)) + ".jpg"

	dstPath := filepath.Join(imgDir, dstName)

	err := resizeImage(src, dstPath)
	if err != nil {
		return C.CString(fmt.Sprintf("Error: %v", err))
	}

	markdownLink := fmt.Sprintf("![%s](/img/%s)", srcName, dstName)
	return C.CString(markdownLink)
}

//export GetHugoURL
func GetHugoURL(filePathC, repoPathC *C.char) *C.char {
	filePath := C.GoString(filePathC)
	repoPath := C.GoString(repoPathC)

	// Get the relative path from the repo root
	relPath, err := filepath.Rel(repoPath, filePath)
	if err != nil {
		return C.CString("")
	}

	// Hugo content is typically in /content/ directory
	// URL mapping: /content/section/page.md -> /section/page/
	// or for posts: /content/post/YYYY/post-name.md -> /post/YYYY/post-name/

	// Check if it's in the content directory
	contentPrefix := "content"
	if strings.HasPrefix(relPath, contentPrefix+string(filepath.Separator)) {
		// Remove the "content/" prefix
		urlPath := relPath[len(contentPrefix)+1:]

		// Remove .md extension
		urlPath = strings.TrimSuffix(urlPath, ".md")

		// Replace backslashes with forward slashes for URL
		urlPath = strings.ReplaceAll(urlPath, "\\", "/")

		// Hugo URLs typically don't end with /index, remove that suffix
		urlPath = strings.TrimSuffix(urlPath, "/index")

		return C.CString("/" + urlPath + "/")
	}

	// Not in content directory, return empty
	return C.CString("")
}

// =============================================================================
// LSP Client Integration
// =============================================================================

var lspManager *lsp.Manager

// getLSPConfigPath returns the path to LSP configuration
func getLSPConfigPath() string {
	return filepath.Join(getConfigDir(), "lsp.json")
}

//export LSPSetCallbacks
func LSPSetCallbacks(diagnosticCb, hoverCb, logCb unsafe.Pointer) {
	C.setDiagnosticCallback((C.lspDiagnosticCallback)(diagnosticCb))
	C.setHoverCallback((C.lspHoverCallback)(hoverCb))
	C.setLogCallback((C.lspLogCallback)(logCb))
}

// handleDiagnostics forwards diagnostics to C++
func handleDiagnostics(uri string, diagnostics []lsp.Diagnostic) {
	log.Printf("[Backend] handleDiagnostics called for %s with %d diagnostics", uri, len(diagnostics))
	// Convert diagnostics to JSON
	type diagnosticJSON struct {
		Line      int    `json:"line"`
		Character int    `json:"character"`
		EndLine   int    `json:"endLine"`
		EndChar   int    `json:"endCharacter"`
		Severity  int    `json:"severity"`
		Code      string `json:"code,omitempty"`
		Source    string `json:"source,omitempty"`
		Message   string `json:"message"`
	}

	jsonDiags := make([]diagnosticJSON, len(diagnostics))
	for i, d := range diagnostics {
		jsonDiags[i] = diagnosticJSON{
			Line:      d.Range.Start.Line,
			Character: d.Range.Start.Character,
			EndLine:   d.Range.End.Line,
			EndChar:   d.Range.End.Character,
			Severity:  d.Severity,
			Source:    d.Source,
			Message:   d.Message,
		}
		if d.Code != nil {
			jsonDiags[i].Code = fmt.Sprintf("%v", d.Code)
		}
	}

	data, _ := json.Marshal(jsonDiags)
	uriC := C.CString(uri)
	dataC := C.CString(string(data))
	defer C.free(unsafe.Pointer(uriC))
	defer C.free(unsafe.Pointer(dataC))

	log.Printf("[Backend] Calling C.callDiagnosticCallback with %d diagnostics", len(jsonDiags))
	C.callDiagnosticCallback(uriC, dataC)
	log.Printf("[Backend] C.callDiagnosticCallback completed")
}

// handleHover forwards hover results to C++
func handleHover(uri string, line, char int, hover *lsp.Hover) {
	uriC := C.CString(uri)
	contentsC := C.CString(hover.Contents.Value)
	defer C.free(unsafe.Pointer(uriC))
	defer C.free(unsafe.Pointer(contentsC))

	C.callHoverCallback(uriC, C.int(line), C.int(char), contentsC)
}

// handleLog forwards log messages to C++
func handleLog(uri string, diagnostics []lsp.Diagnostic) {
	msgC := C.CString(uri)
	defer C.free(unsafe.Pointer(msgC))
	C.callLogCallback(msgC)
}

//export LSPInitialize
func LSPInitialize() int {
	if lspManager != nil {
		return 1 // already initialized
	}

	configPath := getLSPConfigPath()
	log.Printf("[Backend] LSPInitialize called, config path: %s", configPath)

	lspManager = lsp.NewManager(configPath, handleDiagnostics, handleLog, handleHover)

	if err := lspManager.LoadConfig(); err != nil {
		log.Printf("[Backend] Failed to load LSP config: %v", err)
		// Continue with default config
	} else {
		log.Printf("[Backend] LSP config loaded successfully")
	}

	log.Printf("[Backend] LSP enabled: %v", lspManager.IsEnabled())

	return 1
}

//export LSPCleanup
func LSPCleanup() {
	if lspManager != nil {
		lspManager.StopClients()
		lspManager = nil
	}
}

//export LSPSetWorkspaceRoot
func LSPSetWorkspaceRoot(rootC *C.char) {
	if lspManager == nil {
		return
	}
	root := C.GoString(rootC)
	lspManager.SetWorkspaceRoot(root)
}

//export LSPDocumentOpened
func LSPDocumentOpened(uriC, languageIDC, contentC *C.char) {
	if lspManager == nil {
		return
	}

	uri := C.GoString(uriC)
	languageID := C.GoString(languageIDC)
	content := C.GoString(contentC)

	lspManager.DocumentOpened(uri, languageID, content)
}

//export LSPDocumentChanged
func LSPDocumentChanged(uriC, contentC *C.char) {
	if lspManager == nil {
		log.Printf("[Backend] LSPDocumentChanged: lspManager is nil!")
		return
	}

	uri := C.GoString(uriC)
	content := C.GoString(contentC)
	log.Printf("[Backend] LSPDocumentChanged called for %s (content length: %d)", uri, len(content))

	lspManager.DocumentChanged(uri, content)
}

//export LSPDocumentClosed
func LSPDocumentClosed(uriC *C.char) {
	if lspManager == nil {
		return
	}

	uri := C.GoString(uriC)
	lspManager.DocumentClosed(uri)
}

//export LSPRequestHover
func LSPRequestHover(uriC *C.char, line, character int) {
	if lspManager == nil {
		return
	}

	uri := C.GoString(uriC)
	lspManager.Hover(uri, line, character)
}

//export LSPIsEnabled
func LSPIsEnabled() int {
	if lspManager == nil {
		return 0
	}

	if lspManager.IsEnabled() {
		return 1
	}
	return 0
}

//export LSPSetEnabled
func LSPSetEnabled(enabled int) int {
	if lspManager == nil {
		return 0
	}

	if err := lspManager.SetEnabled(enabled == 1); err != nil {
		log.Printf("Failed to set LSP enabled: %v", err)
		return 0
	}
	return 1
}

//export LSPStartClients
func LSPStartClients() int {
	if lspManager == nil {
		return 0
	}

	if err := lspManager.StartClients(); err != nil {
		log.Printf("Failed to start LSP clients: %v", err)
		return 0
	}
	return 1
}

//export LSPStopClients
func LSPStopClients() {
	if lspManager == nil {
		return
	}

	lspManager.StopClients()
}

//export LSPGetServers
func LSPGetServers() *C.char {
	if lspManager == nil {
		return C.CString("[]")
	}

	servers := lspManager.GetServers()
	data, err := json.Marshal(servers)
	if err != nil {
		return C.CString("[]")
	}
	return C.CString(string(data))
}

//export LSPAddServer
func LSPAddServer(jsonConfigC *C.char) int {
	if lspManager == nil {
		return 0
	}

	jsonConfig := C.GoString(jsonConfigC)

	var server lsp.ServerConfig
	if err := json.Unmarshal([]byte(jsonConfig), &server); err != nil {
		log.Printf("Failed to parse server config: %v", err)
		return 0
	}

	if err := lspManager.AddServer(server); err != nil {
		log.Printf("Failed to add server: %v", err)
		return 0
	}
	return 1
}

//export LSPRemoveServer
func LSPRemoveServer(nameC *C.char) int {
	if lspManager == nil {
		return 0
	}

	name := C.GoString(nameC)

	if err := lspManager.RemoveServer(name); err != nil {
		log.Printf("Failed to remove server: %v", err)
		return 0
	}
	return 1
}

//export LSPSetServerEnabled
func LSPSetServerEnabled(nameC *C.char, enabled int) int {
	if lspManager == nil {
		return 0
	}

	name := C.GoString(nameC)
	servers := lspManager.GetServers()

	for i, s := range servers {
		if s.Name == name {
			servers[i].Enabled = (enabled == 1)
			// Need to update via manager's config
			break
		}
	}

	return 1
}

func main() {}
