from pathlib import Path
from PIL import Image,ImageDraw
import struct
out=Path('notes/ring-side-41161')
panels=[]
for i in range(3):
 b=Path(f'ring-size-{i}.bmp').read_bytes();offset=struct.unpack_from('<I',b,10)[0];w,h=struct.unpack_from('<ii',b,18);h=abs(h)
 im=Image.frombytes('RGBA',(w,h),b[offset:],'raw','BGRA')
 # Production DIB is premultiplied: composite directly onto neutral grey.
 data=bytearray(im.tobytes())
 for j in range(0,len(data),4):
  a=data[j+3]
  for k in range(3): data[j+k]=min(255,data[j+k]+(24*(255-a)+127)//255)
  data[j+3]=255
 im=Image.frombytes('RGBA',(w,h),bytes(data)).convert('RGB').crop((380,0,1540,h))
 panels.append(im)
img=Image.new('RGB',(1200,sum(p.height+48 for p in panels)+44),(24,24,24));d=ImageDraw.Draw(img)
d.text((20,12),'PRODUCTION RENDERER / OFFLINE SAMPLE / 1920 x 1080 / NO GAME CAPTURE',fill='white')
y=44
for title,p in zip(['SMALL','MEDIUM','LARGE'],panels):
 d.text((20,y),title,fill='white');img.paste(p,(20,y+25));y+=p.height+48
img.save(out/'side-legends.png')
