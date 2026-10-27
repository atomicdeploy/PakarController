# Keycontroller box workspace

Flutter uses one `KeyBoxDesigner` for the board-owned minigame panel and the
first-wins controls. It keeps the board's existing enabled mask, pixel layout,
effects and command ownership. Opening or arranging it sends no game command.

- Each box shows separate circular Button and Indicator groups. Segments use
  published physical RGB through `keyPixelGroup`, including custom group lengths.
  When that stream is absent, only press-to-win can show its configured semantic
  color; other modes keep a neutral ring until an observed frame arrives.
- Live inputs, winner, lost and latched state have distinct accents and labels.
  Disabled boxes remain visible and dimmed.
- Arrange boxes changes only this board's local workspace. Tap to select;
  Multiple selection moves a selection together. Movement keeps its grab offset,
  clamps the whole selection to the canvas, and snaps on release. Positions and
  selection persist in the existing workspace store.
- Three-dot actions expose selection, enabled-box settings and layout reset.
  Per-box actions verify the current board/boot before any hardware command.
  The older first-wins interface retains long-press actions and Reset to white.
  Minigames retain their serialized physical-style down/up hold behavior.
- A brief input-ownership fence disables commands without replacing the entire
  panel. Reboot preserves the local designer; the existing release logic still
  retires held input across peer/boot changes.

The control card uses the shared theme, font, header and badge components.
PCController's complete `mobile/lib/unified/workspace_chrome.dart` was reread
and its hash verified as
`f00ed672c1f72f1a66fb65cff1cfe0a89dea5f7b6222f154db00a7c52c2e1a82`.
Its reserved borders, semantic icon/accent surfaces and bounded text layout
continue through Pakar's shared components. Pealayer `layout.rs` drag-handle,
drop-target and effect-card header functions were inspected for grab-offset
preservation and reserved action gutters. This is a scoped adaptation, not a
claim that the entire Pealayer layout file has completed donor review.

Focused checks cover group drag without IO, workspace roundtrip, bounds,
hover geometry, disabled actions, phone large text, tablet rendering, held-input
serialization, stale-peer action sheets, and existing OutputPanel controls.
Renderer captures use synthetic board frames; deployed app screenshots and
actual hardware feedback remain separate evidence.
