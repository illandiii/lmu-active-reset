# Third-party notices

Sector's original code and documentation use the MIT license. Third-party code,
fonts and artwork retain their own terms; the project license does not relicense
them. Portable packages include the Sector license as `LICENSE-Sector.txt` and
retain dependency notices alongside the application.

## Interface assets

| Asset | Attribution and license |
| --- | --- |
| Solar Linear icons | **480 Design**, [original set](https://www.figma.com/community/file/1166831539721848736), distributed through Iconify under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| Manrope | Copyright 2019 The Manrope Project Authors, [project](https://github.com/sharanda/manrope), SIL Open Font License 1.1 |
| Barlow Condensed | Copyright 2017 The Barlow Project Authors, [project](https://github.com/jpt/barlow), SIL Open Font License 1.1 |
| Barlow Semi Condensed and JetBrains Mono | Font files supplied with the overlay design; their original notices are retained in `renderer/overlays/fonts/LICENSE.txt` |

Solar icon paths are used without modification; size and color follow the UI.
Fonts and icon data are bundled locally through the npm lockfile. Font notices
are included in `licenses/`; the overlay font notice is also copied there during
packaging. No remote page runtime is required.

The Sector logo is an original generated asset. Its provenance is documented in
the repository's [asset guide](https://github.com/sector-lmu/Sector/blob/dev/assets/README.md).

## Overlay designs and manufacturer marks

Widget layouts and logo artwork derive from the user-supplied `LMU Overlay.html`
and `loghi_LMU.png`. The supplied logo sheet is retained, while widgets use a
background-removal variant displayed through SVG viewports. Manufacturer names
and marks belong to their respective owners; inclusion does not imply affiliation
or endorsement, and Sector's MIT license grants no rights to those marks.

## Software dependencies

- Electron retains its `LICENSE` and `LICENSES.chromium.html` in the portable distribution.
- The embedded CPython distribution retains its original license files.
- MinHook retains its original license and authors in `third_party/minhook/LICENSE.txt`; packaging includes a copy under `resources/app/licenses/minhook.txt`.
- npm dependencies, including React, Base UI, Iconify and build tools, retain
  their package notices. The release builder copies `LICENSE`, `COPYING` and
  `NOTICE` files into `resources/app/licenses/npm/` with their package paths.

The dependency lockfile records exact resolved versions. Updating a dependency
or asset must preserve its applicable license text and attribution.
