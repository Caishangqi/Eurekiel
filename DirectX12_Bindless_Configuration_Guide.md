# DirectX 12 Bindless渲染配置指南

## 目录
1. [核心概念](#核心概念)
2. [三种配置方法](#三种配置方法)
3. [硬件支持检查](#硬件支持检查)
4. [索引管理系统](#索引管理系统)
5. [实际使用示例](#实际使用示例)
6. [常见问题](#常见问题)

---

## 核心概念

### 什么是Bindless渲染
Bindless渲染允许着色器通过索引直接访问全局描述符堆中的资源，而不需要传统的资源绑定流程。

### 主要优势
- ✅ **CPU瓶颈解除**：从每帧数千次绑定调用 → 每物体1次常量设置
- ✅ **统一根签名**：所有管线可以使用相同的根签名
- ✅ **灵活性**：着色器可以动态访问任意资源
- ✅ **GPU驱动支持**：计算着色器可以动态生成资源索引

### 关键组件
1. **超大描述符堆**：1,000,000个CBV/SRV/UAV + 2,048个采样器
2. **索引管理系统**：管理资源在堆中的位置
3. **根常量**：传递资源索引给着色器
4. **Shader Model 6.6+**：支持直接访问全局描述符堆

---

## 三种配置方法

### 方法1: Shader Model 6.6+（推荐）

#### 特点
- ✅ 最简单、最高效
- ✅ 不需要在Root Signature中声明descriptor tables
- ✅ 直接使用`ResourceDescriptorHeap[index]`和`SamplerDescriptorHeap[index]`
- ❌ 需要较新的硬件和驱动（2021+）

#### Root Signature配置

```cpp
void CreateModernBindlessRootSignature() {
    CD3DX12_ROOT_PARAMETER1 rootParams[1];
    
    // 只需要Root Constants来传递资源索引
    rootParams[0].InitAsConstants(
        32,    // 32个32位常量 = 128字节（最大限制）
        0,     // register(b0)
        0,     // space0
        D3D12_SHADER_VISIBILITY_ALL
    );
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(
        _countof(rootParams), rootParams,
        0, nullptr,  // 不需要静态采样器
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );
    
    // 序列化和创建
    ComPtr<ID3DBlob> signature, error;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    
    D3DX12SerializeVersionedRootSignature(&rootSigDesc, 
        featureData.HighestVersion, &signature, &error);
    
    device->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
}
```

#### 使用方式

```cpp
void RenderWithSM66Bindless() {
    // 1. 设置描述符堆（只需要一次）
    ID3D12DescriptorHeap* heaps[] = { 
        resourceDescriptorHeap.Get(),  // CBV_SRV_UAV堆
        samplerDescriptorHeap.Get()    // Sampler堆
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    
    // 2. 设置Root Signature
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    
    // 3. 传递资源索引
    struct ResourceIndices {
        uint32_t albedoTextureIndex;    // 例如：1001
        uint32_t normalTextureIndex;    // 例如：1002
        uint32_t vertexBufferIndex;     // 例如：100101
        uint32_t materialBufferIndex;   // 例如：100104
    } indices = { 1001, 1002, 100101, 100104 };
    
    commandList->SetGraphicsRoot32BitConstants(0, 4, &indices, 0);
    
    // 4. 绘制
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
```

#### HLSL着色器代码

```hlsl
// Shader Model 6.6+
cbuffer ResourceIndices : register(b0, space0) {
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint vertexBufferIndex;
    uint materialBufferIndex;
};

float4 PSMain(VSOutput input) : SV_TARGET {
    // 直接访问全局描述符堆！
    Texture2D albedoTex = ResourceDescriptorHeap[albedoTextureIndex];
    Texture2D normalTex = ResourceDescriptorHeap[normalTextureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[0];
    
    float4 albedo = albedoTex.Sample(linearSampler, input.texCoord);
    float3 normal = normalTex.Sample(linearSampler, input.texCoord).xyz;
    
    return albedo;
}
```

---

### 方法2: Shader Model 6.5及以下 - Unbounded Descriptor Tables

#### 特点
- ✅ 兼容性更好，支持旧硬件
- ✅ 仍然可以实现Bindless效果
- ❌ 需要在Root Signature中声明unbounded descriptor tables
- ❌ 需要额外的`SetGraphicsRootDescriptorTable`调用

#### Root Signature配置

```cpp
void CreateUnboundedDescriptorTableRootSignature() {
    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    
    // Range 0: Unbounded SRV range
    ranges[0].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        UINT_MAX,  // ⚠️ 关键：UINT_MAX表示unbounded！
        0,         // t0
        0,         // space0
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
        0
    );
    
    // Range 1: Unbounded UAV range
    ranges[1].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        UINT_MAX,  // ⚠️ Unbounded
        0,         // u0
        0,         // space0
        D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
    );
    
    // 根参数0：Root Constants
    rootParams[0].InitAsConstants(32, 0, 0);
    
    // 根参数1：Unbounded Descriptor Table
    rootParams[1].InitAsDescriptorTable(
        _countof(ranges), 
        ranges,
        D3D12_SHADER_VISIBILITY_ALL
    );
    
    // 根参数2：Sampler descriptor table
    CD3DX12_DESCRIPTOR_RANGE1 samplerRange;
    samplerRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
        2048,  // 采样器数量
        0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0
    );
    
    rootParams[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
    
    // 创建Root Signature
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    // ... 序列化和创建
}
```

#### 使用方式

```cpp
void RenderWithUnboundedTables() {
    // 1. 设置描述符堆
    ID3D12DescriptorHeap* heaps[] = { 
        resourceDescriptorHeap.Get(),
        samplerDescriptorHeap.Get()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    
    // 2. 设置Root Signature
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    
    // 3. 设置Root Constants
    struct Indices { uint32_t albedo, normal; } indices = { 1001, 1002 };
    commandList->SetGraphicsRoot32BitConstants(0, 2, &indices, 0);
    
    // 4. ⚠️ 额外步骤：绑定Descriptor Tables
    D3D12_GPU_DESCRIPTOR_HANDLE resourceHeapStart = 
        resourceDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE samplerHeapStart = 
        samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    
    commandList->SetGraphicsRootDescriptorTable(1, resourceHeapStart);
    commandList->SetGraphicsRootDescriptorTable(2, samplerHeapStart);
    
    // 5. 绘制
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
```

#### HLSL着色器代码

```hlsl
// Shader Model 6.5及以下
cbuffer ResourceIndices : register(b0, space0) {
    uint albedoTextureIndex;
    uint normalTextureIndex;
};

// ⚠️ 需要显式声明unbounded数组
Texture2D textures[] : register(t0, space0);  // Unbounded array
SamplerState samplers[] : register(s0, space0);

float4 PSMain(VSOutput input) : SV_TARGET {
    // 通过索引访问unbounded数组
    Texture2D albedoTex = textures[albedoTextureIndex];
    Texture2D normalTex = textures[normalTextureIndex];
    SamplerState linearSampler = samplers[0];
    
    float4 albedo = albedoTex.Sample(linearSampler, input.texCoord);
    return albedo;
}
```

---

### 方法3: 混合方式

#### 特点
- ✅ 平衡性能和灵活性
- ✅ 小型常量缓冲区使用Inline CBV（性能更好）
- ✅ 纹理和大缓冲区使用Bindless
- 适用于需要频繁更新的Per-Frame/Per-Object常量

#### Root Signature配置

```cpp
void CreateHybridRootSignature() {
    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    
    // 根参数0：Root Constants（Bindless索引）
    rootParams[0].InitAsConstants(16, 0, 0);
    
    // 根参数1：内联CBV（Per-Frame常量）
    rootParams[1].InitAsConstantBufferView(
        1, 0,  // b1, space0
        D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC,
        D3D12_SHADER_VISIBILITY_ALL
    );
    
    // 根参数2：内联CBV（Per-Object常量）
    rootParams[2].InitAsConstantBufferView(
        2, 0,  // b2, space0
        D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC,
        D3D12_SHADER_VISIBILITY_ALL
    );
    
    // ... 创建Root Signature
}
```

#### 使用方式

```cpp
void RenderWithHybrid() {
    // 设置Root Constants（Bindless索引）
    struct Indices { uint32_t albedo, vertex; } indices = { 1001, 100101 };
    commandList->SetGraphicsRoot32BitConstants(0, 2, &indices, 0);
    
    // 设置内联CBV（传统方式）
    commandList->SetGraphicsRootConstantBufferView(1, perFrameCB->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, perObjectCB->GetGPUVirtualAddress());
    
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
```

---

## 硬件支持检查

### 检查代码

```cpp
struct BindlessSupportInfo {
    bool supportsShaderModel66;
    bool supportsUnboundedDescriptorTables;
    UINT maxDescriptorsInHeap;
    D3D12_RESOURCE_BINDING_TIER bindingTier;
};

BindlessSupportInfo CheckBindlessSupport(ID3D12Device* device) {
    BindlessSupportInfo info = {};
    
    // 1. 检查Shader Model 6.6支持
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
    if (SUCCEEDED(device->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))) {
        info.supportsShaderModel66 = 
            (shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);
    }
    
    // 2. 检查资源绑定层级
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
    info.bindingTier = options.ResourceBindingTier;
    
    // Tier 2及以上支持unbounded descriptor tables
    info.supportsUnboundedDescriptorTables = 
        (info.bindingTier >= D3D12_RESOURCE_BINDING_TIER_2);
    
    // 3. 描述符堆大小
    info.maxDescriptorsInHeap = 1000000;  // Tier 2+: 100万
    
    return info;
}

void SelectBindlessMethod(const BindlessSupportInfo& info) {
    if (info.supportsShaderModel66) {
        printf("✅ 使用方法1: SM6.6 Native Bindless (推荐)\n");
    } else if (info.supportsUnboundedDescriptorTables) {
        printf("⚠️ 使用方法2: Unbounded Descriptor Tables\n");
    } else {
        printf("❌ 硬件不支持完整Bindless，考虑使用混合方式\n");
    }
}
```

### 编译着色器

```cpp
ComPtr<IDxcBlob> CompileBindlessShader(const wchar_t* path, bool useSM66) {
    ComPtr<IDxcCompiler> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    
    // 编译参数
    std::vector<LPCWSTR> args = {
        L"-E", L"PSMain",
        L"-T", useSM66 ? L"ps_6_6" : L"ps_6_5",
        DXC_ARG_WARNINGS_ARE_ERRORS
    };
    
    // ... 编译着色器
}
```

---

## 索引管理系统

### 索引范围规划

```cpp
struct IndexRanges {
    // 预留0-99给特殊用途
    static constexpr uint32_t ERROR_TEXTURE_INDEX = 0;
    static constexpr uint32_t DEFAULT_WHITE_TEXTURE = 1;
    static constexpr uint32_t DEFAULT_BLACK_TEXTURE = 2;
    static constexpr uint32_t DEFAULT_NORMAL_TEXTURE = 3;
    
    // Buffer SRV范围：100 - 100,099
    static constexpr uint32_t BUFFER_SRV_START = 100;
    static constexpr uint32_t BUFFER_SRV_COUNT = 100000;
    
    // Buffer UAV范围：100,100 - 150,099
    static constexpr uint32_t BUFFER_UAV_START = 100100;
    static constexpr uint32_t BUFFER_UAV_COUNT = 50000;
    
    // Texture 2D范围：150,100 - 650,099
    static constexpr uint32_t TEXTURE_2D_START = 150100;
    static constexpr uint32_t TEXTURE_2D_COUNT = 500000;
    
    // Texture Cube范围：650,100 - 700,099
    static constexpr uint32_t TEXTURE_CUBE_START = 650100;
    static constexpr uint32_t TEXTURE_CUBE_COUNT = 50000;
};
```

### 资源注册示例

```cpp
class BindlessResourceManager {
public:
    uint32_t RegisterTexture2D(ID3D12Resource* texture, DXGI_FORMAT format, 
                               const std::string& debugName) {
        // 1. 分配索引
        uint32_t index = AllocateIndex(ResourceType::TEXTURE_2D);
        
        // 2. 创建SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
        
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
            descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            index,
            descriptorSize
        );
        
        device->CreateShaderResourceView(texture, &srvDesc, handle);
        
        // 3. 记录资源信息
        activeResources[index] = { texture, ResourceType::TEXTURE_2D, debugName };
        
        return index;
    }
    
    uint32_t RegisterStructuredBuffer(ID3D12Resource* buffer, 
                                     uint32_t elementSize, uint32_t elementCount,
                                     const std::string& debugName) {
        uint32_t index = AllocateIndex(ResourceType::BUFFER_SRV);
        
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = elementCount;
        srvDesc.Buffer.StructureByteStride = elementSize;
        
        // ... 创建SRV和记录
        
        return index;
    }
};
```

---

## 实际使用示例

### 完整的渲染流程

```cpp
class BindlessRenderer {
public:
    void Initialize() {
        // 1. 创建描述符堆
        CreateDescriptorHeaps();
        
        // 2. 创建Root Signature
        CreateBindlessRootSignature();
        
        // 3. 加载和注册资源
        uint32_t albedoIndex = resourceManager.RegisterTexture2D(
            LoadTexture("albedo.png"), DXGI_FORMAT_R8G8B8A8_UNORM, "Albedo");
        uint32_t normalIndex = resourceManager.RegisterTexture2D(
            LoadTexture("normal.png"), DXGI_FORMAT_R8G8B8A8_UNORM, "Normal");
        uint32_t vertexIndex = resourceManager.RegisterStructuredBuffer(
            CreateVertexBuffer(), sizeof(Vertex), 1000, "Vertices");
        
        // 4. 存储索引
        materialResources.albedoTextureIndex = albedoIndex;
        materialResources.normalTextureIndex = normalIndex;
        materialResources.vertexBufferIndex = vertexIndex;
    }
    
    void Render() {
        // 设置描述符堆（每帧一次）
        ID3D12DescriptorHeap* heaps[] = { 
            resourceDescriptorHeap.Get(),
            samplerDescriptorHeap.Get()
        };
        commandList->SetDescriptorHeaps(2, heaps);
        
        // 设置Root Signature
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        
        // 渲染多个物体
        for (const auto& mesh : meshes) {
            // 更新资源索引
            materialResources.transformIndex = mesh.transformIndex;
            
            // 一次性传递所有索引
            commandList->SetGraphicsRoot32BitConstants(
                0, sizeof(materialResources) / 4, &materialResources, 0);
            
            // 绘制
            commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }
    }
};
```

### 着色器中的使用

```hlsl
cbuffer MaterialResources : register(b0, space0) {
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint roughnessTextureIndex;
    uint metallicTextureIndex;
    uint vertexBufferIndex;
    uint materialBufferIndex;
    uint transformIndex;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 texCoord;
};

VSOutput VSMain(uint vertexID : SV_VertexID) {
    // Bindless访问顶点缓冲区
    StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[vertexBufferIndex];
    Vertex vertex = vertices[vertexID];
    
    // ... 顶点变换
}

float4 PSMain(VSOutput input) : SV_TARGET {
    // Bindless访问所有纹理
    Texture2D albedoTex = ResourceDescriptorHeap[albedoTextureIndex];
    Texture2D normalTex = ResourceDescriptorHeap[normalTextureIndex];
    Texture2D roughnessTex = ResourceDescriptorHeap[roughnessTextureIndex];
    Texture2D metallicTex = ResourceDescriptorHeap[metallicTextureIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[0];
    
    // PBR材质采样
    float4 albedo = albedoTex.Sample(linearSampler, input.texCoord);
    float3 normal = normalTex.Sample(linearSampler, input.texCoord).xyz * 2.0 - 1.0;
    float roughness = roughnessTex.Sample(linearSampler, input.texCoord).r;
    float metallic = metallicTex.Sample(linearSampler, input.texCoord).r;
    
    // ... 光照计算
    return albedo;
}
```

---

## 常见问题

### Q1: RTV和DSV需要放在Bindless堆中吗？

**A:** 不需要。RTV和DSV使用传统的`OMSetRenderTargets`绑定，存储在专用的RTV/DSV描述符堆中。这些堆不需要`SHADER_VISIBLE`标志。

但是，如果你需要在着色器中读取render target（例如后处理），需要额外创建一个SRV视图并注册到Bindless系统中。

```cpp
// RTV视图 - 用于输出（传统绑定）
device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

// SRV视图 - 用于读取（Bindless访问）
uint32_t backBufferSRVIndex = resourceManager.RegisterTexture2D(
    backBuffer, DXGI_FORMAT_R8G8B8A8_UNORM, "BackBuffer");
```

### Q2: 如何处理NonUniformResourceIndex？

**A:** 当索引值不是uniform时（例如从texture中采样得到的索引），必须使用`NonUniformResourceIndex()`：

```hlsl
// 错误：直接使用可能导致未定义行为
uint dynamicIndex = materialBuffer[instanceID].textureIndex;
Texture2D tex = ResourceDescriptorHeap[dynamicIndex];  // ❌

// 正确：使用NonUniformResourceIndex
uint dynamicIndex = materialBuffer[instanceID].textureIndex;
uint safeIndex = NonUniformResourceIndex(dynamicIndex);
Texture2D tex = ResourceDescriptorHeap[safeIndex];  // ✅
```

**何时需要使用：**
- ✅ 从常量缓冲区读取的索引 - **不需要**
- ✅ 从根常量读取的索引 - **不需要**
- ❌ 从结构化缓冲区读取的索引 - **需要**
- ❌ 从纹理采样的索引 - **需要**
- ❌ 任何非常量表达式计算的索引 - **需要**

### Q3: Root Constants有128字节限制怎么办？

**A:** 有几种解决方案：

1. **使用结构化缓冲区存储材质数据**：
```cpp
// Root Constants只传递缓冲区索引和材质索引
struct DrawConstants {
    uint32_t materialBufferIndex;  // 材质缓冲区在堆中的索引
    uint32_t materialIndex;        // 材质在缓冲区中的索引
};

// 着色器中
StructuredBuffer<MaterialData> materials = ResourceDescriptorHeap[materialBufferIndex];
MaterialData material = materials[materialIndex];
// 现在可以访问材质中的所有纹理索引
```

2. **使用多个Root Constants槽**：
```cpp
rootParams[0].InitAsConstants(32, 0, 0);  // 前32个常量
rootParams[1].InitAsConstants(32, 1, 0);  // 后32个常量
```

3. **使用Inline CBV作为补充**：
```cpp
// 混合方式
rootParams[0].InitAsConstants(16, 0, 0);           // Bindless索引
rootParams[1].InitAsConstantBufferView(1, 0);      // 其他数据
```

### Q4: 如何调试Bindless资源访问？

**A:** 调试技巧：

1. **预留错误纹理**：
```cpp
// 索引0预留给错误纹理（红色）
CreateErrorTexture(resourceDescriptorHeap, 0);

// 删除资源时用错误纹理替换
void UnregisterResource(uint32_t index) {
    CopyDescriptor(errorTextureHandle, descriptorHeap[index]);
    // 这样访问已删除的资源会显示红色
}
```

2. **可视化索引值**：
```hlsl
// 将索引值转换为颜色
float4 PSDebug() : SV_TARGET {
    uint index = albedoTextureIndex;
    float r = float(index & 0xFF) / 255.0;
    float g = float((index >> 8) & 0xFF) / 255.0;
    float b = float((index >> 16) & 0xFF) / 255.0;
    return float4(r, g, b, 1.0);
}
```

3. **使用GPU Based Validation**：
```cpp
// 启用GPU验证层
D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
debugController->EnableDebugLayer();
debugController->SetEnableGPUBasedValidation(TRUE);
```

### Q5: Bindless对性能的影响？

**A:** 性能特点：

- ✅ **CPU性能**：显著提升，减少90%+的绑定调用
- ⚠️ **GPU性能**：可能略有下降（增加间接访问）
- ✅ **总体性能**：通常整体提升，尤其是CPU受限场景
- ✅ **批处理**：更容易合并DrawCall

建议：
- 大型场景：Bindless几乎总是更好
- 简单场景：传统绑定可能更简单
- 后处理：Bindless特别适合

---

## 快速参考

### 方法选择决策树

```
检查硬件是否支持SM6.6？
├─ 是 → 使用方法1（SM6.6 Native Bindless）⭐推荐
│       - 最简单的Root Signature
│       - 直接使用ResourceDescriptorHeap[index]
│       - 无需额外descriptor table绑定
│
└─ 否 → 检查是否支持Tier 2？
        ├─ 是 → 使用方法2（Unbounded Descriptor Tables）
        │       - 设置NumDescriptors = UINT_MAX
        │       - 需要SetGraphicsRootDescriptorTable
        │       - 使用数组语法 textures[]
        │
        └─ 否 → 使用方法3（混合方式）
                - Root Constants + Inline CBV
                - 部分Bindless，部分传统绑定
```

### 关键代码清单

| 操作 | SM6.6方法 | Unbounded Table方法 |
|------|-----------|---------------------|
| Root Signature | `InitAsConstants(32, 0, 0)` | `InitAsDescriptorTable(ranges)` + `UINT_MAX` |
| 描述符堆标志 | `SHADER_VISIBLE` | `SHADER_VISIBLE` |
| 额外绑定调用 | 不需要 | `SetGraphicsRootDescriptorTable` |
| 着色器语法 | `ResourceDescriptorHeap[idx]` | `textures[idx]` |
| Shader Model | 6.6+ | 6.0+ |
| 硬件要求 | 2021+ | Tier 2+ |

---

## 参考资源

### 官方文档
- [HLSL Shader Model 6.6 公告](https://devblogs.microsoft.com/directx/hlsl-shader-model-6-6/)
- [DirectX 12 Resource Binding](https://docs.microsoft.com/en-us/windows/win32/direct3d12/resource-binding)

### 教程和博客
- [Bindless Rendering in DirectX12 and SM6.6](https://rtarun9.github.io/blogs/bindless_rendering/)
- [Alex Tardif: Bindless Rendering](https://alextardif.com/Bindless.html)
- [Wicked Engine: Bindless Descriptors](https://wickedengine.net/2021/04/bindless-descriptors/)

### 示例代码
- [BasicBindlessRendering (GitHub)](https://github.com/DDreher/BasicBindlessRendering)
- [HelloBindlessD3D12 (GitHub)](https://github.com/TheSandvichMaker/HelloBindlessD3D12)
- [MJP's DeferredTexturing (GitHub)](https://github.com/TheRealMJP/DeferredTexturing)

---

**文档版本**: 1.0  
**最后更新**: 2025年9月  
**目标读者**: 使用DirectX 12开发引擎的C++开发者
