# Compact ring sizes — 40972

Base 430624ad (combined 1.7.85); 01a19dbdd is an ancestor. Existing hit facts, smooth ring and Linux opener are retained.

The default ring is Small: 160 px diameter at 1080p instead of 260 (38.5% smaller). Medium is 210; Large is the prior 260. Minimum diameters at small viewports are 128/154/180 px; maximum stays 512. Centre labels scale to the ring. Shared readout and legend retain their existing size, and Row mode is unchanged.

Persistent Training ABI slot 34 / [Practice] FrameMeterRingSize: 0 Small (default), 1 Medium, 2 Large. The new EngineFix Ring size row is directly below Frame meter style. Left/right clamp; confirm cycles. Existing INIs gain the safe Small default when the key is absent. An old Training module shows needs newer Training.

Validation: all seven Training offline suites pass, including 16,261,412 production-renderer assertions across all three sizes at 800x600 through 4K. Both actual menu suites pass. Tests cover ABI/INI roundtrip, default, clamping/cycling, menu dispatch, row invariance, increasing ring diameters, colors/alpha and lane visibility. Production previews generated without CPU timing runs or game access.

Training release SHA256: 8fb6b814368e4dd23aa6069e4be8d0927cdb6af1b3693ca1f2c482c27876b498.
Install Training for the smaller default; include the companion EngineFix build for the menu size selector. Do not overwrite the player's INI with the repository sample. No installed files were modified.
