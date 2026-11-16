import sys, os
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]
def font(sz, bold=False):
    for f in (["C:/Windows/Fonts/segoeuib.ttf"] if bold else []) + ["C:/Windows/Fonts/segoeui.ttf","C:/Windows/Fonts/arial.ttf"]:
        if os.path.exists(f): return ImageFont.truetype(f, sz)
    return ImageFont.load_default()
TABS = ["Boss Matches","Training","Player","Performance","Display","Camera","General"]
# rows: (text, kind) kind: '>' submenu, 'v:val' value, 'h' header, 'a' advanced, 'n' new, 'b' back, '' action
PAGES = [
 ("Mod menu", None, [
   ("Boss Matches",">"),("Training",">"),("Player",">"),("Performance",">"),("Display",">"),("Camera",">"),("General",">"),("Close","b")],
   "7 sections, most used first. One Close row (was two)."),
 ("Boss Matches", 0, [
   ("List: All  |  Multi  |  Era  |  Dojos  |  Scrolls  |  Favorites","v:"),
   ("Saito Fuku (Dojo)",">"),("Ibaraki Doji",">"),("Enenra & Jakotsu-baba",">"),("...every boss row...",""),
   ("Leave this Boss Match","n"),("Match rules: death and loot",">"),
   ("Yamagata mission replay  (advanced)","a"),("Back","b")],
   "Leave this Boss Match = one row (was Leave Boss Match / Leave Picture Scroll / Return)."),
 ("Boss: Ibaraki Doji", 0, [
   ("Fight now",""),("Start at the shrine before the boss",""),("Practice this boss here",""),
   ("Practice in this arena, no boss","n"),("Back","b")],
   "Was: Train here / Train in this arena / Shrine before the boss."),
 ("Training", 1, [
   ("Practice a boss here",">"),("Practice in a boss arena",">"),("Summon another boss",">"),
   ("Bosses in this fight (2)",">"),("Freeze fight","v:Off"),("Step one frame",""),("Reset fight",""),
   ("Time of day","v:Game"),("Weather","v:Game"),("Leave arena","n"),("Reset to defaults",""),("Back","b")],
   "Boss Matches door removed from here. Time/Weather moved in from Display."),
 ("Performance", 3, [
   ("Preset","v:Main PC"),("Picture quality",">"),("CPU fixes",">"),("Frame pacing and input delay",">"),
   ("Experiments  (advanced)","a"),("Back","b")],
   "Was 'Optimizations'. ~33 experiment rows now under Experiments (hidden unless Show advanced)."),
 ("Display", 4, [
   ("FPS counter",">"),("Frame meter",">"),("Controller overlay",">"),("Input history",">"),("Game HUD",">"),("Back","b")],
   "Was 'Display Overlays'. Button style moved to General."),
 ("General", 6, [
   ("Controller button style","v:Xbox"),("Mod menu reminder (top-right)","v:On"),("Freeze game while menu is open","v:On"),
   ("Skip load tip screen","v:On"),("Show advanced rows","v:Off"),("Save files",">"),("Boss Match startup settings",">"),
   ("Check for updates",""),("Report a problem",""),("Turn mods off at next start  (advanced)","a"),("Back","b")],
   "New page: all menu-wide switches in one place. Startup settings reachable again."),
]
W,H = 1280,720
BG=(16,16,22); PANEL=(28,28,38); SEL=(150,110,40); TXT=(235,232,225); DIM=(150,150,160); GOLD=(225,185,90); NEW=(110,200,120); ADV=(170,140,210)
for i,(title,tab,rows,cap) in enumerate(PAGES):
    im = Image.new("RGB",(W,H),BG); d = ImageDraw.Draw(im)
    d.text((40,18),"MOCKUP - proposed mod menu (not built yet)",font=font(18),fill=DIM)
    x=40
    for t,name in enumerate(TABS):
        f=font(20,True); w=d.textlength(name,font=f)+28
        d.rounded_rectangle([x,50,x+w,86],8,fill=SEL if t==tab else PANEL)
        d.text((x+14,55),name,font=f,fill=TXT if t==tab else DIM); x+=w+8
    d.rounded_rectangle([40,100,W-40,H-90],12,fill=PANEL)
    d.text((64,112),title,font=font(30,True),fill=GOLD)
    y=164
    for r,(text,kind) in enumerate(rows):
        if r==0: d.rounded_rectangle([56,y-4,W-56,y+34],6,fill=(60,50,30))
        col = NEW if kind=="n" else ADV if kind=="a" else DIM if kind=="b" else TXT
        d.text((80,y),text,font=font(22),fill=col)
        if kind==">": d.text((W-100,y),">",font=font(22,True),fill=GOLD)
        if kind.startswith("v:") and kind[2:]: 
            v="< "+kind[2:]+" >"; d.text((W-80-d.textlength(v,font=font(22)),y),v,font=font(22),fill=GOLD)
        y+=36
    d.text((40,H-78),cap,font=font(20),fill=TXT)
    d.text((40,H-46),"A / Enter: open    B / Esc: back    (prompts follow your Controller button style)     green = new/renamed   purple = advanced only",font=font(17),fill=DIM)
    im.save(os.path.join(out,f"menu{i+1:02d}.png"))
print(len(PAGES))
