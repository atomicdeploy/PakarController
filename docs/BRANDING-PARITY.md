# One PakarController application icon

The mismatch was an incomplete integration: the native PE/window/tray generated
Phosphor Circuitry, the native menu/About used a separate theme-tinted font
glyph, and the Web header hardcoded a diamond with no application favicon.
These were independent branding sources, not a deliberate design requirement.

Read actual Pealayer `src/branding.rs`, `build.rs`, `assets/icons/README.md`,
`web_ui/src/components/HeaderBar.tsx` and `web_ui/src/webPlatform.ts` completely.
Its resolved artwork feeds native windows/tray and the Web header/favicon;
runtime preferences do not silently rewrite executable or pinned icons.

## Canonical master and exact code imports

`assets/branding/app-icon.png.base64` is a 740-byte lossless PNG encoding of the
existing 64x64 native Phosphor Circuitry raster. Verified current warm-cache
native pixels SHA-256:
`d24d849f01bfeaa0cffef1cc9b861062b52e1a320f4c943608fb5fd952ded387`.
PNG SHA-256:
`05bc6ce48c57eb8bf6c8f1acd98f10389ae2e6fcc40b4bf41b09f9ce39c85783`.
No new drawing, font substitution, donor build, dependency download or duplicate
cache was needed. The neutral blue-gray color is the existing shell artwork;
branding is no longer independently tinted by each surface's accent palette.

| Actual donor code | Pakar consumer |
|---|---|
| `src/branding.rs::icon_data_from_bytes` | Imported unchanged in `desktop/src/branding.rs`; window icon and cached header/About texture decode the same master. |
| `src/branding.rs::native_icon_bytes` | Actual native-master preservation, Lanczos3 alpha-preserving resize, seven PNG-frame ICO directory/offset encoding in `desktop/src/brand_image.rs`. Only the AppConfig/playback resolver is replaced by supplied shared bytes; made host-independent for build use. |
| `shell_derivative_preserves_native_master_and_multiple_resolutions` test | Adapted to parse/decode every ICO PNG frame, as Pakar deliberately enables PNG-only image decoding. Native multi-frame preservation is tested too. |
| `builtin_icon_bytes`/`icon_bytes` embedded/no-cache behavior | Single Pakar master, decoded once in memory; no media states/preset copies and no disk cache. Full custom/preset/state resolver remains pending below. |
| `HeaderBar.tsx` resolved `img src={appIconPath...}` with application-logo alt text | Web header and login now render an image from the same master, not a diamond or font fallback. `tools/generate-branding.mjs` mechanically embeds it. |
| `webPlatform.ts` favicon/touch artwork binding | Static standalone ESP page binds favicon and touch image to the same resolved master. Inline data works before login and avoids a separate protected icon endpoint/external dependency. |
| `build.rs` application icon resource vs quick-action resource separation | Pakar PE/windows/tray use master derivatives; actual imported Phosphor quick-action rasterizer remains for semantic action icons, not application branding. |

Native consumers: executable resource, startup window/taskbar icon, tray,
toolbar application mark and About. Web consumers: authenticated header, login,
browser favicon and touch artwork. Semantic settings/hardware/action icons keep
their actual Phosphor vocabulary; they are not application-logo variants.

## Explicit unfinished useful code and reasons

- Pealayer `resolved_icon_path`, state environment overrides, Current/Classic
  presets, fallback/precedence tests and validated custom-icon preferences are
  still useful generic configuration work. Their filesystem paths and media
  playback state cannot be copied blindly to a standalone ESP. They remain
  pending with a shared board/client transport and bounded storage contract.
- `sync_native_window_icon` and source-signature caching are now adapted to
  firmware application metadata in `branding::sync_peer`. Connected name/title,
  icon, header/About texture and tray update in memory; stale/disconnected peers
  reset to host branding. Web header/favicon/touch use the same bounded PNG.
  Native execution is still blocked by the canonical cache guard; the icon
  editor, playback resolver/presets and installed/pinned resource rewrites are
  not claimed. See BOARD-APPLICATION-IDENTITY.md for exact contracts/proof.
- Header collapse/connection modal/capability-aware action cluster, localization,
  and WebPlatform install/update/share/notifications/haptics/audio/wake-lock/
  badge lifecycle remain pending full imports; reading those files or using
  their image-binding mechanism does not certify the whole files.
- Only media title, video state/chapters/seeking and media-session transport are
  excluded media business. Generic capability checks and preferences are not.
- Build metadata, external branding manifest/environment override validation,
  generic publisher/version/provenance resource fields remain pending. macOS
  mpv linker probing is excluded by the requested mpv-free controller.
- The preserved master is currently 64x64, not a claimed 512x512 editable icon
  pack/PWA artwork. ICO derivatives retain the donor's seven sizes; high-DPI
  editable master and full installable PWA manifest/cache remain open.

`tests/branding_test.mjs` verifies exact preserved native pixels, four identical
embedded Web images, production native consumers and no-write generation.
Three Rust tests verify native pixels/window/tray derivatives, all ICO frames
and shared texture reuse in both themes. Source tests do not certify compiled
resource extraction or deployed runtime/board appearance. CI runs asynchronously;
no claim that the current running app or board has already received this patch.
