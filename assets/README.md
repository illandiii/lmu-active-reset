# Sector assets

| Asset | Purpose |
| --- | --- |
| `sector-logo.png` | Transparent source mark for Sector |
| `sector.ico` | Windows application/tray icon at 16, 24, 32, 48, 64, 128 and 256 pixels |

The blue geometric S mark was generated with the image-generation tool on
16 October 2026 for Sector. It represents interlocking track sectors and layered
information. The source mark has no background tile, lettering or embedded UI.

To rebuild the Windows icon from the repository root:

```powershell
powershell -STA -File scripts/build/Build-SectorIcon.ps1
```

The desktop uses locally bundled Solar Linear SVG icons, separate from the
Sector mark. Overlay manufacturer artwork comes from the user-supplied logo
sheet and belongs to the respective mark owners. See [third-party notices](../src/ui/electron/THIRD_PARTY_NOTICES.md)
for icon attribution and font licenses.
