// Read-only: current display modes (refresh rate) via EnumDisplaySettings.
open System.Runtime.InteropServices
[<Struct; StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)>]
type DEVMODE =
    [<MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)>] val mutable dmDeviceName: string
    val mutable dmSpecVersion: uint16
    val mutable dmDriverVersion: uint16
    val mutable dmSize: uint16
    val mutable dmDriverExtra: uint16
    val mutable dmFields: uint32
    val mutable dmPositionX: int
    val mutable dmPositionY: int
    val mutable dmDisplayOrientation: uint32
    val mutable dmDisplayFixedOutput: uint32
    val mutable dmColor: int16
    val mutable dmDuplex: int16
    val mutable dmYResolution: int16
    val mutable dmTTOption: int16
    val mutable dmCollate: int16
    [<MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)>] val mutable dmFormName: string
    val mutable dmLogPixels: uint16
    val mutable dmBitsPerPel: uint32
    val mutable dmPelsWidth: uint32
    val mutable dmPelsHeight: uint32
    val mutable dmDisplayFlags: uint32
    val mutable dmDisplayFrequency: uint32
[<DllImport("user32.dll", CharSet = CharSet.Unicode)>]
extern bool EnumDisplaySettingsW(string dev, int mode, DEVMODE& dm)
let mutable dm = DEVMODE()
dm.dmSize <- uint16 (Marshal.SizeOf<DEVMODE>())
if EnumDisplaySettingsW(null, -1, &dm) then
    printfn "current: %dx%d @ %d Hz" dm.dmPelsWidth dm.dmPelsHeight dm.dmDisplayFrequency
let rates = System.Collections.Generic.SortedSet<uint32>()
let mutable i = 0
while EnumDisplaySettingsW(null, i, &dm) do
    if dm.dmPelsWidth = 2560u then rates.Add dm.dmDisplayFrequency |> ignore
    i <- i + 1
printfn "2560-wide modes support Hz: %A" (Seq.toList rates)
