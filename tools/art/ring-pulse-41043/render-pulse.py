from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import struct
def load(path):
 data=Path(path).read_bytes();off=struct.unpack_from('<I',data,10)[0];w,h=struct.unpack_from('<ii',data,18);h=abs(h)
 raw=data[off:off+w*h*4];out=bytearray(w*h*3);bg=(29,35,44)
 for i in range(w*h):
  b,g,r,a=raw[i*4:i*4+4]
  for j,v in enumerate((r,g,b)):out[i*3+j]=min(255,v+(bg[j]*(255-a)+127)//255)
 return Image.frombytes('RGB',(w,h),bytes(out))
bg=(29,35,44)
im=Image.new('RGB',(1000,1120),bg);d=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',17)
d.text((30,16),'Perfect Ki Pulse: actual ring renderer, before / after',font=font,fill='white')
d.text((30,50),'Native pixel size | Small / Medium / Large | offline preview',font=small,fill='#aab6c8')
d.text((95,90),'Before perfect pulse',font=font,fill='white')
d.text((570,90),'After perfect pulse',font=font,fill='white')
for size,diam in enumerate((160,210,260)):
 before=load(f'pulse-{size}-0.bmp');after=load(f'pulse-{size}-1.bmp')
 cx=960-(diam+26)//2;cy=24+(diam+8)//2
 rect=(cx-diam//2-2,cy-diam//2-2,cx+diam//2+2,cy+diam//2+2)
 y=150+size*305
 d.text((30,y-24),('SMALL','MEDIUM','LARGE')[size],font=small,fill='#aab6c8')
 for x,src in ((95,before),(570,after)):
  im.paste(src.crop(rect),(x,y))
 # Locate actual changed pixels and box the mark, without moving or resizing it.
 points=[]
 a=before.load();b=after.load()
 for py in range(rect[1],rect[3]):
  for px in range(rect[0],rect[2]):
   if a[px,py]!=b[px,py]:points.append((px,py))
 assert points
 left=min(p[0] for p in points)-rect[0]+570;right=max(p[0] for p in points)-rect[0]+570
 top=min(p[1] for p in points)-rect[1]+y;bottom=max(p[1] for p in points)-rect[1]+y
 d.rectangle((left-3,top-3,right+3,bottom+3),outline='#40e8d6',width=1)
 d.text((875,y+diam//2-12),'Perf Ki',font=small,fill='#40e8d6')
 d.line(((right+4,(top+bottom)//2),(866,y+diam//2)),fill='#40e8d6',width=1)
d.text((30,1080),'Only the PLAYER checkmark changes. Ring lanes, colours and numbers remain intact.',font=small,fill='white')
im.save('ring-perfect-ki-audit.png')
