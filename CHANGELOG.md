# Changelog

What changed in each version, newest first. Downloads: the GitHub releases of this repo.

## Units Laboratory (custom-modes)

### 0.1.47 (2026-10-26)

Performance:
- Less stutter while you play: Units Laboratory no longer searches the whole game scene twice every 30 frames to check
  for the title screen. It now reads which screen the game shows, so it costs nothing in a world or while a world loads.
- The icons that load ahead for the Units Laboratory windows load only on the title screen. Before, they went on loading
  in a world, a few milliseconds every 50 ms.
- In a DPS Check world the DPS meter remembers the Inspect window instead of searching for it every 30 frames.
- With a mod loader that has its shared window lookup, Units Laboratory finds the game's title menu and Inspect window
  through it, so it never searches the scene for them.

Test Fights:
- World Units shows four unit cards at once (it showed two): the cards are shorter (the name and the race share one
  line), and the window grows taller when the screen has room.
- Units picked straight from a save keep that save's Auto Skill switch and their per-skill auto-use choices. Before,
  every unit started with Auto Skill on and every skill on auto. Units from My Units, the Gallery and the Unit Builder
  still start with Auto Skill on.
- Test worlds stay out of the Load list and the Steam Cloud backup for good. Before, a test world was only known until
  you went back to the title, so a copy left in the cloud showed up as a normal save, also on another computer. New test
  worlds carry a mark in their save id, and this computer keeps a list of every test world it made.

DPS Check:
- "DPS Log in Chat" acts at once: off takes the log out of the chat, on brings the whole log back, including the hits
  made while it was off. Before, off left every line in the chat and on lost the lines of the off time.
- The DPS meter and the DPS log panel have "-" and "+" buttons at the right end of their title bar. Each click makes the
  panel and its text 10 % smaller or bigger, from 60 % to 150 %. The size is kept for your next game.

### 0.1.43 (2026-10-25)

Windows:
- My Units, the Unit Builder, the Wiki Build Gallery and Test Fights have one window style: a dim backdrop that blocks
  clicks, a centred title, and the game's red X at the top right as the only close control. The My Units "Close" button
  and the Unit Builder "Cancel" button are gone: the X does their job.
- Escape closes the Units Laboratory windows. On a page with a Back button (for example the Gallery's Details page), it
  goes back one level first. In the Unit Builder, unsaved changes ask "Discard your changes?" first.
- The title screen's corner buttons (Roadmap, Wishlist, Discord) hide while a Units Laboratory window is open.
- A click anywhere on a card ticks it: My Units cards, and the world cards and team rows of Test Fights.
- The Delete Team box: Cancel comes first, the whole screen is dimmed, and its red X cancels.
- Text fit: talent tier chips are as wide as their labels, and a boss card shows one stat per row when the names do not
  fit.
- English buttons, rows and headings use Title Case. Better German and Russian wording.
My Units:
- Unit cards show the unit's teams.
- A "Rename Team" button next to "Delete Team".
- Four units per page again: the snapshot's source line is now inside the card (hover it to read all of it).
Test Fights:
- World Units: pick units straight from one of your saves. They bring the save's skill bar and bag. The switch "Bring
  Buffs and Effects from the Save" (off by default) also brings their effects.
- "Pick 1-4" on a world row ticks the save's first four units, in the order of the game's portrait bar.
- Teams and worlds start folded. An arrow opens one.
- Units cast their skills in test fights: Auto Skill is on for every squad unit.
- A renamed unit fights under its My Units name. Default unit names show as names, not as raw text keys.
- A test fight no longer changes the title screen's background or your game records.
- "Restart Test Floor", in every test fight, starts the fight again from its start.
DPS Check:
- The scarecrow's HP shows in full on one line, with your language's thousands separator (for example 997,296 /
  1,000,000).
- The bottom panel and the Inspect window follow the scarecrow's HP while you look at them. With the loader's "Live
  building HP" fix on, that fix updates them.
- A DPS meter panel: Start, Stop and Reset, "Start on First Hit", damage per unit and per skill, a Healing column, an
  Absorbed line and what the scarecrow repaired.
- The DPS log goes into the multiplayer chat (without it, into a small log panel). It shows every damage, heal (+X, in
  green), shield absorb and effect put on, one line per unit and game tick, under the unit's name. A shield absorb never
  counts as damage.
- Basic attacks show as "<name> (basic)".
- A unit's summons count for that unit, in the log ("<name> (summon)") and in the meter.
- An effect writes a line only when it adds stacks: an effect put on again at its cap writes nothing.
- The switches Scarecrow Speech, Scarecrow Heal Numbers (both off by default), DPS Log in Chat and Effects in Log are on
  the meter, not on the start screen, and act at once. "Effects in Log" also hides or shows the effect lines that are
  already in the chat.
- The meter, the Restart panel and the log panel move with a right-click drag and keep their place.
- Unit names in the log use the chat's normal font, in gold.
- New texts in all 10 languages.

### 0.1.16 (2026-10-25)

- My Units, Delete team: a box asks each time: "Team and units", "Team only" or "Cancel", and shows how many units go. A
  unit that is also in another team stays.
- My Units, World units: the worlds of Load Campaign (game saves and modded saves) in one list, newest first, each with
  its newest save and save time. A world's units show as read-only cards. Saves are only read.
- Save snapshot: tick units of a world to keep them as normal My Units units. The card shows where the unit came from
  (world, save, save time).
- Texts in all 10 languages.

### 0.1.14 (2026-10-24)

- The texts of Units Laboratory now look like the game's own menus: the same text style as the title menu, Settings and
  Load Campaign (in every language), and the Units Laboratory buttons in the title menu have the same size as the game's
  own buttons there.

### 0.1.13 (2026-10-21)

- The Gallery, Test Fights and the Unit Builder follow the game's UI scale and fit inside the game window at every size.
- Cancel in the Unit Builder after a change asks "Discard your changes?".
- Gallery card buttons are no longer cut; lists have scroll bars.
- Chinese, Japanese and Korean text is never smaller than 14 pt (design size).
- Better German, Russian and Japanese wording, and the decimal comma in German, Spanish, French, Portuguese and Russian.

### 0.1.12 (2026-10-21)

- PvP and Foreign Expedition can bring units from My Units, and "Build a unit" opens the Unit Builder from the Bring
  Units window. The unit travels with Bring, so other players do not need Units Laboratory.

### 0.1.11 (2026-10-20)

- PvP and the Foreign Expedition hub moved into the Multiplayer mod. Units Laboratory no longer needs Multiplayer, and it
  never blocks joining a friend.

### 0.1.10 (2026-10-20)

- Windows: units you save to My Units from the Gallery now show in My Units.
- The Unit Builder and the Gallery no longer stutter when you click: the icons load ahead while the Units Laboratory page
  is open.

### 0.1.9 (2026-10-20)

- Listed as a launcher feature in the Mods window: it is no longer part of a world's mod set, so worlds saved with or
  without it load the same way.

### 0.1.8 (2026-10-19)

- Works with newer versions of the multiplayer mod.

### 0.1.7 (2026-10-18)

- Built for Dungeon Settlers Modded v0.1.15. Nothing changes in the game.

### 0.1.6 (2026-10-18)

- Built for Dungeon Settlers Modded v0.1.14. Nothing changes in the game.

### 0.1.5 (2026-10-18)

- Custom Modes is now called Units Laboratory. Its title button shows a random recruit, new each time.
- Unit Builder: the look of the character sheet, search and filters (several choices at once), Level-ups and Presets
  tabs. You can start from a Wiki Build Gallery build. It shows the unit's portrait and its character in the game.
- Wiki Build Gallery: Compact Plus and List views, a Details preview, each unit's portrait and sprite. Race, background
  and weapon names are the game's own words.
- Test Fights: pick squad units one by one. Boss card and DPS check card. Battle kit names are translated.
- PvP: the Rules panel fits its text.
- Its texts are in the game's language from the first second.

### 0.1.4 (2026-10-17)

- All its texts follow the game's language: English, French, German, Spanish, Portuguese (Brazil), Russian, Japanese,
  Korean, Chinese (Simplified) and Chinese (Traditional).

### 0.1.3 (2026-10-17)

- The first release under the new version numbers. It plays the same as the Custom Modes in bundle-v0.1.16.

## Old bundles (bundle)

### 0.1.16 (2026-10-16)

- Custom Modes is the same as in bundle-v0.1.15.

### 0.1.15 (2026-10-16)

- Custom Modes adds My Units, the Unit Builder, the Gallery, Test Fights, the Foreign Expedition hub and PvP to Dungeon
  Settlers Modded. It needs the Multiplayer mod.
- We rewrote its menu text and descriptions in plainer words.
