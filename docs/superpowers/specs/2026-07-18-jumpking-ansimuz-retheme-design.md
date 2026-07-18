# Jump King Re-theme with Ansimuz CC0 Assets — Design

**Goal:** Replace the current Pixel Adventure / Kenney / Kings-and-Pigs art with a
cohesive Ansimuz (Luis Zuno) CC0 set so the game reads as a Jump King style
medieval ascent: one armoured hero climbing through forest, castle, and frozen
peak zones.

**Status:** Design approved (direction). Blocked on asset acquisition (see Open
Questions) before implementation.

## Why Ansimuz

Jump King's own art is copyrighted (Nexile) and cannot be used. Ansimuz's CC0
pixel packs share one sombre medieval-fantasy tone, which matches Jump King far
better than the current mixed packs, and a single author keeps all four atlases
visually cohesive across the climb.

## Art Direction

- **Hero (all 12 rooms):** an armoured knight-like hero from **Gothicvania**
  (the SunnyLand default character is a woodland animal and does not fit).
  Mapped to the six existing animation states:
  - `idle` ← Idle, `run` ← Run, `charge` ← Crouch/ground pose,
    `rise` ← Jump, `fall` ← Fall, `respawn` ← Hurt/Dead.
- **Three biomes = three ascent zones** (rooms 1-4 / 5-8 / 9-12):
  - Biome 1 `pixel_adventure` slot → **Forest** (SunnyLand Tall Forest / Forest of Illusion).
  - Biome 2 `kenney` slot → **Castle / Gothic ruins** (Gothicvania Cold Corridors).
  - Biome 3 `kings_and_pigs` slot → **Frozen peak** (SunnyLand Winter Forest).

The `Biome` enum values stay the same (`pixel_adventure`, `kenney`,
`kings_and_pigs`) to avoid churning level files and code; only the art each slot
points at changes. A follow-up may rename the enum, but that is out of scope here.

## Pipeline Changes (architecture preserved)

The CC0 reproducible-atlas pipeline is kept as-is; only the extraction inputs change.

- `tools/build_assets.py`: rewrite the per-pack extraction (source member names,
  frame sizes, terrain cell picks) for the Ansimuz packs. The King-registration
  logic added in commit `97aca8f` (shared content box, feet-on-floor, centred)
  is reused for the new hero so the renderer's feet anchor keeps working.
- `assets/sources/<pack>/SOURCE.md`: one per Ansimuz pack, recording creator,
  official URL, **CC0-1.0**, retrieval date, archive name, generated outputs.
- `assets/generated/`: regenerate `player.png` + three biome atlases + `manifest.json`;
  must be byte-for-byte reproducible and pass `tools/verify_assets.py`.
- `assets/sources/downloads/`: stays gitignored; new archives never committed.
- Renderer: `player_sprite_display` and the feet anchor in
  `include/jumpcastle/player_view.hpp` may be re-tuned for the hero's proportions;
  no structural change. New per-state frame counts come only from the manifest.

## Out of Scope (YAGNI)

- No enemies, combat, collectibles, or extra props even though the packs ship them.
- No level-geometry changes; the twelve verified rooms and reachability proof stay.
- No `Biome` enum rename.

## Verification

- `verify_assets.py` green (schema, bounds, digests, CC0 sources, King states,
  no tracked ZIPs).
- All existing tests still pass (37/37), including reachability and the
  `player_view` anchor tests.
- Runtime smoke: hero visible and animating in rooms 01/05/09/12; feet on ground;
  flip symmetric; each biome's terrain/background renders.

## Open Questions / Blockers

1. **Per-pack CC0 confirmation.** The Ansimuz index lists packs as "free" without
   an explicit CC0 label on the landing page. Each pack's own itch page license
   field must confirm CC0-1.0 before use; if any is not CC0, pick an alternative
   CC0 pack of the same theme.
2. **Archive download.** Implementation cannot start until the confirmed Ansimuz
   archives are present in `assets/sources/downloads/`. Downloading is a
   permissioned/manual step (the original Task 5 had the user download packs
   manually via itch's "just take me to the downloads"). Candidate packs:
   Gothicvania (hero + castle), SunnyLand Tall Forest, SunnyLand Winter Forest.
3. **Exact frame dimensions** per hero state and per tileset are verified against
   the real archives during acquisition; `build_assets.py` fails loudly on any
   mismatch, so wrong assumptions surface immediately rather than silently.
