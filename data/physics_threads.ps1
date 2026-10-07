# Physics solver threads: Physics::Scene.MaxThreadCount (system DB 0x908ab6e2, object f43035be) 2 -> N.
# Read once at boot, so it is changed in the game's data archive (append-and-repoint, see tools\n3inject.py).
#   powershell -File physics_threads.ps1 [-Threads 4]   apply (game must be closed; backs up first)
#   powershell -File physics_threads.ps1 -Revert        restore the shipped files byte-for-byte
#   powershell -File physics_threads.ps1 -Status        show the value the game will load
# Only the thread count changes; the simulation itself (time step, iterations) is untouched.
param([int]$Threads = 4, [switch]$Revert, [switch]$Status)
$ErrorActionPreference = "Stop"
$pkg    = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
$tools  = Join-Path $PSScriptRoot "tools"
$backup = Join-Path $PSScriptRoot "physics_backup"
$work   = Join-Path $PSScriptRoot "physics_work"
$valueOffset = 294440        # MaxThreadCount in the decompressed 0x908ab6e2.kidsobjdb (v2.02); verified below
New-Item -ItemType Directory -Force $work | Out-Null
$cur = Join-Path $work "current.kidsobjdb"
py -I "$tools\n3rdb.py" get "$pkg" system 908ab6e2 "$cur" | Out-Null
$read = { param($f) py -I -c "import struct,sys; d=open(sys.argv[1],'rb').read(); print(*[struct.unpack_from('<I',d,294428+4*i)[0] for i in range(7)])" $f }
$vals = (& $read $cur).Trim().Split(' ')
if ($Status) { "Physics::Scene fields now: MaxDynamic=$($vals[0]) MaxKinematic=$($vals[1]) MaxJoint=$($vals[2]) MaxThreadCount=$($vals[3])"; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Close Nioh 3 first." }
if ($Revert) {
    if (-not (Test-Path "$backup\backup.json")) { "nothing to revert"; exit 0 }
    py -I "$tools\n3inject.py" revert "$pkg" system "$backup"
    Remove-Item -Recurse -Force $backup
    exit 0
}
if ($vals[0] -ne "2200" -or $vals[1] -ne "150" -or $vals[2] -ne "8000") { throw "unexpected data layout (game updated?) - not touching anything" }
if ($vals[3] -eq "$Threads") { "already MaxThreadCount=$Threads"; exit 0 }
$mod = Join-Path $work "mod.kidsobjdb"
py -I -c "import struct,sys; d=bytearray(open(sys.argv[1],'rb').read()); d[${valueOffset}:${valueOffset}+4]=struct.pack('<I',int(sys.argv[3])); open(sys.argv[2],'wb').write(d)" $cur $mod $Threads
py -I "$tools\n3inject.py" inject "$pkg" system 908ab6e2 "$mod" "$backup"
& $PSCommandPath -Status
