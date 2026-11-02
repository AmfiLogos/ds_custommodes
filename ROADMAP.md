# Roadmap: Units Laboratory

What we are working on, what comes next, and where you can help.

Updated: 2026-11-02

Dungeon Settlers Modded is made of three repos. Each item below is listed once, in the repo that does the work:

- **Units Laboratory** (this repo, also called Custom Modes): My Units, the Unit Builder, the Wiki Build Gallery, Test Fights and the DPS Check.
- **Mod loader** ([ds_modloader](https://github.com/AmfiLogos/ds_modloader), [its roadmap](https://github.com/AmfiLogos/ds_modloader/blob/main/ROADMAP.md)): the app, the Mods window, the Load Campaign list, the parts that every mod window shares (moving, the "-" and "+" size buttons), exporting your units to the wiki, the installers.
- **Multiplayer mod** ([ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer), [its roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md)): playing together, the Multiplayer page, the chat, the Foreign Expedition hub and PvP.

## Now

- Nothing is being built right now. New items come here when work starts.

## Next

- Nothing is planned yet.

## Done

- **0.1.48 (2026-11-02, kit v0.1.30).** Test Fights keep all your choices, also after a restart, and every section folds. A new Advanced section: Time of Day (day by default), Weather (clear by default) and a DPS Meter switch for Custom and Boss fights. Test units start with Auto Skill off. "Open in Wiki Builder" sends your world's units to the wiki's Character Builder. The DPS meter shows every unit, keeps its size when you fold it, and in a DPS Check every unit first gets a move order toward the scarecrow.
- **0.1.47 (2026-10-26, kit v0.1.29).** Less stutter while you play. Test Fights World Units shows four unit cards at once. Units picked from a save keep that save's Auto Skill choices. Test worlds stay out of the Load list and the Steam Cloud backup for good. "DPS Log in Chat" acts at once, and the DPS meter and the DPS log have "-" and "+" size buttons.
- **0.1.43 (2026-10-25, kit v0.1.28).** One window style for all Units Laboratory windows, closed with the game's red X or Escape. The DPS Check: a DPS meter, a hit log in the chat and Restart Test. Units from your saves in Test Fights. Rename Team.
- **0.1.16 (2026-10-25).** Delete Team asks "Team and units" or "Team only". World units: the units of your worlds as read-only cards. Save snapshot.
- **0.1.11 to 0.1.14 (2026-10-20 to 2026-10-24).** PvP and the Foreign Expedition hub moved into the Multiplayer mod, so Units Laboratory never blocks joining a friend. PvP and the hub can bring units from My Units. The texts use the game's own look.
- **Earlier.** My Units, the Unit Builder, the Gallery and Test Fights; plainer text. Every version is in the [CHANGELOG](CHANGELOG.md).

## Needs help / testing

Have you tried one of these? Please open an issue and tell us what happened.

- **An expedition on Windows with a My Units unit.** We tested this on Mac only. On Windows our test could open Bring Units, but it could not tick a unit.
- A real Windows PC, Steam on Windows, and playing over the internet are on the [multiplayer roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md).

## Known issues

- **The Unit Builder window does not edit everything.** Hunger or exhaustion and the shortcut bar are not in it. A unit you open from a wiki link keeps these as they are.
- **Gear rolls use the middle value.** In the Unit Builder you pick which stat each random gear roll gives. Its value is the middle of its range, the same default the wiki uses. There is no slider.
