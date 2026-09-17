# Erling

**Erling** is a stylized football playground and modular character creator built in **Unreal Engine 5.8** as a gameplay, animation and UI portfolio project.

The current prototype focuses on making a small football sandbox feel responsive: the player can run, sprint, dribble, charge shots, jump, slide and celebrate goals while the ball remains physically simulated and visually tied to the character's movement.

## Highlights

- **Foot-aware dribbling** — the ball reacts to the character's left and right foot positions instead of simply following a fixed point. Sprinting uses a contact, release and chase cycle so the player visibly pushes the ball forward and catches up to it.
- **Physics-based ball interaction** — charged shots, ground passes, goal detection, net collision, missed-shot handling and ball reset are built around the simulated ball.
- **Custom animation runtime** — 24 animation clips cover locomotion, kicks, jumps, turns, slides, trips, recovery, celebrations and emotes. Gait transitions preserve animation phase and blend between clips while CharacterMovement owns world motion.
- **Three gameplay cameras** — Smooth, Reduced and a fixed **Pitch** view inspired by football games, with camera-relative movement where appropriate.
- **Keyboard, mouse and gamepad support** — gameplay and character-editor controls are mapped for both desktop input and Xbox-style controllers.
- **Modular character creator** — hairstyle, face, shirt, shorts and footwear are assembled from skeletal-mesh pieces and face textures driven by `wardrobe.json`, with saved appearance presets.
- **Slate UI and HUD** — main menu, character editor, settings, pause screen, credits, goal counter, contextual shot feedback and an in-game control strip share one visual language.
- **Persistent settings** — appearance, camera mode, sensitivity, audio levels and graphics quality are stored through Unreal's save system.

## Controls

| Action | Keyboard / Mouse | Xbox-style gamepad |
| --- | --- | --- |
| Move | `WASD` | Left stick |
| Camera | Mouse | Right stick |
| Ball control | `Left Ctrl` | `LT` |
| Sprint | `Left Shift` | `RT` |
| Jump / goal celebration | `Space` | `A` |
| Charge and release shot | Hold / release `LMB` | Hold / release `X` |
| Slide | `RMB` | `B` |
| Reset ball | `R` | `Y` |
| Pause | `Esc` | Menu button |

The character editor also supports drag-to-rotate, `Q` / `E` rotation and mouse-wheel zoom. The right stick provides rotation and zoom on a gamepad.

## Technical overview

- Unreal Engine 5.8
- C++ gameplay, movement, animation and UI logic
- Slate-based menus and HUD
- Unreal physics simulation for the ball and goal net
- Custom `UAnimInstance` proxy for clip sampling and blending
- Custom `UCharacterMovementComponent` behavior for momentum and action movement
- JSON-driven modular wardrobe data
- `USaveGame` persistence for player appearance and settings

The pitch, gameplay state and core interaction systems are created and controlled from the runtime code, while the character, animations, materials, audio and map assets live under `Content/Erling`.

## Project status

**Work in progress.** Erling is an actively developed portfolio prototype focused on responsive character control, readable football interactions and a cohesive presentation rather than a full competitive football game.
