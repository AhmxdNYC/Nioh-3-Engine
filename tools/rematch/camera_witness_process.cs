using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
// Read-only retained process identities. No input/focus/process mutation.
public sealed class CWProcessLease : IDisposable {
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int id);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr h,out long create,out long exit,out long kernel,out long user);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr h,uint flags,StringBuilder path,ref int size);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateToolhelp32Snapshot(uint flags,uint id);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Process32FirstW(IntPtr h,ref Entry e);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Process32NextW(IntPtr h,ref Entry e);
 [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint id);
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Entry {
  public uint size,usage,pid; public UIntPtr heap; public uint module,threads,parent; public int priority; public uint flags;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=260)] public string exe;
 }
 readonly IntPtr[] handles=new IntPtr[3];
 public readonly int Id; public readonly IntPtr MainWindowHandle;
 public CWProcessLease(int owner,long ownerStart,int harness,long harnessStart,int game,long gameStart,string path,IntPtr window) {
  Id=game;MainWindowHandle=window;
  try { handles[0]=Bind(owner,ownerStart,null);handles[1]=Bind(harness,harnessStart,null);handles[2]=Bind(game,gameStart,path);Check(); }
  catch { Dispose();throw; }
 }
 static IntPtr Bind(int id,long start,string expectedPath) {
  IntPtr h=OpenProcess(0x100000|0x1000,false,id);
  if(h==IntPtr.Zero)throw new Win32Exception(Marshal.GetLastWin32Error(),"Cannot retain process identity");
  try {
   long c,e,k,u;if(!GetProcessTimes(h,out c,out e,out k,out u)||c!=start)throw new InvalidOperationException("Process creation identity mismatch");
   if(expectedPath!=null){var b=new StringBuilder(32768);int n=b.Capacity;if(!QueryFullProcessImageName(h,0,b,ref n)||!String.Equals(b.ToString(),expectedPath,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Game executable mismatch");}
   if(WaitForSingleObject(h,0)!=258)throw new InvalidOperationException("Process is not live");return h;
  }catch{CloseHandle(h);throw;}
 }
 // Also used by the offline self-identity fixture; never opens a game in that test.
 public static bool CheckIdentity(int id,long start,string path) {IntPtr h=Bind(id,start,path);try{return WaitForSingleObject(h,0)==258;}finally{CloseHandle(h);}}
 public void Check() {
  foreach(IntPtr h in handles)if(h==IntPtr.Zero||WaitForSingleObject(h,0)!=258)throw new InvalidOperationException("Retained owner/harness/game exited");
  uint pid;if(MainWindowHandle==IntPtr.Zero||GetWindowThreadProcessId(MainWindowHandle,out pid)==0||pid!=Id||GetForegroundWindow()!=MainWindowHandle)throw new InvalidOperationException("Owned game is not foreground; no focus performed");
  IntPtr snapshot=CreateToolhelp32Snapshot(2,0);if(snapshot==new IntPtr(-1))throw new Win32Exception(Marshal.GetLastWin32Error());
  try {
   var e=new Entry();e.size=(uint)Marshal.SizeOf(typeof(Entry));int count=0;
   if(!Process32FirstW(snapshot,ref e))throw new Win32Exception(Marshal.GetLastWin32Error());
   do{if(String.Equals(e.exe,"Nioh3.exe",StringComparison.OrdinalIgnoreCase)){count++;if(e.pid!=Id)throw new InvalidOperationException("Unexpected second game");}}while(Process32NextW(snapshot,ref e));
   if(Marshal.GetLastWin32Error()!=18||count!=1)throw new InvalidOperationException("Game enumeration incomplete or count changed");
  }finally{CloseHandle(snapshot);}
 }
 public void Dispose(){for(int i=0;i<handles.Length;i++){if(handles[i]!=IntPtr.Zero){CloseHandle(handles[i]);handles[i]=IntPtr.Zero;}}}
}
