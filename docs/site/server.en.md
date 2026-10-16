> Language: **English** | [简体中文](./server.md)

# Server Roadmap

::: tip Status: planning document, pending review
The full plan is in [docs/server_plan.md](https://github.com/cuihairu/unidict/blob/main/docs/server_plan.md). **No implementation moves before review passes** — this page is a summary.
:::

In one sentence: **a dictionary repository hosted in your own home + a multi-device installation coordinator**. Not a cloud sync service, not an account hub.

## Three Core Capabilities

| Capability | Content | Anonymous/account |
|---|---|---|
| Dictionary metadata catalog | Name, format, entry count, language, source, summary | Browsable anonymously |
| Dictionary file distribution | Each client pulls the files and loads them through the existing local parsing pipeline (the server never parses dictionaries) | Downloadable anonymously |
| Multi-device installation coordination | Manage in one place, each device pulls the list of what it should install: new ones get installed, updated ones get upgraded, removed ones get uninstalled | Requires an account (installation state is private data) |

## Boundaries

- **Anonymous use**: catalog browsing and downloads. **Requires an account**: device registration, my installation list, cross-device push install/uninstall. The account model stays minimal (single admin + optional read-only passphrase, long random device tokens)
- **Privacy (established security defaults)**: dictionary files are public material and fetchable anonymously; vocabulary book/lookup history **never go to the cloud**, syncing is opt-in with the scope stated, device information is the minimal set, server access logs are off by default
- **Deployment**: self-hosting first, `unidict_server` as a single binary (reusing the Qt-free core) + one-command Docker; listens on 127.0.0.1 only by default, exposing it is an explicit action
- **Explicitly not doing**: cloud lookup, storefront-style listing, resident background sync, third-party login

## Milestones

| Milestone | Content | Depends on |
|---|---|---|
| S-M1 Minimal usable server | Catalog API + file distribution (SHA-256 verification) + local directory scan and import; Docker image | None |
| S-M2 Desktop/Android integration | Remote catalog → one-tap install (through the local loading pipeline); first batch of device registration | S-M1 |
| S-M3 Installation coordination | Manage in one place, devices pull install/uninstall; dictionaries unique to this machine are kept | S-M2 |
| S-M4 Chinese search surface reinforcement | Pinyin lookup, Chinese word-segmentation full-text improvements, traditional-simplified conversion (core is self-governed, runs in parallel with the S line) | Independent of S-M1/M2 |
| S-M5 Enhancements | Ruby annotation display, radical/stroke filtering, vocabulary book sync channel, pronunciation resource cache distribution | S-M4 |

## Chinese Dictionary Format Surface (Summary)

Installing Chinese reference works like the Xinhua Dictionary: `.mdx` **works today** (the MDict parser includes encryption/`.mdd` resources/GB18030). What is missing is the Chinese-specific search surface — pinyin lookup (high priority), ruby annotation display, radical/stroke lookup, Hanzi stroke order, traditional-simplified conversion; the per-item current state and change surface are in section 2 of the planning document. These are **client core capabilities**, decoupled from the server; the server only carries metadata tags.
