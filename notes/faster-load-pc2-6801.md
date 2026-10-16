# Faster cross-world Fight now - pc2 (task 6801)

Recommended INI (Nioh3Training.ini / Rematch section):
    CrossWorldPrewarm=2
    HoldMs=120000

Behaviour: while the Rematch menu is open the target world is prewarmed, so Fight now into another world skips most of the black cover.

Measured on pc2 (16 GB RAM), Shingen cross-world, ~27 s menu dwell:
- OFF: 33.2 / 30.5 s (earlier runs 28.1-41.4 s)
- ON (CrossWorldPrewarm=2, HoldMs=120000): 2.2 / 3.3 s (one earlier run 1.0 s)
- Shorter dwell helps less: 10 s dwell -> 17 s.

Caveats: pc2 only (n=2 ON). pc untested. Ally (15 GB RAM) cannot do cross-world swap. An invalid run happened when the game read CrossWorldPrewarm=0 (INI not applied before fight fired) - verify the value is loaded.
Branch: pc2/faster-load 9e53d7b.
