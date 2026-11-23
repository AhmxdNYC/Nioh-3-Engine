# Style-rank sprite set: transparent PNGs (letters, words, gauge) + atlas + previews.
import sys, math, json, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
F="C:/Windows/Fonts/"; OUT="sprites/"
RANKS=[("D","Dull",(200,205,212),(120,130,140)),("C","Cunning",(205,210,218),(150,160,170)),
       ("B","Bold",(232,170,70),(214,190,140)),("A","Ardent",(240,120,40),(214,190,140)),
       ("S","Asura",(215,40,30),(225,195,130)),("SS","Sword Saint",(215,40,30),(235,205,140)),
       ("SSS","Heavenly",(225,35,25),(250,220,150))]
H=520
def noise(w,h,scale,seed):
    r=np.random.default_rng(seed); sm=r.random((h//scale+2,w//scale+2))
    return np.asarray(Image.fromarray((sm*255).astype(np.uint8)).resize((w,h),Image.BICUBIC),np.float32)/255
def letter(txt,ink,rimc,seed):
    fnt=ImageFont.truetype(F+"georgiaz.ttf",360)
    tw=int(ImageDraw.Draw(Image.new("L",(1,1))).textlength(txt,font=fnt)*0.86)+260; W=tw
    m=Image.new("L",(W,H),0); ImageDraw.Draw(m).text((W//2,H//2),txt,font=fnt,fill=255,anchor="mm")
    if len(txt)>1:  # overlap letters like a stamped rank
        m=Image.new("L",(W,H),0); dm=ImageDraw.Draw(m); step=fnt.getlength("S")*0.62; x0=W/2-step*(len(txt)-1)/2
        for i,c in enumerate(txt): dm.text((x0+i*step,H//2),c,font=fnt,fill=255,anchor="mm")
    m=m.transform((W,H),Image.AFFINE,(1,0.18,-0.18*H/2,0,1,0),Image.BICUBIC)
    a=np.asarray(m.filter(ImageFilter.GaussianBlur(2)),np.float32)/255
    a=a+(noise(W,H,10,seed)-0.5)*0.22+(noise(W,H,2,seed+1)-0.5)*0.10
    g=np.clip((a-0.5)*20,0,1)
    rng=np.random.default_rng(seed+2)
    st=np.asarray(Image.fromarray((rng.random((H,8))*255).astype(np.uint8)).resize((W,H),Image.BILINEAR).rotate(-62,resample=Image.BILINEAR),np.float32)/255
    g=g*np.clip(1.15-np.clip(st-0.80,0,1)*6,0,1)
    # sword cut across the letter
    X,Y=np.meshgrid(np.arange(W),np.arange(H)); x0,y0,x1,y1=40,380,W-40,130
    dx,dy=x1-x0,y1-y0; n=math.hypot(dx,dy); dist=((X-x0)*dy-(Y-y0)*dx)/n
    g=np.where(dist<0,np.roll(np.roll(g,-9,0),-16,1),g)*np.clip((np.abs(dist)-5)/2,0,1)
    yy=np.linspace(0,1,H)[:,None]*np.ones((1,W)); tex=0.6*noise(W,H,12,seed+3)+0.4*noise(W,H,2,seed+4)
    hi=np.clip(1-yy*2.2,0,1)**2; ink=np.array(ink,np.float32)
    fill=ink*(0.72+0.28*(1-yy[...,None]))*(0.88+0.12*tex[...,None])+np.array([40,30,20],np.float32)*hi[...,None]
    gl=Image.fromarray((g*255).astype(np.uint8))
    rim=np.clip(np.asarray(gl.filter(ImageFilter.MaxFilter(5)),np.float32)/255-g,0,1)
    rimc=np.array(rimc,np.float32)*(1-0.3*yy[...,None])
    sh=np.asarray(gl.filter(ImageFilter.MaxFilter(13)).filter(ImageFilter.GaussianBlur(6)),np.float32)/255
    line=np.clip(1-np.abs(dist)/2.4,0,1)*np.clip(1-np.abs(((X-x0)*dx+(Y-y0)*dy)/n-n/2)/(n/2),0,1)**0.6
    rgb=np.zeros((H,W,3),np.float32); al=np.zeros((H,W),np.float32)
    for col,alpha in ((np.zeros(3,np.float32),sh*0.75),(rimc,rim),(fill,g),(np.array([255,236,200],np.float32),line)):
        rgb=col*alpha[...,None]+rgb*(1-alpha[...,None]); al=alpha+al*(1-alpha)
    out=Image.fromarray(np.dstack([np.clip(rgb,0,255),al*255]).astype(np.uint8),"RGBA")
    glow=Image.new("RGBA",(W,H),(0,0,0,0)); glow.paste(tuple(int(c) for c in ink)+(150,),mask=gl.filter(ImageFilter.GaussianBlur(22)))
    glow.alpha_composite(out); return glow.crop(glow.getbbox())
def word(t):
    f=ImageFont.truetype(F+"palai.ttf",84); w=int(f.getlength(t+"!"))+40
    im=Image.new("RGBA",(w,120),(0,0,0,0)); ImageDraw.Draw(im).text((w//2,60),t+"!",font=f,fill=(236,232,222,255),anchor="mm",stroke_width=2,stroke_fill=(0,0,0,170))
    return im.crop(im.getbbox())
sprites={}
for i,(r,wd,ink,rim) in enumerate(RANKS):
    sprites["letter_"+r]=letter(r,ink,rim,11+i*7); sprites["word_"+r]=word(wd)
gb=Image.new("RGBA",(440,10),(0,0,0,0)); ImageDraw.Draw(gb).line([(2,5),(437,5)],fill=(255,255,255,80),width=5); sprites["gauge_bg"]=gb
gf=Image.new("RGBA",(440,10),(0,0,0,0)); ImageDraw.Draw(gf).line([(2,5),(437,5)],fill=(214,190,140,245),width=5); sprites["gauge_fill"]=gf
for k,v in sprites.items(): v.save(OUT+k+".png")
# atlas (shelf pack) + json rects for the in-game draw
AW=2048; x=y=rowh=0; rects={}; items=sorted(sprites.items(),key=lambda kv:-kv[1].height)
for k,v in items:
    if x+v.width>AW: x=0; y+=rowh+4; rowh=0
    rects[k]=[x,y,v.width,v.height]; x+=v.width+4; rowh=max(rowh,v.height)
atlas=Image.new("RGBA",(AW,y+rowh),(0,0,0,0))
for k,(ax,ay,w,h) in rects.items(): atlas.paste(sprites[k],(ax,ay))
atlas.save(OUT+"rank_atlas.png"); json.dump({"size":[AW,y+rowh],"sprites":rects,"draw":{"letter_height_1080p":300,"anchor":"right-center (1690,380) at 1920x1080","order":["letter","gauge_bg","gauge_fill (crop to progress)","word"]}},open(OUT+"rank_atlas.json","w"),indent=1)
# sheet: all ranks on dark game-like background, same letter height, variable columns
Ls=[]; 
for r,*_ in RANKS:
    L=sprites["letter_"+r]; s_=220/L.height; Ls.append(L.resize((int(L.width*s_),220),Image.LANCZOS))
cw=[max(230,L.width+30) for L in Ls]; sh=Image.new("RGBA",(sum(cw),400),(14,12,14,255)); x=0
for (r,*_),L,w in zip(RANKS,Ls,cw):
    cx=x+w//2; sh.alpha_composite(L,(cx-L.width//2,40))
    Wd=sprites["word_"+r]; s_=0.42; Wd=Wd.resize((int(Wd.width*s_),int(Wd.height*s_)),Image.LANCZOS); sh.alpha_composite(Wd,(cx-Wd.width//2,300)); x+=w
sh.convert("RGB").save("rank_sheet.png")
# in-scene preview built only from the sprites
exec(open("rank4.py",encoding="utf-8").read().split("ov=Image.new")[0].replace("SRC=sys.argv[1]; OUT=sys.argv[2]","SRC=sys.argv[1]"))
cx,cy=1690,380; L=sprites["letter_S"]; s=300/L.height; L=L.resize((int(L.width*s),300),Image.LANCZOS)
im.alpha_composite(L,(cx-L.width//2,cy-150))
gy=cy+175; gbs=sprites["gauge_bg"].resize((220,6)); im.alpha_composite(gbs,(cx-110,gy)); im.alpha_composite(sprites["gauge_fill"].resize((220,6)).crop((0,0,136,6)),(cx-110,gy))
Wd=sprites["word_S"]; s=40/Wd.height; Wd=Wd.resize((int(Wd.width*s),40),Image.LANCZOS); im.alpha_composite(Wd,(cx-Wd.width//2,gy+16))
ImageDraw.Draw(im).text((24,1050),"MOCKUP  ·  built from the rank sprites, on a real Nioh 3 frame (not in game yet)",font=ImageFont.truetype(F+"pala.ttf",18),fill=(230,230,230,200))
im.convert("RGB").save("rank_preview.png")
print("ok",len(sprites),atlas.size)
