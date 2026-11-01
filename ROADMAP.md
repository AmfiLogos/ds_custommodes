# Roadmap: Units Laboratory

What we are working on, what comes next, and where you can help.

Updated: 2026-11-01

Dungeon Settlers Modded is made of three repos. Each item below is listed once, in the repo that does the work:

- **Units Laboratory** (this repo, also called Custom Modes): My Units, the Unit Builder, the Wiki Build Gallery, Test Fights and the DPS Check.
- **Mod loader** ([ds_modloader](https://github.com/AmfiLogos/ds_modloader), [its roadmap](https://github.com/AmfiLogos/ds_modloader/blob/main/ROADMAP.md)): the app, the Mods window, the Load Campaign list, the parts that every mod window shares (moving, the "-" and "+" size buttons), exporting your units to the wiki, the installers.
- **Multiplayer mod** ([ds_multiplayer](https://github.com/AmfiLogos/ds_multiplayer), [its roadmap](https://github.com/AmfiLogos/ds_multiplayer/blob/main/ROADMAP.md)): playing together, the Multiplayer page, the chat, the Foreign Expedition hub and PvP.

## Now

These are being built for the next version.

- **Test Fights keep your settings.** The settings of your last test stay when you start a new test, also after you restart the game. <!-- id: test-settings-kept -->
- **The hover card of the kits in a test shows next to the item.** Today it shows at the far left of the test window. <!-- id: test-kit-hover-card -->
- **The DPS meter shows every unit in the fight.** Today it shows at most two rows. It will show one row per unit, also when the fight has only one unit. The list grows with the units, or scrolls when the window cannot grow. <!-- id: dps-all-units -->
- **"Waiting for the first hit" sits in the right place.** Since the "-" and "+" buttons came, this text is out of place. It will stay centred in the empty list, clear of the buttons, at every panel size. <!-- id: dps-waiting-text -->
- **Fold the sections of the test window.** Click the title of Settings, Scarecrows, Pitch and the other sections to fold or unfold them. They stay folded when you open the window again. <!-- id: test-sections-fold -->
- **Test fights start with Auto Skill off.** <!-- id: test-autoskill-off -->
- **Test fights take place in the day.** A test fight starts at 8 AM by default, and a setting picks day or night. <!-- id: test-time-of-day -->

## Next

- Nothing else is planned for the next version. The Now list comes first.

## Done

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
