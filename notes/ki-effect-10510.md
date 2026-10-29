# Ki pulse effects 10500 and 10510

Probe #2532 logged effect 10510 on the one native-perfect press and effect 10500 on the other presses. Both came back from caller 2C6D2F.

10500 is hex 2904. 10510 is hex 290E. The executable has no text name for either id.

2C6D2F is the return address inside the wrapper at 2C6CE0. That wrapper forwards the id it was given. The id is chosen at 84422C: start from 10500, and add 10 when the incoming byte is not zero. The Ki press function 844674 passes the byte from 8440FC and uses that same byte to set native bit 0x19.

8440FC returns 1 only when helper F6594 says the tick is active and the timer at +0x74 is not above 4.0. The 4.0 constant is at file offset 3BD433C.

Two other callers can force one id: 200594B always passes the perfect byte when record flag 0x8000000 is set, and 2005983 always passes the normal byte when flag 0x4000000 is set.

## Static rows (#36878)

The 0x68 definition rows are not in re/Nioh3.dump.exe. Confidence 90. Lookup 0x1613D8 reads *(g_DataMgr+0x30), and g_DataMgr is 0x45B9E30. Both ids are members of a packed u32 list at file 0x4121920 (3804 ids, 0x4120eec through 0x4124a5c) with no record body. The float triple at 0x39bb810 is a per-100 curve, not the effect row, and 10510 is absent from that curve. The live dump is bridge file #2537, sha 656cf675, commit b5e370b5, with KiPressProbe=1 and the KIROWDEF line.


## Resource hashes (#36904)

Row +4 is two u32 resource ids, not a file name. 10500 is 0x9D74DF53 and 0xB6FE48A5. 10510 is 0x205ECDCD and 0x88B71D1F. The other 92 bytes of the two rows are identical.

Those 8 bytes occur only in the effect-definition blob data/extract/lfm/archive_03/8D/4CF09EA40BA8F98076F26CBCD62583E15C0FBCC4BA8EAE0285EF6BF2667A62 (455424 bytes, 8-byte header, 4379 rows). The file has no strings. actionsets, rdb names, the rdb index, and exe strings do not contain the ids. kids, ktgl, FNV, and CRC of those names, including UTF-16, do not match.
