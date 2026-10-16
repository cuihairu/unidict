> Language: **English** | [简体中文](./faq.md)

# FAQ

## Installation

### Windows double-click is blocked by SmartScreen

Daily builds are not code-signed: click "More info → Run anyway". The same applies to the installer version.

### macOS won't open ("damaged" or Gatekeeper block)

Unsigned apps must have the quarantine attribute cleared first: `xattr -cr unidict_qml.app && open unidict_qml.app`. Packages downloaded from a browser usually carry the quarantine flag, so this step cannot be skipped.

### Why is there only a CLI package for Linux?

Without bundled dependencies the Qt desktop app will not run on other machines (glibc/Qt version hell) — so the daily build ships only the Qt-free CLI (zip/deb/rpm); build the desktop GUI yourself [from source](/quickstart#源码构建). The deb/rpm system packages are built on an ubuntu-24.04 runner (glibc 2.39+); on older distros, build from source.

### The one-click install script reports "unsupported CPU architecture"

x86_64 and arm64 (aarch64) are supported. Intel Macs are not in the daily build matrix (Qt 6.10 stopped distributing Intel macOS libraries); for other architectures, open a request in the repository issues and include your `uname -m` output.

## Dictionaries

### How to supply the password for an encrypted .mdx

The `--mdict-password <pw>` argument or the `UNIDICT_MDICT_PASSWORD` environment variable.

### Where dictionaries go and how to use several at once

Any path you like: point `UNIDICT_DICTS` at files (join multiple paths with the path separator) or `UNIDICT_DICT_DIR` at a directory; after importing in the desktop GUI, order and enable/disable state are remembered automatically. See [Dictionary Management](/dictionaries).

### Can Chinese dictionaries (like the Xinhua Dictionary) be installed?

Yes — `.mdx`, the dominant format for Chinese reference works, installs directly. Chinese-specific surfaces such as pinyin lookup and radical/stroke search are on the [roadmap](/server#里程碑).

## Distribution and Privacy

### Artifacts are not visible in the GitHub App

The GitHub **App** does not show Actions artifacts; open the [nightly Release](https://github.com/cuihairu/unidict/releases/tag/nightly) in a mobile browser (reachable anonymously), or use this site's [download page](/download).

### What version is the nightly Release?

It is not a version number — it is a rolling daily build (a fixed tag `nightly`: every run clears the old assets and uploads new ones). Official releases are on the roadmap.

### Privacy and Data

Outbound data is off by default, opt-in, and states its scope up front when enabled: local TTS, lookups, the vocabulary book, and history make no network requests; [online pronunciation](/pronunciation) sends only the query word to dictionaryapi.dev, is off by default, and states so right in the settings page when enabled — no history, no vocabulary book; the vocabulary book and lookup history never go to the cloud. CSV export lets you take your data away at any time.
