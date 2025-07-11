#pragma once

#include <cstdint>
#include <cstddef>

#include <Zibra/Foundation.h>

#if defined(_MSC_VER)
#define ZRHI_API_IMPORT extern "C" __declspec(dllimport)
#define ZRHI_CALL_CONV __cdecl
#elif defined(__GNUC__)
#define ZRHI_API_IMPORT extern "C"
#define ZRHI_CALL_CONV
#else
#error "Unsupported compiler"
#endif

#ifdef ZRHI_USE_D3D11_INTEGRATION
#include <d3d11.h>
#endif // ZRHI_USE_D3D11_INTEGRATION

#ifdef ZRHI_USE_D3D12_INTEGRATION
#include <d3d12.h>
#endif // ZRHI_USE_D3D12_INTEGRATION

#ifdef ZRHI_USE_VULKAN_INTEGRATION
#include <vulkan/vulkan.h>
#endif // ZRHI_USE_VULKAN_INTEGRATION

#ifdef ZRHI_USE_METAL_INTEGRATION
#include <Metal/Metal.hpp>
#endif // ZRHI_USE_METAL_INTEGRATION

#define ZRHI_DEFINE_BITMASK_ENUM(enumType)                                                 \
    inline enumType operator&(enumType a, enumType b) noexcept                             \
    {                                                                                      \
        return static_cast<enumType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); \
    }                                                                                      \
    inline enumType operator|(enumType a, enumType b) noexcept                             \
    {                                                                                      \
        return static_cast<enumType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); \
    }                                                                                      \
    inline bool operator||(bool a, enumType b) noexcept                                    \
    {                                                                                      \
        return a || static_cast<uint32_t>(b) != 0;                                         \
    }                                                                                      \
    inline bool operator&&(bool a, enumType b) noexcept                                    \
    {                                                                                      \
        return a && static_cast<uint32_t>(b) != 0;                                         \
    }                                                                                      \
    inline bool operator!(enumType a) noexcept                                             \
    {                                                                                      \
        return static_cast<uint32_t>(a) == 0;                                              \
    }

namespace Zibra::RHI
{
    constexpr Version ZRHI_VERSION = {4, 0, 0, 0};

    enum class GFXAPI : int8_t
    {
        /// NULL API
        Null = 0,
        /// Microsoft Direct3D 11
        D3D11 = 1,
        /// Microsoft Direct3D 12
        D3D12 = 2,
        /// Apple Metal API
        Metal = 3,
        /// Lunarg Vulkan
        Vulkan = 4,
        /// Auto-detect API
        Auto = 64,
        /// Delegate
        Custom = 100,
        /// Invalid value
        Invalid = -1,
    };

    enum ReturnCode
    {
        ZRHI_SUCCESS = 0,
        ZRHI_TIMEOUT = 10,
        ZRHI_QUEUE_EMPTY = 20,

        ZRHI_ERROR = 100,
        ZRHI_ERROR_OUT_OF_CPU_MEMORY = 120,
        ZRHI_ERROR_OUT_OF_GPU_MEMORY = 121,

        ZRHI_ERROR_INVALID_CALL = 300,
        ZRHI_ERROR_NOT_IMPLEMENTED = 310,
        ZRHI_ERROR_NOT_SUPPORTED = 311,
    };

    namespace Integration
    {

        /**
         * GFXCore is an adapter between RHI backend and parent engine.
         * Used for resource access, latency sync and other.
         */
        class GFXCore
        {
        public:
            virtual ~GFXCore() = default;
            [[nodiscard]] virtual GFXAPI GetGFXAPI() const noexcept = 0;
        };

#pragma region D3D11 GFXCore
#ifdef ZRHI_USE_D3D11_INTEGRATION
        struct D3D11Texture2DDesc
        {
            ID3D11Texture2D* texture;
        };
        struct D3D11Texture3DDesc
        {
            ID3D11Texture3D* texture;
        };
        struct D3D11BufferDesc
        {
            ID3D11Buffer* buffer;
        };

        class D3D11GFXCore : public GFXCore
        {
        public:
            virtual ID3D11Device* GetDevice() noexcept = 0;
            virtual ID3D11DeviceContext* GetDeviceContext() noexcept = 0;

            virtual ReturnCode AccessBuffer(void* resourceHandle, D3D11BufferDesc& bufferDesc) noexcept = 0;
            virtual ReturnCode AccessTexture2D(void* resourceHandle, D3D11Texture2DDesc& texture2dDesc) noexcept = 0;
            virtual ReturnCode AccessTexture3D(void* resourceHandle, D3D11Texture3DDesc& texture3dDesc) noexcept = 0;
        };
#endif // ZRHI_USE_D3D11_INTEGRATION
#pragma endregion D3D11 GFXCore

#pragma region D3D12 GFXCore
#ifdef ZRHI_USE_D3D12_INTEGRATION
        struct D3D12Texture2DDesc
        {
            ID3D12Resource* texture;
        };
        struct D3D12Texture3DDesc
        {
            ID3D12Resource* texture;
        };
        struct D3D12BufferDesc
        {
            ID3D12Resource* buffer;
        };

        struct D3D12TrackedResourceState
        {
            D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_COMMON;
            ID3D12Resource* resource = nullptr;
        };

        class D3D12GFXCore : public GFXCore
        {
        public:
            virtual ID3D12Device* GetDevice() noexcept = 0;
            virtual ID3D12GraphicsCommandList* GetCommandList() noexcept = 0;
            virtual ID3D12Fence* GetFrameFence() noexcept = 0;
            virtual size_t GetNextFrameFenceValue() noexcept = 0;

            // May return nullptr if not supported.
            virtual ID3D12CommandQueue* GetCommandQueue() noexcept = 0;

            virtual ReturnCode AccessBuffer(void* resourceHandle, D3D12BufferDesc& bufferDesc) noexcept = 0;
            virtual ReturnCode AccessTexture2D(void* resourceHandle, D3D12Texture2DDesc& texture2dDesc) noexcept = 0;
            virtual ReturnCode AccessTexture3D(void* resourceHandle, D3D12Texture3DDesc& texture3dDesc) noexcept = 0;

            virtual ReturnCode StartRecording() noexcept = 0;
            /**
             * Submits job to queue and stops recording state.
             * @param statesCount Resources states count in states param.
             * @param states Resources state array for command list tail.
             * @param finishEvent GPU work completion CPU event handle. (Consumer will close event handle by itself)
             */
            virtual ReturnCode StopRecording(size_t statesCount, const D3D12TrackedResourceState* states, HANDLE* finishEvent) noexcept = 0;
        };
#endif // ZRHI_USE_D3D12_INTEGRATION
#pragma endregion D3D12 GFXCore

#pragma region Vulkan GFXCore
#ifdef ZRHI_USE_VULKAN_INTEGRATION
        struct VulkanMemoryDesc
        {
            VkDeviceMemory memory;
            VkDeviceSize offset; // offset within memory
            VkDeviceSize size;   // size in bytes, may be less than the total size of memory;
            void* mapped; // pointer to mapped memory block, NULL if not mappable, offset is already applied, remaining block still has at least
                          // the given size.
            VkMemoryPropertyFlags flags; // Vulkan memory properties
            uint32_t memoryTypeIndex;    // index into VkPhysicalDeviceMemoryProperties::memoryTypes
        };

        struct VulkanTextureDesc
        {
            VulkanMemoryDesc memory;
            VkImage image;
            VkImageLayout layout;
            VkImageAspectFlags aspect;
            VkImageUsageFlags usage;
            VkFormat format;
            VkExtent3D extent;
            VkImageTiling tiling;
            VkImageType type;
            VkSampleCountFlagBits samples;
            int layers;
            int mipCount;
        };

        struct VulkanBufferDesc
        {
            VulkanMemoryDesc memory;
            VkBuffer buffer;
            VkDeviceSize sizeInBytes;
            VkBufferUsageFlags usage;
        };

        class VulkanGFXCore : public GFXCore
        {
        public:
            virtual VkInstance GetInstance() noexcept = 0;
            virtual VkDevice GetDevice() noexcept = 0;
            virtual VkPhysicalDevice GetPhysicalDevice() noexcept = 0;
            virtual VkCommandBuffer GetCommandBuffer() noexcept = 0;
            virtual PFN_vkVoidFunction GetInstanceProcAddr(const char* procName) noexcept = 0;
            virtual size_t GetCurrentFrame() noexcept = 0;
            virtual size_t GetSafeFrame() noexcept = 0;
            virtual ReturnCode AccessBuffer(void* resourceHandle, VulkanBufferDesc& bufferDesc) noexcept = 0;
            virtual ReturnCode AccessImage(void* resourceHandle, VkImageLayout layout, VulkanTextureDesc& textureDesc) noexcept = 0;
            virtual ReturnCode StartRecording() noexcept = 0;
            /**
             * Submits job to queue and stops recording state.
             * @param finishFence GPU work completion fence. (Consumer will release fence by itself)
             */
            virtual ReturnCode StopRecording(VkFence* finishFence) noexcept = 0;
        };
#endif // ZRHI_USE_VULKAN_INTEGRATION
#pragma endregion Vulkan GFXCore

#pragma region Metal GFXCore
#ifdef ZRHI_USE_METAL_INTEGRATION
        struct MetalBufferDesc
        {
            MTL::Buffer* buffer = nullptr;
        };

        struct MetalTexture2DDesc
        {
            MTL::Texture* texture = nullptr;
        };

        struct MetalTexture3DDesc
        {
            MTL::Texture* texture = nullptr;
        };

        class MetalGFXCore : public GFXCore
        {
        public:
            virtual MTL::Device* GetDevice() noexcept = 0;
            virtual ReturnCode AccessBuffer(void* bufferHandle, MetalBufferDesc& bufferDesc) noexcept = 0;
            virtual ReturnCode AccessTexture2D(void* bufferHandle, MetalTexture2DDesc& bufferDesc) noexcept = 0;
            virtual ReturnCode AccessTexture3D(void* bufferHandle, MetalTexture3DDesc& bufferDesc) noexcept = 0;
            virtual MTL::CommandBuffer* GetCommandBuffer() noexcept = 0;
            virtual ReturnCode StartRecording() noexcept = 0;
            virtual ReturnCode StopRecording() noexcept = 0;
        };
#endif // ZRHI_USE_METAL_INTEGRATION
#pragma endregion Metal GFXCore

    } // namespace Integration

    /**
     * Resource base interface. Used for bindings
     * @interface
     */
    class Resource
    {
    public:
        /**
         * Resource type. Used for fast RTTI.
         */
        enum Type : uint8_t
        {
            Buffer,
            Texture2D,
            Texture3D,
        };

    public:
        virtual ~Resource() noexcept = default;
        /**
         * Returns resource type for run time type identification.
         * @return RTTI resource type.
         */
        [[nodiscard]] virtual Type GetType() const noexcept = 0;
    };

    /**
     * RHI controlled buffer object.
     * @warning Buffer object created in one RHI instance can't be used in other RHI instance.
     */
    class Buffer : public Resource
    {};
    /**
     * RHI controlled 2D texture object.
     * @warning Texture2D object created in one RHI instance can't be used in other RHI instance.
     */
    class Texture2D : public Resource
    {};
    /**
     * RHI controlled 3D texture object.
     * @warning Texture3D object created in one RHI instance can't be used in other RHI instance.
     */
    class Texture3D : public Resource
    {};
    /**
     * RHI controlled Compute Pipeline State Object (PSO).
     * @warning ComputePSO object created in one RHI instance can't be used in other RHI instance.
     */
    class ComputePSO
    {
    public:
        virtual ~ComputePSO() noexcept = default;
    };
    /**
     * RHI controlled Graphics Pipeline State Object (PSO).
     * @warning GraphicPSO object created in one RHI instance can't be used in other RHI instance.
     */
    class GraphicsPSO
    {
    public:
        virtual ~GraphicsPSO() noexcept = default;
    };
    /**
     * RHI controller Framebuffer object.
     * @warning Framebuffer object created in one RHI instance can't be used in other RHI instance.
     */
    class Framebuffer
    {
    public:
        virtual ~Framebuffer() noexcept = default;
    };
    /**
     * RHI controlled Descriptor Heap object.
     * @warning DescriptorHeap object created in one RHI instance can't be used in other RHI instance.
     */
    class DescriptorHeap
    {
    public:
        virtual ~DescriptorHeap() noexcept = default;
    };
    /**
     * RHI controlled Query Heap object.
     * @warning QueryHeap object created in one RHI instance can't be used in other RHI instance.
     */
    class QueryHeap
    {
    public:
        virtual ~QueryHeap() noexcept = default;
    };
    /**
     * RHI controlled Sampler State object.
     * @warning Sampler object created in one RHI instance can't be used in other RHI instance.
     */
    class Sampler
    {
    public:
        virtual ~Sampler() noexcept = default;
    };

    namespace Limits
    {
        // Increase limits as needed when intending to use more
        constexpr uint32_t MaxInputElementsCount = 1;
        constexpr uint32_t MaxUploadBufferSize = 2048;
        // Metal allows up to 4kb to set via fast path
        // In case it's less than 4kb you will need to upload it
        constexpr uint32_t MaxUploadAndSetConstantBufferSize = 4096;
        constexpr uint32_t MaxBoundSRVs = 8;
        constexpr uint32_t MaxBoundUAVs = 8;
        constexpr uint32_t MaxBoundCBVs = 8;
        constexpr uint32_t MaxBoundRTVs = 1;
        constexpr uint32_t MaxBoundSamplers = 1;
        constexpr uint32_t MaxBoundVertexBuffers = 1;
        constexpr uint32_t MaxDebugRegionNestingDepth = 7;
    } // namespace Limits

    enum class Feature : uint8_t
    {
        /// Timestamp query/counter support.
        Timestamp,
        /// Timestamp calibration support. Includes CPU timestamps.
        TimestampCalibration,
        /// Uniform Memory Architecture.
        /// Allows creation of most resource types in CPU accessible memory.
        UMA,
        Count
    };

    enum class ResourceHeapType : uint8_t
    {
        /**
         * Specifies the default heap, which typically corresponds to VRAM.
         * This heap type experiences the most bandwidth for the GPU, but cannot provide CPU access.
         * The GPU can read and write to the memory from this pool, and all resource states are available in this heap.
         */
        Default,
        /**
         * Specifies the Upload heap.
         * This heap type experiences the sub-optimal bandwidth for the GPU, but allows for CPU access.
         * So it's recommended to only use it for data that will be read by GPU only once after each CPU write.
         * The GPU can only read the memory from this pool, The CPU can only write the memory from this pool.
         * And only read-only states can be used in this heap.
         */
        Upload,
        /**
         * Specifies the Readback heap.
         * This heap type experiences the sub-optimal bandwidth for the GPU, but allows for CPU access.
         * So it's recommended to only use it for data that will be written by GPU only once before CPU read.
         * The GPU can only write the memory from this pool, The CPU can only read the memory from this pool.
         * And only COPY_DEST state can be used in this heap.
         */
        Readback,
        Count
    };

    enum class ResourceUsage : uint16_t
    {
        /// Empty value.
        None = 0x0,
        /// Resource is used as Vertex Buffer for draw calls.
        /// @warning Only available for buffers
        VertexBuffer = 1 << 0,
        /// Resource is used as Index Buffer for draw calls.
        /// @warning Only available for buffers
        IndexBuffer = 1 << 1,
        /// Resource is used as Constant Buffer.
        /// @warning Only available for buffers
        ConstantBuffer = 1 << 2,
        /// Resource is used as Shader Resource.
        ShaderResource = 1 << 3,
        /// Resource is used as Unordered Access.
        UnorderedAccess = 1 << 4,
        /// Resource is used as Indirect Arguments Buffer.
        /// @warning Only available for buffers
        Indirect = 1 << 5,
        /// Resource is used as Copy Source.
        CopySource = 1 << 6,
        /// Resource is used as Copy Destination.
        CopyDest = 1 << 7,
        /// Resource is used as graphics pipeline color render target.
        ColorRenderTarget = 1 << 8,
        /// Resource is used as graphics pipeline depth stencil render target.
        DepthStencilRenderTarget = 1 << 9,

        ValidMask = 0xFFF,
    };
    ZRHI_DEFINE_BITMASK_ENUM(ResourceUsage)

    enum class DescriptorHeapType : uint8_t
    {
        /// Descriptor Heap for Shader Resources, Constant Buffers & Unordered Access descriptors.
        SRV_CBV_UAV,
        /// Descriptor Heap for Render Target descriptors.
        RTV,
        /// Descriptor Heap for Depth Stencil descriptors.
        DSV,
        /// Descriptor Heap for Sampler State descriptors.
        Sampler,
        Count
    };

    enum class ResourceState : uint8_t
    {
        /// Default state
        /// on some APIs (D3D12), resources can be implicitly promoted and decayed to this state
        Common,
        /// Shader Resource View in compute pipeline.
        ComputeSRV,
        /// Shader Resource View in graphics pipeline.
        GraphicSRV,
        /// Unordered Access View.
        UAV,
        /// Constant Buffer View
        CBV,
        /// Render Target View
        RTV,
        /// Depth Stencil View
        DSV,
        /// Indirect Arguments
        IndirectArgument,
        /// Copy Source
        CopySource,
        /// Copy Destination
        CopyDest,
        Count,
    };

    enum class DescriptorType : uint8_t
    {
        /// Buffer Constant Buffer View (CBV)
        BufferCBV,
        /// Buffer Shader Resource View (SRV)
        BufferSRV,
        /// Buffer Shader Resource View (SRV)
        BufferFormattedSRV,
        /// Buffer Unordered Access View (UAV)
        BufferUAV,
        /// Buffer Unordered Access View (UAV)
        BufferFormattedUAV,
        /// 2D Texture Shader Resource View (SRV)
        Texture2DSRV,
        /// 2D Texture Unordered Access View (UAV)
        Texture2DUAV,
        /// 3D Texture Shader Resource View (SRV)
        Texture3DSRV,
        /// 3D Texture Unordered Access View (UAV)
        Texture3DUAV,
        /// Sampler State Object
        Sampler,
    };

    enum class TextureFormat : uint8_t
    {
        None,
        /// A four-component, 32-bit unsigned-normalized-integer format that supports 8 bits per channel including alpha.
        R8G8B8A8_UNorm,
        /// A four-component, 64-bit floating-point format that supports 16 bits per channel including alpha.
        R16G16B16A16_SFloat,
        /// A four-component, 128-bit floating-point format that supports 32 bits per channel including alpha.
        R32G32B32A32_SFloat,
        /// A single-component, 16-bit floating-point format that supports 16 bits for the red channel.
        R16_SFloat,
        /// A single-component, 32-bit floating-point format that supports 32 bits for the red channel.
        R32_SFloat,
        /// A 32-bit floating-point component, and two unsigned-integer components (with an additional 32 bits).
        /// This format supports 32-bit depth, 8-bit stencil, and 24 bits are unused.
        D32_SFloat_S8_UInt,
        Count,
    };

    enum class QueryHeapType : uint8_t
    {
        /// Query Heap for Timestamp queries.
        Timestamp,
        Count
    };

    enum class QueryType : uint8_t
    {
        /// Timestamp query.
        Timestamp,
        Count,
    };

    struct TimestampQueryResult
    {
        /// Non decoded timestamp value.
        uint64_t timestamp;
    };

    enum class ShaderStage : uint8_t
    {
        /// Vertex Shader
        VS,
        /// Pixel Shader / Fragment Shader
        PS,
        /// Compute Shader
        CS,
        Count,
    };

    enum class SamplerAddressMode : uint8_t
    {
        /**
         * The Wrap texture address mode makes sampler repeat the texture on every integer junction.
         * Suppose, for example, your application creates a square primitive and specifies texture coordinates of
         * (0.0,0.0), (0.0,3.0), (3.0,3.0), and (3.0,0.0). Setting the texture addressing mode to "Wrap" results in the texture being
         * applied three times in both the u-and v-directions.
         * @see https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/texture-addressing-modes#wrap-texture-address-mode
         */
        Wrap,
        /**
         * The Clamp texture address mode causes Direct3D to clamp your texture coordinates to the [0.0, 1.0] range;
         * Clamp mode applies the texture once, then smears the color of edge pixels.
         * Suppose that your application creates a square primitive and assigns texture coordinates of
         * (0.0,0.0), (0.0,3.0), (3.0,3.0), and (3.0,0.0) to the primitive's vertices.
         * Setting the texture addressing mode to "Clamp" results in the texture being applied once.
         * The pixel colors at the top of the columns and the end of the rows are extended to the top and right
         * of the primitive respectively.
         * @see https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/texture-addressing-modes#clamp-texture-address-mode
         */
        Clamp,
        Count,
    };

    enum class SamplerFilter : uint8_t
    {
        /// @see https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures-texel-nearest-filtering
        Point,
        /// @see https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures-texel-linear-filtering
        Trilinear,
        /// @see https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures-texel-anisotropic-filtering
        Aniso16x,
        Count,
    };

    enum class DepthComparisonFunc : uint8_t
    {
        /// If the source data is less than the destination data, the comparison passes.
        Less,
        /// If the source data is less than or equal to the destination data, the comparison passes.
        LessEqual,
        /// If the source data is greater than the destination data, the comparison passes.
        Greater,
        /// If the source data is greater than or equal to the destination data, the comparison passes.
        GreaterEqual,
        /// If the source data is equal to the destination data, the comparison passes.
        Equal,
        /// Always pass the comparison.
        Always,
        Count,
    };

    enum class TriangleCullMode : uint8_t
    {
        /// Culling is disabled.
        None,
        /// Cull back faces, leave front faces.
        CullBack,
        /// Cull front faces, leave back faces.
        CullFront,
        Count,
    };

    enum class PrimitiveTopology : uint8_t
    {
        /**
         * A triangle list is a list of isolated triangles. They might or might not be near each other.
         * A triangle list must have at least three vertices and the total number of vertices must be divisible by three.
         */
        TriangleList,
        Count,
    };

    enum class InputElementFormat : uint8_t
    {
        /// 16 bit half precision floating point number.
        Float16,
        /// 32 bit full precision floating point number.
        Float32,
        /// 16 bit unsigned integer number.
        Uint16,
        /// 32 bit unsigned integer number.
        Uint32,
        Count,
    };

    enum class BlendFactor : uint8_t
    {
        /// RGB(0,0,0) A(0)
        Zero,
        /// RGB(1,1,1) A(1)
        One,

        /// RGB(R_s0,G_s0,B_s0) A(A_s0)
        SrcColor,
        /// RGB(1-R_s0,1-G_s0,1-B_s0) A(1-A_s0)
        OneMinusSrcColor,
        /// RGB(A_s0,A_s0,A_s0) A(A_s0)
        SrcAlpha,
        /// RBG(1-A_s0,1-A_s0,1-A_s0) A(1-A_s0)
        OneMinusSrcAlpha,
        /// RGB(R_d,G_d,B_d) A(A_d)
        DstColor,
        /// RGB(1-R_d,1-G_d,1-B_d) A(1-A_d)
        OneMinusDstColor,
        /// RGB(A_d,A_d,A_d) A(A_d)
        DstAlpha,
        /// RGB(1-A_d,1-A_d,1-A_d) A(1-A_d)
        OneMinusDstAlpha,
        /// RGB(f,f,f) f=min(A_s0,1-A_d) A(1)
        SrcAlphaSaturated,
    };

    enum class BlendOperation : uint8_t
    {
        /**
         * R = R_s0 × S_r + R_d × D_r
         * G = G_s0 × S_g + G_d × D_g
         * B = B_s0 × S_b + B_d × D_b
         * A = A_s0 × S_a + A_d × D_a
         */
        Add,
        /**
         * R = R_s0 × S_r - R_d × D_r
         * G = G_s0 × S_g - G_d × D_g
         * B = B_s0 × S_b - B_d × D_b
         * A = A_s0 × S_a - A_d × D_a
         */
        Subtract,
        /**
         * R = R_d × D_r - R_s0 × S_r
         * G = G_d × D_g - G_s0 × S_g
         * B = B_d × D_b - B_s0 × S_b
         * A = A_d × D_a - A_s0 × S_a
         */
        ReverseSubtract,
        /**
         * R = min(R_s0,R_d)
         * G = min(G_s0,G_d)
         * B = min(B_s0,B_d)
         * A = min(A_s0,A_d)
         */
        Min,
        /**
         * R = max(R_s0,R_d)
         * G = max(G_s0,G_d)
         * B = max(B_s0,B_d)
         * A = max(A_s0,A_d)
         */
        Max,
    };

    enum class ColorWriteMask : uint8_t
    {
        /// None channel will be affected.
        None = 0x0,
        /// Red(0) channel will be affected.
        Red = 0x1,
        /// Green(1) channel will be affected.
        Green = 0x2,
        /// Blue(2) channel will be affected.
        Blue = 0x4,
        /// Alpha(3) channel will be affected.
        Alpha = 0x8,
        /// Red(0) & Green(1) & Blue(2) channels will be affected.
        RGB = static_cast<uint8_t>(Red | Green | Blue),
        /// Red(0) & Green(1) & Blue(2) & Alpha(3) channels will be affected.
        All = static_cast<uint8_t>(RGB | Alpha),
    };
    ZRHI_DEFINE_BITMASK_ENUM(ColorWriteMask);

    // Caution: this data type is used in bridge structs. Consider change c# code or separate Bridge from RHI
    /// DESC for texture payload. Used for texture upload.
    struct TextureData
    {
        /// Pointer to binary data of texture content.
        const void* data;
        /// Binary data size.
        int dataSize;
        /// The size in bytes of one row of the source texture data.
        int rowPitch;
        /// Texture data dimension X
        int dimensionX;
        /// Texture data dimension Y
        int dimensionY;
        /// Texture data dimension Z
        int dimensionZ;
    };

    /// DESC for depth stencil state of GraphicsPSO.
    struct DepthStencilDesc
    {
        /// If true - depth test is enabled.
        bool depthTest;
        /// If true - depth write is enabled.
        bool depthWrite;
        /// Depth comparison function.
        DepthComparisonFunc depthComparison;
        /// Depth attachment format.
        TextureFormat format;
    };

    /// DESC for color blend state of GraphicsPSO.
    struct ColorBlendDesc
    {
        /// If true - color blending is enabled.
        bool blendEnabled = false;
        /// Blend factor that is used to determine the source factors (S_r,S_g,S_b).
        BlendFactor srcColorBlendFactor = BlendFactor::One;
        /// Blend factor that is used to determine the destination factors (D_r,D_g,D_b).
        BlendFactor dstColorBlendFactor = BlendFactor::Zero;
        /// Blend operation that is used to calculate the RGB values to write to the color attachment.
        BlendOperation colorBlendOperation = BlendOperation::Add;
        /// Blend factor that is used to determine the source factor S_a.
        BlendFactor srcAlphaBlendFactor = BlendFactor::One;
        /// Blend factor that is used to determine the destination factor D_a.
        BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
        /// Blend operation that is used to calculate the alpha values to write to the color attachment.
        BlendOperation alphaBlendOperation = BlendOperation::Add;
        /// The mask that specifying which of the R, G, B, and/or A components are enabled for writing.
        ColorWriteMask writeMask = ColorWriteMask::All;
    };

    /// DESC for single render target of GraphicsPSO.
    struct RenderTargetDesc
    {
        /// Render target texture format.
        TextureFormat format = TextureFormat::None;
        /// Color blend state DESC
        ColorBlendDesc colorBlend{};
    };

    /// DESC for single IA element of GraphicsPSO.
    struct InputElementDesc
    {
        /// The name of the Direct3D semantic name.
        const char* d3dSemanticName;
        /// Attribute location for non Direct3D APIs.
        uint32_t attribLocation;
        /// Input evement format.
        InputElementFormat format;
        /// Count of elements.
        uint32_t elementCount;
        /// Offset in bytes in input elements structure.
        uint32_t offset;
    };

    /// DESC for input assembly state of GraphicsPSO.
    struct InputLayoutDesc
    {
        /// Input elements DESCs elements count.
        uint32_t inputElementsCount;
        /// Input elements DESCs
        const InputElementDesc* inputElements;
        /// Input elements structure stride.
        uint32_t stride;
    };

    /// DESC for rasterization state of GraphicsPSO.
    struct RasterizerDesc
    {
        /// Culling mode.
        TriangleCullMode cullMode;
        /// If true - front face is meant to be counter clockwise, otherwise - clockwise.
        bool frontCounterClockwise;
        /// If true - wireframe rendering mode is enabled.
        bool wireframe;
    };

    /// DESC for pipeline heap space slot resource descriptors layout.
    struct DescriptorBindingDesc
    {
        /// Binding slot.
        uint32_t slot;
        /// Resource type.
        DescriptorType type;
    };

    /// DESC for pipeline heap space resource descriptors layout.
    struct DescriptorSetLayoutDesc
    {
        /// An array of binding DESCs.
        size_t bindingDescsCount = 0;
        const DescriptorBindingDesc* bindingDescs = nullptr;
    };

    /// DESC for pipeline heap resource descriptors layout.
    struct PipelineLayoutDesc
    {
        /// An array of Descriptor Set Layout DESCs elements count.
        size_t setsDescsCount;
        /// An array of Descriptor Set Layout DESCs
        DescriptorSetLayoutDesc const* setDescs;
    };

    /// DESC for compute pipeline state object (PSO)
    struct ComputePSODesc
    {
        /// Pointer to Compute Shader source.
        const void* CS = nullptr;
        /// Compute Shader source size in bytes.
        size_t CSSize = 0;
        /// Shader module entry point function name.
        const char* CSEntryPoint = nullptr;
        /// Work group size information.
        /// @note Used in Metal
        uint32_t workgroupSize[3]{};
        /// An array of Descriptor Set Layout DESCs elements count.
        size_t descriptorSetLayoutsCount = 0;
        /// An array of Descriptor Set Layout DESCs.
        DescriptorSetLayoutDesc const* descriptorSetLayouts = nullptr;
        /// Debug name. In debug configurations will be applied to PSO as debug marker.
        const char* debugName = nullptr;
    };


    /// DESC for graphics pipeline state object (PSO)
    struct GraphicsPSODesc
    {
        /// Pointer to Vertex Shader source.
        const void* VS = nullptr;
        /// Vertex Shader source size in bytes.
        size_t VSSize = 0;
        /// Shader module entry point function name.
        const char* VSEntryPoint = nullptr;
        /// Pointer to Pixel Shader / Fragment Shader source.
        const void* PS = nullptr;
        /// Pixel Shader / Fragment Shader source size in bytes.
        size_t PSSize = 0;
        /// Shader module entry point function name.
        const char* PSEntryPoint = nullptr;
        /// Depth Stencil state DESC structure.
        DepthStencilDesc depthStencil{};
        /// An array of Render Targets DESCs elements count.
        size_t RTDescsCount = 0;
        /// An array of Render Targets DESCs.
        const RenderTargetDesc* RTDescs = nullptr;
        /// Input Layout DESC structure.
        InputLayoutDesc inputLayout{};
        /// Rasterizer state DESC structure.
        RasterizerDesc rasterizer{};
        /// Primitive topology.
        PrimitiveTopology topology{};
        /// An array of Descriptor Set Layout DESCs elements count.
        size_t descriptorSetLayoutsCount = 0;
        /// An array of Descriptor Set Layout DESCs.
        const DescriptorSetLayoutDesc* descriptorSetLayouts = nullptr;
        /// Pipeline state object debug name. In debug configurations will be applied to PSO as debug marker.
        const char* PSODebugName = nullptr;
        /// Vertex Shader debug name. In debug configurations will be applied to the Shader Module as debug marker.
        const char* VSDebugName = nullptr;
        /// Pixel Shader debug name. In debug configurations will be applied to the Shade Module as debug marker.
        const char* PSDebugName = nullptr;
    };

    /// DESC for descriptor location in Descriptor Heap.
    struct DescriptorLocation
    {
        /// Descriptor set in DescriptorHeap.
        uint32_t descriptorSet;
        /// Descriptor binding slot in corresponding set in DescriptorHeap.
        uint32_t bindingSlot;
        /// Target descriptor heap.
        DescriptorHeap* heap;
    };

    /// Arguments for draw instanced indirect command. Meant to be used in indirect draw buffer.
    struct DrawInstancedIndirectParams
    {
        /// Total count of vertices to read from vertex buffer.
        uint32_t vertexCount;
        /// Total count of draw instances.
        /// Each instance will read data using vertex buffer with per instance data input rate.
        uint32_t instanceCount;
        /// Offset in vertex buffer.
        int startVertex;
        /// Offset in vertex buffer if data input rate is per instance.
        uint32_t startInstance;
    };

    /// Arguments for draw indexed instanced indirect command. Meant to be used in indirect draw buffer.
    struct DrawIndexedInstancedIndirectParams
    {
        /// Total count of indices to read from index buffer.
        uint32_t indexCount;
        /// Total count of draw instances.
        /// Each instance will read data using vertex buffer with per instance data input rate.
        uint32_t instanceCount;
        /// Offset in index buffer.
        uint32_t startIndex;
        /// Offset in vertex buffer.
        int startVertex;
        /// Offset in vertex buffer if data input rate is per instance.
        uint32_t startInstance;
    };

    /// Arguments for dispatch indirect command. Meant to be used in indirect dispatch buffer.
    struct DispatchIndirectParams
    {
        /**
         * Work group count x dimension.
         * @warning Not equal to total threads on X dimension in GPU work.
         * Total threads count on X dimension is calculated by: GroupCountX * WorkGroupSizeX.
         * WorkGroupSize is declared in shader code.
         */
        uint32_t groupCountX;
        /**
         * Work group count x dimension.
         * @warning Not equal to total threads on Y dimension in GPU work.
         * Total threads count on Y dimension is calculated by: GroupCountY * WorkGroupSizeY.
         * WorkGroupSize is declared in shader code.
         */
        uint32_t groupCountY;
        /**
         * Work group count x dimension.
         * @warning Not equal to total threads on Z dimension in GPU work.
         * Total threads count on Z dimension is calculated by: GroupCountZ * WorkGroupSizeZ.
         * WorkGroupSize is declared in shader code.
         */
        uint32_t groupCountZ;
    };

    struct QueryDesc
    {
        QueryType type;
    };

    struct TimestampCalibrationResult
    {
        // CPU timestamp value queried at the same time as GPU timestamp.
        uint64_t CPUTimestamp;
        // GPU timestamp value queried at the same time as CPU timestamp.
        uint64_t GPUTimestamp;
        // Frequency of CPU timestamp counter in Hz.
        uint64_t CPUTimestampFrequency;
    };

    /// Viewport DESC
    struct Viewport
    {
        /// Offset x in pixels.
        float x;
        /// Offset y in pixels.
        float y;
        /// Width in pixels.
        float width;
        /// Height in pixels.
        float height;
    };

    /// Color attachment clear DESC
    struct ColorClearDesc
    {
        /// If true - corresponding Color attachment will be cleared.
        bool clearColor = false;
        /// Clear value for Red(0) channel.
        float r = 0.0f;
        /// Clear value for Green(1) channel.
        float g = 0.0f;
        /// Clear value for Blue(2) channel.
        float b = 0.0f;
        /// Clear value for Alpha(3) channel.
        float a = 0.0f;
    };

    /// DepthStencil attachment clear DESC
    struct DepthStencilClearDesc
    {
        /// If true - corresponding DepthStencil attachment depth will be cleared.
        bool clearDepth = false;
        /// Clear value for depth aspect.
        float depthValue = 0.0f;
        /// If true - corresponding DepthStencil attachment stencil will be cleared.
        bool clearStencil = false;
        /// Clear value for stencil aspect.
        uint8_t stencilValue = 0;
    };

    /// Base DESC for pipeline resources binging.
    struct BindingsDescBase
    {
        /// Descriptor heap.
        DescriptorHeap* heap = nullptr;
        /// Constant Buffer Views (CBVs) array elements count.
        size_t CBVsCount = 0;
        /// Constant Buffer Views (CBVs) array.
        Buffer* const* CBVs = nullptr;
        /// Shader Resource Views (SRVs) array elements count.
        size_t SRVsCount = 0;
        /// Shader Resource Views (SRVs) array.
        Resource* const* SRVs = nullptr;
        /// Unordered Access Views (UAVs) array elements count.
        size_t UAVsCount = 0;
        /// Unordered Access Views (UAVs) array.
        Resource* const* UAVs = nullptr;
        /// Sampler states array elements count.
        size_t samplersCount = 0;
        /// Sampler states array.
        Sampler* const* samplers = nullptr;
    };

    /// DESC for DispatchIndirect command.
    struct DispatchIndirectDesc : public BindingsDescBase
    {
        /// Compute Pipeline State Object (ComputePSO)
        ComputePSO* PSO = nullptr;
        /// Indirect arguments buffer.
        Buffer* indirectArgumentsBuffer = nullptr;
        /// Offset in bytes in bound indirect arguments buffer.
        uint32_t indirectArgumentsOffset = 0;
    };

    /// DESC for Dispatch command.
    struct DispatchDesc : public BindingsDescBase
    {
        /// Compute Pipeline State Object (ComputePSO)
        ComputePSO* PSO = nullptr;
        DispatchIndirectParams dimensions = { 1, 0, 0 };
    };

    /// Base DESC for Draw command.
    struct DrawDescBase : public BindingsDescBase
    {
        /// Graphics Pipeline State Object (GraphicsPSO)
        GraphicsPSO* PSO = nullptr;

        /// Viewport DESC
        Viewport viewport{};

        /// Vertex buffers array elements count.
        size_t vertexBuffersCount = 0;
        /// Vertex buffers array.
        Buffer* const* vertexBuffers = nullptr;
        /// Vertex buffers strides in bytes array.
        const uint32_t* vertexBufferStrides = nullptr;
        /// Vertex buffers sizes in bytes array.
        const uint32_t* vertexBufferSizes = nullptr;

        /// Framebuffer
        Framebuffer* framebuffer = nullptr;
        /// Clear values for each render target array elements count.
        /// @note Must match PSO and framebuffer render targets count.
        size_t rtClearValuesCount = 0;
        /// Clear state for each render target array.
        const ColorClearDesc* rtClearValues = nullptr;
        /// DepthStencil clear state.
        DepthStencilClearDesc dsClearValues{};
    };

    /// DESC for Draw command.
    struct DrawInstancedDesc : public DrawDescBase
    {
        /// The number of vertices to draw.
        size_t vertexCount = 0;
        /// The number of instances to draw.
        size_t instanceCount = 0;
        /// The index of the first vertex to draw.
        size_t firstVertex = 0;
    };

    /// DESC for DrawIndexedInstanced command.
    struct DrawIndexedInstancedDesc : public DrawDescBase
    {
        /// Index buffer.
        Buffer* indexBuffer = nullptr;
        /// Index buffer stride in bytes.
        uint32_t indexBufferStride = 0;
        /// Index buffer size in bytes.
        uint32_t indexBufferSize = 0;

        /// The number of vertices to draw.
        size_t indexCount = 0;
        /// The number of instances to draw.
        size_t instanceCount = 0;
        /// The base index within the index buffer.
        size_t firstIndex = 0;
        /// The value added to the vertex index before indexing into the vertex buffer.
        size_t vertexOffset = 0;
    };

    /// DESC for DrawInstancedIndirect command.
    struct DrawInstancedIndirectDesc : public DrawDescBase
    {
        /// Indirect arguments buffer.
        Buffer* indirectArgumentsBuffer = nullptr;
        /// Offset in bytes in bound indirect arguments buffer.
        uint32_t indirectArgumentsOffset = 0;
    };

    /// DESC for DrawIndexedInstancedIndirect command.
    struct DrawIndexedInstancedIndirectDesc : public DrawDescBase
    {
        /// Indirect arguments buffer.
        Buffer* indirectArgumentsBuffer = nullptr;
        /// Offset in bytes in bound indirect arguments buffer.
        uint32_t indirectArgumentsOffset = 0;

        /// Index buffer.
        Buffer* indexBuffer = nullptr;
        /// Index buffer stride in bytes.
        uint32_t indexBufferStride = 0;
        /// Index buffer size in bytes.
        uint32_t indexBufferSize = 0;
    };

    enum class ViewDataType
    {
        None,
        FormattedBuffer,
        StructuredBuffer,
        Count
    };

    enum class BufferFormat
    {
        None,
        R32_UINT,
        R16_SFloat,
        Count,
    };

    struct StructuredBufferViewDesc
    {
        uint32_t structuredStride;
    };

    struct FormattedBufferViewDesc
    {
        BufferFormat format;
    };

    /// DESC for BufferView creation commands.
    struct InitializeViewDesc {
        ViewDataType type;
        union
        {
            StructuredBufferViewDesc structuredBufferViewDesc;
            FormattedBufferViewDesc rawBufferViewDescr;
        };

        InitializeViewDesc() noexcept
            : type(ViewDataType::None)
        {
        }

        explicit InitializeViewDesc(const StructuredBufferViewDesc& desc) noexcept
            : type(ViewDataType::StructuredBuffer)
            , structuredBufferViewDesc(desc)
        {
        }

        explicit InitializeViewDesc(const FormattedBufferViewDesc& desc) noexcept
            : type(ViewDataType::FormattedBuffer)
            , rawBufferViewDescr(desc)
        {
        }
    };

    /**
     * Zibra Effects Render Hardware Interface (RHI) Runtime
     * @interface
     */
    class RHIRuntime
    {
    protected:
        virtual ~RHIRuntime() noexcept = default;

    public:
        /**
         * Initializes RHI instance resources & Graphics API dependent resources.
         */
        virtual ReturnCode Initialize() noexcept = 0;
        /**
         * Releases RHI instance resources & Graphics API dependent resources.
         */
        virtual void Release() noexcept = 0;
        /**
         * Releases resources from garbage queue that safe frame attribute is less than current safe frame.
         * @note Current safe frame is got from EngineInterface that was passed with RHIInitInfo on Initialize().
         * @return ZRHI_SUCCESS or ZRHI_QUEUE_EMPTY in case of success or other code in case of error.
         */
        virtual ReturnCode GarbageCollect() noexcept = 0;

        /**
         * @return Returns API type of RHI instance.
         */
        [[nodiscard]] virtual GFXAPI GetGFXAPI() const noexcept = 0;

        /**
         * Queries feature support
         * @param feature - Feature to query
         * @return true if feature is supported, false otherwise
         */
        [[nodiscard]] virtual bool QueryFeatureSupport(Feature feature) const noexcept = 0;

        /**
         * Enables/Disables "Stable Power State". Makes timestamp queries reliable at cost of performance.
         * @note NOOP for Metal, Direct3D 11, Vulkan
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode SetStablePowerState(bool enable) noexcept = 0;

        /**
         * Compiles Compute Pipeline State Object (Compute PSO) from binary/text source and returns it's instance.
         * @param [in] desc - Compute PSO DESC structure.
         * @param [out] outPSO - out PSO instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CompileComputePSO(const ComputePSODesc& desc, ComputePSO** outPSO) noexcept = 0;
        /**
         * Compiles Graphics Pipeline State Object (Graphics PSO) from binary/text source and returns it's instance.
         * @param [in] desc - Graphics PSO DESC structure.
         * @param [out] outPSO - out PSO instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CompileGraphicPSO(const GraphicsPSODesc& desc, GraphicsPSO** outPSO) noexcept = 0;

        /**
         * Releases Compute Pipeline State Object (Compute PSO) resources.
         * @param [in] pso - Target Compute PSO.
         * @warning After releasing, handle passed to pso param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseComputePSO(ComputePSO* pso) noexcept = 0;
        /**
         * Releases Graphics Pipeline State Object (Graphics PSO) resources.
         * @param [in] pso - Target Graphics PSO.
         * @warning After releasing, handle passed to pso param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseGraphicPSO(GraphicsPSO* pso) noexcept = 0;

        /**
         * Registers native buffer from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be
         * decorated with this name.
         * @param [out] outBuffer - out Buffer instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode RegisterBuffer(void* resourceHandle, const char* name, Buffer** outBuffer) noexcept = 0;
        /**
         * Registers native 2D texture from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outTexture - out Texture2D instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode RegisterTexture2D(void* resourceHandle, const char* name, Texture2D** outTexture) noexcept = 0;
        /**
         * Registers native 3D texture from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outTexture - out Texture3D instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode RegisterTexture3D(void* resourceHandle, const char* name, Texture3D** outTexture) noexcept = 0;

        /**
         * Allocates graphics buffer in GPU or CPU memory (depends on usage flags).
         * @param [in] size - Buffer size in bytes.
         * @param [in] heapType - Descriptor Heap type. Depends on how resource is going to be used.
         * @param [in] usage - Resource usage flags.
         * @param [in] structuredStride - Buffer structured stride. Must be 0 for unstructured buffers.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outBuffer - out Buffer instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateBuffer(size_t size, ResourceHeapType heapType, ResourceUsage usage, uint32_t structuredStride, const char* name,
                                        Buffer** outBuffer) noexcept = 0;
        /**
         * Allocates 2D texture in GPU memory.
         * @param [in] width - Texture width (x).
         * @param [in] height - Texture height (y).
         * @param [in] mips - Quantity of texture mipmap levels.
         * @param [in] format - Texture pixel format.
         * @param [in] usage - Resource usage flags.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outTexture - out Texture2D instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateTexture2D(size_t width, size_t height, size_t mips, TextureFormat format, ResourceUsage usage, const char* name,
                                           Texture2D** outTexture) noexcept = 0;
        /**
         * Allocates 3D texture in GPU memory.
         * @param [in] width - Texture width (x).
         * @param [in] height - Texture height (y).
         * @param [in] depth - Texture depth (z).
         * @param [in] mips - Quantity of texture mipmap levels.
         * @param [in] format - Texture pixel format.
         * @param [in] usage - Resource usage flags.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outTexture - out Texture3D instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateTexture3D(size_t width, size_t height, size_t depth, size_t mips, TextureFormat format, ResourceUsage usage,
                                           const char* name, Texture3D** outTexture) noexcept = 0;
        /**
         * Allocates Sampler object.
         * @note NOOP for OpenGL/OpenGL ES
         * @param [in] addressMode - Address mode for texture sampling.
         * @param [in] filter - Sampling filter.
         * @param [in] descriptorHeap - Target Descriptor Heap for bindless APIs.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outSampler - out Sampler instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateSampler(SamplerAddressMode addressMode, SamplerFilter filter, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations, const char* name, Sampler** outSampler) noexcept = 0;

        /**
         * Releases resources of RHI Buffer.
         * @param [in] buffer - Target Buffer instance.
         * @warning After releasing, handle passed to buffer param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseBuffer(Buffer* buffer) noexcept = 0;
        /**
         * Releases resources of RHI 2D Texture.
         * @param [in] texture - Target Texture2D instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseTexture2D(Texture2D* texture) noexcept = 0;
        /**
         * Releases resources of RHI 3D Texture.
         * @param [in] texture - Target Texture23 instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseTexture3D(Texture3D* texture) noexcept = 0;
        /**
         * Releases resources of RHI Sampler.
         * @note NOOP for OpenGL/OpenGL ES
         * @param [in] sampler - Target Sampler instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseSampler(Sampler* sampler) noexcept = 0;

        /**
         * Creates RHI Descriptor Heap instance. Used for APIs with bindless system.
         * @note NOOP for OpenGL/OpenGL ES, Metal, Direct3D 11
         * @param [in] heapType - Descriptor Heap type.
         * @param [in] pipelineLayoutDesc - DESC structure for pipeline bindings layout.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outHeap - out DescriptorHeap instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateDescriptorHeap(DescriptorHeapType heapType, const PipelineLayoutDesc& pipelineLayoutDesc, const char* name,
                                                DescriptorHeap** outHeap) noexcept = 0;

        /**
         * Clones descriptor heap with descriptors.
         * @param [in] src - Source descriptor heap.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outHeap - out DescriptorHeap instance with copied descriptors.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CloneDescriptorHeap(const DescriptorHeap* src, const char* name, DescriptorHeap** outHeap) noexcept = 0;

        /**
         * Releases resources of DescriptorHeap
         * @param [in] heap - Target DescriptorHeap instance.
         * @warning After releasing, handle passed to heap param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseDescriptorHeap(DescriptorHeap* heap) noexcept = 0;

        /**
         * Creates RHI Query Heap instance. Used for performing GPU queries.
         * @note NOOP for OpenGL/OpenGL ES, Metal, Direct3D 11
         * @param [in] heapType - Query Heap type.
         * @param [in] queryCount - Quantity of queries in heap.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outHeap - out QueryHeap instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateQueryHeap(QueryHeapType heapType, uint64_t queryCount, const char* name, QueryHeap** outHeap) noexcept = 0;

        /**
         * Releases resources of QueryHeap
         * @param [in] heap - Target QueryHeap instance.
         * @warning After releasing, handle passed to heap param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseQueryHeap(QueryHeap* heap) noexcept = 0;

        /**
         * Creates RHI Framebuffer instance.
         * @param [in] renderTargets - Render target views.
         * @param [in] depthStencil - Depth stencil views.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @param [out] outFramebuffer - out Framebuffer instance.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CreateFramebuffer(size_t renderTargetsCount, Texture2D* const* renderTargets, Texture2D* depthStencil, const char* name,
                                             Framebuffer** outFramebuffer) noexcept = 0;
        /**
         * Releases resources of Framebuffer
         * @param [in] framebuffer - Target Framebuffer instance
         * @warning After releasing, handle passed to framebuffer param becomes invalid.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ReleaseFramebuffer(Framebuffer* framebuffer) noexcept = 0;

        /**
         * Creates & initializes Shader Resource View (SRV) for selected buffer resource. Buffer can be either Structured or Raw.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] desc - Specifies buffer type and stride/format.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have ShaderResource usage flag enabled.
         * @attention Buffer can't have structured and formatted SRV simultaneously.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeSRV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Shader Resource View (SRV) for selected 2D texture resource.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture2D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeSRV(Texture2D* texture, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;
        /**
         * Creates & initializes Shader Resource View (SRV) for selected 3D texture resource.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture3D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeSRV(Texture3D* texture, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Unordered Access View (UAV) for selected buffer resource. Buffer can be either Structured or Raw.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] desc - Specifies buffer type and stride/format.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have UnorderedAccess usage flag enabled.
         * @attention Buffer can't have structured and formatted UAV simultaneously.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeUAV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Unordered Access View (UAV) for selected 2D texture resource.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture2D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeUAV(Texture2D* texture, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;
        /**
         * Creates & initializes Unordered Access View (UAV) for selected 3D texture resource.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture3D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeUAV(Texture3D* texture, size_t descriptorLocationsCount,
                                         const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Constance Buffer View (CBV) for selected buffer resource.
         * @note For bindless APIs creates CBV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have ConstantBuffer usage flag enabled.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode InitializeCBV(Buffer* buffer, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Starts recording state. Used on Metal API for Command Encoder manipulations.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode StartRecording() noexcept = 0;
        /**
         * Stops recording state. Used on Metal API for Command Encoder manipulations.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode StopRecording() noexcept = 0;

        /**
         * Uploads binary data to buffer using Upload Ring on platforms where it is used.
         * @note If destination buffer is CBV - replaces whole buffer on DX11, independently of size
         * @attention Uploaded data will be available in frame CurrentFrame + MaxFramesInFlight
         * @attention You should always pass correct size of the buffer, so Metal can work correctly
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] data - binary upload data.
         * @param [in] size - upload data size in bytes.
         * @param [in] offset - offset in destination buffer memory in bytes.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode UploadBuffer(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept = 0;
        /**
         * Uploads data bypassing Upload Ring on platforms where it is used.
         * @remark Needed to upload a large data on initialization
         * @attention Creates new buffer and replaces old one in future frames, so shouldn't be
         * abused for best performance.
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] data - binary upload data.
         * @param [in] size - upload data size in bytes.
         * @param [in] offset - offset in destination buffer memory in bytes.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode UploadBufferSlow(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept = 0;
        /**
         * Uploads 3D texture data from CPU memory.
         * @remark Needed to upload a 3D texture initial data on initialization.
         * @attention Creates new staging buffer for transferring data. Not very performant.
         * @param [in] texture - Target RHI Texture instance.
         * @param [in] uploadData - upload data DESC.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode UploadTextureSlow(Texture3D* texture, const TextureData& uploadData) noexcept = 0;

        /**
         * Maps buffer and copies data to address in destination param.
         * @param [in] buffer - Target RHI Buffer to read from.
         * @param [out] destination - Address of memory to copy to.
         * @param [in] size - Size in bytes or requested data.
         * @param [in] srcOffset - Offset in input Buffer to read from.
         * @attention destination must point on allocated memory sith size in bytes greater or equal than value in size param.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode GetBufferData(Buffer* buffer, void* destination, size_t size, size_t srcOffset) noexcept = 0;
        /**
         * Waits for GPU work finish and gets data from Buffer. Buffer must have ResourceUsage::CopySource.
         * @attention Stalls CPU till GPU work related to target buffer finished.
         * @param buffer Target RHI Buffer to read from.
         * @param destination Address of memory to copy to.
         * @param size Size in bytes or requested data.
         * @param srcOffset Offset in input Buffer to read from.
         * @attention destination must point on allocated memory sith size in bytes greater or equal than value in size param.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode GetBufferDataImmediately(Buffer* buffer, void* destination, size_t size, size_t srcOffset) noexcept = 0;
        /**
         * Enqueues Dispatch command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode Dispatch(const DispatchDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Indexed Instanced command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode DrawIndexedInstanced(const DrawIndexedInstancedDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Instanced command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode DrawInstanced(const DrawInstancedDesc& desc) noexcept = 0;

        /**
         * Enqueues Dispatch Indirect command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode DispatchIndirect(const DispatchIndirectDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Instanced Indirect command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode DrawInstancedIndirect(const DrawInstancedIndirectDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Indexed Instanced Indirect command in GPU.
         * @param desc - command Desc structure.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode DrawIndexedInstancedIndirect(const DrawIndexedInstancedIndirectDesc& desc) noexcept = 0;

        /**
         * Enqueues Copy command in GPU.
         * @param [in] dstBuffer - Copy destination RHI Buffer instance.
         * @param [in] dstOffset - Copy destination buffer offset in bytes.
         * @param [in] srcBuffer - Copy source RHI Buffer instance.
         * @param [in] srcOffset - Copy source buffer offset in bytes.
         * @param [in] size - Copy size in bytes.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode CopyBufferRegion(Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept = 0;
        /**
         * Clears formatted RHI Buffer data with value in clearValue param.
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] bufferSize - Buffer size in bytes that will be cleared.
         * @param [in] clearValue - Value that will be written in each byte of cleared region.
         * @param [in] descriptorLocation - Target DescriptorHeap and location for bindless APIs.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode ClearFormattedBuffer(Buffer* buffer, uint32_t bufferSize, uint8_t clearValue,
                                                const DescriptorLocation& descriptorLocation) noexcept = 0;

        /**
         * Submits GPU query.
         * @param [in] queryHeap - Target QueryHeap instance.
         * @param [in] queryIndex - Index of query in heap.
         * @param [in] queryDesc - Query description.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode SubmitQuery(QueryHeap* queryHeap, uint64_t queryIndex, const QueryDesc& queryDesc) noexcept = 0;

        /**
         * Retrieves GPU query results.
         * @attention Stalls CPU till all GPU work is finished.
         * @param [in] queryHeap - Target QueryHeap instance.
         * @param [in] queryType - Type of resolved queries.
         * @param [in] offset - Offset in query heap to start resolving from.
         * @param [in] count - Number of queries to resolve. Resolves queries with index from offset to offset+count-1.
         * @param [out] result - Query result. Must be allocated by caller. Must to be large enough to hold .
         * @param [in] size - Size of result buffer in bytes. If size is not large enough the call will fail.
         * @return true if query is successful and false otherwise.
         */
        virtual ReturnCode ResolveQuery(QueryHeap* queryHeap, QueryType queryType, uint64_t offset, uint64_t count, void* result,
                                        size_t size) noexcept = 0;

        /**
         * Gets GPU timestamp frequency.
         * @note Only available when "Timestamp" feature is supported.
         * @note May not be accurate on some APIs.
         * @note May not be accurate if "Stable Power State" is disabled.
         * @return Timestamp frequency in Hz.
         */
        [[nodiscard]] virtual uint64_t GetTimestampFrequency() const noexcept = 0;

        /**
         * Gets Timestamp calibration.
         * @note Only available when "TimestampCalibration" feature is supported.
         * @return TimestampCalibrationResult.
         */
        [[nodiscard]] virtual TimestampCalibrationResult GetTimestampCalibration() const noexcept = 0;

        /**
         * Gets CPU timestamp.
         * @note Only available when "TimestampCalibration" feature is supported.
         * @returns CPU timestamp.
         */
        [[nodiscard]] virtual uint64_t GetCPUTimestamp() const noexcept = 0;

        /**
         * Pushed debug region marker if Graphics API has these functional.
         * @note NOOP in non debug/profile configurations.
         * @param [in] regionName - Debug region name
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode StartDebugRegion(const char* regionName) noexcept = 0;
        /**
         * Pops debug region marker if Graphics API has these functional.
         * @note NOOP in non debug/profile configurations.
         * @return ZRHI_SUCCESS in case of success or other code in case of error.
         */
        virtual ReturnCode EndDebugRegion() noexcept = 0;
        /**
         * Tries to start frame capture with graphics debugger like RenderDoc or PIX.
         * Support for different debuggers is platform and API dependent.
         * Resulting capture location is platform and API dependent.
         * @note NOOP if app was not launched via graphic debugger.
         * @param [in] captureName - Name of capture. May have no effect depending on graphics debugger used. (Optional)
         */
        virtual void StartFrameCapture(const char* captureName) noexcept = 0;
        /**
         * Stops frame capture if one was started.
         * @note NOOP if app was not launched via graphic debugger or if capture wasn't started.
         */
        virtual void StopFrameCapture() noexcept = 0;
    };

    /**
     * DebugRegion - helper class that controls debug region in scope.
     * Automatically ends debug region in the end of the scope.
     * @example
     * ```
     * {
     *     DebugRegion parentRegion(g_RHI, "Parent Region")
     *     ...
     *     {
     *          DebugRegion nestedRegion(g_RHI, "Nested Region");
     *          ...
     *     }
     * }
     * ```
     */
    class DebugRegion
    {
    public:
        explicit DebugRegion(RHIRuntime* base, const char* regionName) noexcept
            : m_Base(base)
        {
            m_Base->StartDebugRegion(regionName);
        }
        ~DebugRegion() noexcept
        {
            m_Base->EndDebugRegion();
        }

    private:
        RHIRuntime* m_Base;
    };

    class RHIFactory
    {
    public:
        virtual ~RHIFactory() noexcept = default;

    public:
        virtual ReturnCode SetGFXAPI(GFXAPI type) noexcept = 0;
        virtual ReturnCode UseGFXCore(Integration::GFXCore* gfxAdapter) noexcept = 0;
        virtual ReturnCode UseForcedAdapter(int32_t adapterIndex) noexcept = 0;
        virtual ReturnCode UseAutoSelectedAdapter() noexcept = 0;
        virtual ReturnCode ForceSoftwareDevice() noexcept = 0;
        virtual ReturnCode ForceEnableDebugLayer() noexcept = 0;
        virtual ReturnCode Create(RHIRuntime** outInstance) noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    typedef ReturnCode (ZRHI_CALL_CONV *PFN_CreateRHIFactory)(RHIFactory** outFactory);
#ifdef ZRHI_STATIC_LINKING
    ReturnCode CreateRHIFactory(RHIFactory** outFactory) noexcept;
#elif ZRHI_DYNAMIC_IMPLICIT_LINKING
    ZRHI_API_IMPORT ReturnCode ZRHI_CALL_CONV CreateRHIFactory(RHIFactory** outFactory) noexcept;
#else
    constexpr const char* CreateRHIFactoryExportName = "Zibra_RHI_CreateRHIFactory";
#endif

    typedef Version (ZRHI_CALL_CONV *PFN_GetVersion)();
#ifdef ZRHI_STATIC_LINKING
    Version GetVersion() noexcept;
#elif ZRHI_DYNAMIC_IMPLICIT_LINKING
    ZRHI_API_IMPORT Version ZRHI_CALL_CONV GetVersion() noexcept;
#else
    constexpr const char* GetVersionExportName = "Zibra_RHI_GetVersion";
#endif
}

#undef ZRHI_API_IMPORT
#undef ZRHI_CALL_CONV
