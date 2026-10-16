# Custom Modes for Dungeon Settlers

**Roadmap:** what we work on now and next, and where you can help: [ROADMAP.md](ROADMAP.md).

Custom Modes is a code mod for Dungeon Settlers. It adds:

- **My Units**
- **Unit Builder**: race, background, traits, level, skills and gear, then Save to My Units.
- **Gallery**
- **Test Fights**
- **Foreign Expedition hub**: a third choice on the multiplayer Host page. Players bring units from a save or from My Units.
- **PvP**: from the Esc menu, and in the hub.

Foreign Expedition and PvP need the multiplayer mod. Without it, they stay hidden.

## What you need

- The mod loader: [github.com/AmfiLogos/ds_modloader](https://github.com/AmfiLogos/ds_modloader)
- The multiplayer mod: [github.com/AmfiLogos/ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer)

## Download

Do not build this repo to play. Custom Modes ships inside the Mac and Windows kits on the
[ds_multiplayer releases page](https://github.com/AmfiLogos/ds_multiplayer/releases).

## Build

`build.sh` builds the Mac (`custom-modes.dylib`, `custom-modes-mp.dylib`) and Windows
(`custom-modes.dll`, `custom-modes-mp.dll`) files. It uses headers and the Windows build from one
ds_multiplayer commit, named in `mp_pin.txt`. Read the top of `build.sh` for the settings.
The Windows build also needs local game files that are not in any repo.

No game files, dumps or saves are in this repo.

## License

MIT. See [LICENSE](LICENSE).
