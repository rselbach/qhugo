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
cmake -S . -B build  && cmake --build build -- -j 10 
```

## LICENSE
MIT

the highlighter is using code from https://github.com/pbek/qmarkdowntextedit
```
The MIT License (MIT)
Copyright (c) 2014-2026 Patrizio Bekerle -- <patrizio@bekerle.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  
```

## TODO

https://doc.qt.io/qt-6/qtwidgets-richtext-syntaxhighlighter-example.html

Full git support



## Doc

using shortcodes/img.html

```
{{/* Usage: {{< img src="hero.jpg" alt="My caption" >}} */}}

{{ $src := .Get "src" }}
{{ $alt := .Get "alt" | default "" }}
{{ $img := .Page.Resources.Get $src }}

{{ if $img }}
  {{ $webp := $img.Resize "800x webp q85" }}
  {{ $jpg  := $img.Resize "800x jpeg q85" }}

  <picture>
    <source type="image/webp" srcset="{{ $webp.RelPermalink }}">
    <img
      src="{{ $jpg.RelPermalink }}"
      width="{{ $jpg.Width }}"
      height="{{ $jpg.Height }}"
      alt="{{ $alt }}"
      loading="lazy">
  </picture>
{{ else }}
  <p style="color:red">Image not found: {{ $src }}</p>
{{ end }}
```
