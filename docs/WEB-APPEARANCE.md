# Actual Pealayer Web appearance import

Tracking: [settings #4](https://github.com/atomicdeploy/PakarController/issues/4),
[donor comparisons #3](https://github.com/atomicdeploy/PakarController/issues/3),
[storage #9](https://github.com/atomicdeploy/PakarController/issues/9).

## Exact source and implementation

Canonical Pealayer was read at d3f11b18d3b969398df18c26bc5871b426d4c332.
Original-byte source and normalized target/test hashes are bound in the existing
file-comparison-evidence.json and expanded into file-comparison.csv. This is not
a separate approval ledger or whole-repository parity claim.

| Actual donor | Pakar use | Remaining |
|---|---|---|
| web_ui/src/appearance.ts | All five actual functions in assets/themes/appearance.js, generated into WebUi.h | Native/Web/mobile preference transport and authoritative host events |
| web_ui/scripts/appearance.test.mjs | All four contract tests ported in tests/web_appearance_test.mjs, plus six shipped-code tests | Connected-host acceptance is separate |
| assets/themes/palettes.json | Same catalog, all four theme/palette combinations and fourteen roles, embedded by tools/generate-web-appearance.mjs | Mobile consumption and shared preference synchronization |
| web_ui/src/App.tsx | Actual appearance role/accent assignment and persistence patterns adapted in embedded Web | Full generic routing/sidebar, IPC acknowledgments, reconnect, shared toasts and remote browsing still need exact mappings |
| web_ui/src/styles.css | Actual semantic shadows, button glyph centering, shrinkable content/forms, reachable dialog and responsive patterns in assets/themes/controller-surfaces.css | Full generic menus/status/sidebar/channel/strip/buzzer/display/diagnostic styles remain pending |

App.tsx and styles.css were read fully but are not certified complete-file
comparisons merely because their appearance slice is used. Generic controls are
not excluded as media business. The user-excluded playback/timeline/cue logic
does not excuse omission of generic UI infrastructure.

## Shipped browser behavior

Appearance is available from the login screen or workspace. The dialog offers
System/Dark/Light, Studio/Neutral, named accents and custom color. It validates
stored values and chooses contrast-aware button text using the actual donor
formula. Cancel leaves the applied state untouched; Apply stores only browser
preferences. Identical values skip writes. Storage failure is visible and applies
only for the current session. Cross-tab and System media events repaint without
echo writes. System here follows the browser: no live desktop appearance transport
is claimed. Device settings, authentication and LED colors are unchanged.

Cells use alternate **cell** surfaces, half-row odd-column offset, fixed two-pixel
borders and palette-safe active/red/selected fills. Search matches keep amber
priority even when selected/hovered. LED-state dots stay hidden by default.
The responsive layout shrinks within the viewport instead of overflowing.

Generation uses one actual palette asset and shared function module, skips
unchanged writes, and fails with --check if embedding is stale. ESP builds and CI
run that check before compiling; no parallel manually maintained palette engine.

## Executed verification

- Ten appearance tests pass, including all four actual donor contracts and the
  actual embedded script, not a substitute mock appearance implementation.
- Existing Web access, display, cells, remote update and PU-route tests pass.
- A read-only loopback fixture served the actual WebUi.h. Its header visibly says
  SIMULATED WEB FIXTURE — NOT A BOARD; all mutations are rejected. No board was
  contacted or programmed through the fixture.
- Real browser controls applied all four palettes during this pass. Current
  Neutral/light/green persisted through reload. Switching appearance and search
  selection left all eighty cell rectangles unchanged at the normal viewport.
- Phone-width check at 390 pixels initially exposed horizontal overflow. After
  adapting shrinkable layout, document width was 375 and grid scroll width 319;
  all eighty cells fit without horizontal scrolling. Dense phone targets are not
  yet certified as complete mobile/touch usability.
- Saved and inspected small JPEG evidence under outputs/PakarController/audit:
  web-appearance-table.jpg, web-appearance-preferences.jpg and
  web-appearance-responsive.jpg. Earlier dark/dialog images remain unmodified.
  Temporary viewport override reset, owned browser tab closed and fixture stopped.

No native rebuild/replacement, donor build, Android rebuild, private preference
overwrite or cache deletion was required for this Web change. ESP compile and
hardware/deployment receipts are recorded separately; screenshots are not LAN,
physical peripheral or production-release acceptance.
