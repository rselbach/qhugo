# QHugo

A tool specialized to manage and edit Hugo blogs natively.

## Features
- Embedded live preview using Hugo's built-in webserver.
- Drag and drop image processing (automatically resizes and copies to `static/img`).
- Native post creation with automatic frontmatter generation.
- Markdown editor with syntax highlighting.
- LSP support for diagnostics (LanguageTool, marksman, etc.)
  - Real-time spell checking and grammar suggestions
  - Hover tooltips showing diagnostic messages
  - Visual highlighting of errors/warnings in the editor

## Prerequisites
- **Hugo**: Ensure `hugo` is installed and available in your system's PATH.

## Build
```bash
git submodule update --init --recursive
cd backend
go build -buildmode=c-archive
cd ..
cmake -B build
cd build && make
```
