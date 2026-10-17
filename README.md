# Custom Modes for Dungeon Settlers

**Roadmap:** what we work on now and next, and where you can help: [ROADMAP.md](ROADMAP.md).

Custom Modes is a code mod for Dungeon Settlers. It adds:

- **My Units**
- **Unit Builder**: race and portrait, background, traits (also those gained in play and a soul stone), level-ups (offering, stat gain, talents, elixirs), skills, gear (with quick sets), inscriptions, with search and filters, then Save to My Units.
- **Gallery**
- **Test Fights**
- **Foreign Expedition hub**: a third choice on the multiplayer Host page. Players bring units from a save or from My Units.
- **PvP**: from the Esc menu, and in the hub.

Foreign Expedition and PvP need the multiplayer mod. Without it, they stay hidden.

It is made for Steam build 25846389 of Dungeon Settlers.

## The three repositories

- ds_custommodes (this repo): the Custom Modes mod.
- [ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer): the Multiplayer mod.
- [ds_modloader](https://github.com/AmfiLogos/ds_modloader): the mod loader, the Mac app and the Windows installer.
  Its releases page has the installers.
  Read its README for install steps, save folders, known limits and bug reports.

## What you need

- The mod loader: [github.com/AmfiLogos/ds_modloader](https://github.com/AmfiLogos/ds_modloader)
- The multiplayer mod: [github.com/AmfiLogos/ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer)

## Download

Do not build this repo to play. Custom Modes is inside the installers (`Dungeon-Settlers-Modded-Mac.dmg`,
`Dungeon-Settlers-Modded-Setup.exe`) on the
[ds_modloader releases page](https://github.com/AmfiLogos/ds_modloader/releases).
This repo's releases page has `custom-modes-<version>.zip` (tag `custom-modes-v<version>`), to add or update the
mod by hand.

## Known limits

- We tested an expedition with a My Units unit on Mac only, not on Windows.

## Report a bug

Open an issue on this repo's Issues page. Say your system, the release and what you did, and attach the log
files. The ds_modloader README says where they are. Do not attach save files.

## Build

Full steps: [BUILDING.md](BUILDING.md). `build.sh` builds the Mac (`custom-modes.dylib`, `custom-modes-mp.dylib`) and Windows
(`custom-modes.dll`, `custom-modes-mp.dll`) files. It uses headers and the Windows build from one
ds_multiplayer commit, named in `mp_pin.txt`. Read the top of `build.sh` for the settings.
The Windows build also needs local game files that are not in any repo.

No game files, dumps or saves are in this repo.

## License

MIT. See [LICENSE](LICENSE). Code from other projects keeps its own license: QuickJS
(`fights/quickjs/LICENSE`) and tinf (`fights/tinf/LICENSE`).
