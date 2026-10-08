"""d3d12_iids.py -- locate the D3D12/DXGI interface IIDs in re/Nioh3.dump.exe.
usage: py -I d3d12_iids.py | sort -k2 > iids.tsv   (rva<TAB>interface; input of d3d12_sites.py)
"""
import uuid,sys,re
IIDS = {
"ID3D12Device":"189819f1-1db6-4b57-be54-1821339b85f7","ID3D12Device1":"77acce80-638e-4e65-8895-c1f23386863e",
"ID3D12Device2":"30baa41e-b15b-475c-a0bb-1af5c5b64328","ID3D12Device3":"81dadc15-2bad-4392-93c5-101345c4aa98",
"ID3D12Device4":"e865df17-a9ee-46f9-a463-3098315aa2e5","ID3D12Device5":"8b4f173b-2fea-4b80-8f58-4307191ab95d",
"ID3D12CommandQueue":"0ec870a6-5d7e-4c22-8cfc-5baae07616ed","ID3D12CommandAllocator":"6102dee4-af59-4b09-b999-b44d73f09b24",
"ID3D12GraphicsCommandList":"5b160d0f-ac1b-4185-8ba8-b3ae42a5a455","ID3D12GraphicsCommandList1":"553103fb-1fe7-4557-bb38-946d7d0e7ca7",
"ID3D12GraphicsCommandList4":"8754318e-d3a9-4541-98cf-645b50dc4874","ID3D12Fence":"0a753dcf-c4d8-4b91-adf6-be5a60d95a76",
"ID3D12Fence1":"433685fe-e22b-4ca0-a8db-b5b4f4dd0e4a","ID3D12DescriptorHeap":"8efb471d-616c-4f49-90f7-127bb763fa51",
"ID3D12Resource":"696442be-a72e-4059-bc79-5b5c98040fad","ID3D12Heap":"6b3b2502-6e51-45b3-90ee-9884265e8df3",
"ID3D12PipelineState":"765a30f3-f624-4c6f-a828-ace948622445","ID3D12RootSignature":"c54a6b66-72df-4ee8-8be5-a946a1429214",
"ID3D12QueryHeap":"0d9658ae-ed45-469e-a61d-970ec583cab4","ID3D12CommandSignature":"c36a797c-ec80-4f0a-8985-a7b2475082d1",
"ID3D12PipelineLibrary":"c64226a8-9201-46af-b4cc-53fb9ff7414f","ID3D12PipelineLibrary1":"80eabf42-2568-4e5e-bd82-c37f86961dc3",
"IDXGIFactory":"7b7166ec-21c7-44ae-b21a-c9ae321ae369","IDXGIFactory1":"770aae78-f26f-4dba-a829-253c83d1b387",
"IDXGIFactory2":"50c83a1c-e072-4c48-87b0-3630fa36a6d0","IDXGIFactory4":"1bc6ea02-ef36-464f-bf0c-21ca39e5168a",
"IDXGIFactory5":"7632e1f5-ee65-4dca-87fd-84cd75f8838d","IDXGIFactory6":"c1b6694f-ff09-44a9-b03c-77900a0a1d17",
"IDXGISwapChain":"310d36a0-d2e7-4c0a-aa04-6a9d23b8886a","IDXGISwapChain1":"790a45f7-0d42-4876-983a-0a55cfe6f4aa",
"IDXGISwapChain2":"a8be2ac4-199f-4946-b331-79599fb98de7","IDXGISwapChain3":"94d99bdb-f1f8-4ab0-b236-7da0170edab1",
"IDXGISwapChain4":"3d585d5a-bd4a-489e-b1f4-3dbcb6452ffb","IDXGIAdapter":"2411e7e1-12ac-4ccf-bd14-9798e8534dc0",
"IDXGIAdapter1":"29038f61-3839-4626-91fd-086879011a05","IDXGIAdapter3":"645967a4-1392-4310-a798-8053ce3e93fd",
"IDXGIAdapter4":"3c8d99d1-4fbf-4181-a82c-af66bf7bd24e","IDXGIOutput6":"068346e8-aaec-4b84-add7-137f513f77a1",
"IDXGIOutput":"ae02eedb-c735-4690-8d52-5a8dc20213aa","IDXGIDevice":"54ec77fa-1377-44e6-8c32-88fd5f44c84c",
"ID3D12Debug":"344488b7-6846-474b-b989-f027448245e0","ID3D12Debug1":"affaa4ca-63fe-4d8e-b8ad-159000af4304",
"ID3D12InfoQueue":"0742a90b-c387-483f-b946-30a7e4e61458","ID3D12Device8":"9218e6bb-f944-4f7e-a75c-b1b2c7b701f3",
"ID3D12GraphicsCommandList2":"38c3e585-ff17-412c-9150-4fc6f9d72a28","ID3D12GraphicsCommandList3":"6fda83a7-b84c-4e38-9ac8-c7bd22016b3d",
"ID3D12GraphicsCommandList5":"55050859-4024-474c-87f5-6472eaee44ea","ID3D12GraphicsCommandList6":"c3827890-e548-4cfa-96cf-5689a9370f80",
"ID3D12Device6":"c70b221b-40e4-4a17-89af-025a0727a6dc","ID3D12Device7":"5c014b53-68a1-4b9b-8bd1-dd6046b9358b",
"ID3D12Device9":"4c80e962-f032-4f60-bc9e-ebc2cfa1d83c","ID3D12Device10":"517f8718-aa66-49f9-b02b-a7ab89c06031",
"ID3D12CommandList":"7116d91c-e7e4-47ce-b8c6-ec8168f437e5","ID3D12Object":"c4fec28f-7966-4e95-9f94-f431cb56c3b8","ID3D12DeviceChild":"905db94b-a00c-4140-9df5-2b64ca9ea357","ID3D12Resource1":"9d5e227a-4430-4161-88b3-3eca6bb16e19","ID3D12Resource2":"be36ec3b-ea85-4aeb-a45a-e9d76404a495","ID3D12Heap1":"572f7389-2168-49e3-9693-d6df5871bf6d","ID3D12GraphicsCommandList7":"dd171223-8b61-4769-90e3-160ccde4e2c1","ID3D12CommandQueue1":"3a3c3165-0ee7-4b8e-a0af-6356b4c3bbb9","IDXGIFactory3":"25483823-cd46-4c7d-86ca-47aa95b837bd","IDXGIFactory7":"a4966eed-76db-44da-84c1-ee9a7afb20a8","IDXGIAdapter2":"0aa1ae0a-fa0e-4b84-8644-e05ff8e5acb5","IDXGIOutput1":"00cddea8-939b-4b83-a340-a685226666cc","IDXGIOutput4":"dc7dca35-2196-414d-9f53-617884032a60","IDXGIOutput5":"80a07424-ab52-42eb-833c-0c42fd282d98","ID3D12DeviceRemovedExtendedDataSettings":"82bc481c-6b9b-4030-aedb-7ee3d1df1e63","ID3D12ShaderReflection":"5a58797d-a72c-478d-8ba2-efc6b0efe88e",
}
img=open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe","rb").read()
for n,g in IIDS.items():
    b=uuid.UUID(g).bytes_le
    for m in re.finditer(re.escape(b),img): print("%X\t%s"%(m.start(),n))
