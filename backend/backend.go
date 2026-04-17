package main

/*
#include <stdlib.h>
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
	err := os.WriteFile(C.GoString(path), []byte(C.GoString(content)), 0644)
	if err != nil {
		log.Println(err)
		return 0
	}
	return 1
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

func main() {}
