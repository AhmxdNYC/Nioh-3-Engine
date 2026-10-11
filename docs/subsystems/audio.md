# Audio (KIDS mixer on XAudio2)
CPU share 2.2 % (Ally 0.4 %, PC 3.9 %); coverage 97.8 %. Threads: audio mix thread (entry 1A786A8, unnamed, TID 5892 in r9, ~0.13 core), AudioThread, SoundSystemUpdateThread (2.7 % of one core in capture 2).

## Role
In-house KIDS mixer (`kids::audio` BeginUpdate/EndUpdate, Mixer/Mastering/DuckingComp strings, libVorbis 1.3.7 decode) feeding XAudio2_9 (0.7 %, incl. X3DAudioCalculate). No Wwise/CRI/FMOD.

## Key data
- Mix thread loop: 87 % blocked in `HandleWaitMs` (100 ms timeout wait); drives sink objects (vfunc +60h state 7 = ready, +88h get buffer) into a ring of 18h-byte slots; mixer block frames +12Ch, channels +11Ch, ring slots +130h; sets MXCSR FTZ.
- Effect chain 36044C -> E2050 (7.6 % of thread) -> reverb E3FF0 -> `ConvReverbProcess` E5394 -> `ConvReverbPartitionMac` E5840 (partitioned FFT convolution: zero-pad to 2n, FFT E7840/E7D00, per-partition MAC); `AudioBiquadProcess` E1740 (direct-form, history f+24h).
- Voice decode `AudioVoiceListDecode` 18AA7C -> `AudioVoiceDecodeBlock` 18AB80 (reader vfunc +60h, 5-tap IIR) -> `AudioVoiceDecodeInner` 69220 (libvorbis). Demand-driven ring refill; only paused voices skipped, zero-gain voices still decode.
- `AudioVoiceMixQueuedBuffers` E08A0 (6.3 KB, ring of 58h-byte buffers) on AudioMixThreadProc -> 361428. `SoundVoiceUpdate` 578E4 (processor chain v+358h).

## Hot functions
| RVA | name | % |
|---|---|---|
| E5840 | ConvReverbPartitionMac | 0.50 |
| 18AB80 | AudioVoiceDecodeBlock | 0.37 |
| E1740 | AudioBiquadProcess | 0.17 |
| 69220 | AudioVoiceDecodeInner | 0.12 |
| 4DBA00 | FftRadix2SplitComplex | 0.08 |

Note: DDC70 PlaceableVisibilityUpdateRange (0.28), SinCosApprox 54360 (0.25), AabbCenter 6A9E0 (0.13) are grouped here by the address heuristic but are not audio.

## What drives the cost
Reverb convolution length x active effect busses, and active voice count. Cheap overall; PC samples include more audio than the Ally. Probe plan (silent-block ratio, zero-gain voices) is in perf-coverage.md.

## Deeper notes
notes/perf-coverage.md (audio thread addendum 2026-10-07), cpu-profile.md (thread table).
