> Language: **English** | 简体中文

# Pronunciation

## Local TTS (Shipped)

- **Desktop**: click the speaker icon on lookup results and vocabulary cards to read aloud, with voice/rate/pitch/volume adjustment, automatic pronunciation (read as soon as a match is found), and voice presets; clipboard word capture + global hotkey for hands-free pronunciation
- **Android**: read aloud through the system TextToSpeech engine with one-tap playback on cards; a missing engine or failed initialization never blocks other features (the status line `M4-TTS-READY` / `M4-TTS-FAIL` helps diagnose)

Local synthesis makes no network requests, and nothing about pronunciation is sent anywhere.

## Online "Global Pronunciation" (Shipped)

Real human voices with accent switching. Desktop and Android share one and the same semantics, with three entries in settings:

### Three Pronunciation Source Modes

| Mode | Behavior | On network failure |
| --- | --- | --- |
| **Local voice** (default) | Uses system TTS, makes no network requests | — |
| **Online pronunciation** | Plays online human voices only | Reports status only, never interrupts local features |
| **Automatic (fall back to local)** | Prefers online | Falls back to local TTS automatically |

The default is "Local voice" — going online is an explicit action and never changes on its own.

### Accents

US / UK / Australian, or "Automatic". Selection rule: a preference hit wins, otherwise fall back in the order US → UK → Australian → unlabeled → first entry. The accent is inferred from the `-us` / `-uk` / `-au` marker in the audio file name; when the file name carries no marker, the `American` / `British` / `Australian` annotation in the phonetic text is used.

### API: dictionaryapi.dev

Powered by the [Free Dictionary API](https://dictionaryapi.dev/) (dictionaryapi.dev) — no API key, no call quota:

```
GET https://api.dictionaryapi.dev/api/v2/entries/en/<查询词>
```

`phonetics[].audio` in the response is a direct audio link (mostly `.mp3`), and `phonetics[].text` is the phonetic/accent annotation. Empty `audio` entries are skipped automatically, and identical URLs are deduplicated automatically.

Switching providers needs no UI changes: `PronunciationSourceStd` in `core/std/online_pron_std.h` is a pure virtual interface (three functions: `name` / `request_url` / `parse_response`) — implement a new source and register it at the platform layer.

### Privacy Stance

- **Off by default**: online pronunciation is only enabled when you explicitly pick "Online pronunciation" or "Automatic" in settings
- **Only the query word goes out**: the only content sent in the request URL is the word you asked to be read — no history, no vocabulary book, no identifying information
- **Stated when enabled**: once you choose online, the settings page shows this note right there, no doc digging required

Desktop sends requests over the network stack, Android uses the system `HttpURLConnection`, and both go over HTTPS.

::: tip Travel / offline scenarios
"Automatic (fall back to local)" is made for shaky networks: when the online voice is reachable it plays the human recording, and when it is not (timeout, no audio for the word, service unreachable) it falls back to local TTS automatically, so you never get "clicked but no sound".
:::

## Pronunciation Scoring (Experimental)

Local offline pronunciation scoring (M3): an onnxruntime acoustic model + CTC forced alignment + GOP per-phoneme scoring, for read-aloud practice. Build flags, model download, and usage are in [docs/pronunciation-plan.md](https://github.com/cuihairu/unidict/blob/main/docs/pronunciation-plan.md) (the model is about 635MB and is not shipped in the package).
