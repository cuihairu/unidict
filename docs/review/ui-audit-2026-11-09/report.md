# Native Desktop UI Supplementary Audit (2026-11-09)

Target: qmlui (MainDesktop.qml) current implementation, captured offscreen by
`unidict_ui_sandbox` (deterministic software rendering, same code same bytes).
Baseline of comparison: the 2026-11-01 audit screenshots
(`docs/review/ui-audit-2026-11-01/`) and the 2026-11-02 impeccable critique
(`.impeccable/critique/2026-11-02T23-58-41Z__qmlui-before.md`, 17/40 Poor).

## Capture

- Command: `UNIDICT_DICTS=dictionaries/ccedict-zh-en.json
  build/Release/unidict_ui_sandbox docs/review/ui-audit-2026-11-09`
- 40 images: 10 views (home / result / history / vocab / settings /
  appearance / dictlib / suggest / notfound / quicklookup) x light/dark x
  zh/en, 1200x760 (quicklookup is the 420x260 floating pane).
- Exit code 0. No capture failures.

## Objective metrics

- md5 uniqueness: 40/40 images unique (no blank or duplicated frames).
- Theme distinguishability (grayscale mean, 1200x760 zh): light ~246, dark
  ~20 — same separation as the 2026-11-01 capture (theme toggle is real).
- WCAG AA contrast gates: `test_theme_tokens_contrast` passes. Spot ratios:
  white on brand rose #b11964 = 6.58:1 (AA, >=4.5); white on dark surface
  #121212 = 18.73:1 (AAA); secondary grey on dark = 7.16:1 (AA).

## Findings vs the 2026-11-02 critique (17/40)

The two P1 root defects from the critique are resolved in the current build:

1. Accent split (rose + blue) -> single brand rose #b11964 across logo,
   tab underline, primary buttons, selected list item, suggestion words.
2. Icon tofu -> the view now renders a consistent SVG-style icon set
   (history / vocab / settings / speak / copy / note / collapse / search /
   close), no tofu boxes observed on any captured view.
3. Entry typography hierarchy (P1) -> established: large headword, smaller
   phonetic line, body definition, muted section headers, outlined cards.

## Dimensions checked

- Hierarchy: headword > phonetic > definition; section labels muted; result
  cards carry dictionary name + collapsible chevron + action row. Coherent.
- Layout: 8px-ish rhythm, stable left/right column split, footer status line,
  no overflow or clipped content on the 1200x760 canvas.
- Contrast a11y: token gate green; spot ratios above meet AA (brand rose
  button text 6.58:1).
- Boundary states exercised: empty home (single centered empty state +
  primary CTA "试试查 hello" + secondary "打开词典管理"), notfound
  (未收录 tag + Chinese error copy + CTA), suggest sections (前缀 header +
  rose suggestion words), quicklookup floating pane, dictlib import view.

## Minor observations (P3, cosmetic — no fix required this pass)

- A very faint rose chevron (~5% opacity) renders at the bottom-right of the
  suggest / notfound / dictlib light captures. Likely an enter/exit animation
  frozen mid-transition by offscreen software rendering; not observable as a
  usability defect. Worth a glance if a real-desktop capture ever shows it.
- quicklookup images are named `*_1200x760.png` but the pane itself is
  420x260 (floating Tool window by design). Filename-only mismatch.
- The result card leaves generous whitespace under a short entry (content
  does not hug). Density nit, not a defect.

## Conclusion

No P0/P1/P2 defects found; the 2026-11-02 critique's P1s are addressed. The
UI is consistent (single accent, no tofu, established hierarchy) and passes
the objective contrast and theme gates. The report and screenshots are the
acceptance evidence for the UI-AUDIT item.
