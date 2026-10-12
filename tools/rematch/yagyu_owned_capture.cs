using System;using System.Drawing;using System.Drawing.Imaging;using System.Runtime.InteropServices;
public static class OwnedCapture {
[DllImport("user32.dll")]static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
[DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr h,out RECT r);
struct RECT{public int L,T,R,B;}
public static void Capture(IntPtr h,string output){
 Exception failure=null;
 var worker=new System.Threading.Thread(delegate(){try{CaptureCore(h,output);}catch(Exception e){failure=e;}});
 worker.IsBackground=true;worker.Start();
 if(!worker.Join(1500))throw new TimeoutException("Owned PrintWindow capture exceeded1500ms; no subsequent input");
 if(failure!=null)throw failure;
}
static void CaptureCore(IntPtr h,string output){RECT r;if(h==IntPtr.Zero||!GetWindowRect(h,out r))throw new Exception("No owned window");
using(var b=new Bitmap(r.R-r.L,r.B-r.T)){using(var g=Graphics.FromImage(b)){var dc=g.GetHdc();bool ok;try{ok=PrintWindow(h,dc,2);}finally{g.ReleaseHdc(dc);}if(!ok)throw new Exception("PrintWindow failed");}b.Save(output,ImageFormat.Png);}}}
