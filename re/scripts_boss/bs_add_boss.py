import csv, sys
src, dst = sys.argv[1:3]
B = {
 23: 'tutorial (no boss)', 24: 'tutorial (no boss)', 25: 'training dummy area (no boss)',
 26: 'Tokugawa Kunimatsu (prologue)',
 34: 'Yamagata Masakage', 35: 'Jakotsu-baba (old woman with serpents)', 36: 'Baba Nobuharu (yokai)',
 37: 'Takeda Shingen (human) [+ mounted Yamagata per PowerPyx]', 38: 'Takeda Shingen (yokai)',
 39: 'Ryomen Sukuna', 40: 'Hino-enma',
 45: 'Great Tengu', 46: 'Otoroshi', 47: 'Kajiwara Kagetoki', 48: 'Ibaraki Doji', 49: 'Minamoto no Yoritomo',
 50: 'woman resembling Naotora/Shizuka (web: Kusanagi)', 51: 'Hiruko', 52: 'Tokugawa Kunimatsu',
 57: 'Takasugi Shinsaku', 58: 'armory yokai (unnamed in text)', 59: 'Onmoraki',
 60: 'possessed Buddha statue (web: Nuribotoke)', 61: 'Tokugawa Yoshinobu (yokai) [+ Okita Soji per web]',
 62: 'Shinsengumi remnants at Mibu Temple', 63: 'Hiruko (final) [+ Death Veil per web]',
 67: 'Yagyu Jubei (DLC)', 68: 'Kanai Hanbei (yokai, DLC)', 69: 'Yui Shosetsu (DLC)', 70: 'Yagyu warriors turned yokai (DLC)',
 72: 'Yamagata Masakage (first-play variant)',
 74: 'formidable enemies (web: Kosaka, Naito, Shingen)', 75: "Hattori Hanzo (+ Hanzo's underlings)",
 76: '? (spirit; no boss named in text)', 77: '? (Muramasa grudge spirit)', 78: '? (Horie Castle legend)',
 79: '? (Haunting Caves origin)', 80: '? (web: Kamaitachi)', 81: '? (Yoritomo plot)',
 82: 'formidable enemies (web: Tomoe Gozen, Yoshitsune, Nasu no Yoichi, Kajiwara)',
 83: 'yokai / ascetic monk spirit / warrior ghost', 84: 'Kiyomizu vengeful spirits (all enemies)',
 85: 'formidable enemies (three oni)', 86: "? (Nobunaga's blade, Kappa)", 87: 'Shinsengumi five (web: Okita, Kondo, Hijikata, Nagakura, Saito)',
 88: 'yokai duels (multi)', 89: 'past allies rush', 90: 'Haunting Caves yokai', 91: 'lost soul (Edo)',
 92: 'rush of past bosses ("warriors I crossed blades with across time")',
 93: 'ghosts at Hirakawa Gate (all enemies)', 94: 'Chokodo enemies (all enemies)', 95: "Shosetsu's warriors rush",
 113: 'Honda Tadakatsu (duel) - NOT listed (flag 0x40)', 114: 'Minamoto no Yoshitsune (duel) - NOT listed (flag 0x40)',
 115: 'mighty warriors + Giant Toad (Araki Mataemon)', 116: 'Yagyu Jubei + Marubashi Chuya - NOT listed (flag 0x40)',
 119: 'Yagyu Munenori (Eternal Rift training)', 120: 'Yagyu Munenori', 121: 'Yagyu Munenori',
 122: 'Hattori Hanzo (Eternal Rift training)', 123: 'Hattori Hanzo', 124: 'Hattori Hanzo',
 125: 'Saito Fuku (Eternal Rift training)', 126: 'Saito Fuku', 127: 'Saito Fuku',
 128: 'generic mission 0xA19A (unnamed)', 129: 'generic Emaki/Picture Scroll mission 0xCC96 (boss = emaki record +0x10)',
}
rows = list(csv.DictReader(open(src, encoding='utf-8')))
fields = list(rows[0].keys())
fields.insert(fields.index('name') + 1, 'boss_or_content')
for r in rows:
    r['boss_or_content'] = B.get(int(r['row']), '')
with open(dst, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
print(len(rows))
