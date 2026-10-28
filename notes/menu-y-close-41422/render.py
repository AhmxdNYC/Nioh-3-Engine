from pathlib import Path
from PIL import Image,ImageDraw
import importlib.util
root=Path.cwd();spec=importlib.util.spec_from_file_location('font',root/'mod/Nioh3EngineFix/tools/gen_font_stroke.py');f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f);f.import_approved()
def color(n):
 n=int(n);return (n&255,(n>>8)&255,(n>>16)&255)
for style in range(2):
 im=Image.new('RGB',(1920,1080),(12,17,25));d=ImageDraw.Draw(im)
 for line in (root/f'notes/menu-y-close-41422/menu-{style}.tsv').read_text().splitlines():
  a=line.split('\t',5)
  if a[0]=='R':
   x,y,w,h=map(float,a[1:5]);d.rectangle((x,y,x+w,y+h),fill=color(a[5]))
  elif a[0]=='T':
   x,y,s=map(float,a[1:4]);c=color(a[4])
   for ch in a[5]:
    for x1,y1,x2,y2 in f.GLYPHS.get(ch,[]):d.line((x+x1*7*s,y+y1*14*s,x+x2*7*s,y+y2*14*s),fill=c,width=max(1,round(s)))
    x+=8*s
 im.save(root/f'notes/menu-y-close-41422/menu-{style}.png')
