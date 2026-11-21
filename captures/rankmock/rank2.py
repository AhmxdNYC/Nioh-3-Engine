from PIL import Image, ImageDraw, ImageFont, ImageFilter
import sys
SRC=sys.argv[1]; OUT=sys.argv[2]
F="C:/Windows/Fonts/"
im=Image.open(SRC).convert("RGBA"); W,H=im.size
# hide the perf OSD (top-left) and mod-menu hint (top-right) with a blurred patch of the scene
# cover the perf OSD / mod hint with neighbouring scenery (mirrored), soft-edged
src=im.convert("RGB"); fill=src.copy()
fill.paste(src.crop((420,100,840,315)).transpose(Image.FLIP_LEFT_RIGHT),(0,0))
fill.paste(src.crop((1490,0,1915,10)).resize((425,52)),(1490,0))
m=Image.new("L",im.size,0); md=ImageDraw.Draw(m)
md.rectangle((0,0,415,205),fill=255); md.rectangle((1495,6,1912,46),fill=255)
m=m.filter(ImageFilter.GaussianBlur(8)); im=Image.composite(fill,src,m).convert("RGBA")

ov=Image.new("RGBA",im.size,(0,0,0,0)); d=ImageDraw.Draw(ov)
pal=lambda s:ImageFont.truetype(F+"pala.ttf",s); pali=lambda s:ImageFont.truetype(F+"palai.ttf",s)
kan=lambda s:ImageFont.truetype(F+"YuGothB.ttc",s)
GOLD=(214,190,140); WHITE=(236,232,222); DIM=(150,145,135); RED=(196,48,40)
R=1840  # right edge, like the game's right-side HUD
# soft dark fade behind the block (game HUD uses fades, not boxes)
fade=Image.new("L",(560,700),0); fd=ImageDraw.Draw(fade)
fd.rectangle((230,60,560,680),fill=215)
fade=fade.filter(ImageFilter.GaussianBlur(70)); blk=Image.new("RGBA",(560,700),(8,6,8,255)); blk.putalpha(fade)
ov.alpha_composite(blk,(W-560,250))
def rtext(x,y,t,f,fill,anchor="ra"): d.text((x,y),t,font=f,fill=fill,anchor=anchor)
def diamond(x,y,r,fill): d.polygon([(x,y-r),(x+r,y),(x,y+r),(x-r,y)],fill=fill)
y0=300
# header like the boss-name line: small caps serif + thin rule with diamond
rtext(R,y0,"STYLE",pal(22),GOLD)
d.line([(R-430,y0+38),(R,y0+38)],fill=GOLD+(160,),width=1); diamond(R-215,y0+38,5,GOLD)
# rank: big kanji in red ink with glow, English name in game serif
glow=Image.new("RGBA",im.size,(0,0,0,0)); gd=ImageDraw.Draw(glow)
gd.text((R-255,y0+78),"修羅",font=kan(112),fill=(200,40,30,200),anchor="ra"); glow=glow.filter(ImageFilter.GaussianBlur(14))
ov.alpha_composite(glow)
d.text((R-255,y0+78),"修羅",font=kan(112),fill=(222,62,46,255),anchor="ra")
rtext(R,y0+95,"ASURA",pal(64),WHITE)
rtext(R,y0+170,"Relentless",pali(26),GOLD)
# gauge: thin double bar like the player HP/Ki bars
gy=y0+235; gx0=R-430
d.rectangle([gx0,gy,R,gy+8],outline=(200,200,200,170),fill=(20,18,20,150))
d.rectangle([gx0+2,gy+2,gx0+2+int(426*0.68),gy+6],fill=(205,70,50,240))
# rank ladder: 7 kanji, past ranks lit, current red, future dim
lad=["凡","武","剛","鬼","修羅","剣聖","天"]
x=R; cur=4
for i in range(len(lad)-1,-1,-1):
    t=lad[i]; f=kan(30); w=d.textlength(t,font=f)
    col=(222,62,46,255) if i==cur else (GOLD+(230,) if i<cur else (110,105,100,170))
    d.text((x,gy+26),t,font=f,fill=col,anchor="ra"); x-=w+18
# hit counter: game number font (serif digits)
rtext(R-95,y0+325,"32",pal(84),WHITE)
rtext(R,y0+370,"HITS",pal(24),GOLD)
d.line([(R-430,y0+430),(R,y0+430)],fill=GOLD+(110,),width=1)
# feed of style events, newest bright
feed=[("Ki Pulse","+150",255),("Flow Switch","+250",225),("Burst Counter","+500",190),("Living Weapon","+120",120)]
for i,(a,b,al) in enumerate(feed):
    yy=y0+450+i*38; rtext(R,yy,b,pal(24),GOLD[:3]+(al,)); rtext(R-80,yy,a,pal(24),WHITE[:3]+(al,))
im.alpha_composite(ov)
d2=ImageDraw.Draw(im); d2.text((24,1050),"MOCKUP  ·  style rank overlay on a real Nioh 3 frame (not in game)",font=pal(18),fill=(230,230,230,200))
im.convert("RGB").save(OUT)
