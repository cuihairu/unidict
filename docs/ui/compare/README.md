> Language: **English** | [简体中文](../../zh/ui/compare/README.md)

# Implementation Screenshot Archive (BUG-010 Screen-by-Screen Comparison)

Screenshot-side evidence for BUG-010 acceptance; the wiring-side evidence is the real-tap checklist for
both ends (see BUGS.md). Captured after building with `-DUNIDICT_BUILD_UI_SANDBOX=ON` and then running

```bash
UNIDICT_DICTS=examples/dict.json build/Release/unidict_ui_sandbox <出图目录>
UNIDICT_DICTS=qmlui/dev/fixtures/click_audit_dict.json \
    build/Release/unidict_gui_sandbox <出图目录>
```

## qmlui (`home/result/vocab/settings × light/dark`, eight images, 1200x760)

- **Design baseline = `docs/ui/`** (2026-10-01 draft; from 2026-10-04 the implementation was re-laid-out to the
  Eudic standard: result-area color, hierarchy and annotations de-emphasized (759f93e) plus wiring fixes (05dab90).
  On 2026-10-05 the baseline was refreshed to the current implementation output, the standing baseline of the "implementation is the prototype" doctrine).
- The eight composite images here: **left = the 10-01 old draft, right = the 10-05 current implementation**, split by a
  red vertical bar. They preserve the design-evolution record; later walkthroughs use the refreshed `docs/ui/` baseline as the comparison surface.
- The GUI main program `unidict_qml` in the daily Windows/macOS build loads exactly this QML (fix record in BUGS.md
  BUG-002); historical fixes such as drawer height pinning are in BUG-003.

### Evolution Record Policy (set 2026-10-08, P-11 UI redesign D3 batch)

- **Record only major milestone nodes, not every batch in full**: one composite-image line = one redesign
  milestone (10-01 old draft -> 10-05 Eudic re-layout -> D3 result-panel cardification ...), named `<节点>-<屏>-<主题>.png`.
  The eight `docs/ui/` screens always stay in the current "the implementation is the prototype" state, are
  re-rendered with a new image on every batch, and are not re-archived here.
- **Capture only the screens a node affects**: what D3 touched is the result panel (visible globally in the
  right column and present on all eight screens, but semantically a single "result panel" evolution), so
  only the two `D3-result-*.png` images were produced; every other screen follows the rolling state of `docs/ui/`.
- **Capture and measurement methodology unchanged**: ui_sandbox offscreen software rendering (deterministic, same
  code same bytes); acceptance uses objective metrics (md5 for dedup / grayscale mean for theme / pixel diff
  for local elements). Reading images by eye aids layout comprehension only, never acceptance conclusions.

### D3 Result Panel Cardification (2026-10-08, `D3-result-light/dark.png`)

- Left = the D2 baseline (an outer hard-bordered box and flat group headers), right = the D3 current implementation
  (the outer box is retired, and each dictionary gets one radiusL rounded card: the card header carries the
  dictionary name, an outlined pill chip with the entry count, a 120ms rotating collapse arrow and a hover
  ghost surface; the entry rows inside are hierarchy-marked blue links plus light speak/copy actions on the right).
- Objective metrics: all eight docs/ui screens were fully refreshed (every md5 changed, since the right-column result
  surface appears on every screen); ui_click_audit S8 group collapse and S9 headword-link hierarchy marks all pass (contract preserved byte for byte).

## gui (12 images under the `gui/` subdirectory: home/result/examples/history/vocab/manage x light/dark)

- **The design baseline is `docs/gui-ui-structure.md`** (Qt Widgets, one focus per screen), not the same design
  baseline as the QML prototype drafts in `docs/ui/`. Both ends are accepted as "implemented from their own
  design drafts with full wiring", and they are not compared pixel by pixel.
- Objective metrics (re-verified on the 2026-10-05 capture): all 12 md5s are unique; the light and dark
  grayscale means are 250 vs 45 (theme distinguishability met). Images are captured offscreen by
  `unidict_gui_sandbox`; storage is pinned to a temp directory and never touches real user data.
- Known capture artifact: when the offscreen container lacks IPA fonts the phonetic row renders as tofu boxes, while a real desktop machine looks normal.
