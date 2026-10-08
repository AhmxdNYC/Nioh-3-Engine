"""d3d12_sites.py -- classify every D3D12/DXGI vtable call site in Nioh3.exe (except ResourceBarrier/Barrier).

usage:  dotnet fsi d3d12_vsites.fsx > vsites.tsv
        py -I d3d12_sites.py vsites.tsv [--holders] > classified.tsv   (then: py -I d3d12_census.py classified.tsv)
Input rows (see d3d12_vsites.fsx): V virtual call, D direct call, P &local-struct argument, W register store.
Typing evidence, iterated to a fixpoint (column `evidence`):
  iid      the call passes a known IID (iids.tsv, d3d12_iids.py) in that method's riid slot: slot+argpos+IID give the
           interface; the out-param slot becomes a holder (followed through callers' struct arguments)
  holder   this-key ends in a holder: SEED fields (GfxDevice +4268/+4270/+4278/+7730, SynCtx +580/+8E0/+8E8), IID
           out-params (field >= 0x100, used within 0x40000 of where it was learned), globals; only g_RenderGlobals
           chains (GFXROOTS) take field suffixes; holder hits on generic slots < 0x40 are dropped
  local    stack/frame slot or store typed inside the same function
  param    this = a function argument (or [arg+0]) that >= half of the callers pass typed
  review / scoped:  hand-reviewed (REVIEWED, SCOPED below; reasons in the comments)
--holders prints every holder with its slot histogram (stderr).
"""
import sys, collections, re, os

HERE = os.path.dirname(os.path.abspath(__file__))
UNK = ['QueryInterface', 'AddRef', 'Release']
OBJ = UNK + ['GetPrivateData', 'SetPrivateData', 'SetPrivateDataInterface', 'SetName']
CHILD = OBJ + ['GetDevice']
DXO = UNK + ['SetPrivateData', 'SetPrivateDataInterface', 'GetPrivateData', 'GetParent']
DXSUB = DXO + ['GetDevice']
M = {
 'Device': OBJ + ['GetNodeCount', 'CreateCommandQueue', 'CreateCommandAllocator', 'CreateGraphicsPipelineState',
   'CreateComputePipelineState', 'CreateCommandList', 'CheckFeatureSupport', 'CreateDescriptorHeap',
   'GetDescriptorHandleIncrementSize', 'CreateRootSignature', 'CreateConstantBufferView', 'CreateShaderResourceView',
   'CreateUnorderedAccessView', 'CreateRenderTargetView', 'CreateDepthStencilView', 'CreateSampler', 'CopyDescriptors',
   'CopyDescriptorsSimple', 'GetResourceAllocationInfo', 'GetCustomHeapProperties', 'CreateCommittedResource', 'CreateHeap',
   'CreatePlacedResource', 'CreateReservedResource', 'CreateSharedHandle', 'OpenSharedHandle', 'OpenSharedHandleByName',
   'MakeResident', 'Evict', 'CreateFence', 'GetDeviceRemovedReason', 'GetCopyableFootprints', 'CreateQueryHeap',
   'SetStablePowerState', 'CreateCommandSignature', 'GetResourceTiling', 'GetAdapterLuid',
   'CreatePipelineLibrary', 'SetEventOnMultipleFenceCompletion', 'SetResidencyPriority', 'CreatePipelineState',
   'OpenExistingHeapFromAddress', 'OpenExistingHeapFromFileMapping', 'EnqueueMakeResident', 'CreateCommandList1',
   'CreateProtectedResourceSession', 'CreateCommittedResource1', 'CreateHeap1', 'CreateReservedResource1',
   'GetResourceAllocationInfo1', 'CreateLifetimeTracker', 'RemoveDevice', 'EnumerateMetaCommands',
   'EnumerateMetaCommandParameters', 'CreateMetaCommand', 'CreateStateObject', 'GetRaytracingASPrebuildInfo',
   'CheckDriverMatchingIdentifier', 'SetBackgroundProcessingMode', 'AddToStateObject', 'CreateProtectedResourceSession1',
   'GetResourceAllocationInfo2', 'CreateCommittedResource2', 'CreatePlacedResource1', 'CreateSamplerFeedbackUAV',
   'GetCopyableFootprints1', 'CreateShaderCacheSession', 'ShaderCacheControl', 'CreateCommandQueue1',
   'CreateCommittedResource3', 'CreatePlacedResource2', 'CreateReservedResource2'],
 'Queue': CHILD + ['UpdateTileMappings', 'CopyTileMappings', 'ExecuteCommandLists', 'SetMarker', 'BeginEvent', 'EndEvent',
   'Signal', 'Wait', 'GetTimestampFrequency', 'GetClockCalibration', 'GetDesc'],
 'Alloc': CHILD + ['Reset'],
 'Fence': CHILD + ['GetCompletedValue', 'SetEventOnCompletion', 'Signal', 'GetCreationFlags'],
 'DescHeap': CHILD + ['GetDesc', 'GetCPUDescriptorHandleForHeapStart', 'GetGPUDescriptorHandleForHeapStart'],
 'Resource': CHILD + ['Map', 'Unmap', 'GetDesc', 'GetGPUVirtualAddress', 'WriteToSubresource', 'ReadFromSubresource',
   'GetHeapProperties'],
 'Heap': CHILD + ['GetDesc'],
 'PSO': CHILD + ['GetCachedBlob'],
 'RootSig': CHILD, 'QueryHeap': CHILD, 'CmdSig': CHILD,
 'PipeLib': CHILD + ['StorePipeline', 'LoadGraphicsPipeline', 'LoadComputePipeline', 'GetSerializedSize', 'Serialize',
   'LoadPipeline'],
 'CL': CHILD + ['GetType', 'Close', 'Reset', 'ClearState', 'DrawInstanced', 'DrawIndexedInstanced', 'Dispatch',
   'CopyBufferRegion', 'CopyTextureRegion', 'CopyResource', 'CopyTiles', 'ResolveSubresource', 'IASetPrimitiveTopology',
   'RSSetViewports', 'RSSetScissorRects', 'OMSetBlendFactor', 'OMSetStencilRef', 'SetPipelineState', 'ResourceBarrier',
   'ExecuteBundle', 'SetDescriptorHeaps', 'SetComputeRootSignature', 'SetGraphicsRootSignature',
   'SetComputeRootDescriptorTable', 'SetGraphicsRootDescriptorTable', 'SetComputeRoot32BitConstant',
   'SetGraphicsRoot32BitConstant', 'SetComputeRoot32BitConstants', 'SetGraphicsRoot32BitConstants',
   'SetComputeRootConstantBufferView', 'SetGraphicsRootConstantBufferView', 'SetComputeRootShaderResourceView',
   'SetGraphicsRootShaderResourceView', 'SetComputeRootUnorderedAccessView', 'SetGraphicsRootUnorderedAccessView',
   'IASetIndexBuffer', 'IASetVertexBuffers', 'SOSetTargets', 'OMSetRenderTargets', 'ClearDepthStencilView',
   'ClearRenderTargetView', 'ClearUnorderedAccessViewUint', 'ClearUnorderedAccessViewFloat', 'DiscardResource',
   'BeginQuery', 'EndQuery', 'ResolveQueryData', 'SetPredication', 'SetMarker', 'BeginEvent', 'EndEvent',
   'ExecuteIndirect', 'AtomicCopyBufferUINT', 'AtomicCopyBufferUINT64', 'OMSetDepthBounds', 'SetSamplePositions',
   'ResolveSubresourceRegion', 'SetViewInstanceMask', 'WriteBufferImmediate', 'SetProtectedResourceSession',
   'BeginRenderPass', 'EndRenderPass', 'InitializeMetaCommand', 'ExecuteMetaCommand', 'BuildRaytracingAS',
   'EmitRaytracingASPostbuildInfo', 'CopyRaytracingAS', 'SetPipelineState1', 'DispatchRays', 'RSSetShadingRate',
   'RSSetShadingRateImage', 'DispatchMesh', 'Barrier'],
 'SwapChain': DXSUB + ['Present', 'GetBuffer', 'SetFullscreenState', 'GetFullscreenState', 'GetDesc', 'ResizeBuffers',
   'ResizeTarget', 'GetContainingOutput', 'GetFrameStatistics', 'GetLastPresentCount', 'GetDesc1', 'GetFullscreenDesc',
   'GetHwnd', 'GetCoreWindow', 'Present1', 'IsTemporaryMonoSupported', 'GetRestrictToOutput', 'SetBackgroundColor',
   'GetBackgroundColor', 'SetRotation', 'GetRotation', 'SetSourceSize', 'GetSourceSize', 'SetMaximumFrameLatency',
   'GetMaximumFrameLatency', 'GetFrameLatencyWaitableObject', 'SetMatrixTransform', 'GetMatrixTransform',
   'GetCurrentBackBufferIndex', 'CheckColorSpaceSupport', 'SetColorSpace1', 'ResizeBuffers1', 'SetHDRMetaData'],
 'Factory': DXO + ['EnumAdapters', 'MakeWindowAssociation', 'GetWindowAssociation', 'CreateSwapChain',
   'CreateSoftwareAdapter', 'EnumAdapters1', 'IsCurrent', 'IsWindowedStereoEnabled', 'CreateSwapChainForHwnd',
   'CreateSwapChainForCoreWindow', 'GetSharedResourceAdapterLuid', 'RegisterStereoStatusWindow',
   'RegisterStereoStatusEvent', 'UnregisterStereoStatus', 'RegisterOcclusionStatusWindow', 'RegisterOcclusionStatusEvent',
   'UnregisterOcclusionStatus', 'CreateSwapChainForComposition', 'GetCreationFlags', 'EnumAdapterByLuid',
   'EnumWarpAdapter', 'CheckFeatureSupport', 'EnumAdapterByGpuPreference'],
 'Adapter': DXO + ['EnumOutputs', 'GetDesc', 'CheckInterfaceSupport', 'GetDesc1', 'GetDesc2',
   'RegisterHardwareContentProtectionTeardownStatusEvent', 'UnregisterHardwareContentProtectionTeardownStatus',
   'QueryVideoMemoryInfo', 'SetVideoMemoryReservation', 'RegisterVideoMemoryBudgetChangeNotificationEvent',
   'UnregisterVideoMemoryBudgetChangeNotification', 'GetDesc3'],
 'Output': DXO + ['GetDesc', 'GetDisplayModeList', 'FindClosestMatchingMode', 'WaitForVBlank', 'TakeOwnership',
   'ReleaseOwnership', 'GetGammaControlCapabilities', 'SetGammaControl', 'GetGammaControl', 'SetDisplaySurface',
   'GetDisplaySurfaceData', 'GetFrameStatistics', 'GetDisplayModeList1', 'FindClosestMatchingMode1',
   'GetDisplaySurfaceData1', 'DuplicateOutput', 'SupportsOverlays', 'CheckOverlaySupport', 'CheckOverlayColorSpaceSupport',
   'DuplicateOutput1', 'GetDesc1', 'CheckHardwareCompositionSupport'],
}
IIDTYPE = {}
for n in ['ID3D12Device', 'ID3D12Device1', 'ID3D12Device2', 'ID3D12Device3', 'ID3D12Device4', 'ID3D12Device5',
          'ID3D12Device6', 'ID3D12Device7', 'ID3D12Device8', 'ID3D12Device9', 'ID3D12Device10']: IIDTYPE[n] = 'Device'
for n in ['ID3D12GraphicsCommandList'] + ['ID3D12GraphicsCommandList%d' % i for i in range(1, 7)]: IIDTYPE[n] = 'CL'
for n in ['IDXGISwapChain', 'IDXGISwapChain1', 'IDXGISwapChain2', 'IDXGISwapChain3', 'IDXGISwapChain4']: IIDTYPE[n] = 'SwapChain'
for n in ['IDXGIFactory', 'IDXGIFactory1', 'IDXGIFactory2', 'IDXGIFactory4', 'IDXGIFactory5', 'IDXGIFactory6']: IIDTYPE[n] = 'Factory'
for n in ['IDXGIAdapter', 'IDXGIAdapter1', 'IDXGIAdapter3', 'IDXGIAdapter4']: IIDTYPE[n] = 'Adapter'
for n in ['IDXGIOutput', 'IDXGIOutput6']: IIDTYPE[n] = 'Output'
IIDTYPE.update({'ID3D12CommandList': 'CL', 'ID3D12GraphicsCommandList7': 'CL', 'ID3D12Resource1': 'Resource',
    'ID3D12Resource2': 'Resource', 'ID3D12Heap1': 'Heap', 'ID3D12CommandQueue1': 'Queue', 'IDXGIFactory3': 'Factory',
    'IDXGIFactory7': 'Factory', 'IDXGIAdapter2': 'Adapter', 'IDXGIOutput1': 'Output', 'IDXGIOutput4': 'Output',
    'IDXGIOutput5': 'Output'})
IIDTYPE.update({'ID3D12CommandQueue': 'Queue', 'ID3D12CommandAllocator': 'Alloc', 'ID3D12Fence': 'Fence',
    'ID3D12Fence1': 'Fence', 'ID3D12DescriptorHeap': 'DescHeap', 'ID3D12Resource': 'Resource', 'ID3D12Heap': 'Heap',
    'ID3D12PipelineState': 'PSO', 'ID3D12RootSignature': 'RootSig', 'ID3D12QueryHeap': 'QueryHeap',
    'ID3D12CommandSignature': 'CmdSig', 'ID3D12PipelineLibrary': 'PipeLib', 'ID3D12PipelineLibrary1': 'PipeLib'})
ARGPOS = ['rdx', 'r8', 'r9', 's20', 's28', 's30', 's38', 's40']
# (slot, riid arg) -> (this type, created type or None = the IID's own type). Methods whose riid sits in a fixed argument.
CREATE = {
 (0x40, 'r8'): 'Device', (0x48, 'r8'): 'Device', (0x50, 'r8'): 'Device', (0x58, 'r8'): 'Device',
 (0x70, 'r8'): 'Device', (0x80, 's20'): 'Device', (0xE0, 'r8'): 'Device', (0xF0, 's20'): 'Device',
 (0x100, 'r8'): 'Device', (0x120, 'r9'): 'Device', (0x138, 'r8'): 'Device', (0x148, 'r9'): 'Device',
 (0x160, 'r9'): 'Device', (0xD8, 's30'): 'Device', (0xE8, 's30'): 'Device', (0x60, 's28'): 'Device',
 (0x198, 's20'): 'Device', (0x1A8, 's38'): 'Device', (0x228, 's38'): 'Device', (0x230, 's30'): 'Device', (0x258, 'r9'): 'Device', (0x178, 'r8'): 'Device',
}
SWAPGETBUF = (0x48, 'r8')        # IDXGISwapChain::GetBuffer(idx, riid=Resource, pp)
# Seeds: field holders proven in earlier notes (GfxDevice, SynCtx) -- see symbols.tsv GfxDevice::*/SynCtx::*.
GFXROOTS = {'G:45BA588'}                     # g_RenderGlobals (GfxDevice*); other singletons are not D3D12 holders
SEED = {'>4268': 'Device', '>4270': 'Queue', '>4278': 'SwapChain', '>7730': 'SwapChain', '>580': 'CL',
        '>8E0': 'Device', '>8E8': 'Device'}
# Suffixes below 0x100 reviewed by slot histogram (--holders) and accepted; others need the 2-level key.
TRUST = set()
# Hand-reviewed (function, this-key) -> type, from slot pattern + argument shape (notes/d3d12-sites.md "Reviewed").
REVIEWED = {
    ('4FBD4', 'arg:rcx>30'): 'Resource', ('4FBD4', 'arg:rcx>10'): 'Resource',          # GpuBufferUnmap: Unmap(0, &range)
    ('2F0075C', 'arg:r8'): 'Resource', ('2F0FA0C', 'arg:r8'): 'Resource',               # CopyBufferRegion src/dst GetDesc/GetGPUVA
    ('7A1DF0', 'arg:rdx'): 'Resource', ('9E7744', 'arg:rdx'): 'Resource',
    ('45DDC8', 'ret:459F10>10'): 'Resource', ('2EE1970', 'ret:45A9A0>10'): 'Resource',
    ('2EF3724', 'arg:rdx'): 'DescHeap', ('2EF3824', 'arg:rdx'): 'DescHeap',             # GetDevice(IID_Device) on it + 40/48/50
    ('2EF3724', 'nv:rsp>FFFFFFFFFFFFFFD0'): 'Device', ('2EF3824', 'nv:rsp>FFFFFFFFFFFFFFD0'): 'Device',
    ('494E00', '?'): 'CL', ('494E00', '?>FFFFFFFFFFFFFFC7'): 'CL',                      # SynBeginList: IASetVertexBuffers/IndexBuffer
    ('3780A9C', 'arg:rcx>8'): 'CL', ('3780A9C', 'S:40'): 'Device',                       # FFX FI UI composition pass
    ('2EE7C30', 'arg:rdx>0'): 'Device',                                                   # CreateDepthStencilView(res=[rdx+8], &desc rdx+38)
    ('2F03FA8', 'arg:rdx>0'): 'Device', ('2F03FD0', 'arg:rdx>0'): 'Device',               # CreateSRV / CreateUAV(res, counter, desc)
    ('2EF9544', 'arg:rcx>60'): 'Device', ('2EF9580', 'arg:rcx>60'): 'Device',             # CreateConstantBufferView + CreateSampler
    ('2EEBCCC', 'arg:rcx'): 'Device', ('2EEBCCC', 'nv:rsp>FFFFFFFFFFFFFFB0'): 'Adapter',  # GetAdapterLuid; IDXGIAdapter3 QueryVideoMemoryInfo
    ('2F11614', 'arg:rcx>8'): 'Device', ('2F118CC', 'arg:rcx>8'): 'Device',               # Evict / MakeResident(n, objs)
    ('2F1165C', 'S:48'): 'Device',                                                        # SetResidencyPriority on the heap it created
    ('2EF319C', 'arg:rdx'): 'DescHeap', ('2EF356C', 'arg:rdx'): 'DescHeap',               # GetDesc / GetGPUDescriptorHandleForHeapStart(&ret)
    ('2F10E30', 'arg:rcx>10'): 'Resource',                                                # Map(0, &range, &p) / Unmap
    ('4F91C0', '?'): 'Queue', ('4F9214', '?'): 'Queue', ('4F93C8', '?'): 'Queue',         # ECL thread: Signal / ExecuteCommandLists(1) / Wait
    ('117D6C', 'arg:rcx>18'): 'Fence', ('117D6C', 'arg:rcx>8'): 'Fence',                 # GfxFencePollWorker GetCompletedValue
    ('1180A4', 'arg:rcx>0'): 'Fence', ('7D9E1C', '&arg:rcx+6D8>0'): 'Fence',             # IsFenceComplete / GfxWaitFenceValue SetEventOnCompletion
}
# Resource wrappers in the texture/buffer upload family 2EF9C40..2F0A1FC: [this+10] / [[this+10]+10] is the ID3D12Resource
for _f in ['2EF9C40', '2EF9D94', '2EFA1A0', '2F08C90', '2F08E28', '2F09174', '2F09334', '2F09528', '2F098D0', '2F09A5C',
           '2F09C68', '2F0A010']:
    for _k in ('arg:rcx>10', 'arg:rcx>10>10', '?>10', '&arg:rcx+A0>10'):
        REVIEWED[(_f, _k)] = 'Resource'
# Module-scoped field holders (reviewed): (lo, hi, suffix) -> type
SCOPED = {
    (0x3770000, 0x3780000, '>B40'): 'Queue', (0x3770000, 0x3780000, '>B50'): 'Queue',  # FFX frame-interpolation swapchain
}
DENY = set()


def load_iids():
    d = {}
    p = os.path.join(HERE, 'iids.tsv')
    for l in open(p):
        a, n = l.split()
        d['&G:' + a] = n
    return d


def last(k, n):
    """last n '>D' components of a key, or the key itself for globals"""
    parts = k.split('>')
    if len(parts) <= n:
        return k if k.startswith('G:') else None
    return '>' + '>'.join(parts[-n:])


def main():
    path = sys.argv[1]
    iids = load_iids()
    V, D, W, P = [], [], [], []
    for l in open(path):
        f = l.rstrip('\n').split('\t')
        if f[0] == 'V':
            V.append(f)
        elif f[0] == 'D':
            D.append(f)
        elif f[0] == 'W':
            W.append(f)
        elif f[0] == 'P':
            P.append(f)
    holders = dict(SEED)                     # key form -> type
    hsrc = {k: 'seed' for k in SEED}
    params = {}                              # (func, reg) -> type
    site_t = {}                              # site -> (type, evidence)
    Wby = collections.defaultdict(list)
    for f in W:
        Wby[f[1]].append((f[3], f[4]))
    Dby = collections.defaultdict(list)
    for f in D:
        Dby[f[3]].append(f)

    def outhold(func, ok, t, why, depth=0):
        """record the holder that an out-param expression `ok` (pointer to the slot) designates"""
        if ok.startswith('&G:'):
            add(ok[1:], t, why)
        elif ok.startswith('&S:'):
            add((func, ok[1:]), t, why)                  # the local itself
            for dst, src in Wby[func]:
                if src == ok[1:]:
                    addkey(dst, t, why, func)
        elif ok.startswith('&arg:') and '+' in ok and depth < 6:
            base, d = ok[1:].rsplit('+', 1)              # slot inside a struct the caller passed in
            add((func, base + '>' + d), t, why)
            for cf in Dby.get(func, []):
                k = {'arg:rcx': cf[4], 'arg:rdx': cf[5], 'arg:r8': cf[6], 'arg:r9': cf[7]}.get(base)
                if not k or k.startswith('?') or k.startswith('&&'):
                    continue
                if k.startswith('&') and '+' in k:
                    b, e = k[1:].rsplit('+', 1)
                    if 'FFFFFFFF' not in e:
                        outhold(cf[1], '&%s+%X' % (b, int(e, 16) + int(d, 16)), t, why + '<' + cf[2], depth + 1)
                elif k.startswith('&S:') and k[3:].isalnum():
                    outhold(cf[1], '&S:%X' % (int(k[3:], 16) + int(d, 16)), t, why + '<' + cf[2], depth + 1)
                elif not k.startswith('&'):
                    addkey(k + '>' + d, t, why + '<' + cf[2], cf[1])
        elif ok.startswith('&') and '+' in ok:
            base, d = ok[1:].rsplit('+', 1)
            add((func, base + '>' + d), t, why)          # same slot read back in this function
            if not d.startswith('FFFFFFFF') and base not in ('nv:rsp',) and not base.startswith('&'):
                addkey(base + '>' + d, t, why, func)
        elif ok.startswith('arg:'):
            for cf in Dby.get(func, []):
                k = {'arg:rcx': cf[4], 'arg:rdx': cf[5], 'arg:r8': cf[6], 'arg:r9': cf[7]}.get(ok)
                if k:
                    if depth < 6:
                        outhold(cf[1], k, t, why + '<' + cf[2], depth + 1)

    def addkey(k, t, why, func=None, strict=False):
        if k.startswith('G:'):
            add(k, t, why)                               # global chain: exact key
            return
        s1 = last(k, 1)
        if s1 is None or 'FFFFFFFF' in s1:
            return
        base = k[:len(k) - len(s1)]
        stack = base.startswith(('&S:', 'S:', '&nv:rsp', 'nv:rsp', '&&'))
        if func and (stack or (int(s1[1:], 16) < 0x100 and s1 not in TRUST)):
            add((func, k), t, why)                       # function-local object
        elif strict and func:
            add((func, k), t, why)                       # stores: a field typed only inside the storing function
        elif not stack and (int(s1[1:], 16) >= 0x100 or s1 in TRUST):
            if add(s1, t, why) and func:
                hfunc[s1] = int(func, 16)

    def add(h, t, why):
        if h in DENY:
            return
        if h in holders and holders[h] != t:
            conflicts.add(h)
            return
        if h not in holders:
            holders[h] = t
            hsrc[h] = why
            changed[0] = True
            return True
        return False

    def keytype(k, func=None):
        if func and (func, k) in holders:
            return holders[(func, k)], 'local'
        if k.startswith('G:') and k in holders:
            return holders[k], 'holder:' + k
        if k.startswith('G:') and k.split('>')[0] not in GFXROOTS:
            return None, None                            # other singletons reuse the same field offsets
        s1 = last(k, 1)
        if s1 and s1 in holders and s1 not in DENY:
            if s1 not in SEED and int(s1[1:], 16) < 0x1000 and func and s1 in hfunc                     and abs(int(func, 16) - hfunc[s1]) > 0x40000:
                return None, None                        # learned field, used far from its module: offset reuse
            return holders[s1], 'holder:' + s1
        return None, None

    conflicts = set()
    iidname = {}
    hfunc = {}
    ncallers = collections.Counter()
    for f in D:
        for reg in ('rcx', 'rdx', 'r8', 'r9'):
            ncallers[(f[3], reg)] += 1
    changed = [True]
    rounds = 0
    while changed[0] and rounds < 12:
        changed[0] = False
        rounds += 1
        # 1. IID evidence
        for f in V:
            func, site, slot, this = f[1], f[2], int(f[3], 16), f[4]
            args = f[6:14]
            for i, a in enumerate(args):
                if a not in iids:
                    continue
                pos = ARGPOS[i]
                it = IIDTYPE.get(iids[a])
                iidname.setdefault(site, iids[a])
                nxt = (args + ['-'])[i + 1]
                if slot == 0 and pos == 'rdx' and it:               # QueryInterface(riid, ppv)
                    site_t.setdefault(site, ('?Unknown', 'iid'))
                    outhold(func, nxt, it, 'QI@' + site)
                elif slot == 0x38 and pos == 'rdx' and it == 'Device':   # DeviceChild::GetDevice
                    site_t.setdefault(site, ('?Child', 'iid'))
                    outhold(func, nxt, it, 'GetDevice@' + site)
                elif slot == 0x30 and pos == 'rdx' and it == 'Factory':  # DXGIObject::GetParent
                    site_t.setdefault(site, ('?DXGIObject', 'iid'))
                    outhold(func, nxt, it, 'GetParent@' + site)
                elif (slot, pos) == SWAPGETBUF and it == 'Resource':
                    site_t[site] = ('SwapChain', 'iid')
                    addkey(this, 'SwapChain', 'GetBuffer@' + site, func) if this != '?' else None
                elif (slot, pos) in CREATE and it:
                    site_t[site] = (CREATE[(slot, pos)], 'iid')
                    if this not in ('?',) and not this.startswith('?'):
                        addkey(this, 'Device', 'create@' + site, func)
                    if this.startswith('arg:'):
                        params[(func, this[4:])] = 'Device'
                    if nxt != '-':
                        outhold(func, nxt, it, 'out@' + site)
                elif slot in (0x48, 0x50, 0x68) and pos == 'r9' and it == 'PSO':   # PipelineLibrary::Load*Pipeline
                    site_t[site] = ('PipeLib', 'iid')
                elif slot == 0xD0 and pos == 'r8' and it == 'Adapter':  # IDXGIFactory4::EnumAdapterByLuid
                    site_t[site] = ('Factory', 'iid')
        # 2. typed stores: dst holds what src holds
        for f in W:
            t, _ = keytype(f[4], f[1])
            if t:
                addkey(f[3], t, 'store@' + f[2], f[1], strict=True)
        # 3. params: callers passing typed keys
        votes = collections.defaultdict(collections.Counter)
        for f in D:
            for reg, k in zip(('rcx', 'rdx', 'r8', 'r9'), f[4:8]):
                t, _ = keytype(k, f[1])
                if t is None and k.startswith('arg:'):
                    t = params.get((f[1], k[4:]))
                if t:
                    votes[(f[3], reg)][t] += 1
                # pointer to a holder: &K+D with K>D typed, or a pointer param passed through
                t0 = None
                if k.startswith('&') and '+' in k and not k.startswith('&&'):
                    b, d = k[1:].rsplit('+', 1)
                    t0, _ = keytype(b + '>' + d, f[1])
                elif k.startswith('arg:'):
                    t0 = params.get((f[1], k[4:] + '>0'))
                if t0:
                    votes[(f[3], reg + '>0')][t0] += 1
        for f in P:                                   # &local struct whose +0 holds a typed value
            t, _ = keytype(f[5], f[1])
            if t is None and f[5].startswith('arg:'):
                t = params.get((f[1], f[5][4:]))
            if t:
                votes[(f[3], f[4] + '>0')][t] += 1
        for key, c in votes.items():
            tgt, reg = key[0], key[1].split('>')[0]
            if len(c) == 1 and key not in params and sum(c.values()) * 2 >= ncallers[(tgt, reg)]:
                params[key] = next(iter(c))
                changed[0] = True
    # classify sites
    rows = []
    untyped = collections.defaultdict(list)
    localt = {}
    for f in V:
        if site_t.get(f[2], ('', ''))[1] == 'iid' and not site_t[f[2]][0].startswith('?'):
            localt[(f[1], f[4])] = site_t[f[2]][0]
    for f in V:
        func, site, slot, this = f[1], f[2], int(f[3], 16), f[4]
        t, ev = site_t.get(site, (None, None))
        if t and t.startswith('?'):
            t2, ev2 = keytype(this, func)
            t, ev = (t2, 'iid+' + ev2) if t2 else (t, ev)
        if not t:
            t, ev = keytype(this, func)
        if not t and this.startswith('arg:') and (func, this[4:]) in params:
            t, ev = params[(func, this[4:])], 'param'
        if not t and (func, this) in localt and this != '?':
            t, ev = localt[(func, this)], 'local'
        if not t and (func, this) in REVIEWED:
            t, ev = REVIEWED[(func, this)], 'review'
        if not t:
            fa = int(func, 16)
            for (lo, hi, suf), ty in SCOPED.items():
                if lo <= fa < hi and this.endswith(suf):
                    t, ev = ty, 'scoped:' + suf

        if not t:
            untyped[(func, this)].append(f)
            continue
        if ev.startswith('holder:>') and slot < 0x40 and (func, this) not in localt:
            continue                                       # suffix holders collide on generic slots (dtor/AddRef/Release/...)
        if ev == 'param' and slot < 0x38 and t not in ('Factory', 'SwapChain', 'Adapter', 'Output'):
            t = 'Unknown'                                  # helper taking any COM object: keep the method, drop the type
            name = OBJ[slot // 8]
            rows.append((func, site, slot, t, name, this, ev))
            continue
        if t.startswith('?'):
            name = {'?Unknown': 'QueryInterface', '?Child': 'GetDevice', '?DXGIObject': 'GetParent'}[t]
            t = 'Unknown'
            if site in iidname:
                name += '(%s)' % iidname[site]
        else:
            tab = M[t]
            if slot // 8 >= len(tab):
                continue                                   # slot outside the interface: not this type
            name = tab[slot // 8]
            if name in ('QueryInterface', 'GetDevice', 'GetParent') and site in iidname:
                name += '(%s)' % iidname[site]
        rows.append((func, site, slot, t, name, this, ev))
    rows.sort(key=lambda r: int(r[1], 16))
    if '--holders' in sys.argv:
        hist = collections.defaultdict(collections.Counter)
        for r in rows:
            hist[r[6]][r[2]] += 1
        for h, t in sorted(holders.items(), key=lambda x: (x[1], str(x[0]))):
            if isinstance(h, tuple):
                continue
            hh = hist.get('holder:' + h, {})
            print('%s\t%s\t%s\t%d\t%s' % (h, t, hsrc[h], sum(hh.values()),
                  ' '.join('%X:%d' % kv for kv in sorted(hh.items(), key=lambda x: -x[1])[:14])), file=sys.stderr)
        print('local holders:', sum(1 for h in holders if isinstance(h, tuple)), file=sys.stderr)
        print('conflicts:', sorted(map(str, conflicts)), file=sys.stderr)
        print('params:', len(params), file=sys.stderr)
    for r in rows:
        print('%s\t%s\t%X\t%s\t%s\t%s\t%s' % r)


if __name__ == '__main__':
    main()
