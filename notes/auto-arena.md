# Automatic arena rows for every boss (user #23436/#23447/#23451/#23479) - DRAFT

Goal: Nioh3Training_arenas.tsv generated from the game folder's own data for every boss (incl. DLC), no hand lists, no exe addresses.

Status: WIP. tools/n3lfm.py reads the LFM archives straight from the game folder (archive/archive_NN.bin + lfm_order_NN.bin + loose
files); tools/auto_arena.py discovers the id table / mission table / chara table by structure and rebuilds the arena table.
Per-boss table and runtime-side analysis follow.
