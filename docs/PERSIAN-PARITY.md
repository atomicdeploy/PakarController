# Persian support: actual donor code and acceptance boundaries

Tracking: [settings #4](https://github.com/atomicdeploy/PakarController/issues/4),
[exhaustive donor ledger #3](https://github.com/atomicdeploy/PakarController/issues/3),
[storage #9](https://github.com/atomicdeploy/PakarController/issues/9).

## Current CI acceptance boundary

Terminal source c814379 run 38091589105 executes native logical-copy and
contextual Persian glyph/shaping tests successfully, including Proportional and
Monospace. However language-change font retention FAILS, along with dropdown and
cell-surface tests, and the harness ends with 0xc0000409. Therefore symbol/font
retention and the production native UI are not accepted. Source inspection is
not a substitute for that failed assertion. Restore the actual donor serial
test/config-isolation/backtrace contract and immediate failure output before
choosing a font fix; do not guess the hidden assertion or remove its coverage.
Full font source reread preserves the pending translation/group/runtime work.
The same run passes both firmware variants and Android preview, not Android
runtime/localization completeness. The local receipt below is historical.

## Regression cause

The font was already bundled in native source, but its registration covered only
Proportional. Monospace editors did not have the Persian fallback. Changing the
language did not refresh font priority; many native controls never called the
imported translation table. Web and Android had no shipped Vazirmatn face. These
are implementation gaps, not a requirement to install a font on Windows.

Elegance does not erase host fonts: its actual `Theme::install` uses `add_font`.
Conversely, resetting our font registry to defaults on a later language change
would erase Elegance symbols. The fix preserves the current registry, removes
only duplicate application priority entries, and retains Phosphor and symbols.
Initial installation is owned once by `config::apply` before the first frame.

## Exact source-to-target mapping

| Actual Pealayer source | Pakar implementation |
|---|---|
| `src/ui/i18n.rs` (complete read) | `desktop/src/ui/i18n.rs`: actual font definitions, platform face fallback, bundled Vazirmatn, dictionary, logical Unicode, script detection, field alignment and directional layout. Added Monospace coverage and preference-refresh registry preservation. |
| `src/config.rs` (named functions only; NOT certified as complete) | `desktop/src/config.rs`: adapted `parse_language_tag`, language resolution and direction overrides; existing Windows binding resolves the user locale. `System`, English/Persian and Auto/LTR/RTL persist compatibly. |
| `web_ui/src/i18n.ts` (complete read) | `assets/web/localization.js`: actual dictionary fallback semantics for application-owned copy. Native dictionary generates the applicable Web subset, not an unrelated media UI. |
| `tests/i18n_surface_audit_test.rs` (complete read) | `tests/localization_test.mjs`: adapted forbidden raw-caption guards plus font-byte, language, direction, placeholder, ownership and persistence tests. Media/cue test surfaces replaced with real controller surfaces. |
| `assets/fonts/Vazirmatn-Regular.ttf` | Existing shared assets and native font; deterministic generator embeds gzip bytes in ESP PROGMEM and identical TTF bytes in Flutter assets. SIL OFL accompanies distribution. |

The exact paths, original source hashes, normalized target/test hashes, omissions
and line anchors remain in the existing `file-comparison-evidence.json` and
generated `file-comparison.csv`, not a separate completion checklist.

## Current behavior

- Native shell menu captions, panel titles, common dialog controls, Preferences
  rail and settings use the imported localization boundary. Text input follows
  its script/UI direction without reversing or changing stored Unicode.
- Native language preview/save/cancel updates fonts and direction. The added
  contextual-shaping test exercises both Proportional and Monospace. Another
  test checks that language changes retain Elegance symbols and avoid duplicate
  fallbacks. These tests now execute in CI: shaping passes, font retention fails;
  no native production acceptance follows from the passing subset.
- Web Language and Direction controls persist per browser, with system locale
  and explicit overrides. Labels, settings catalog/navigation, hints, selected
  settings states, placeholders and accessibility captions use application copy.
  No mutation observer translates arbitrary device names or hostnames.
- Table/physical pixel geometry and seven-segment layout stay LTR regardless of
  UI direction. Peer-advertised names, capabilities, mode labels and protocol IDs
  remain authoritative values, not dictionary substitutions.
- ESPAsyncWebServer serves the real compressed font from PROGMEM, without
  allocating a font-sized heap copy. Only exact GET font/license paths are public;
  API authentication and write guards are unchanged.
- Flutter now bundles and uses the identical font. This is NOT a complete mobile
  locale implementation or accepted Android runtime. Preview CI build passes.

## Earlier local evidence, not production claims

The actual embedded Web UI was rendered against the read-only, explicitly
simulated fixture. Persian settings show joined text, translated hints and
navigation. Browser inspection reports `lang=fa`, `dir=rtl`, Vazirmatn loaded,
and no horizontal overflow. The inspected screenshot is retained in local
`outputs/PakarController/localization/persian-settings-refined.jpg`.

The ESP busy-wire build passed: RAM 56620/80192, IRAM 63055/65536 and IROM
844784/1048576 bytes. Candidate SHA-256:
`2676c89f113a846112f61e3febdbbacce97cd2107789067c18cf8e070d5afd5e`.
It has NOT been flashed. No AVR changes or unnecessary AVR build occurred.

Native Test stopped before Cargo: the existing 13.49 GiB canonical cache exceeds
the unchanged 8 GiB budget. No cache deletion, relocation, alternate target or
budget bypass occurred. The existing production app is still running unchanged.
Verified obsolete-debug-only cleanup was requested, not silently performed.
SSH localhost:1022 refused and ledboard.local did not resolve. No physical,
ASUS, OTA, native-render or mobile-runtime acceptance is inferred from Web proof.

## Useful work still pending (not accepted exclusions)

- Resolve native font-retention assertion and the aborted harness; run restored
  serial tests and inspect actual native Persian menus/editors/dialogs.
- Full native device-settings bodies, status/transport surfaces, formatted
  messages, search keyword localization and translation completeness audit.
- Web dynamic error/confirmation copy, status/menu attributes generated after
  startup, mixed-script inputs, narrow/mobile-width and live-board acceptance.
- Android locale preferences, translated strings, RTL and APK/runtime tests.
- Donor generic folder-management translations and behavior remain pending until
  actual controller folder/group semantics exist; they are not media exclusions.
- The donor's separate TrueType parser coverage assertions are not copied as a
  new dependency. Actual renderer glyph/shaping tests are adapted instead;
  broad additional Unicode/font-family acceptance remains pending.
- Timeline/cue reveal and playback/keyframe-specific copy is not applicable to
  this mpv-free controller. Generic dialogs, notifications, shell, tooling and
  other unreviewed files are NOT dismissed for that reason.

This fixes a real subset of the regression. It does not certify that all Pealayer
features have been imported or that the production desktop has been replaced.
