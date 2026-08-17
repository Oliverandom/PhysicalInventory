# Physical Inventory System for OFP/CWR 

<img width="1919" height="1199" alt="Iventory Demo 1" src="https://github.com/user-attachments/assets/26dca477-001d-4b5b-aa4d-71de0765d373" />


Physical Inventory replaces Cold War Assault's abstract gear handling with a modern, physical inventory — a Reforger-style drag-and-drop grid where every item has weight and volume — and builds a small web of systems on top of it: encumbrance, stamina, first-aid, and a full jerry-can fuel economy with realistic per-vehicle tank sizes. Nothing is scripted per mission; it all lives in the engine, so it works everywhere in the base game, the campaign, and the editor.

It's an unofficial community mod built on Bohemia Interactive's GPL-3.0 source release. It is not affiliated with or endorsed by Bohemia Interactive.

---

## Requirements

- *Arma: Cold War Assault* (Remastered). The free Steam demo (app 4819000) provides compatible game data.
- Windows x64.

## Install (short version)

Run `install.bat` — it drops a separate `PhysicalInventory.exe` next to your normal game exe (nothing is overwritten) plus four icon files, and creates a launcher/shortcut. Launch with **"Play Physical Inventory.bat"** or the desktop shortcut (they start the game with `-mod=Remastered`, which is required for the correct look). Full details in `README.txt`.

## Default keys

- **O** — open / close the inventory
- **K** — open the tuning / debug menu (sliders and toggles below)

Both are rebindable under Options → Controls ("Inventory" and "Inventory Edit Menu"). They were chosen because nothing in the stock game uses them.

Scroll through your inventory with the scroll wheel and clickable arrows on the right, and through Vicinity using Q and E, just like in Reforager.

**Hotkeys:**

You can bind any weapon, grenade, mine and medkit to a hotkey by right clicking the inventory slot and pressing a number from 1 - 0. You can then swap between your items quickly like in any modern game. 

---

## What it changes — full feature list

### 1. Grid inventory
A drag-and-drop inventory screen with two panes: your own storage grid on one side, and a "Vicinity" list of everything nearby on the other (containers, ammo crates, dead bodies, the gear of squadmates you command, and vehicles you're next to).

**How it works.** Every item occupies a rectangular footprint sized to what it is — pistols and most items are 1 tile, rifles and launchers span their real length, a jerry can is 2×3. You drag items between your grid and any nearby source to pick up, drop, give, or store them. Your carry capacity is limited by both **weight** and **volume**, not a fixed slot count, so a light-but-bulky load and a small-but-heavy load are both constrained realistically. Rifles and launchers normally live only in the two loadout slots; an optional toggle lets them sit in the backpack grid too, like in STALKER or Tarkov (Rifles in Backpack). 

### 2. Encumbrance
What you carry actually weighs you down.

**How it works.** Total carried weight is summed from every item's real mass. Past ~20 kg your sprint stamina starts burning faster; the penalty ramps up steeply through the "amber" band and by ~50 kg you're **too encumbered to run at all** and are forced to a walk. Weight also **degrades mouse-look sensitivity** — a heavy pack makes your aim swing slower and heavier, easing back as you drop weight. The mod assumes an invisible backpack for all sides: 

## Backpack / carry-volume capacity by side (~1985)

| Side | Backpack | Pack | + Extra (pouches/pockets) | **Total** |
|---|---|---:|---:|---:|
| **West** (US) | Large ALICE rucksack | 62 L | ~13 L | **~75 L** |
| **East** (USSR) | Veshmeshok "sidor" | 30 L | ~6 L | **~36 L** |
| **Resistance** (ČSLA) | vz. 85 rucksack | 35 L | ~10 L | **~45 L** |
| **Civilian** (Czechoslovak) | everyday *batoh* | ~25 L | ~3 L | **~28 L** |

The **Total** column is the effective carried-volume limit per side (backpack plus what fits in web gear / bread bag / pockets). West carries the most, civilians the least.

For reference, that pairs with the **weight** limits — civilian 50 kg, resistance 55 kg, soldiers (West/East) 60 kg — so both volume and weight cap your load.

### 3. Stamina
A sprint bar that behaves like a real one.

**How it works.** Only sprinting costs stamina — walking and normal jogging are free. A full bar is worth roughly 2 km of running, drained by the distance you actually cover, and it recharges over time (fastest crouched/prone, slower standing, slowest while walking). Conditioning is per faction: regular soldiers get the full ~2 km, resistance fighters ~1.2 km with slower recovery, and civilians ~0.8 km with the slowest recovery. An on-screen stamina bar (and a health bar) show the current state; both can be toggled and dimmed in the tuning menu.

### 4. First-aid kits
Two carryable medkits — a US IFAK and a Soviet/Resistance AI-2 kit — each good for three full heals.

**How it works.** With a kit in your inventory you get a **"Use First Aid Kit"** action. It plays a medic animation and, on completion, fully heals you and spends one use — **but pressing a movement key cancels it** (no kit wasted) so an accidental use costs nothing. Aiming at a wounded ally within 2 m instead gives a **"Heal soldier / civilian / guerilla"** action (labeled by their side) that plays the being-treated animation, heals them, and updates their status in the command bar. **AI medics use their own kits too** — a wounded AI below ~80 % health will bandage itself, but only in a lull (no active fire target) so it won't stop mid-firefight. Ambulances (BMP and M113 ambulance variants) and the MASH field hospital each carry 12 kits.

### 5. Jerry cans and a fuel economy
A 20-litre jerry can you can carry, fill, and pour.

**How it works.** The can shows a green fuel-level bar and weighs realistically — about 4 kg empty plus 0.85 kg per litre, so ~21 kg full. Standing near a vehicle or a map gas station gives you **"Syphon Fuel from X"** (pull fuel into the can) and **"Refuel X"** (pour the can into the vehicle). Transfer is gradual — one litre every 0.35 s, ~7 s for a full can — with the same lower-screen "Refueling" / "Syphoning" flicker text and pouring sound the stock fuel truck uses. Every military car and truck spawns with one can; every tank with two.

### 6. Vehicle fuel + speed HUD
When you're in a ground vehicle, the on-foot health/stamina bars are replaced by a fuel gauge.

**How it works.** A colour-coded bar (green ≥ 50 %, amber 10–50 %, red < 10 %) stretches across the lower-right with a fuel-pump icon, your current speed in **km/h** above its left end and the litres (`current / max`) above its right end. Aircraft are skipped — they have their own instruments. The gauge can be toggled and dimmed in the tuning menu.

### 7. Realistic vehicle fuel capacities
Stock Cold War Assault gives every tank 700 L, every APC 700 L, every aircraft 1000 L, and so on. Physical Inventory replaces those coarse defaults with each vehicle's actual tank size, so range, refuelling, and the fuel gauge all read true to life. (Full table below.)

### 8. Tuning / debug menu (press K)
An in-game panel of sliders and toggles: global and per-icon size, grid columns, volume cap, panel dimensions, accent colours; show/hide and opacity for the health-stamina bars and the fuel/speed HUD; "allow rifles/launchers in backpack"; aim-sensitivity floor; and infinite weight / volume / stamina cheats for testing.

### 9. Arsenal crate
Adds an **"Ammo Crates (Arsenal)"** editor object — an infinite gear source that lists every weapon and magazine, so you can kit out from the full catalogue in the editor and mess around.

---

## Vehicle fuel capacities (litres)

Everything below was set to its researched real-world figure. "Stock" is the value vanilla used.

| Class | Vehicle | Stock | Physical Inventory |
|---|---|---:|---:|
| Tank | M1 Abrams | 700 | 1909 |
| Tank | M60 | 700 | 1420 |
| Tank | T-55 | 700 | 680 |
| Tank | T-72 | 700 | 705 |
| Tank | T-80 | 700 | 1090 |
| Tank | ZSU-23-4 Shilka | 700 | 515 |
| APC/IFV | BMP-1 | 700 | 462 |
| APC/IFV | BMP-2 | 700 | 460 |
| APC/IFV | M2 Bradley | 700 | 662 |
| APC/IFV | M113 | 700 | 360 |
| APC/IFV | M163 Vulcan | 700 | 360 |
| Car | Jeep (M151) | 50 | 67 |
| Car | UAZ-469 | 100 | 78 |
| Car | Škoda | 50 | 45 |
| Car | Aston (Rapid) | 50 | 80 |
| Car | Mini | 50 | 48 |
| Car | Tractor | 50 | 65 |
| Truck | Bus | 100 | 200 |
| Truck | Ural-4320 | 100 | 360 |
| Truck | Praga V3S | 100 | 120 |
| Truck | M35 "5-ton" | 200 | 189 |
| Truck | Scud TEL | 100 | 700 |
| Bike | Jawa motorcycle | 50 | 14 |
| Bike | Kolo (bicycle) | 50 | 0 |
| Heli | AH-64 Apache | 1000 | 1423 |
| Heli | AH-1 Cobra | 1000 | 935 |
| Heli | Mi-8/17 | 1000 | 1870 |
| Heli | Mi-24 Hind | 1000 | 1840 |
| Heli | Ka-50 (Kamov) | 1000 | 1730 |
| Heli | OH-58 Kiowa | 1000 | 424 |
| Heli | UH-60 Black Hawk | 1000 | 1361 |
| Plane | A-10 | 1000 | 6132 |
| Plane | Cessna | 1000 | 212 |
| Plane | Sopwith Camel | 1000 | 37 |
| Boat | Zodiac | 700 | 95 |
| Boat | PBR (Mark II) | 700 | 400 |

Variants (ambulance, reammo/refuel/repair, resistance, etc.) inherit their base vehicle's value.

---

## License & credits

The engine and this modification are licensed **GPL-3.0-or-later** (with Bohemia's Section 7 additional terms). Corresponding source: **https://github.com/Oliverandom/PhysicalInventory** — a fork of Bohemia Interactive's [CWR release](https://github.com/BohemiaInteractive/CWR).

Game data (models, textures, sounds, missions) is separate under the Arma Public License Share Alike (APL-SA) and is **not** included. The fuel-pump HUD icon is recolored from a base-game icon and is used under APL-SA.

"ARMA" and "Operation Flashpoint" are trademarks of their respective owners. Physical Inventory is an unofficial community mod, not affiliated with Bohemia Interactive.
