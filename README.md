# Burnout Masters: PSP Edition

An independent PSP homebrew prototype built with C and PSPSDK. It uses original procedural geometry and does not include proprietary assets from the mobile game.

## Current prototype

- Native PSP application target (`EBOOT.PBP`)
- 3D practice arena with a follow camera, perimeter barriers and a pylon slalom layout with basic collision response
- Three proportion families for Australian sedans, Japanese coupes and American muscle, with procedural body panels, cabin glass, mirrors, grille, headlights, tail lamps, side accents and family-specific details
- Throttle, brake, analog/D-pad steering, handbrake and car-specific base grip
- Drift scoring with a building combo multiplier, smoke particles and tyre marks
- Six selectable Australian, Japanese and American-inspired car profiles
- Car purchases using earned in-game cash only
- Garage upgrade shop: engine, grip and tyres, five levels each
- Three timed events: 90-second Drift Session, 60-second High-Speed Challenge and 75-second Slalom Challenge; slalom markers must be hit in sequence and completed laps award bonuses
- Version 3 local save data for cash, cars and upgrade levels, with strict ownership-flag validation
- No real-money purchases, paid currency or ads

## Controls

| PSP control | Action |
|---|---|
| Analog stick / D-pad Left-Right | Steering; car selection in showroom |
| Cross | Throttle / buy or select car / buy upgrade / resume |
| Square | Brake |
| Circle | Handbrake / return to garage while paused |
| Start | Free drive from garage / pause while driving |
| Select | Bank drift score in free drive |
| L trigger | Switch between showroom and upgrade shop |
| Square (garage) | Cycle Drift Session / High-Speed Challenge / Slalom Challenge |
| R trigger | Start the selected event from the garage |
| D-pad Up / Down | Choose an upgrade in the shop |
| Triangle | Save and exit from garage; save while paused |

## Build requirements

Install the PSPSDK toolchain on a computer. The SDK must provide `psp-gcc`, `psp-config`, and the PSP libraries.

From this project directory, run:

```sh
make
```

The build should produce `EBOOT.PBP`. Copy it to:

```text
/PSP/GAME/BMPSP/EBOOT.PBP
```

Then launch **Burnout Masters PSP** from the PSP Game menu. For PPSSPP, copy the same `BMPSP` folder into the emulator's `PSP/GAME/` directory.

## Build and test status

This delivery contains source code and build files, not a compiled or hardware-tested EBOOT. PSPSDK is not installed in the current build environment, so compilation and PSP/PPSSPP runtime testing remain outstanding.

## Known prototype limitations

This is an early playable-game foundation, not a finished commercial-scale recreation. Geometry and effects are procedural and low-poly for PSP-1000 memory constraints. The six vehicles use a shared low-poly base mesh with family-specific proportions and add-on details; fully unique body meshes, more advanced collisions and more varied arenas remain future work. The next development pass should improve vehicle geometry, suspension/tyre behaviour, collision response, event variety, menu rendering and performance. The two current event modes are an early scoring prototype, not a full career system. Save files use a version marker; older prototype saves without that marker are ignored and the game starts with its default garage funds.

## Cloud build (GitHub Actions)

The repository includes `.github/workflows/build.yml`. Open the **Actions** tab and run **Build PSP EBOOT** with **Run workflow**, or push a commit to `main`. A successful run publishes `EBOOT.PBP` as the `Burnout-Masters-PSP-EBOOT` workflow artifact. This workflow downloads the latest Linux x86_64 PSPDEV release; if PSPDEV changes its release asset naming or archive format, the toolchain-install step may need updating.
