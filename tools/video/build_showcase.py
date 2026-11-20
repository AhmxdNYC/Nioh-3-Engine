# first cut of the showcase trailer (#121603): ffmpeg only, real footage + real measured numbers
import subprocess, os
FF = r"C:\Users\Ahmad Hamza\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.2-full_build\bin\ffmpeg.exe"
C = r"D:\nioh3-re\showcase-121603\clips"; E = r"D:\nioh3-re\showcase-121603\edit"
FONT = "C\:/Windows/Fonts/segoeuib.ttf"; FONTR = "C\:/Windows/Fonts/segoeui.ttf"
CLEAN = "delogo=x=1806:y=666:w=42:h=166,scale=1920:1080:flags=lanczos,fps=60,format=yuv420p"
def txt(t, size, y, font=FONT, color="white", start=0, extra=""):
    t = t.replace(":", "\:").replace("'", "\u2019")
    return (f"drawtext=fontfile='{font}':text='{t}':fontsize={size}:expansion=none:fontcolor={color}:x=(w-text_w)/2:y={y}"
            f":shadowcolor=black@0.8:shadowx=3:shadowy=3:alpha='if(lt(t,{start}),0,min(1,(t-{start})/0.4))'{extra}")
def lower(title, sub):
    return (f"drawbox=x=0:y=ih-230:w=iw:h=230:color=black@0.55:t=fill,"
            + txt(title, 64, "h-205", start=0.2) + "," + txt(sub, 40, "h-120", font=FONTR, color="0xE8D8B0", start=0.5))
segs = []
def seg(name, src, ss, dur, vf):
    out = os.path.join(E, name + ".mp4"); segs.append(out)
    subprocess.run([FF, "-y", "-loglevel", "error", "-ss", str(ss), "-t", str(dur), "-i", src, "-an",
                    "-vf", f"{CLEAN},{vf},fade=t=in:st=0:d=0.3,fade=t=out:st={dur-0.3}:d=0.3",
                    "-c:v", "h264_amf", "-b:v", "20M", out], check=True)
def card(name, src, ss, dur, lines):
    vf = "boxblur=20:2,eq=brightness=-0.25," + ",".join(txt(t, s, y, font=f, color=c, start=st) for t, s, y, f, c, st in lines)
    seg(name, src, ss, dur, vf)
RM = os.path.join(C, "rematch_R7B35.mp4"); TR = os.path.join(C, "train", "training_210803.mp4"); FM = TR
card("s1_title", RM, 52, 4, [("NIOH 3 ENGINE MOD", 110, "h/2-110", FONT, "white", 0.1),
                             ("Free  \u00b7  Open source  \u00b7  v1.8.24", 46, "h/2+30", FONTR, "0xE8D8B0", 0.6)])
seg("s2_load", RM, 4, 3, lower("BOSS REMATCH", "Pick any boss you beat. Fight it again, right now."))
seg("s3_fight", RM, 31, 11, lower("BOSS REMATCH", "Straight into the arena. No replaying the level."))
seg("s4_train", TR, 8, 9, lower("TRAINING", "Summon a boss anywhere. Repeat its moves. No death."))
card("s5_ally", FM, 20, 4, [("ROG ALLY", 60, "h/2-150", FONT, "0xE8D8B0", 0.1),
                           ("30.7  \u2192  41.0 FPS", 120, "h/2-70", FONT, "white", 0.3),
                           ("+34%  \u00b7  World shading switch  \u00b7  same look", 42, "h/2+90", FONTR, "white", 0.7)])
card("s6_crash", FM, 26, 3.5, [("51", 170, "h/2-150", FONT, "white", 0.1), ("crashes solved", 58, "h/2+40", FONTR, "0xE8D8B0", 0.3)])
card("s7_reports", FM, 31, 3.5, [("19", 170, "h/2-150", FONT, "white", 0.1), ("player reports solved", 58, "h/2+40", FONTR, "0xE8D8B0", 0.3)])
card("s8_end", RM, 60, 5, [("NIOH 3 ENGINE MOD", 90, "h/2-140", FONT, "white", 0.1),
                           ("Free download", 52, "h/2-10", FONTR, "0xE8D8B0", 0.4),
                           ("github.com/AhmxdNYC/Nioh-3-Engine", 54, "h/2+60", FONT, "white", 0.6)])
lst = os.path.join(E, "list.txt"); open(lst, "w").write("".join(f"file '{s}'\n" for s in segs))
out = os.path.join(E, "showcase_v1.mp4")
subprocess.run([FF, "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-c:v", "h264_amf", "-b:v", "16M", "-movflags", "+faststart", out], check=True)
print("ok", out)
