# Roadmap: Custom Modes

What we are working on, what comes next, and where you can help.

Dungeon Settlers Modded is made of three repos. Each item below is listed once, in the repo that does the work:

- **Custom Modes** (this repo): My Units, Unit Builder, Gallery, Test Fights, the Foreign Expedition hub, PvP.
- **Mod loader** ([ds_modloader](https://github.com/AmfiLogos/ds_modloader), [its roadmap](https://github.com/AmfiLogos/ds_modloader/blob/main/ROADMAP.md)): the app, the Mods window, which mods each world uses, the installers.
- **Multiplayer mod** ([ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer), [its roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md)): playing together, the Multiplayer page, the release downloads.

## Now

- **PvP between Mac and Windows stays in sync.** The fix is done here and ships in v0.1.14 (see the [multiplayer roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md)).
- **Plainer text.** We are rewriting the text in the Custom Modes windows and hover cards in plain words. Some of the current text reads like a programmer's notes.

## Next

- **A proper map for the Foreign Expedition hub.** Today the hub uses the wiki's Finished Base. We want something nicer: a world built by a real player (with that player's OK), or a map made for the hub.

## Later / ideas

- **PvP v2: fog of war and respawns.** PvP v1 has no fog of war between teams, and units that die stay dead.
- **Foreign Expedition with Steam Cloud.** So far we have only tested the hub on copies of saves. Before we call it finished, we want to test it with Steam Cloud saves turned on.

## Needs help / testing

Have you tried one of these? Please open an issue and tell us what happened.

- **A world for the hub.** Do you have a good-looking, well-built world you would be happy to share? It could become the Foreign Expedition hub. We could not find one shared online.
- **An expedition on Windows with a My Units unit.** We tested this on Mac only. On Windows our test could open Bring Units, but it could not tick a unit.
- A real Windows PC, Steam on Windows, and playing over the internet are on the [multiplayer roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md).

## Known issues

- **The Unit Builder window does not edit everything.** Talents, portrait, soul stones, traits gained in play, elixirs and hunger or exhaustion are not in it. A unit you open from a wiki link keeps these as they are.
- **Gear rolls use the middle value.** In the Unit Builder you pick which stat each random gear roll gives. Its value is the middle of its range, the same default the wiki uses. There is no slider.
