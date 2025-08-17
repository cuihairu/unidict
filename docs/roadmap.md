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
- [ ] Cross-platform framework selection and setup (C++/Qt)
- [ ] Database schema design (for user data and dictionary metadata)
- [ ] Sync engine design (for user data cloud sync)
- [ ] Plugin system architecture

## 📚 Dictionary Support & Management
- [ ] **Multi-format Dictionary Support**
  - [ ] StarDict (.dz, .dict, .idx)
  - [ ] MDict (.mdx, .mdd)
  - [ ] DSL
  - [ ] EPUB
  - [ ] Custom JSON/SQLite format
- [ ] **Dictionary Management**
  - [ ] Online dictionary store browser/downloader
  - [ ] Local dictionary import/export
  - [ ] Dictionary priority settings
  - [ ] Dictionary grouping
  - [ ] Corrupted dictionary detection

## 🔍 Lookup Features
- [ ] **Basic Search**
  - [ ] Exact match
  - [ ] Fuzzy search
  - [ ] Full-text search
  - [ ] Wildcard search
  - [ ] Regex search
- [ ] **Quick Access**
  - [ ] In-text lookup (mouseselect/hotkey)
  - [ ] Clipboard listener
  - [ ] Global hotkey
  - [ ] Mouse hover lookup
- [ ] **Visual Lookup**
  - [ ] OCR from screenshot/camera
  - [ ] PDF word extraction

## 🎯 Smart Features
- [ ] **AI Integration**
  - [ ] LLM integration (GPT, Claude, Gemini, etc.)
  - [ ] AI-powered translation
  - [ ] AI-powered grammar check & polish
  - [ ] AI-powered contextual sentence generation
- [ ] **Voice Features**
  - [ ] TTS pronunciation (multiple engines)
  - [ ] Voice search
  - [ ] Pronunciation practice & scoring

## 📖 Learning Features
- [ ] **Vocabulary Management**
  - [ ] Vocabulary book (with tagging/grouping)
  - [ ] Search history
  - [ ] Learning progress tracking
  - [ ] Forgetting curve algorithm (Spaced Repetition)
- [ ] **Memory System**
  - [ ] Anki-style flashcard review
  - [ ] Customizable review schedules
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
  - [ ] Modern UI/UX design
  - [ ] Light/Dark themes
  - [ ] Custom theme/color support
  - [ ] Font and layout customization
- [ ] **Interaction**
  - [ ] Fast, responsive search
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
  - [ ] Multi-device sync
  - [ ] Incremental sync algorithm
  - [ ] Conflict resolution
- [ ] **Plugin System**
  - [ ] Third-party plugin support
  - [ ] JavaScript/QML plugin engine
  - [ ] Plugin store/repository
  - [ ] Developer API
- [ ] **Import/Export**
  - [ ] Anki deck export
  - [ ] CSV export
  - [ ] Full data backup and restore

## 🔒 Privacy & Security
- [ ] **Data Protection**
  - [ ] Local data encryption
  - [ ] Privacy mode
  - [ ] Secure data wipe

---
## 📈 Phased Rollout Plan
- **v1.1**: Introduce OCR and voice features.
- **v1.2**: Integrate AI translation and writing assistance.
- **v1.3**: Refine sync engine and launch plugin system.
- **v2.0**: Introduce community features and advanced learning analytics.
