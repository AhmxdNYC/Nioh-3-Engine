# Boot-time engine settings in the system object database (0x908ab6e2.kidsobjdb, package\system.rdb).
# The engine reads these once at startup, so they are changed in the data archive (append-and-repoint via
# tools\n3inject.py; the shipped .rdb is backed up and the .fdata is truncated back on revert = exact original).
#
#   powershell -File sysdb_tweaks.ps1 -PhysicsThreads 4 -ObjectUpdateUnits 32   apply (game closed)
#   powershell -File sysdb_tweaks.ps1 -Revert                                    shipped files, byte-for-byte
#   powershell -File sysdb_tweaks.ps1 -Status                                    what the game will load
#
# PhysicsThreads     Physics::Scene f43035be MaxThreadCount (shipped 2). Thread count only.
# ObjectUpdateUnits  Param::Parallel 2710f411 ParallelUnitNum (shipped 0 = serial). Used by the frame task graph's
#                    Behavior::Update (x2) and EvaluateScript nodes, which run on the main game thread. 32 is
#                    what the engine's other parallel tasks use. EXPERIMENTAL: thread safety of that code is unknown.
# Every apply starts from the shipped file, so options combine; defaults on all options = revert.
param([int]$PhysicsThreads = 2, [int]$ObjectUpdateUnits = 0, [switch]$Revert, [switch]$Status)
$ErrorActionPreference = "Stop"
$pkg    = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\package"
$tools  = Join-Path $PSScriptRoot "tools"
$backup = Join-Path $PSScriptRoot "physics_backup"          # name kept: earlier tool versions used it
$work   = Join-Path $PSScriptRoot "physics_work"
New-Item -ItemType Directory -Force $work | Out-Null
# Offsets in the decompressed file (v2.02): MaxThreadCount 294440 (record values start 294428: 2200,150,8000,...),
# ParallelUnitNum of 2710f411 at 0x75A0 (record KTID at 0x757C). Both checked before any write.
$cur = Join-Path $work "current.kidsobjdb"
py -I "$tools\n3rdb.py" get "$pkg" system 908ab6e2 "$cur" | Out-Null
$readCode = "import struct,sys; d=open(sys.argv[1],'rb').read(); print(struct.unpack_from('<i',d,294440)[0], struct.unpack_from('<i',d,0x75A0)[0], struct.unpack_from('<I',d,0x757C)[0]==0x2710f411, [struct.unpack_from('<I',d,294428+4*i)[0] for i in range(3)]==[2200,150,8000])"
$v = (py -I -c $readCode $cur).Trim().Split(' ')
if ($Status) { "System DB now: PhysicsThreads=$($v[0]) ObjectUpdateUnits=$($v[1]) (shipped: 2 and 0); injected=$(Test-Path "$backup\backup.json")"; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Close Nioh 3 first." }
if ($v[2] -ne "True" -or $v[3] -ne "True") { throw "unexpected data layout (game updated?) - not touching anything" }
# always restore the shipped files first, so every apply starts from the original
if (Test-Path "$backup\backup.json") {
    py -I "$tools\n3inject.py" revert "$pkg" system "$backup" | Out-Null
    Remove-Item -Recurse -Force $backup
    py -I "$tools\n3rdb.py" get "$pkg" system 908ab6e2 "$cur" | Out-Null
}
if ($Revert -or ($PhysicsThreads -eq 2 -and $ObjectUpdateUnits -eq 0)) { "shipped system DB restored"; & $PSCommandPath -Status; exit 0 }
if ($PhysicsThreads -lt 1 -or $PhysicsThreads -gt 12 -or $ObjectUpdateUnits -lt 0 -or $ObjectUpdateUnits -gt 64) { throw "value out of range" }
$mod = Join-Path $work "mod.kidsobjdb"
$writeCode = "import struct,sys; d=bytearray(open(sys.argv[1],'rb').read()); struct.pack_into('<i',d,294440,int(sys.argv[3])); struct.pack_into('<i',d,0x75A0,int(sys.argv[4])); open(sys.argv[2],'wb').write(d)"
py -I -c $writeCode $cur $mod $PhysicsThreads $ObjectUpdateUnits
py -I "$tools\n3inject.py" inject "$pkg" system 908ab6e2 "$mod" "$backup"
& $PSCommandPath -Status
