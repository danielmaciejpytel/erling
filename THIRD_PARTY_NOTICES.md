# Third-party notices and asset provenance

Project: **Erling**. Maintainer: **Daniel Maciej Pytel**.
Review date: **2026-11-16**.

This document distinguishes the maintainer's contributions from external material.
It is an inventory, not a replacement for the applicable licences and not proof
that every asset is cleared for publication. The repository's `LICENSE` applies
only to rights held by the maintainer. **SFX provenance is resolved; background-music review is deferred by the maintainer, with permission unconfirmed.**

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

## Background music — credited; review deferred, permission unconfirmed

- **Work:** “Haaland (Ha Ha Ha)”
- **Performer:** CTID
- **Songwriters credited by the release catalogue:** Matt Cadman, Cris Nuttall,
  James Hill and Lee Monteverde.
- **Label / recording rights notice:** Xploded Music Limited; the catalogues list
  2022 and 2025 releases. The exact edition of the local recording is not verified.
- **Release references:** [2022 release](https://music.apple.com/us/album/haaland-ha-ha-ha-single/1638121163),
  [2025 release](https://music.apple.com/us/album/haaland-ha-ha-ha-single/1795477824),
  [songwriter credits](https://www.qobuz.com/fi-en/album/haaland-ha-ha-ha-ctid/wt5ppu1it8coa).
- **Project asset:** `Content/Erling/Audio/background.uasset`
- **Local import source name:** `background.mp3`
- **Identification:** supplied by the maintainer; the title and performer are also
  listed on [Apple Music](https://music.apple.com/us/song/haaland-ha-ha-ha/1795477839).
- **Permission status:** no permission covering this project's use and distribution
  was supplied or located in the inspected project records.

**Maintenance status (2026-11-16): deferred by the maintainer for the current
local development phase.** The search found no public licence granting use in a
game and redistribution through GitHub. This records the decision to stop further
clearance work for now; it is not permission or a publication clearance. The audio
is retained unchanged. Reopen this item before public distribution unless the
required permissions have been obtained or the recording excluded.

The song and recording are not claimed as the maintainer's work. The listing is
identification only, not a licence. Publishing the recording in a repository,
including it in a game, or using it in a public trailer requires appropriate
rights for the intended use. Rights in the composition and recording may be
separate. Crediting CTID does not resolve these permissions.

Before public distribution, obtain the necessary permissions or replace the
track with material whose terms cover the intended uses. Review Git history,
LFS objects and release packages as well as the current working tree.

## Sound effects — sources identified; provenance issue resolved

The five project SFX were traced to their original Pixabay download URLs stored
in the local MP3 files' Windows `Zone.Identifier` metadata. On 2026-11-16, all five
files fetched from those URLs matched the local MP3s byte-for-byte by SHA-256.
Their MD5 hashes also match the import hashes stored in the Unreal assets.
This supersedes the earlier finding that individual sources were unknown.

| Project asset under `Content/Erling/Audio/` | Work and contributor | Source and licence |
| --- | --- | --- |
| `click.uasset` | UI Pop sound — 47313572 | [Pixabay 316482](https://pixabay.com/sound-effects/film-special-effects-ui-pop-sound-316482/) — Pixabay Content License |
| `fail.uasset` | Failure Trumpet — Universfield | [Pixabay 242645](https://pixabay.com/sound-effects/musical-failure-trumpet-242645/) — Pixabay Content License |
| `goal.uasset` | Good! — syseQ | [Pixabay 6081](https://pixabay.com/sound-effects/film-special-effects-good-6081/); [original Freesound 267528](https://freesound.org/people/syseQ/sounds/267528/) — CC0 1.0 at the original source |
| `jump.uasset` | Game Character Fall — Universfield | [Pixabay 131797](https://pixabay.com/sound-effects/film-special-effects-game-character-fall-131797/) — Pixabay Content License |
| `kick.uasset` | FX Impact ball — Mori_sound | [Pixabay 526576](https://pixabay.com/sound-effects/film-special-effects-fx-impact-ball-526576/) — Pixabay Content License |

For `goal`, Pixabay identifies syseQ (Freesound) as the artist. The original
Freesound page expressly offers the sound under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
Decoded audio from the local MP3 and the original page's preview had normalized
waveform correlation 0.9924 after alignment; MP3 encodings differ. CC0 permits
copying, modification and redistribution, including in a public repository.
For `kick`, the individual page could not be read during review; the download URL,
exact file hash and contributor listing establish the source, while the licence
assessment relies on Pixabay's general terms rather than a separately read badge.

The applicable Pixabay references are the [Content License summary](https://pixabay.com/service/license-summary/)
and [binding Terms of Service](https://pixabay.com/service/terms/) (reviewed
2026-11-16; terms dated 20 October 2026). Section 5 permits commercial and
non-commercial creative uses, subject to its restrictions; section 6 does not
require attribution. The credits above are retained to document provenance.
These SFX remain third-party material and are not relicensed under this project's
proprietary `LICENSE`.

**Audit status:** the missing-source/licence-identification issue for SFX is
resolved. Their use as integrated sound effects in Erling is supported by the
identified licences. Pixabay prohibits standalone redistribution of substantially
unchanged content. This status does not assert unconditional permission to
publish separately reusable audio files or a sound library. Public source-asset
distribution must respect that restriction; importing an MP3 into `.uasset` does
not by itself settle its application. This notice records the applicable terms,
not additional permission from the contributors. The original download-date
licence snapshots were not recovered.

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
Resolve the background-music permissions and review the exact public tree, history and any
release package, including the notices required by its bundled dependencies.
