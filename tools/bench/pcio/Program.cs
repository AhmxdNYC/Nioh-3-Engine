using System;using System.Diagnostics;using System.IO;using System.Runtime.InteropServices;using System.Threading;using Microsoft.Win32.SafeHandles;
unsafe class P{
const uint GR=0x80000000,SHR=1,OPEN=3,OVL=0x40000000,NOBUF=0;
[StructLayout(LayoutKind.Sequential)]struct OV{public IntPtr Internal,InternalHigh;public uint Off,OffHigh;public IntPtr hEvent;}
[DllImport("kernel32",CharSet=CharSet.Ansi)]static extern IntPtr CreateEventA(IntPtr a,int m,int i,string n);
[DllImport("kernel32")]static extern int CloseHandle(IntPtr h);
[DllImport("kernel32")]static extern int SetEvent(IntPtr h);
[DllImport("kernel32")]static extern int ResetEvent(IntPtr h);
[DllImport("kernel32",CharSet=CharSet.Unicode)]static extern IntPtr CreateFileW(string f,uint a,uint s,IntPtr sa,uint d,uint fl,IntPtr t);
[DllImport("kernel32")]static extern int ReadFile(IntPtr h,byte*b,uint n,out uint r,OV*o);
[DllImport("kernel32")]static extern int GetOverlappedResult(IntPtr h,OV*o,out uint r,int w);
[DllImport("kernel32")]static extern int WaitForSingleObject(IntPtr h,uint ms);
delegate void Cb(uint e,uint n,OV*o);
[DllImport("kernel32")]static extern int ReadFileEx(IntPtr h,byte*b,uint n,OV*o,IntPtr cb);
[DllImport("kernel32")]static extern uint SleepEx(uint ms,int alert);
static int done;
static void Done(uint e,uint n,OV* o){done++;}
static double Med(double[] a){Array.Sort(a);return a[a.Length/2];}
static double Run(Func<double> f){var r=new double[3];for(int i=0;i<3;i++)r[i]=f();return Med(r);}
static void Main(string[] args){
 string path=Path.Combine(Path.GetTempPath(),"pcio_64mb.bin");
 if(!File.Exists(path)){var b=new byte[1<<20];new Random(1).NextBytes(b);using var fs=File.Create(path);for(int i=0;i<64;i++)fs.Write(b);}
 byte[] warm=File.ReadAllBytes(path);
 const long SZ=64L<<20;
 Console.WriteLine("file "+path);
 int N=540000;
 // 1 event pair
 Console.WriteLine($"CreateEventA+CloseHandle x540k: {Run(()=>{var sw=Stopwatch.StartNew();for(int i=0;i<N;i++){var h=CreateEventA(IntPtr.Zero,1,0,null);CloseHandle(h);}return sw.Elapsed.TotalMilliseconds*1000/N;}):F3} us/pair");
 int M=200000;
 var rnd=new Random(7);var offs=new long[M];for(int i=0;i<M;i++)offs[i]=(long)rnd.Next(0,(int)(SZ/1024)-1)*1024;
 byte* buf=(byte*)NativeMemory.AlignedAlloc(1<<20,4096);
 // sync
 IntPtr hs=CreateFileW(path,GR,SHR,IntPtr.Zero,OPEN,0,IntPtr.Zero);
 Console.WriteLine($"1KB sync (SetFilePointer-free positioned via OVERLAPPED offset on sync handle) : {Run(()=>{var sw=Stopwatch.StartNew();OV o=default;for(int i=0;i<M;i++){o.Off=(uint)offs[i];o.OffHigh=(uint)(offs[i]>>32);ReadFile(hs,buf,1024,out uint r,&o);}return sw.Elapsed.TotalMilliseconds*1000/M;}):F3} us/read");
 IntPtr ho=CreateFileW(path,GR,SHR,IntPtr.Zero,OPEN,OVL,IntPtr.Zero);
 Console.WriteLine($"1KB overlapped + per-read CreateEvent/Close + GetOverlappedResult(wait): {Run(()=>{var sw=Stopwatch.StartNew();for(int i=0;i<M;i++){OV o=default;o.Off=(uint)offs[i];o.OffHigh=(uint)(offs[i]>>32);o.hEvent=CreateEventA(IntPtr.Zero,1,0,null);int ok=ReadFile(ho,buf,1024,out uint r,&o);if(ok==0)GetOverlappedResult(ho,&o,out r,1);CloseHandle(o.hEvent);}return sw.Elapsed.TotalMilliseconds*1000/M;}):F3} us/read");
 IntPtr ev=CreateEventA(IntPtr.Zero,1,0,null);
 Console.WriteLine($"1KB overlapped + pooled event (ResetEvent) + GetOverlappedResult: {Run(()=>{var sw=Stopwatch.StartNew();for(int i=0;i<M;i++){OV o=default;o.Off=(uint)offs[i];o.OffHigh=(uint)(offs[i]>>32);o.hEvent=ev;ResetEvent(ev);int ok=ReadFile(ho,buf,1024,out uint r,&o);if(ok==0)GetOverlappedResult(ho,&o,out r,1);}return sw.Elapsed.TotalMilliseconds*1000/M;}):F3} us/read");
 // ReadFileEx
 var cb=new Cb(Done);IntPtr cbp=Marshal.GetFunctionPointerForDelegate(cb);
 foreach(uint sz in new uint[]{4096,262144}){
  int K=sz==4096?50000:5000;var ko=new long[K];for(int i=0;i<K;i++)ko[i]=(long)rnd.Next(0,(int)((SZ-sz)/4096))*4096;
  Console.WriteLine($"ReadFileEx+SleepEx {sz/1024}KB: {Run(()=>{done=0;var sw=Stopwatch.StartNew();OV o=default;for(int i=0;i<K;i++){o=default;o.Off=(uint)ko[i];o.OffHigh=(uint)(ko[i]>>32);if(ReadFileEx(ho,buf,sz,&o,cbp)!=0)SleepEx(1000,1);}if(done!=K)Console.WriteLine("done="+done);return sw.Elapsed.TotalMilliseconds*1000/K;}):F3} us/read");
  Console.WriteLine($"  sync ReadFile {sz/1024}KB: {Run(()=>{var sw=Stopwatch.StartNew();OV o=default;for(int i=0;i<K;i++){o.Off=(uint)ko[i];o.OffHigh=(uint)(ko[i]>>32);ReadFile(hs,buf,sz,out uint r,&o);}return sw.Elapsed.TotalMilliseconds*1000/K;}):F3} us/read");
  Console.WriteLine($"  overlapped+pooled ev {sz/1024}KB: {Run(()=>{var sw=Stopwatch.StartNew();for(int i=0;i<K;i++){OV o=default;o.Off=(uint)ko[i];o.OffHigh=(uint)(ko[i]>>32);o.hEvent=ev;ResetEvent(ev);int ok=ReadFile(ho,buf,sz,out uint r,&o);if(ok==0)GetOverlappedResult(ho,&o,out r,1);}return sw.Elapsed.TotalMilliseconds*1000/K;}):F3} us/read");
 }
}}
