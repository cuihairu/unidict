# Unidict Development Roadmap

This document outlines the detailed development plan for Unidict. It is organized by modules and features.

##  MVP (Minimum Viable Product) Priorities
1.  **Core Lookup Functionality** - Support for 2-3 major dictionary formats (e.g., MDict, StarDict).
2.  **Basic UI** - A clean, modern, and fast interface for search and display.
3.  **Vocabulary Book** - Basic functions for saving and reviewing words.
4.  **Cross-Platform Support** - Initial support for Windows, macOS, and Linux.
5.  **Offline First** - Core features must work without an internet connection.

---

## 🏗️ Core Architecture
- [x] Cross-platform framework selection and setup (C++/Qt)
- [x] Database schema design (JSON-based DataStore MVP)
- [ ] Sync engine design (cloud-grade: accounts, incremental sync, E2EE)
- [x] Plugin system architecture (extension -> parser factory registry)
- [ ] Entry rendering pipeline (HTML/CSS subset, link handling, asset resolving)
- [ ] Text normalization strategy (Unicode/case/diacritics folding, configurable)

## 📚 Dictionary Support & Management
- [ ] **Multi-format Dictionary Support**
  - [x] StarDict (.ifo/.idx/.dict/.dict.dz)
  - [ ] MDict (.mdx/.mdd) (⚠️ .mdd resources not implemented; real-world compatibility ongoing)
  - [x] DSL
  - [ ] EPUB (minimal: zip container + OPF + heading-based entry extraction; real-world layout compatibility ongoing)
  - [x] Custom JSON format
  - [x] CSV/TSV/plain-text (simple custom formats)
- [ ] **Dictionary Management**
  - [ ] Online dictionary store browser/downloader
  - [x] Local dictionary import (paths/env var + scan-dir)
  - [ ] Local dictionary export/packaging
  - [x] Dictionary priority settings (UI + persisted ordering)
  - [x] Dictionary grouping/profiles (e.g., EN-EN vs EN-ZH)
  - [x] Corrupted dictionary detection (load-failure diagnostics + quarantine:
        parse failures quarantined until explicit retry, missing files
        self-heal; GUI 词典管理 ⚠ 行 + 重试/移除, CLI --list [FAILED])

## 🔍 Lookup Features
- [ ] **Basic Search**
  - [x] Exact match
  - [x] Fuzzy search
  - [x] Full-text search (inverted index + TF/IDF + persistence)
  - [x] Wildcard search
  - [x] Regex search
- [ ] **Quick Access**
  - [ ] In-text lookup (mouseselect/hotkey)
  - [x] Clipboard listener (Qt Widgets GUI toolbar toggle; ClipboardMonitor
        polling + filtering, auto-raises window and fills the query)
  - [x] Global hotkey (Windows: RegisterHotKey + WM_HOTKEY, Ctrl+Alt+U raises
        the window; Linux/macOS remain stubs — revisit on demand)
  - [ ] Mouse hover lookup
- [ ] **Visual Lookup**
  - [ ] OCR from screenshot/camera
  - [ ] PDF word extraction

## 🎯 Smart Features
- [ ] **AI Integration**
  - [ ] LLM integration (provider auth, streaming, caching, prompts)
  - [x] AI-powered translation (via external command bridge)
  - [x] AI-powered grammar check & polish (via external command bridge)
  - [ ] AI-powered contextual sentence generation
- [ ] **Voice Features**
  - [x] TTS pronunciation (Qt TextToSpeech; voice selection/presets)
  - [ ] Voice search
  - [ ] Pronunciation practice & scoring

## 📖 Learning Features
- [ ] **Vocabulary Management**
  - [ ] Vocabulary book (with tagging/grouping)
  - [x] Search history
  - [x] Vocabulary book (basic CRUD + export CSV)
  - [ ] Learning progress tracking (analytics, streaks, goals)
  - [x] Forgetting curve algorithm (basic scheduled reviews)
- [ ] **Memory System**
  - [x] Anki-style flashcard review (basic)
  - [ ] Customizable review schedules (user-configurable algorithms)
  - [ ] Learning statistics and visualizations
  - [ ] Achievement/gamification system
- [ ] **Note-Taking System**
  - [ ] Add notes to dictionary entries
  - [ ] Markdown support
  - [ ] Export notes (PDF/HTML)
  - [ ] Search within notes

## 🔄 Translation Features
- [ ] **Online Translation Engines**
  - [ ] Google Translate
  - [ ] DeepL
  - [ ] Custom API integration
- [ ] **Document Translation**
  - [ ] PDF/Word translation (preserving format)
  - [ ] Bilingual side-by-side view

## 🎨 User Interface
- [ ] **Main Interface**
  - [x] Modern UI/UX design (QML)
  - [x] Light/Dark themes (basic)
  - [ ] Custom theme/color support
  - [ ] Font and layout customization
- [ ] **Interaction**
  - [x] Fast, responsive search
  - [ ] Keyboard shortcut mastery
  - [ ] Gesture support (mobile)

## 📱 Platform Specifics
- [ ] **Desktop (Windows, macOS, Linux)**
  - [ ] System tray / Menu bar integration
  - [ ] Startup on login
  - [ ] Native notifications
- [ ] **Mobile (Android, iOS)**
  - [ ] Floating lookup widget
  - [ ] Share menu integration
  - [ ] Homescreen widgets

## 🔧 Advanced Features
- [ ] **Data Sync**
  - [ ] Multi-device sync (cloud)
  - [ ] Incremental sync algorithm
  - [x] Conflict preview & merge (file-based sync MVP)
- [ ] **Plugin System**
  - [ ] Third-party plugin support (dynamic loading + API/ABI versioning)
  - [ ] JavaScript/QML plugin engine
  - [ ] Plugin store/repository
  - [x] Developer API (parser factory registration; built-ins)
- [ ] **Import/Export**
  - [ ] Anki deck export (apkg/AnkiConnect)
  - [x] CSV export
  - [ ] Full data backup and restore

## 🔒 Privacy & Security
- [ ] **Data Protection**
  - [ ] Local data encryption
  - [ ] Privacy mode
  - [ ] Secure data wipe

---
## 📈 Phased Rollout Plan
- **v1.1**: Introduce OCR and voice features. (voice MVP done; OCR pending)
- **v1.2**: Integrate AI translation and writing assistance. (AI bridge MVP done)
- **v1.3**: Refine sync engine and launch plugin system.
- **v2.0**: Introduce community features and advanced learning analytics.
