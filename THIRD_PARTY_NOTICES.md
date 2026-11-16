# Third-party notices and asset provenance

Project: **Erling**. Maintainer: **Daniel Maciej Pytel**.
Review date: **2026-11-16**.

This document lists external materials and their authors and sources.
The repository's `LICENSE` applies only to rights held by the maintainer;
third-party materials retain their respective rights and licences.

## Unreal Engine and engine dependencies

Erling uses Unreal Engine by Epic Games, Inc. under the
[Unreal Engine End User License Agreement](https://www.unrealengine.com/eula/unreal).
The project descriptor targets Unreal Engine 5.8. The editor target has been
built locally with 5.8.4. A clean local clone was built and run on 2026-11-16
using locally supplied Git LFS objects. A packaged release has not been verified.

The tracked repository contains project code and assets, not an Unreal Engine
installation. Users must obtain the engine separately under its own terms.
The referenced engine plugins include PythonScriptPlugin, EditorScriptingUtilities,
ModelContextProtocol, AllToolsets and ProceduralMeshComponent. The first four are
restricted to editor targets in `Erling.uproject`. ModelContextProtocol and
AllToolsets are experimental and marked `NoRedist` in the inspected installation;
this project does not grant permission to redistribute those plugins.

Engine components and their bundled third-party libraries are not relicensed by
the project. Packaged builds require a separate dependency and notices review.
Retain the applicable engine and third-party notices supplied with the actual
engine version and release package. Consult the engine's
`Engine/Source/ThirdParty/Licenses` and, where applicable,
`Engine/Extras/ThirdPartyNotUE` directories. The legal notice in the credits uses
the engine's Roboto font; that engine asset is not the project's Dantes Toon font.

The in-game credits include the attribution notices specified by section 7(a)
of the EULA, with the 2026 copyright year. The legal wording is retained in
English in both interface languages. It is stored in `WBP_ErlingInterface`;
the partial `ErlingBuildUI -UpdateCredits` authoring command maintains it.

## Background music

**“Haaland (Ha Ha Ha)” — CTID**. Label: Xploded Music Limited.
[Release](https://music.apple.com/us/album/haaland-ha-ha-ha-single/1638121163).

Project asset: `Content/Erling/Audio/background.uasset`; import file: `background.mp3`.

## Sound effects

Project assets are under `Content/Erling/Audio/`; import files use the same names with the `.mp3` extension.

| File | Title and creator | Source and licence |
| --- | --- | --- |
| `click.uasset` | UI Pop sound — 47313572 | [Pixabay 316482](https://pixabay.com/sound-effects/film-special-effects-ui-pop-sound-316482/) — Pixabay Content License |
| `fail.uasset` | Failure Trumpet — Universfield | [Pixabay 242645](https://pixabay.com/sound-effects/musical-failure-trumpet-242645/) — Pixabay Content License |
| `goal.uasset` | Good! — syseQ | [Pixabay 6081](https://pixabay.com/sound-effects/film-special-effects-good-6081/); [original Freesound 267528](https://freesound.org/people/syseQ/sounds/267528/) — [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| `jump.uasset` | Game Character Fall — Universfield | [Pixabay 131797](https://pixabay.com/sound-effects/film-special-effects-game-character-fall-131797/) — Pixabay Content License |
| `kick.uasset` | FX Impact ball — Mori_sound | [Pixabay 526576](https://pixabay.com/sound-effects/film-special-effects-fx-impact-ball-526576/) — Pixabay Content License |

Pixabay: [Content License summary](https://pixabay.com/service/license-summary/) ·
[Terms of Service](https://pixabay.com/service/terms/).

## Maintainer-provided artwork and production provenance

The following production account was provided by the maintainer on 2026-11-16:

- **Character model:** created by Daniel Maciej Pytel in Blender.
- **Character animations:** original project work, created by the maintainer
  with AI assistance, as separately confirmed during this review.
- **Control graphics and user interface:** created partly by the maintainer in
  Photoshop and partly with AI assistance, based on the maintainer's existing work.
- **Stadium elements:** created in Unreal Engine using MCP tools, based on the
  maintainer's concept artwork and a moodboard containing visual references.

These are author-provided provenance statements, not a claim that every component
was made manually. Reference images retain their own rights; use as inspiration
does not itself permit redistribution of the reference files or copied elements.
Blender, Photoshop, AI assistance and MCP describe tools or processes, not an
independent licence for third-party source material. Any applicable tool/service
terms and third-party input rights must still be respected.

The repository contains 17 animation clips. Their import metadata points to
project working files; attribution relies on the maintainer's confirmation,
not on import paths as independent proof of authorship.

## Dantes Toon typeface

The two distributed font source files are
`Content/Erling/UI/SourceArt/Fonts/DantesToon-Regular.ttf` and
`Content/Erling/UI/SourceArt/Fonts/DantesToon-Bold.ttf`.
Their copyright and licence metadata identify Daniel Maciej Pytel and refer to
the project's proprietary licence. The inspected production notes describe a
custom font generated from the maintainer's Photoshop design with code and
AI assistance, rather than reused outlines from another typeface.

The current `F_Storybook` font asset references the two Dantes Toon font faces.
No Corsair, Brother or Patrick Hand font files were found among the paths in the
55 locally reachable commits inspected. This does not verify old remote
repositories, deleted refs, pull-request attachments or historical releases.

Font-generation tools are outside the tracked repository. If those tools or
their dependencies are distributed later, review their licences separately.

## Names, likenesses and endorsement

The project is an independent portfolio work. No endorsement, sponsorship or
affiliation with CTID, a football player, a club, Epic Games or other third-party
rightsholders is asserted. This statement does not itself grant rights to music,
trademarks, a person's likeness or reference artwork.
