# Units Laboratory for Dungeon Settlers

**Roadmap:** what we work on now and next, and where you can help: [ROADMAP.md](ROADMAP.md).

Units Laboratory (mod id `custom-modes`; this repo's old name is Custom Modes) is a code mod for Dungeon Settlers.
It adds to the title screen:

- **My Units**
- **Unit Builder**: race and portrait, background, traits (also those gained in play and a soul stone), level-ups (offering, stat gain, talents, elixirs), skills, gear (with quick sets), inscriptions, with search and filters, then Save to My Units.
- **Gallery**
- **Test Fights**

It is a launcher mod: you never need it to host or join a multiplayer game. Foreign Expedition and PvP are part of
the [multiplayer mod](https://github.com/AmfiLogos/ds_multiplayer) since 2026-10-20; there every player brings units
from his own saves.

It is made for Steam build 25846389 of Dungeon Settlers.

## The three repositories

- ds_custommodes (this repo): the Units Laboratory mod.
- [ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer): the Multiplayer mod.
- [ds_modloader](https://github.com/AmfiLogos/ds_modloader): the mod loader, the Mac app and the Windows installer.
  Its releases page has the installers.
  Read its README for install steps, save folders, known limits and bug reports.

## What you need

- The mod loader: [github.com/AmfiLogos/ds_modloader](https://github.com/AmfiLogos/ds_modloader)

## Download

Do not build this repo to play. Units Laboratory is inside the installers (`Dungeon-Settlers-Modded-Mac.dmg`,
`Dungeon-Settlers-Modded-Setup.exe`) on the
[ds_modloader releases page](https://github.com/AmfiLogos/ds_modloader/releases).
This repo's releases page has `custom-modes-<version>.zip` (tag `custom-modes-v<version>`), to add or update the
mod by hand.

## Report a bug

Open an issue on this repo's Issues page. Say your system, the release and what you did, and attach the log
files. The ds_modloader README says where they are. Do not attach save files.

## Build

Full steps: [BUILDING.md](BUILDING.md). `build.sh` builds the Mac (`custom-modes.dylib`) and Windows
(`custom-modes.dll`) files. It uses headers and the Windows build tools from one
ds_multiplayer commit, named in `mp_pin.txt`. Read the top of `build.sh` for the settings.
The Windows build also needs local game files that are not in any repo.

No game files, dumps or saves are in this repo.

## License

MIT. See [LICENSE](LICENSE). Code from other projects keeps its own license: QuickJS
(`fights/quickjs/LICENSE`) and tinf (`fights/tinf/LICENSE`).
