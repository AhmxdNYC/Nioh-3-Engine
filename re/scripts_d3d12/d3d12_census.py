"""d3d12_census.py -- turn d3d12_sites.py output into the census + incoming symbol rows.

usage:  py -I d3d12_sites.py vsites.tsv > classified.tsv
        py -I d3d12_census.py classified.tsv            (writes the files below, prints the summary for notes/d3d12-sites.md)
writes  re\\scripts_d3d12\\d3d12_sites.tsv                census: site, api, enclosing function, frequency, subsystem, evidence
        re\\symbols\\incoming\\d3d12-sites.tsv              new function names (RVAs not in symbols.tsv) + notes on known ones
Barrier calls (ResourceBarrier/Barrier) are listed in the census for completeness but are owned by barrier_sites.tsv.
"""
import sys, os, collections, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SYMS = os.path.join(ROOT, 're', 'symbols', 'symbols.tsv')

# (lo, hi, subsystem) -- first match wins; ranges come from the named functions around each cluster
REGIONS = [
    (0x6D7000, 0x6D8000, 'GpuMemory'), (0x9E7000, 0x9E8000, 'GpuMemory'),
    (0x113000, 0x11A000, 'SynRecord'), (0x459000, 0x460000, 'SynRecord'), (0x58B000, 0x58C2B0, 'SynRecord'),
    (0x696000, 0x697000, 'SynRecord'), (0x6BE000, 0x6EE000, 'SynRecord'), (0x7A5000, 0x7B2000, 'SynRecord'),
    (0x7C6000, 0x7FA000, 'SynRecord'), (0x850000, 0x851000, 'SynRecord'), (0x871000, 0x87B000, 'SynRecord'),
    (0x9E2000, 0x9F4000, 'SynRecord'), (0x2EFB000, 0x2EFE000, 'SynRecord'), (0x2EFFF00, 0x2F01000, 'SynRecord'),
    (0x2F0D000, 0x2F0E000, 'SynRecord'), (0x191F000, 0x1920000, 'SynRecord'), (0x2F0075C, 0x2F00760, 'SynRecord'),
    (0x48F000, 0x497000, 'CmdListRing'), (0x4F9000, 0x4FA000, 'EclSubmit'),
    (0x117D00, 0x1180D0, 'FenceSync'), (0x7D9E00, 0x7DA000, 'FenceSync'), (0x8BE000, 0x8BF000, 'FenceSync'),
    (0x8F4000, 0x8F5000, 'FenceSync'),
    (0x3D6000, 0x3D7000, 'Present'), (0x1A82000, 0x1A83000, 'Present'), (0x91F000, 0x920000, 'Present'),
    (0x10EC000, 0x10F3000, 'Present'), (0x19B1000, 0x19B2000, 'Present'), (0x7D9B00, 0x7D9E00, 'Present'),
    (0x4A5000, 0x4A6000, 'Present'), (0x1FE2000, 0x1FE3000, 'Present'),
    (0x58C2B0, 0x58D000, 'Pso'), (0x928000, 0x92A000, 'Pso'), (0x2EC8000, 0x2EC9000, 'Pso'),
    (0x2EE7C30, 0x2EE7C58, 'Views'), (0x2EF9544, 0x2EF95C0, 'Views'), (0x2F03FA8, 0x2F04000, 'Views'),
    (0x2EFD8DC, 0x2EFD900, 'Views'), (0x2EFE5D0, 0x2EFE600, 'Views'), (0x2EFFCB8, 0x2EFFD00, 'Views'),
    (0x2EE300C, 0x2EE3700, 'DeviceInit'), (0x2EE2500, 0x2EE256C, 'DeviceInit'), (0x2EE497C, 0x2EE4B00, 'DeviceInit'),
    (0x2EE5A38, 0x2EE5C58, 'DeviceInit'), (0x2EE93D4, 0x2EE9424, 'DeviceInit'), (0x2EE9DE4, 0x2EEA14C, 'DeviceInit'),
    (0x2EEA690, 0x2EECA00, 'DeviceInit'), (0x2EF2D34, 0x2EF2E00, 'DeviceInit'), (0x2EF3700, 0x2EF3B00, 'DeviceInit'),
    (0x2EF8448, 0x2EF8F00, 'DeviceInit'), (0x2EFEA14, 0x2EFFC00, 'DeviceInit'), (0x2F04F00, 0x2F05800, 'DeviceInit'),
    (0x2F110A8, 0x2F11200, 'DeviceInit'), (0x2F13228, 0x2F13400, 'DeviceInit'), (0x30B9000, 0x30BA000, 'DeviceInit'),
    (0x3420000, 0x3470000, 'Fsr3Swapchain'), (0x370F000, 0x3720000, 'FfxDx12Backend'),
    (0x3770000, 0x3782000, 'FfxFrameInterp'),
    (0x1896800, 0x1896900, 'DeviceLost'), (0x22AB000, 0x22AC000, 'DeviceLost'), (0x298E000, 0x298F000, 'DeviceLost'),
    (0x19B0000, 0x19B1000, 'GpuTiming'), (0x1135000, 0x1136000, 'GpuMemory'), (0x10EF000, 0x10F0000, 'Display'),
    (0x0, 0x60000, 'GpuMemory'), (0x62A000, 0x632000, 'GpuMemory'), (0x699000, 0x69B000, 'GpuMemory'),
    (0x6D7000, 0x6D8000, 'GpuMemory'), (0x7A1000, 0x7A2000, 'GpuMemory'), (0x9E7000, 0x9E8000, 'GpuMemory'),
    (0xA2C000, 0xA2D000, 'GpuMemory'), (0x151000, 0x152000, 'GpuMemory'), (0x24F000, 0x250000, 'GpuMemory'),
    (0x1895000, 0x1897000, 'GpuMemory'), (0x1A61000, 0x1A62000, 'GpuMemory'), (0x259F000, 0x25A0000, 'GpuMemory'),
    (0x345C000, 0x345D000, 'GpuMemory'), (0x2EE0000, 0x2F20000, 'GpuMemory'),
]
PREFIX = {'SynRecord': 'Syn', 'CmdListRing': 'CmdList', 'EclSubmit': 'Ecl', 'FenceSync': 'Gfx', 'Present': 'Gfx',
          'Pso': 'Gfx', 'Views': 'Gfx', 'DeviceInit': 'GfxInit', 'Fsr3Swapchain': 'Fsr3Sc', 'FfxDx12Backend': 'FfxDx12',
          'FfxFrameInterp': 'FfxFi', 'DeviceLost': 'Gfx', 'GpuTiming': 'Gfx', 'GpuMemory': 'GpuMem', 'Display': 'Gfx',
          'Other': 'D3d'}
LIFETIME = {'QueryInterface', 'AddRef', 'Release', 'GetDevice', 'GetParent', 'SetName', 'SetPrivateData',
            'SetPrivateDataInterface', 'GetPrivateData'}
PER_DRAW = {'DrawInstanced', 'DrawIndexedInstanced', 'Dispatch', 'ExecuteIndirect', 'DispatchMesh', 'SetPipelineState',
            'IASetPrimitiveTopology', 'IASetVertexBuffers', 'IASetIndexBuffer', 'SetGraphicsRootDescriptorTable',
            'SetComputeRootDescriptorTable', 'SetGraphicsRootConstantBufferView', 'SetComputeRootConstantBufferView',
            'SetComputeRootUnorderedAccessView', 'OMSetStencilRef', 'OMSetRenderTargets', 'RSSetViewports',
            'RSSetScissorRects', 'CopyDescriptorsSimple', 'CreateConstantBufferView', 'SOSetTargets', 'SetPredication'}
INIT = {'CreateCommandQueue', 'CreateCommandAllocator', 'CreateCommandList', 'CreateFence', 'CreateDescriptorHeap',
        'CreateRootSignature', 'CreateCommandSignature', 'CreateQueryHeap', 'CheckFeatureSupport', 'GetAdapterLuid',
        'EnumAdapterByLuid', 'GetDescriptorHandleIncrementSize', 'SetFullscreenState', 'ResizeBuffers',
        'GetContainingOutput', 'GetFullscreenState', 'GetFullscreenDesc', 'GetDesc1', 'GetHwnd', 'SetColorSpace1',
        'GetFrameLatencyWaitableObject', 'CreateSampler', 'QueryVideoMemoryInfo', 'SetVideoMemoryReservation',
        'GetDeviceRemovedReason'}


def subsystem(rva):
    for lo, hi, s in REGIONS:
        if lo <= rva < hi:
            return s
    return 'Other'


def frequency(api, sub):
    t, m = api.split('.', 1)
    m = m.split('(')[0]
    if m in LIFETIME:
        return 'lifetime'
    if sub == 'DeviceInit' or m in INIT:
        return 'init'
    if t == 'CL':
        return 'per-draw' if m in PER_DRAW else 'per-frame'
    if m in PER_DRAW and sub == 'SynRecord':
        return 'per-draw'
    if t in ('Queue', 'Fence', 'SwapChain') or sub in ('Present', 'FenceSync', 'EclSubmit', 'FfxFrameInterp'):
        return 'per-frame'
    if m.startswith('Create') and 'PipelineState' in m:
        return 'on-demand'                         # PSO cache miss
    return 'streaming'                             # resource/heap alloc, upload, map: driven by streaming and frame data


# combined-call roles, first match wins: (required methods, role)
ROLES = [
    ({'CreateConstantBufferView', 'CopyDescriptorsSimple', 'SetComputeRootDescriptorTable'}, 'UploadComputeCbvTable'),
    ({'CopyDescriptorsSimple', 'SetComputeRootDescriptorTable'}, 'FlushComputeDescTable'),
    ({'CopyDescriptorsSimple', 'SetGraphicsRootDescriptorTable'}, 'FlushGfxDescTable'),
    ({'CopyDescriptorsSimple', 'CreateShaderResourceView', 'CreateUnorderedAccessView'}, 'WriteViewDescriptors'),
    ({'CreateConstantBufferView', 'CreateSampler'}, 'CreateCbvOrSampler'),
    ({'GetCopyableFootprints', 'Map'}, 'MapAndUploadTexture'),
    ({'GetCopyableFootprints', 'Unmap'}, 'UploadTextureAndUnmap'),
    ({'GetCopyableFootprints', 'GetDesc'}, 'GetTextureCopyFootprints'),
    ({'GetResourceTiling', 'GetResourceAllocationInfo'}, 'SizeTiledResourceAlloc'),
    ({'GetResourceTiling'}, 'SizeTiledResource'),
    ({'GetResourceAllocationInfo', 'GetCompletedValue'}, 'AllocResourceMemoryFenced'),
    ({'CreateCommandQueue', 'CreateFence'}, 'CreateQueuesAndFences'),
    ({'CreateCommandQueue', 'CreateCommandAllocator', 'CreateCommandList'}, 'CreateQueueAndList'),
    ({'CreateCommandAllocator', 'CreateCommandList'}, 'CreateAllocatorAndList'),
    ({'CreateRootSignature', 'CreateGraphicsPipelineState'}, 'CreateRootSigAndPsos'),
    ({'CreateRootSignature', 'CreateCommandSignature'}, 'CreateRootAndCommandSignatures'),
    ({'ExecuteCommandLists', 'Signal'}, 'ExecuteAndSignal'),
    ({'Present', 'Signal'}, 'PresentFrame'),
    ({'Signal', 'Wait', 'SetEventOnCompletion'}, 'QueueSyncAndWait'),
    ({'Signal', 'Wait'}, 'QueueSignalWait'),
    ({'GetCurrentBackBufferIndex', 'GetBuffer'}, 'GetCurrentBackBuffer'),
    ({'GetAdapterLuid', 'EnumAdapterByLuid'}, 'FindAdapterByLuid'),
    ({'GetDesc1', 'GetFullscreenDesc'}, 'QuerySwapChainDesc'),
    ({'CreateHeap', 'SetResidencyPriority'}, 'CreateHeapWithPriority'),
    ({'GetDesc', 'GetCPUDescriptorHandleForHeapStart'}, 'WrapDescriptorHeap'),
    ({'CreateDescriptorHeap'}, 'CreateDescriptorHeap'),
    ({'IASetVertexBuffers', 'DrawInstanced'}, 'DrawVb'),
    ({'IASetPrimitiveTopology', 'SetPipelineState'}, 'FlushPsoAndTopology'),
    ({'Map', 'Unmap'}, 'MapUnmapBuffer'),
    ({'GetCompletedValue'}, 'PollFence'),
    ({'GetCurrentBackBufferIndex'}, 'GetBackBufferIndex'),
]


# hand-picked names where the call mix alone undersells the role (evidence in notes/d3d12-sites.md)
OVERRIDE = {
    '3780A9C': 'FfxFiRecordCompositionPass',      # barriers, CopyResource, root sig + heaps from 3780654/3781024, RTV, draw
    '3780654': 'FfxFiCreateCompositionPsos',      # root signature + 2 graphics PSOs into G:4BD25C8/4BD25F0
    '3781024': 'FfxFiCreateCompositionHeaps',     # 2 descriptor heaps into G:4BD25C0/4BD25D8
    '37761E4': 'FfxFiSwapChainInit',              # GetDevice(Device8), 2 queues (+B48/+B58), QI SwapChain4, 5 fences (+B60..B80)
    '3778204': 'FfxFiCreatePresentCmdList',       # allocator + list + fence for the presenter queue
    '2EFEA14': 'SynCtxCreateSignaturesAndQueries', # root sigs +6D8/+6E0, command sigs +6E8..+700, query heap
    '2EE2500': 'GfxInitCreateDirectCmdList',
    '2EF3944': 'GfxInitCreateDescriptorHeapWrap',
    '2EEBCCC': 'GfxInitQueryAdapterMemory',       # GetAdapterLuid -> EnumAdapterByLuid -> IDXGIAdapter3 QueryVideoMemoryInfo/SetVideoMemoryReservation
    '4F90DC': 'EclSubmitAndSignal',
    '2EEC044': 'GfxExecuteAndSignal',
}


def role(methods):
    qi = [m for m in methods if m.startswith('QueryInterface(')]
    if qi and all(m.split('(')[0] in LIFETIME for m in methods):
        return 'Query' + qi[0][len('QueryInterface(') + 1:-1]
    methods = [m.split('(')[0] for m in methods]
    core = [m for m in methods if m not in LIFETIME] or list(methods)
    ms = set(methods)
    for req, r in ROLES:
        if req <= ms:
            return r
    c = collections.Counter(core).most_common()
    r = c[0][0]
    if len(c) > 1 and c[1][0] != r:
        r += 'And' + c[1][0].replace('Get', '').replace('Create', '')
    return r


def main():
    src = sys.argv[1]
    # "known" = named in the committed symbols.tsv (git HEAD); a merge in the working tree may already hold an older
    # run of this file, whose names are then corrected through incoming/d3d12-sites-renames.tsv
    head = subprocess.run(['git', '-C', ROOT, 'show', 'HEAD:re/symbols/symbols.tsv'], capture_output=True,
                          encoding='utf-8').stdout.splitlines()
    syms, names, work = {}, set(), {}
    for l in head:
        f = l.split('\t')
        if f[0].startswith('0x'):
            syms[int(f[0], 16)] = f
            names.add(f[2])
    for l in open(SYMS, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if f[0].startswith('0x'):
            work[int(f[0], 16)] = f
    rows = [l.rstrip('\n').split('\t') for l in open(src)]
    mine = set(int(r[0], 16) for r in rows)
    names |= set(f[2] for a, f in work.items() if a not in mine)
    byf = collections.defaultdict(list)
    for r in rows:
        byf[r[0]].append(r)
    fname = {}
    newsyms, known = [], []
    for fn in sorted(byf, key=lambda x: int(x, 16)):
        rva = int(fn, 16)
        rs = byf[fn]
        sub = subsystem(rva)
        apis = [r[3] + '.' + r[4] for r in rs]
        if rva in syms and syms[rva][1] == 'function':
            fname[fn] = syms[rva][2]
            known.append((fn, rs, sub))
            continue
        base = OVERRIDE.get(fn) or PREFIX[sub] + role([r[4] for r in rs])
        n = base if base not in names else '%s_%X' % (base, rva)
        names.add(n)
        fname[fn] = n
        newsyms.append((fn, n, rs, sub))
    census = os.path.join(HERE, 'd3d12_sites.tsv')
    with open(census, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# D3D12/DXGI vtable call sites in Nioh3.exe v2.02 (dump), built by d3d12_vsites.fsx + d3d12_sites.py + '
                'd3d12_census.py. Barriers are owned by barrier_sites.tsv (listed here too). evidence: iid/holder/local/'
                'param = data flow, review/scoped = hand-reviewed (d3d12_sites.py REVIEWED/SCOPED). notes/d3d12-sites.md\n')
        o.write('site\tapi\tslot\tfunc\tfunc_name\tfrequency\tsubsystem\tevidence\tthis_key\n')
        for r in sorted(rows, key=lambda r: int(r[1], 16)):
            sub = subsystem(int(r[0], 16))
            api = r[3] + '.' + r[4]
            o.write('0x%s\t%s\t0x%s\t0x%s\t%s\t%s\t%s\t%s\t%s\n' % (r[1].lower(), api, r[2], r[0].lower(), fname[r[0]],
                    frequency(api, sub), sub, r[6], r[5]))
    def desc(rs):
        c = collections.Counter(r[3] + '.' + r[4] for r in rs)
        sites = ','.join(r[1] for r in rs[:6]) + (',...' if len(rs) > 6 else '')
        return 'D3D12: ' + ', '.join('%s%s' % (a, ' x%d' % n if n > 1 else '') for a, n in c.items()) + ' (sites ' + sites + ')'
    def conf(rs):
        ev = [r[6] for r in rs]
        return 'high' if all(e.split(':')[0] in ('iid', 'holder', 'local', 'iid+holder') for e in ev) else 'medium'
    inc = os.path.join(ROOT, 're', 'symbols', 'incoming', 'd3d12-sites.tsv')
    with open(inc, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# d3d12-sites: enclosing functions of every D3D12/DXGI vtable call (goal part 3). Evidence per site: '
                're/scripts_d3d12/d3d12_sites.tsv; method: notes/d3d12-sites.md\n')
        o.write('rva\tkind\tname\tsignature\tdescription\tconfidence\tsource\n')
        for fn, n, rs, sub in newsyms:
            o.write('0x%s\tfunction\t%s\t\t%s subsystem; %s\t%s\tnotes/d3d12-sites.md (d3d12_sites.tsv)\n'
                    % (fn.lower(), n, sub, desc(rs), conf(rs)))
        for fn, rs, sub in known:
            o.write('0x%s\tfunction\t%s\t\t%s subsystem; %s\t%s\tnotes/d3d12-sites.md (d3d12_sites.tsv)\n'
                    % (fn.lower(), fname[fn], sub, desc(rs), conf(rs)))
    ren = os.path.join(ROOT, 're', 'symbols', 'incoming', 'd3d12-sites-renames.tsv')
    nren = 0
    with open(ren, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# d3d12-sites-renames: RENAMES of RVAs already named (apply IN PLACE: same rva, replace name/description). '
                'The 80 enclosing functions named in HEAD agree with their D3D12 calls, so rows appear here only when the '
                'working-tree symbols.tsv holds an older run of d3d12-sites.tsv.\n')
        o.write('rva\tkind\tname\tsignature\tdescription\tconfidence\tsource\n')
        for fn, n, rs, sub in newsyms:
            w = work.get(int(fn, 16))
            if w and w[2] != n:
                nren += 1
                o.write('0x%s\tfunction\t%s\t\t%s subsystem; %s; was %s\t%s\tnotes/d3d12-sites.md (d3d12_sites.tsv)\n'
                        % (fn.lower(), n, sub, desc(rs), w[2], conf(rs)))
    print('renames of working-tree names: %d' % nren)
    # summary
    api = collections.Counter(r[3] + '.' + r[4] for r in rows)
    typ = collections.Counter(r[3] for r in rows)
    sub = collections.Counter(subsystem(int(r[0], 16)) for r in rows)
    fq = collections.Counter(frequency(r[3] + '.' + r[4], subsystem(int(r[0], 16))) for r in rows)
    ev = collections.Counter(r[6].split(':')[0] for r in rows)
    print('sites %d in %d functions (%d new names, %d already named)' % (len(rows), len(byf), len(newsyms), len(known)))
    print('by interface:', ', '.join('%s %d' % kv for kv in typ.most_common()))
    print('by frequency:', ', '.join('%s %d' % kv for kv in fq.most_common()))
    print('by subsystem:', ', '.join('%s %d' % kv for kv in sub.most_common()))
    print('by evidence:', ', '.join('%s %d' % kv for kv in ev.most_common()))
    print('by api:', ', '.join('%s %d' % kv for kv in api.most_common()))


if __name__ == '__main__':
    main()
