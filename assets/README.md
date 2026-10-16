# Sector identity

`sector-logo.png` is the transparent source mark. `sector.ico` packages the same
artwork at 16, 24, 32, 48, 64, 128 and 256 pixels for Windows. Rebuild the icon with
`powershell -STA -File scripts/Build-SectorIcon.ps1` from the repository root.
The desktop header, window icon and notification tray use these assets.

`icons/` contains original outline SVGs for reset, save, layers and sound. The WPF
resource geometries in `src/ui/reset_gui.xaml` mirror their path data; keep both in
sync when changing an icon. All use the same stroke width and rounded joins.

The logo was generated with the built-in image generation tool on 16 October
2026. Final generation prompt:

> Use case: logo-brand. Asset type: production app icon for Sector, a Windows sim-racing companion with physical checkpoint reset and future telemetry overlays. Create one distinctive, minimal geometric S monogram formed from two precise interlocking angular circuit sectors. Crisp flat vector-like silhouette, substantial strokes and generous negative space, readable at 24 px. Use a single pale electric blue #73B7FF, matching the app's charcoal dark interface. Square composition, symbol centered occupying roughly 80% of the canvas. Genuinely transparent background, no tile behind the symbol. No lettering or wordmark, no arrows, no speedometer, no car, no gradients, shadows, glow, texture, mockup or presentation board. A restrained technical identity suggesting track sectors and layered information, not just rewind. Deliver only the final isolated symbol.
