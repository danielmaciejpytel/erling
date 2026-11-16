# Third-party notices and asset provenance

Project: **Erling**. Maintainer: **Daniel Maciej Pytel**.
Review date: **2026-11-16**.

This document distinguishes the maintainer's contributions from external material.
It is an inventory, not a replacement for the applicable licences and not proof
that every asset is cleared for publication. The repository's `LICENSE` applies
only to rights held by the maintainer. **Audio clearance remains unresolved.**

## Unreal Engine and engine dependencies

Erling uses Unreal Engine by Epic Games, Inc. under the
[Unreal Engine End User License Agreement](https://www.unrealengine.com/eula/unreal).
The project descriptor targets Unreal Engine 5.8. The editor target has been
built locally with 5.8.4. A clean-clone build and a packaged release have not
been verified in this maintenance work.

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

## Background music — permission not established

- **Work:** “Haaland (Ha Ha Ha)”
- **Performer:** CTID
- **Project asset:** `Content/Erling/Audio/background.uasset`
- **Local import source name:** `background.mp3`
- **Identification:** supplied by the maintainer; the title and performer are also
  listed on [Apple Music](https://music.apple.com/us/song/haaland-ha-ha-ha/1795477839).
- **Permission status:** no permission covering this project's use and distribution
  was supplied or located in the inspected project records.

The song and recording are not claimed as the maintainer's work. The listing is
identification only, not a licence. Publishing the recording in a repository,
including it in a game, or using it in a public trailer requires appropriate
rights for the intended use. Rights in the composition and recording may be
separate. Crediting CTID does not resolve these permissions.

Before public distribution, obtain the necessary permissions or replace the
track with material whose terms cover the intended uses. Review Git history,
LFS objects and release packages as well as the current working tree.

## Sound effects — individual sources not established

The maintainer reports obtaining free sound effects from Pixabay and other
websites found through Google. No reliable per-file author, original page,
download certificate or licence version was found in the inspected materials.
Google is a search service, not the licensor of the files it locates.

| Project asset under `Content/Erling/Audio/` | Local source | Individual source and licence |
| --- | --- | --- |
| `click.uasset` | `click.mp3` | Unverified |
| `fail.uasset` | `fail.mp3` | Unverified |
| `goal.uasset` | `goal.mp3` | Unverified |
| `jump.uasset` | `jump.mp3` | Unverified |
| `kick.uasset` | `kick.mp3` | Unverified |

The MP3 files were matched to the assets' import hashes. Their inspected ID3
metadata did not identify creators or download pages. This confirms the local
file-to-asset mapping, not the source or licence of a recording.

Do not assume that every effect came from Pixabay or that any is CC0. If a file
is confirmed as Pixabay content, check the terms applicable to that download:
[Content License summary](https://pixabay.com/service/license-summary/) and
[binding terms](https://pixabay.com/service/terms/). The current terms permit
many creative uses but restrict standalone redistribution. Publicly supplying
reusable audio assets needs separate assessment from incorporating them in a
finished game; importing an MP3 into `.uasset` does not establish permission.

For each effect, record its original URL, creator, licence/version, acquisition
date where available, and evidence that the intended distribution is permitted.
If this cannot be established, replace it with an original recording or a
verifiably licensed alternative before publishing it. Retain any required notices.

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

## Publication status

This inventory records open issues; it does not clear the repository for release.
Resolve audio permissions and review the exact public tree, history and any
release package, including the notices required by its bundled dependencies.
