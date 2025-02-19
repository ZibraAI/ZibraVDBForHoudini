#pragma once

#include <cstdint>

#if ZRHI_SUPPORT_D3D11
#include <d3d11.h>
#endif // ZRHI_SUPPORT_D3D11

#if ZRHI_SUPPORT_D3D12
#include <d3d12.h>
#endif // ZRHI_SUPPORT_D3D12

#if ZRHI_SUPPORT_VULKAN
#include <vulkan/vulkan.h>
#endif

#if ZRHI_SUPPORT_METAL
#import <Metal/Metal.h>
#endif // ZRHI_SUPPORT_METAL

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
    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };
    inline bool operator==(const Version& l, const Version& r) noexcept
    {
        return l.major == r.major && l.minor == r.minor && l.patch == r.patch && l.build == r.build;
    }
    constexpr Version ZRHI_VERSION = {1, 0, 0, 0};

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
            virtual void InitializeFrameLatencySync(size_t maxFramesInFlight) = 0;
            virtual void SyncFrameLatency() = 0;
        };

#pragma region D3D11 Engine Interface
#if ZRHI_SUPPORT_D3D11
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

            virtual void AccessBuffer(void* resourceHandle, D3D11BufferDesc& bufferDesc) noexcept = 0;
            virtual void AccessTexture2D(void* resourceHandle, D3D11Texture2DDesc& texture2dDesc) noexcept = 0;
            virtual void AccessTexture3D(void* resourceHandle, D3D11Texture3DDesc& texture3dDesc) noexcept = 0;
        };
#endif // ZRHI_SUPPORT_D3D11
#pragma endregion D3D11 Engine Interface

#pragma region D3D12 Engine Interface
#if ZRHI_SUPPORT_D3D12
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

            virtual void AccessBuffer(void* resourceHandle, D3D12BufferDesc& bufferDesc) noexcept = 0;
            virtual void AccessTexture2D(void* resourceHandle, D3D12Texture2DDesc& texture2dDesc) noexcept = 0;
            virtual void AccessTexture3D(void* resourceHandle, D3D12Texture3DDesc& texture3dDesc) noexcept = 0;

            virtual void StartRecording() noexcept = 0;
            /**
             * Submits job to queue and stops recording state.
             * @param statesCount - Resources states count in states param.
             * @param states - Resources state array for command list tail.
             * @return GPU work completion CPU event handle. (Consumer will close event handle by itself)
             */
            virtual HANDLE StopRecording(size_t statesCount, const D3D12TrackedResourceState* states) noexcept = 0;
        };
#endif // ZRHI_SUPPORT_D3D12
#pragma endregion D3D12 Engine Interface

#pragma region Vulkan Engine Interface
#if ZRHI_SUPPORT_VULKAN
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
            virtual void AccessBuffer(void* resourceHandle, VulkanBufferDesc& bufferDesc) noexcept = 0;
            virtual void AccessImage(void* resourceHandle, VkImageLayout layout, VulkanTextureDesc& textureDesc) noexcept = 0;
            virtual void StartRecording() noexcept = 0;
            /**
             * Submits job to queue and stops recording state.
             * @return GPU work completion fence. (Consumer will release fence by itself)
             */
            virtual VkFence StopRecording() noexcept = 0;
        };
#endif ZRHI_SUPPORT_VULKAN
#pragma endregion Vulkan Engine Interface

#pragma region Metal Engine Interface
#if ZRHI_SUPPORT_METAL
        struct MetalBufferDesc
        {
            id<MTLBuffer> buffer = nil;
        };

        struct MetalTexture2DDesc
        {
            id<MTLTexture> texture = nil;
        };

        struct MetalTexture3DDesc
        {
            id<MTLTexture> texture = nil;
        };

        class MetalGFXCore : public GFXCore
        {
        public:
            virtual id<MTLDevice> GetDevice() noexcept = 0;
            virtual void AccessBuffer(void* bufferHandle, MetalBufferDesc& bufferDesc) noexcept = 0;
            virtual void AccessTexture2D(void* bufferHandle, MetalTexture2DDesc& bufferDesc) noexcept = 0;
            virtual void AccessTexture3D(void* bufferHandle, MetalTexture3DDesc& bufferDesc) noexcept = 0;
            virtual id<MTLCommandBuffer> GetCommandBuffer() noexcept = 0;
            virtual void StartRecording() noexcept = 0;
            virtual void StopRecording() noexcept = 0;
        };
#endif // ZRHI_SUPPORT_METAL
#pragma endregion Metal Engine Interface

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

    enum ReturnCode
    {
        ZRHI_SUCCESS = 0,
        ZRHI_ERROR = 100,
        ZRHI_FATAL_ERROR = 110,

        ZRHI_ERROR_NOT_INITIALIZED = 200,
        ZRHI_ERROR_ALREADY_INITIALIZED = 201,

        ZRHI_ERROR_INVALID_ARGUMENTS = 300,
        ZRHI_ERROR_NOT_IMPLEMENTED = 310,
        ZRHI_ERROR_NOT_SUPPORTED = 311,
    };

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
        /// @note Used with Vulkan
        uint32_t descriptorSet;
        /// Descriptor offset in corresponding set in DescriptorHeap.
        /// @note Used with Vulkan
        uint32_t offsetInSet;
        /// Descriptor offset in DescriptorHeap space.
        /// @note Used with D3D12
        uint32_t offsetInHeap;
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
        virtual void Initialize() noexcept = 0;
        /**
         * Releases RHI instance resources & Graphics API dependent resources.
         */
        virtual void Release() noexcept = 0;
        /**
         * Releases resources from garbage queue that safe frame attribute is less than current safe frame.
         * @note Current safe frame is got from EngineInterface that was passed with RHIInitInfo on Initialize().
         * @return true if garbage queue is empty after collecting or false if not.
         */
        virtual bool GarbageCollect() noexcept = 0;

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
         * @note NOOP for OpenGL/OpenGL ES, Metal, Direct3D 11, Vulkan
         */
        virtual void SetStablePowerState(bool enable) noexcept = 0;

        /**
         * Compiles Compute Pipeline State Object (Compute PSO) from binary/text source and returns it's instance.
         * @param desc - Compute PSO DESC structure.
         * @return RHI PSO instance or nullptr in case of error.
         */
        [[nodiscard]] virtual ComputePSO* CompileComputePSO(const ComputePSODesc& desc) noexcept = 0;
        /**
         * Compiles Graphics Pipeline State Object (Graphics PSO) from binary/text source and returns it's instance.
         * @param [in] desc - Graphics PSO DESC structure.
         * @return RHI PSO instance or nullptr in case of error.
         */
        [[nodiscard]] virtual GraphicsPSO* CompileGraphicPSO(const GraphicsPSODesc& desc) noexcept = 0;

        /**
         * Releases Compute Pipeline State Object (Compute PSO) resources.
         * @param [in] pso - Target Compute PSO.
         * @warning After releasing, handle passed to pso param becomes invalid.
         */
        virtual void ReleaseComputePSO(ComputePSO* pso) noexcept = 0;
        /**
         * Releases Graphics Pipeline State Object (Graphics PSO) resources.
         * @param [in] pso - Target Graphics PSO.
         * @warning After releasing, handle passed to pso param becomes invalid.
         */
        virtual void ReleaseGraphicPSO(GraphicsPSO* pso) noexcept = 0;

        /**
         * Registers native buffer from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be
         * decorated with this name.
         * @return RHI Buffer with registered native resource.
         */
        [[nodiscard]] virtual Buffer* RegisterBuffer(void* resourceHandle, const char* name) noexcept = 0;
        /**
         * Registers native 2D texture from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Texture2D with registered native resource.
         */
        [[nodiscard]] virtual Texture2D* RegisterTexture2D(void* resourceHandle, const char* name) noexcept = 0;
        /**
         * Registers native 3D texture from outside of RHI as RHI object for future usage with RHI.
         * @param [in] resourceHandle - resource unique handle. Will be passed to EngineInterface for resource access.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Texture3D with registered native resource.
         */
        [[nodiscard]] virtual Texture3D* RegisterTexture3D(void* resourceHandle, const char* name) noexcept = 0;

        /**
         * Allocates graphics buffer in GPU or CPU memory (depends on usage flags).
         * @param [in] size - Buffer size in bytes.
         * @param [in] heapType - Descriptor Heap type. Depends on how resource is going to be used.
         * @param [in] usage - Resource usage flags.
         * @param [in] structuredStride - Buffer structured stride. Must be 0 for unstructured buffers.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Buffer instance.
         */
        [[nodiscard]] virtual Buffer* CreateBuffer(size_t size, ResourceHeapType heapType, ResourceUsage usage, uint32_t structuredStride,
                                                   const char* name) noexcept = 0;
        /**
         * Allocates 2D texture in GPU memory.
         * @param [in] width - Texture width (x).
         * @param [in] height - Texture height (y).
         * @param [in] mips - Quantity of texture mipmap levels.
         * @param [in] format - Texture pixel format.
         * @param [in] usage - Resource usage flags.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Buffer instance.
         */
        [[nodiscard]] virtual Texture2D* CreateTexture2D(size_t width, size_t height, size_t mips, TextureFormat format, ResourceUsage usage,
                                                         const char* name) noexcept = 0;
        /**
         * Allocates 3D texture in GPU memory.
         * @param [in] width - Texture width (x).
         * @param [in] height - Texture height (y).
         * @param [in] depth - Texture depth (z).
         * @param [in] mips - Quantity of texture mipmap levels.
         * @param [in] format - Texture pixel format.
         * @param [in] usage - Resource usage flags.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Buffer instance.
         */
        [[nodiscard]] virtual Texture3D* CreateTexture3D(size_t width, size_t height, size_t depth, size_t mips, TextureFormat format,
                                                         ResourceUsage usage, const char* name) noexcept = 0;
        /**
         * Allocates Sampler object.
         * @note NOOP for OpenGL/OpenGL ES
         * @param [in] addressMode - Address mode for texture sampling.
         * @param [in] filter - Sampling filter.
         * @param [in] descriptorHeap - Target Descriptor Heap for bindless APIs.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Sampler instance.
         */
        [[nodiscard]] virtual Sampler* CreateSampler(SamplerAddressMode addressMode, SamplerFilter filter, size_t descriptorLocationsCount,
                                                     const DescriptorLocation* descriptorLocations, const char* name) noexcept = 0;

        /**
         * Releases resources of RHI Buffer.
         * @param [in] buffer - Target Buffer instance.
         * @warning After releasing, handle passed to buffer param becomes invalid.
         */
        virtual void ReleaseBuffer(Buffer* buffer) noexcept = 0;
        /**
         * Releases resources of RHI 2D Texture.
         * @param [in] texture - Target Texture2D instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         */
        virtual void ReleaseTexture2D(Texture2D* texture) noexcept = 0;
        /**
         * Releases resources of RHI 3D Texture.
         * @param [in] texture - Target Texture23 instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         */
        virtual void ReleaseTexture3D(Texture3D* texture) noexcept = 0;
        /**
         * Releases resources of RHI Sampler.
         * @note NOOP for OpenGL/OpenGL ES
         * @param [in] sampler - Target Sampler instance.
         * @warning After releasing, handle passed to texture param becomes invalid.
         */
        virtual void ReleaseSampler(Sampler* sampler) noexcept = 0;

        /**
         * Creates RHI Descriptor Heap instance. Used for APIs with bindless system.
         * @note NOOP for OpenGL/OpenGL ES, Metal, Direct3D 11
         * @param [in] heapType - Descriptor Heap type.
         * @param [in] pipelineLayoutDesc - DESC structure for pipeline bindings layout.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI DescriptorHeap instance.
         */
        [[nodiscard]] virtual DescriptorHeap* CreateDescriptorHeap(DescriptorHeapType heapType, const PipelineLayoutDesc& pipelineLayoutDesc,
                                                                   const char* name) noexcept = 0;

        /**
         * Clones descriptor heap with descriptors.
         * @param [in] src - Source descriptor heap.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return Copy of descriptor heap with copied descriptors.
         */
        [[nodiscard]] virtual DescriptorHeap* CloneDescriptorHeap(DescriptorHeap* src, const char* name) noexcept = 0;

        /**
         * Releases resources of DescriptorHeap
         * @param [in] heap - Target DescriptorHeap instance.
         * @warning After releasing, handle passed to heap param becomes invalid.
         */
        virtual void ReleaseDescriptorHeap(DescriptorHeap* heap) noexcept = 0;

        /**
         * Creates RHI Query Heap instance. Used for performing GPU queries.
         * @note NOOP for OpenGL/OpenGL ES, Metal, Direct3D 11
         * @param [in] heapType - Query Heap type.
         * @param [in] queryCount - Quantity of queries in heap.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI QueryHeap instance.
         */
        [[nodiscard]] virtual QueryHeap* CreateQueryHeap(QueryHeapType heapType, uint64_t queryCount, const char* name) noexcept = 0;

        /**
         * Releases resources of QueryHeap
         * @param [in] heap - Target QueryHeap instance.
         * @warning After releasing, handle passed to heap param becomes invalid.
         */
        virtual void ReleaseQueryHeap(QueryHeap* heap) noexcept = 0;

        /**
         * Creates RHI Framebuffer instance.
         * @param [in] renderTargets - Render target views.
         * @param [in] depthStencil - Depth stencil views.
         * @param [in] name - Resource debug name. In debug configuration native resource will be decorated with this name.
         * @return RHI Framebuffer instance.
         */
        [[nodiscard]] virtual Framebuffer* CreateFramebuffer(size_t renderTargetsCount, Texture2D* const* renderTargets, Texture2D* depthStencil,
                                                             const char* name) noexcept = 0;
        /**
         * Releases resources of DescriptorHeap
         * @param [in] framebuffer - Target Framebuffer instance
         */
        virtual void ReleaseFramebuffer(Framebuffer* framebuffer) noexcept = 0;

        /**
         * Creates & initializes Shader Resource View (SRV) for selected buffer resource. Buffer can be either Structured or Raw.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] desc - Specifies buffer type and stride/format.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have ShaderResource usage flag enabled.
         * @attention Buffer can't have structured and formatted SRV simultaneously.
         */
        virtual void InitializeSRV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                                   const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Shader Resource View (SRV) for selected 2D texture resource.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture2D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         */
        virtual void InitializeSRV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;
        /**
         * Creates & initializes Shader Resource View (SRV) for selected 3D texture resource.
         * @note For bindless APIs creates SRV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture3D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         */
        virtual void InitializeSRV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Unordered Access View (UAV) for selected buffer resource. Buffer can be either Structured or Raw.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] desc - Specifies buffer type and stride/format.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have UnorderedAccess usage flag enabled.
         * @attention Buffer can't have structured and formatted UAV simultaneously.
         */
        virtual void InitializeUAV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                                   const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Unordered Access View (UAV) for selected 2D texture resource.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture2D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         */
        virtual void InitializeUAV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;
        /**
         * Creates & initializes Unordered Access View (UAV) for selected 3D texture resource.
         * @note For bindless APIs creates UAV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] texture - RHI Texture3D instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         */
        virtual void InitializeUAV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Creates & initializes Constance Buffer View (CBV) for selected buffer resource.
         * @note For bindless APIs creates CBV descriptor and writes it to Descriptor Heap to selected location.
         * @param [in] buffer - RHI Buffer instance.
         * @param [in] descriptorLocations - Array of target DescriptorHeaps and locations for bindless APIs.
         * @attention Target Buffer must have ConstantBuffer usage flag enabled.
         */
        virtual void InitializeCBV(Buffer* buffer, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept = 0;

        /**
         * Starts recording state. Used on Metal API for Command Encoder manipulations.
         */
        virtual void StartRecording() noexcept = 0;
        /**
         * Stops recording state. Used on Metal API for Command Encoder manipulations.
         */
        virtual void StopRecording() noexcept = 0;

        /**
         * Uploads binary data to buffer using Upload Ring on platforms where it is used.
         * @note If destination buffer is CBV - replaces whole buffer on DX11, independently of size
         * @attention Uploaded data will be available in frame CurrentFrame + MaxFramesInFlight
         * @attention You should always pass correct size of the buffer, so Metal can work correctly
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] data - binary upload data.
         * @param [in] size - upload data size in bytes.
         * @param [in] offset - offset in destination buffer memory in bytes.
         */
        virtual void UploadBuffer(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept = 0;
        /**
         * Uploads data bypassing Upload Ring on platforms where it is used.
         * @remark Needed to upload a large data on initialization
         * @attention Creates new buffer and replaces old one in future frames, so shouldn't be
         * abused for best performance.
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] data - binary upload data.
         * @param [in] size - upload data size in bytes.
         * @param [in] offset - offset in destination buffer memory in bytes.
         */
        virtual void UploadBufferSlow(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept = 0;
        /**
         * Uploads 3D texture data from CPU memory.
         * @remark Needed to upload a 3D texture initial data on initialization.
         * @attention Creates new staging buffer for transferring data. Not very performant.
         * @param [in] texture - Target RHI Texture instance.
         * @param [in] uploadData - upload data DESC.
         */
        virtual void UploadTextureSlow(Texture3D* texture, const TextureData& uploadData) noexcept = 0;

        /**
         * Maps buffer and copies data to address in destination param.
         * @param [in] buffer - Target RHI Buffer to read from.
         * @param [out] destination - Address of memory to copy to.
         * @param [in] size - Size in bytes or requested data.
         * @param [in] srcOffset - Offset in input Buffer to read from.
         * @attention destination must point on allocated memory sith size in bytes greater or equal than value in size param.
         */
        virtual void GetBufferData(Buffer* buffer, void* destination, size_t size, size_t srcOffset) noexcept = 0;
        /**
         * Waits for GPU work finish and gets data from Buffer. Buffer must have ResourceUsage::CopySource.
         * @attention Stalls CPU till GPU work related to target buffer finished.
         * @param buffer Target RHI Buffer to read from.
         * @param destination Address of memory to copy to.
         * @param size Size in bytes or requested data.
         * @param srcOffset Offset in input Buffer to read from.
         * @attention destination must point on allocated memory sith size in bytes greater or equal than value in size param.
         */
        virtual void GetBufferDataImmediately(Buffer* buffer, void* destination, size_t size, size_t srcOffset) noexcept = 0;
        /**
         * Enqueues Dispatch command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void Dispatch(const DispatchDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Indexed Instanced command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void DrawIndexedInstanced(const DrawIndexedInstancedDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Instanced command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void DrawInstanced(const DrawInstancedDesc& desc) noexcept = 0;

        /**
         * Enqueues Dispatch Indirect command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void DispatchIndirect(const DispatchIndirectDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Instanced Indirect command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void DrawInstancedIndirect(const DrawInstancedIndirectDesc& desc) noexcept = 0;
        /**
         * Enqueues Draw Indexed Instanced Indirect command in GPU.
         * @param desc - command Desc structure.
         */
        virtual void DrawIndexedInstancedIndirect(const DrawIndexedInstancedIndirectDesc& desc) noexcept = 0;

        /**
         * Enqueues Copy command in GPU.
         * @param [in] dstBuffer - Copy destination RHI Buffer instance.
         * @param [in] dstOffset - Copy destination buffer offset in bytes.
         * @param [in] srcBuffer - Copy source RHI Buffer instance.
         * @param [in] srcOffset - Copy source buffer offset in bytes.
         * @param [in] size - Copy size in bytes.
         */
        virtual void CopyBufferRegion(Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept = 0;
        /**
         * Clears formatted RHI Buffer data with value in clearValue param.
         * @param [in] buffer - Target RHI Buffer instance.
         * @param [in] bufferSize - Buffer size in bytes that will be cleared.
         * @param [in] clearValue - Value that will be written in each byte of cleared region.
         * @param [in] descriptorLocation - Target DescriptorHeap and location for bindless APIs.
         */
        virtual void ClearFormattedBuffer(Buffer* buffer, uint32_t bufferSize, uint8_t clearValue,
                                          const DescriptorLocation& descriptorLocation) noexcept = 0;

        /**
         * Submits GPU query.
         * @param [in] queryHeap - Target QueryHeap instance.
         * @param [in] queryIndex - Index of query in heap.
         * @param [in] queryDesc - Query description.
         */
        virtual void SubmitQuery(QueryHeap* queryHeap, uint64_t queryIndex, const QueryDesc& queryDesc) noexcept = 0;

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
        virtual bool ResolveQuery(QueryHeap* queryHeap, QueryType queryType, uint64_t offset, uint64_t count, void* result, size_t size) noexcept = 0;

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
         */
        virtual void StartDebugRegion(const char* regionName) noexcept = 0;
        /**
         * Pops debug region marker if Graphics API has these functional.
         * @note NOOP in non debug/profile configurations.
         */
        virtual void EndDebugRegion() noexcept = 0;
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

    ReturnCode CreateRHIFactory(RHIFactory** outFactory) noexcept;
    Version GetVersion() noexcept;
}

#pragma region CAPI
#define ZRHI_API_IMPORT __declspec(dllimport)
#define ZRHI_CONCAT_HELPER(A, B) A##B
#define ZRHI_PFN(name) ZRHI_CONCAT_HELPER(PFN_, name)
#define ZRHI_NS Zibra::RHI

namespace ZRHI_NS::CAPI
{
    using ZRHIRuntimeHandle = void*;
    using ZRHIFactoryHandle = void*;
}

#pragma region GFXCore
namespace ZRHI_NS::CAPI
{
    struct GFXCoreVTable
    {
        void* obj;
        GFXAPI (*GetGFXAPI)(void*);
        void (*InitializeFrameLatencySync)(void*, size_t maxFramesInFlight);
        void (*SyncFrameLatency)(void*);
    };
} // namespace ZRHI_NS::CAPI
#pragma endregion GFXCore

#pragma region VulkanGFXCore
#if ZRHI_SUPPORT_VULKAN
namespace ZRHI_NS::CAPI
{
    struct VulkanGFXCoreVTable
    {
        void* obj;
        GFXAPI (*GetGFXAPI)(void*);
        void (*InitializeFrameLatencySync)(void*, size_t maxFramesInFlight);
        void (*SyncFrameLatency)(void*);

        VkInstance (*GetInstance)(void*);
        VkDevice (*GetDevice)(void*);
        VkPhysicalDevice (*GetPhysicalDevice)(void*);
        VkCommandBuffer (*GetCommandBuffer)(void*);
        PFN_vkVoidFunction (*GetInstanceProcAddr)(void*, const char* procName);
        size_t (*GetCurrentFrame)(void*);
        size_t (*GetSafeFrame)(void*);
        void (*AccessBuffer)(void*, void* resourceHandle, Integration::VulkanBufferDesc& bufferDesc);
        void (*AccessImage)(void*, void* resourceHandle, VkImageLayout layout, Integration::VulkanTextureDesc& textureDesc);
        void (*StartRecording)(void*);
        VkFence (*StopRecording)(void*);
    };

    inline VulkanGFXCoreVTable VTConvert(Integration::VulkanGFXCore* obj) noexcept
    {
        using namespace Integration;
        using T = VulkanGFXCore;

        VulkanGFXCoreVTable vt{};
        vt.obj = obj;

        vt.GetGFXAPI = [](void* o) { return static_cast<T*>(o)->GetGFXAPI(); };
        vt.InitializeFrameLatencySync = [](void* o, size_t m) { return static_cast<T*>(o)->InitializeFrameLatencySync(m); };
        vt.SyncFrameLatency = [](void* o) { return static_cast<T*>(o)->SyncFrameLatency(); };

        vt.GetInstance = [](void* o) { return static_cast<T*>(o)->GetInstance(); };
        vt.GetDevice = [](void* o) { return static_cast<T*>(o)->GetDevice(); };
        vt.GetPhysicalDevice = [](void* o) { return static_cast<T*>(o)->GetPhysicalDevice(); };
        vt.GetCommandBuffer = [](void* o) { return static_cast<T*>(o)->GetCommandBuffer(); };
        vt.GetInstanceProcAddr = [](void* o, const char* p) { return static_cast<T*>(o)->GetInstanceProcAddr(p); };
        vt.GetCurrentFrame = [](void* o) { return static_cast<T*>(o)->GetCurrentFrame(); };
        vt.GetSafeFrame = [](void* o) { return static_cast<T*>(o)->GetSafeFrame(); };
        vt.AccessBuffer = [](void* o, void* r, VulkanBufferDesc& b) { return static_cast<T*>(o)->AccessBuffer(r, b); };
        vt.AccessImage = [](void* o, void* r, VkImageLayout l, VulkanTextureDesc& t) { return static_cast<T*>(o)->AccessImage(r, l, t); };
        vt.StartRecording = [](void* o) { return static_cast<T*>(o)->StartRecording(); };
        vt.StopRecording = [](void* o) { return static_cast<T*>(o)->StopRecording(); };
        return vt;
    }
} // namespace ZRHI_NS::CAPI
#endif // ZRHI_SUPPORT_VULKAN
#pragma endregion VulkanGFXCore

#pragma region D3D11GFXCore
#if ZRHI_SUPPORT_D3D11
namespace ZRHI_NS::CAPI
{
    struct D3D11GFXCoreVTable
    {
        void* obj;
        GFXAPI (*GetGFXAPI)(void*);
        void (*InitializeFrameLatencySync)(void*, size_t maxFramesInFlight);
        void (*SyncFrameLatency)(void*);

        ID3D11Device* (*GetDevice)(void*);
        ID3D11DeviceContext* (*GetDeviceContext)(void*);
        void (*AccessBuffer)(void*, void* resourceHandle, Integration::D3D11BufferDesc& bufferDesc);
        void (*AccessTexture2D)(void*, void* resourceHandle, Integration::D3D11Texture2DDesc& texture3dDesc);
        void (*AccessTexture3D)(void*, void* resourceHandle, Integration::D3D11Texture3DDesc& texture2dDesc);
    };

    inline D3D11GFXCoreVTable VTConvert(Integration::D3D11GFXCore* obj) noexcept
    {
        using namespace Integration;
        using T = D3D11GFXCore;

        D3D11GFXCoreVTable vt{};
        vt.obj = obj;

        vt.GetGFXAPI = [](void* o) { return static_cast<T*>(o)->GetGFXAPI(); };
        vt.InitializeFrameLatencySync = [](void* o, size_t m) { return static_cast<T*>(o)->InitializeFrameLatencySync(m); };
        vt.SyncFrameLatency = [](void* o) { return static_cast<T*>(o)->SyncFrameLatency(); };

        vt.GetDevice = [](void* o) { return static_cast<T*>(o)->GetDevice(); };
        vt.AccessBuffer = [](void* o, void* r, D3D11BufferDesc& b) { return static_cast<T*>(o)->AccessBuffer(r, b); };
        vt.AccessTexture2D = [](void* o, void* r, D3D11Texture2DDesc& t) { return static_cast<T*>(o)->AccessTexture2D(r, t); };
        vt.AccessTexture3D = [](void* o, void* r, D3D11Texture3DDesc& t) { return static_cast<T*>(o)->AccessTexture3D(r, t); };
        return vt;
    }
} // namespace ZRHI_NS::CAPI
#endif // ZRHI_SUPPORT_D3D11
#pragma endregion D3D11GFXCore

#pragma region D3D12GFXCore
#if ZRHI_SUPPORT_D3D12
namespace ZRHI_NS::CAPI
{
    struct D3D12GFXCoreVTable
    {
        void* obj;
        GFXAPI (*GetGFXAPI)(void*);
        void (*InitializeFrameLatencySync)(void*, size_t maxFramesInFlight);
        void (*SyncFrameLatency)(void*);

        ID3D12Device* (*GetDevice)(void*);
        ID3D12GraphicsCommandList* (*GetCommandList)(void*);
        ID3D12Fence* (*GetFrameFence)(void*);
        size_t (*GetNextFrameFenceValue)(void*);
        ID3D12CommandQueue* (*GetCommandQueue)(void*);
        void (*AccessBuffer)(void*, void* resourceHandle, Integration::D3D12BufferDesc& bufferDesc);
        void (*AccessTexture2D)(void*, void* resourceHandle, Integration::D3D12Texture2DDesc& texture3dDesc);
        void (*AccessTexture3D)(void*, void* resourceHandle, Integration::D3D12Texture3DDesc& texture3dDesc);
        void (*StartRecording)(void*);
        HANDLE (*StopRecording)(void*, size_t statesCount, const Integration::D3D12TrackedResourceState* states);
    };

    inline D3D12GFXCoreVTable VTConvert(Integration::D3D12GFXCore* obj) noexcept
    {
        using namespace Integration;
        using T = D3D12GFXCore;

        D3D12GFXCoreVTable vt{};
        vt.obj = obj;

        vt.GetGFXAPI = [](void* o) { return static_cast<T*>(o)->GetGFXAPI(); };
        vt.InitializeFrameLatencySync = [](void* o, size_t m) { return static_cast<T*>(o)->InitializeFrameLatencySync(m); };
        vt.SyncFrameLatency = [](void* o) { return static_cast<T*>(o)->SyncFrameLatency(); };

        vt.GetDevice = [](void* o) { return static_cast<T*>(o)->GetDevice(); };
        vt.GetCommandList = [](void* o) { return static_cast<T*>(o)->GetCommandList(); };
        vt.GetFrameFence = [](void* o) { return static_cast<T*>(o)->GetFrameFence(); };
        vt.GetNextFrameFenceValue = [](void* o) { return static_cast<T*>(o)->GetNextFrameFenceValue(); };
        vt.GetCommandQueue = [](void* o) { return static_cast<T*>(o)->GetCommandQueue(); };
        vt.AccessBuffer = [](void* o, void* r, D3D12BufferDesc& b) { return static_cast<T*>(o)->AccessBuffer(r, b); };
        vt.AccessTexture2D = [](void* o, void* r, D3D12Texture2DDesc& t) { return static_cast<T*>(o)->AccessTexture2D(r, t); };
        vt.AccessTexture3D = [](void* o, void* r, D3D12Texture3DDesc& t) { return static_cast<T*>(o)->AccessTexture3D(r, t); };
        vt.StartRecording = [](void* o) { return static_cast<T*>(o)->StartRecording(); };
        vt.StopRecording = [](void* o, size_t c, const D3D12TrackedResourceState* s) { return static_cast<T*>(o)->StopRecording(c, s); };
        return vt;
    }
} // namespace ZRHI_NS::CAPI
#endif // ZRHI_SUPPORT_D3D12
#pragma endregion D3D12GFXCore

#ifndef ZRHI_NO_CAPI_IMPL

#pragma region RHIRuntime
#define ZRHI_FNPFX(name) Zibra_RHI_RHIRuntime_##name

typedef void (*ZRHI_PFN(ZRHI_FNPFX(Release)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef bool (*ZRHI_PFN(ZRHI_FNPFX(GarbageCollect)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef ZRHI_NS::GFXAPI (*ZRHI_PFN(ZRHI_FNPFX(GetGFXAPI)))(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef bool (*ZRHI_PFN(ZRHI_FNPFX(QueryFeatureSupport)))(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Feature feature) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(SetStablePowerState)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, bool enable) noexcept;
typedef ZRHI_NS::ComputePSO* (*ZRHI_PFN(ZRHI_FNPFX(CompileComputePSO)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                const ZRHI_NS::ComputePSODesc& desc) noexcept;
typedef ZRHI_NS::GraphicsPSO* (*ZRHI_PFN(ZRHI_FNPFX(CompileGraphicPSO)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                 const ZRHI_NS::GraphicsPSODesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseComputePSO)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::ComputePSO* pso) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseGraphicPSO)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::GraphicsPSO* pso) noexcept;
typedef ZRHI_NS::Buffer* (*ZRHI_PFN(ZRHI_FNPFX(RegisterBuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                         const char* name) noexcept;
typedef ZRHI_NS::Texture2D* (*ZRHI_PFN(ZRHI_FNPFX(RegisterTexture2D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                               const char* name) noexcept;
typedef ZRHI_NS::Texture3D* (*ZRHI_PFN(ZRHI_FNPFX(RegisterTexture3D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                               const char* name) noexcept;
typedef ZRHI_NS::Buffer* (*ZRHI_PFN(ZRHI_FNPFX(CreateBuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t size,
                                                                       ZRHI_NS::ResourceHeapType heapType, ZRHI_NS::ResourceUsage usage,
                                                                       uint32_t structuredStride, const char* name) noexcept;
typedef ZRHI_NS::Texture2D* (*ZRHI_PFN(ZRHI_FNPFX(CreateTexture2D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t width, size_t height, size_t mips,
                                                                             ZRHI_NS::TextureFormat format, ZRHI_NS::ResourceUsage usage,
                                                                             const char* name) noexcept;
typedef ZRHI_NS::Texture3D* (*ZRHI_PFN(ZRHI_FNPFX(CreateTexture3D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t width, size_t height, size_t depth,
                                                                             size_t mips, ZRHI_NS::TextureFormat format,
                                                                             ZRHI_NS::ResourceUsage usage, const char* name) noexcept;
typedef ZRHI_NS::Sampler* (*ZRHI_PFN(ZRHI_FNPFX(CreateSampler)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::SamplerAddressMode addressMode,
                                                                         ZRHI_NS::SamplerFilter filter, size_t descriptorLocationsCount,
                                                                         const ZRHI_NS::DescriptorLocation* descriptorLocations,
                                                                         const char* name) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseBuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture2D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture3D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseSampler)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Sampler* sampler) noexcept;
typedef ZRHI_NS::DescriptorHeap* (*ZRHI_PFN(ZRHI_FNPFX(CreateDescriptorHeap)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                       ZRHI_NS::DescriptorHeapType heapType,
                                                                                       const ZRHI_NS::PipelineLayoutDesc& pipelineLayoutDesc,
                                                                                       const char* name) noexcept;
typedef ZRHI_NS::DescriptorHeap* (*ZRHI_PFN(ZRHI_FNPFX(CloneDescriptorHeap)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::DescriptorHeap* src,
                                                                                      const char* name) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseDescriptorHeap)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::DescriptorHeap* heap) noexcept;
typedef ZRHI_NS::QueryHeap* (*ZRHI_PFN(ZRHI_FNPFX(CreateQueryHeap)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeapType heapType,
                                                                             uint64_t queryCount, const char* name) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseQueryHeap)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* heap) noexcept;
typedef ZRHI_NS::Framebuffer* (*ZRHI_PFN(ZRHI_FNPFX(CreateFramebuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t renderTargetsCount,
                                                                                 ZRHI_NS::Texture2D* const* renderTargets,
                                                                                 ZRHI_NS::Texture2D* depthStencil, const char* name) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ReleaseFramebuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Framebuffer* framebuffer) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_B)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer,
                                                           ZRHI_NS::InitializeViewDesc desc, size_t descriptorLocationsCount,
                                                           const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T2D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T3D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_B)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer,
                                                           ZRHI_NS::InitializeViewDesc desc, size_t descriptorLocationsCount,
                                                           const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T2D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T3D)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(InitializeCBV)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, size_t descriptorLocationsCount,
                                                         const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(StartRecording)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(StopRecording)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(UploadBuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, const void* data, uint32_t size,
                                                        uint32_t offset) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(UploadBufferSlow)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, const void* data, uint32_t size,
                                                            uint32_t offset) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(UploadTextureSlow)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             const ZRHI_NS::TextureData& uploadData) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(GetBufferData)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, void* destination, size_t size,
                                                         size_t srcOffset) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(GetBufferDataImmediately)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, void* destination,
                                                                    size_t size, size_t srcOffset) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(Dispatch)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DispatchDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstanced)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                const ZRHI_NS::DrawIndexedInstancedDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(DrawInstanced)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DrawInstancedDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(DispatchIndirect)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DispatchIndirectDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(DrawInstancedIndirect)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                 const ZRHI_NS::DrawInstancedIndirectDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstancedIndirect)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                        const ZRHI_NS::DrawIndexedInstancedIndirectDesc& desc) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(CopyBufferRegion)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* dstBuffer, uint32_t dstOffset,
                                                            ZRHI_NS::Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(ClearFormattedBuffer)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, uint32_t bufferSize,
                                                                uint8_t clearValue, const ZRHI_NS::DescriptorLocation& descriptorLocation) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(SubmitQuery)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* queryHeap, uint64_t queryIndex,
                                                       const ZRHI_NS::QueryDesc& queryDesc) noexcept;
typedef bool (*ZRHI_PFN(ZRHI_FNPFX(ResolveQuery)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* queryHeap,
                                                        ZRHI_NS::QueryType queryType, uint64_t offset, uint64_t count, void* result,
                                                        size_t size) noexcept;
typedef uint64_t (*ZRHI_PFN(ZRHI_FNPFX(GetTimestampFrequency)))(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef ZRHI_NS::TimestampCalibrationResult (*ZRHI_PFN(ZRHI_FNPFX(GetTimestampCalibration)))(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef uint64_t (*ZRHI_PFN(ZRHI_FNPFX(GetCPUTimestamp)))(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(StartDebugRegion)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const char* regionName) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(EndDebugRegion)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(StartFrameCapture)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const char* captureName) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(StopFrameCapture)))(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;

#ifndef ZRHI_NO_STATIC_API_DECL
ZRHI_API_IMPORT void ZRHI_FNPFX(Release)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT bool ZRHI_FNPFX(GarbageCollect)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::GFXAPI ZRHI_FNPFX(GetGFXAPI)(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT bool ZRHI_FNPFX(QueryFeatureSupport)(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Feature feature) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(SetStablePowerState)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, bool enable) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ComputePSO* ZRHI_FNPFX(CompileComputePSO)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                const ZRHI_NS::ComputePSODesc& desc) noexcept;
ZRHI_API_IMPORT ZRHI_NS::GraphicsPSO* ZRHI_FNPFX(CompileGraphicPSO)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                 const ZRHI_NS::GraphicsPSODesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseComputePSO)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::ComputePSO* pso) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseGraphicPSO)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::GraphicsPSO* pso) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Buffer* ZRHI_FNPFX(RegisterBuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                         const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Texture2D* ZRHI_FNPFX(RegisterTexture2D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                               const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Texture3D* ZRHI_FNPFX(RegisterTexture3D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, void* resourceHandle,
                                                                               const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Buffer* ZRHI_FNPFX(CreateBuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t size,
                                                                       ZRHI_NS::ResourceHeapType heapType, ZRHI_NS::ResourceUsage usage,
                                                                       uint32_t structuredStride, const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Texture2D* ZRHI_FNPFX(CreateTexture2D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t width, size_t height, size_t mips,
                                                                             ZRHI_NS::TextureFormat format, ZRHI_NS::ResourceUsage usage,
                                                                             const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Texture3D* ZRHI_FNPFX(CreateTexture3D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t width, size_t height, size_t depth,
                                                                             size_t mips, ZRHI_NS::TextureFormat format,
                                                                             ZRHI_NS::ResourceUsage usage, const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Sampler* ZRHI_FNPFX(CreateSampler)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::SamplerAddressMode addressMode,
                                                                         ZRHI_NS::SamplerFilter filter, size_t descriptorLocationsCount,
                                                                         const ZRHI_NS::DescriptorLocation* descriptorLocations,
                                                                         const char* name) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseBuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseTexture2D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseTexture3D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseSampler)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Sampler* sampler) noexcept;
ZRHI_API_IMPORT ZRHI_NS::DescriptorHeap* ZRHI_FNPFX(CreateDescriptorHeap)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                                       ZRHI_NS::DescriptorHeapType heapType,
                                                                                       const ZRHI_NS::PipelineLayoutDesc& pipelineLayoutDesc,
                                                                                       const char* name) noexcept;
ZRHI_API_IMPORT ZRHI_NS::DescriptorHeap* ZRHI_FNPFX(CloneDescriptorHeap)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::DescriptorHeap* src,
                                                                                      const char* name) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseDescriptorHeap)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::DescriptorHeap* heap) noexcept;
ZRHI_API_IMPORT ZRHI_NS::QueryHeap* ZRHI_FNPFX(CreateQueryHeap)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeapType heapType,
                                                                             uint64_t queryCount, const char* name) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseQueryHeap)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* heap) noexcept;
ZRHI_API_IMPORT ZRHI_NS::Framebuffer* ZRHI_FNPFX(CreateFramebuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, size_t renderTargetsCount,
                                                                                 ZRHI_NS::Texture2D* const* renderTargets,
                                                                                 ZRHI_NS::Texture2D* depthStencil, const char* name) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ReleaseFramebuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Framebuffer* framebuffer) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeSRV_B)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer,
                                                           ZRHI_NS::InitializeViewDesc desc, size_t descriptorLocationsCount,
                                                           const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeSRV_T2D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeSRV_T3D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeUAV_B)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer,
                                                           ZRHI_NS::InitializeViewDesc desc, size_t descriptorLocationsCount,
                                                           const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeUAV_T2D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture2D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeUAV_T3D)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             size_t descriptorLocationsCount,
                                                             const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(InitializeCBV)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, size_t descriptorLocationsCount,
                                                         const ZRHI_NS::DescriptorLocation* descriptorLocations) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(StartRecording)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(StopRecording)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(UploadBuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, const void* data, uint32_t size,
                                                        uint32_t offset) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(UploadBufferSlow)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, const void* data, uint32_t size,
                                                            uint32_t offset) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(UploadTextureSlow)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Texture3D* texture,
                                                             const ZRHI_NS::TextureData& uploadData) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(GetBufferData)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, void* destination, size_t size,
                                                         size_t srcOffset) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(GetBufferDataImmediately)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, void* destination,
                                                                    size_t size, size_t srcOffset) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(Dispatch)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DispatchDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(DrawIndexedInstanced)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                const ZRHI_NS::DrawIndexedInstancedDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(DrawInstanced)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DrawInstancedDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(DispatchIndirect)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const ZRHI_NS::DispatchIndirectDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(DrawInstancedIndirect)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                 const ZRHI_NS::DrawInstancedIndirectDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(DrawIndexedInstancedIndirect)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance,
                                                                        const ZRHI_NS::DrawIndexedInstancedIndirectDesc& desc) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(CopyBufferRegion)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* dstBuffer, uint32_t dstOffset,
                                                            ZRHI_NS::Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(ClearFormattedBuffer)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::Buffer* buffer, uint32_t bufferSize,
                                                                uint8_t clearValue, const ZRHI_NS::DescriptorLocation& descriptorLocation) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(SubmitQuery)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* queryHeap, uint64_t queryIndex,
                                                       const ZRHI_NS::QueryDesc& queryDesc) noexcept;
ZRHI_API_IMPORT bool ZRHI_FNPFX(ResolveQuery)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, ZRHI_NS::QueryHeap* queryHeap,
                                                        ZRHI_NS::QueryType queryType, uint64_t offset, uint64_t count, void* result,
                                                        size_t size) noexcept;
ZRHI_API_IMPORT uint64_t ZRHI_FNPFX(GetTimestampFrequency)(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::TimestampCalibrationResult ZRHI_FNPFX(GetTimestampCalibration)(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT uint64_t ZRHI_FNPFX(GetCPUTimestamp)(const ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(StartDebugRegion)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const char* regionName) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(EndDebugRegion)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(StartFrameCapture)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance, const char* captureName) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(StopFrameCapture)(ZRHI_NS::CAPI::ZRHIRuntimeHandle instance) noexcept;
#else
extern ZRHI_PFN(ZRHI_FNPFX(Release)) ZRHI_FNPFX(Release);
extern ZRHI_PFN(ZRHI_FNPFX(GarbageCollect)) ZRHI_FNPFX(GarbageCollect);
extern ZRHI_PFN(ZRHI_FNPFX(GetGFXAPI)) ZRHI_FNPFX(GetGFXAPI);
extern ZRHI_PFN(ZRHI_FNPFX(QueryFeatureSupport)) ZRHI_FNPFX(QueryFeatureSupport);
extern ZRHI_PFN(ZRHI_FNPFX(SetStablePowerState)) ZRHI_FNPFX(SetStablePowerState);
extern ZRHI_PFN(ZRHI_FNPFX(CompileComputePSO)) ZRHI_FNPFX(CompileComputePSO);
extern ZRHI_PFN(ZRHI_FNPFX(CompileGraphicPSO)) ZRHI_FNPFX(CompileGraphicPSO);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseComputePSO)) ZRHI_FNPFX(ReleaseComputePSO);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseGraphicPSO)) ZRHI_FNPFX(ReleaseGraphicPSO);
extern ZRHI_PFN(ZRHI_FNPFX(RegisterBuffer)) ZRHI_FNPFX(RegisterBuffer);
extern ZRHI_PFN(ZRHI_FNPFX(RegisterTexture2D)) ZRHI_FNPFX(RegisterTexture2D);
extern ZRHI_PFN(ZRHI_FNPFX(RegisterTexture3D)) ZRHI_FNPFX(RegisterTexture3D);
extern ZRHI_PFN(ZRHI_FNPFX(CreateBuffer)) ZRHI_FNPFX(CreateBuffer);
extern ZRHI_PFN(ZRHI_FNPFX(CreateTexture2D)) ZRHI_FNPFX(CreateTexture2D);
extern ZRHI_PFN(ZRHI_FNPFX(CreateTexture3D)) ZRHI_FNPFX(CreateTexture3D);
extern ZRHI_PFN(ZRHI_FNPFX(CreateSampler)) ZRHI_FNPFX(CreateSampler);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseBuffer)) ZRHI_FNPFX(ReleaseBuffer);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture2D)) ZRHI_FNPFX(ReleaseTexture2D);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture3D)) ZRHI_FNPFX(ReleaseTexture3D);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseSampler)) ZRHI_FNPFX(ReleaseSampler);
extern ZRHI_PFN(ZRHI_FNPFX(CreateDescriptorHeap)) ZRHI_FNPFX(CreateDescriptorHeap);
extern ZRHI_PFN(ZRHI_FNPFX(CloneDescriptorHeap)) ZRHI_FNPFX(CloneDescriptorHeap);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseDescriptorHeap)) ZRHI_FNPFX(ReleaseDescriptorHeap);
extern ZRHI_PFN(ZRHI_FNPFX(CreateQueryHeap)) ZRHI_FNPFX(CreateQueryHeap);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseQueryHeap)) ZRHI_FNPFX(ReleaseQueryHeap);
extern ZRHI_PFN(ZRHI_FNPFX(CreateFramebuffer)) ZRHI_FNPFX(CreateFramebuffer);
extern ZRHI_PFN(ZRHI_FNPFX(ReleaseFramebuffer)) ZRHI_FNPFX(ReleaseFramebuffer);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_B)) ZRHI_FNPFX(InitializeSRV_B);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T2D)) ZRHI_FNPFX(InitializeSRV_T2D);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T3D)) ZRHI_FNPFX(InitializeSRV_T3D);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_B)) ZRHI_FNPFX(InitializeUAV_B);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T2D)) ZRHI_FNPFX(InitializeUAV_T2D);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T3D)) ZRHI_FNPFX(InitializeUAV_T3D);
extern ZRHI_PFN(ZRHI_FNPFX(InitializeCBV)) ZRHI_FNPFX(InitializeCBV);
extern ZRHI_PFN(ZRHI_FNPFX(StartRecording)) ZRHI_FNPFX(StartRecording);
extern ZRHI_PFN(ZRHI_FNPFX(StopRecording)) ZRHI_FNPFX(StopRecording);
extern ZRHI_PFN(ZRHI_FNPFX(UploadBuffer)) ZRHI_FNPFX(UploadBuffer);
extern ZRHI_PFN(ZRHI_FNPFX(UploadBufferSlow)) ZRHI_FNPFX(UploadBufferSlow);
extern ZRHI_PFN(ZRHI_FNPFX(UploadTextureSlow)) ZRHI_FNPFX(UploadTextureSlow);
extern ZRHI_PFN(ZRHI_FNPFX(GetBufferData)) ZRHI_FNPFX(GetBufferData);
extern ZRHI_PFN(ZRHI_FNPFX(GetBufferDataImmediately)) ZRHI_FNPFX(GetBufferDataImmediately);
extern ZRHI_PFN(ZRHI_FNPFX(Dispatch)) ZRHI_FNPFX(Dispatch);
extern ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstanced)) ZRHI_FNPFX(DrawIndexedInstanced);
extern ZRHI_PFN(ZRHI_FNPFX(DrawInstanced)) ZRHI_FNPFX(DrawInstanced);
extern ZRHI_PFN(ZRHI_FNPFX(DispatchIndirect)) ZRHI_FNPFX(DispatchIndirect);
extern ZRHI_PFN(ZRHI_FNPFX(DrawInstancedIndirect)) ZRHI_FNPFX(DrawInstancedIndirect);
extern ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstancedIndirect)) ZRHI_FNPFX(DrawIndexedInstancedIndirect);
extern ZRHI_PFN(ZRHI_FNPFX(CopyBufferRegion)) ZRHI_FNPFX(CopyBufferRegion);
extern ZRHI_PFN(ZRHI_FNPFX(ClearFormattedBuffer)) ZRHI_FNPFX(ClearFormattedBuffer);
extern ZRHI_PFN(ZRHI_FNPFX(SubmitQuery)) ZRHI_FNPFX(SubmitQuery);
extern ZRHI_PFN(ZRHI_FNPFX(ResolveQuery)) ZRHI_FNPFX(ResolveQuery);
extern ZRHI_PFN(ZRHI_FNPFX(GetTimestampFrequency)) ZRHI_FNPFX(GetTimestampFrequency);
extern ZRHI_PFN(ZRHI_FNPFX(GetTimestampCalibration)) ZRHI_FNPFX(GetTimestampCalibration);
extern ZRHI_PFN(ZRHI_FNPFX(GetCPUTimestamp)) ZRHI_FNPFX(GetCPUTimestamp);
extern ZRHI_PFN(ZRHI_FNPFX(StartDebugRegion)) ZRHI_FNPFX(StartDebugRegion);
extern ZRHI_PFN(ZRHI_FNPFX(EndDebugRegion)) ZRHI_FNPFX(EndDebugRegion);
extern ZRHI_PFN(ZRHI_FNPFX(StartFrameCapture)) ZRHI_FNPFX(StartFrameCapture);
extern ZRHI_PFN(ZRHI_FNPFX(StopFrameCapture)) ZRHI_FNPFX(StopFrameCapture);
#endif

namespace ZRHI_NS::CAPI
{
    // --- RHIInterface Wrapper ----------------------------------------------------------------------------------------------------------------------


    class RHIInterfaceDLLProxy final : public RHIRuntime
    {
        friend class RHIFactoryDLLProxy;

    private:
        explicit RHIInterfaceDLLProxy(ZRHIRuntimeHandle handle) noexcept
            : m_NativeInstance(handle)
        {
        }
        ~RHIInterfaceDLLProxy() noexcept final = default;

    public:
        void Initialize() noexcept final
        {
            // NOOP
        }

        void Release() noexcept final
        {
            ZRHI_FNPFX(Release)(m_NativeInstance);
            delete this;
        }

        bool GarbageCollect() noexcept final
        {
            return ZRHI_FNPFX(GarbageCollect)(m_NativeInstance);
        }

        [[nodiscard]] GFXAPI GetGFXAPI() const noexcept final
        {
            return ZRHI_FNPFX(GetGFXAPI)(m_NativeInstance);
        }

        [[nodiscard]] bool QueryFeatureSupport(Feature feature) const noexcept final
        {
            return ZRHI_FNPFX(QueryFeatureSupport)(m_NativeInstance, feature);
        }

        void SetStablePowerState(bool enable) noexcept final
        {
            ZRHI_FNPFX(SetStablePowerState)(m_NativeInstance, enable);
        }

        [[nodiscard]] ComputePSO* CompileComputePSO(const ComputePSODesc& desc) noexcept final
        {
            return ZRHI_FNPFX(CompileComputePSO)(m_NativeInstance, desc);
        }

        [[nodiscard]] GraphicsPSO* CompileGraphicPSO(const GraphicsPSODesc& desc) noexcept final
        {
            return ZRHI_FNPFX(CompileGraphicPSO)(m_NativeInstance, desc);
        }

        void ReleaseComputePSO(ComputePSO* pso) noexcept final
        {
            ZRHI_FNPFX(ReleaseComputePSO)(m_NativeInstance, pso);
        }

        void ReleaseGraphicPSO(GraphicsPSO* pso) noexcept final
        {
            ZRHI_FNPFX(ReleaseGraphicPSO)(m_NativeInstance, pso);
        }

        [[nodiscard]] Buffer* RegisterBuffer(void* resourceHandle, const char* name) noexcept final
        {
            return ZRHI_FNPFX(RegisterBuffer)(m_NativeInstance, resourceHandle, name);
        }

        [[nodiscard]] Texture2D* RegisterTexture2D(void* resourceHandle, const char* name) noexcept final
        {
            return ZRHI_FNPFX(RegisterTexture2D)(m_NativeInstance, resourceHandle, name);
        }

        [[nodiscard]] Texture3D* RegisterTexture3D(void* resourceHandle, const char* name) noexcept final
        {
            return ZRHI_FNPFX(RegisterTexture3D)(m_NativeInstance, resourceHandle, name);
        }

        [[nodiscard]] Buffer* CreateBuffer(size_t size, ResourceHeapType heapType, ResourceUsage usage, uint32_t structuredStride,
                                           const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateBuffer)(m_NativeInstance, size, heapType, usage, structuredStride, name);
        }

        [[nodiscard]] Texture2D* CreateTexture2D(size_t width, size_t height, size_t mips, TextureFormat format, ResourceUsage usage,
                                                 const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateTexture2D)(m_NativeInstance, width, height, mips, format, usage, name);
        }

        [[nodiscard]] Texture3D* CreateTexture3D(size_t width, size_t height, size_t depth, size_t mips, TextureFormat format, ResourceUsage usage,
                                                 const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateTexture3D)(m_NativeInstance, width, height, depth, mips, format, usage, name);
        }

        [[nodiscard]] Sampler* CreateSampler(SamplerAddressMode addressMode, SamplerFilter filter, size_t descriptorLocationsCount,
                                             const DescriptorLocation* descriptorLocations, const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateSampler)(m_NativeInstance, addressMode, filter, descriptorLocationsCount,
                                 descriptorLocations, name);
        }

        void ReleaseBuffer(Buffer* buffer) noexcept final
        {
            ZRHI_FNPFX(ReleaseBuffer)(m_NativeInstance, buffer);
        }

        void ReleaseTexture2D(Texture2D* texture) noexcept final
        {
            ZRHI_FNPFX(ReleaseTexture2D)(m_NativeInstance, texture);
        }

        void ReleaseTexture3D(Texture3D* texture) noexcept final
        {
            ZRHI_FNPFX(ReleaseTexture3D)(m_NativeInstance, texture);
        }

        void ReleaseSampler(Sampler* sampler) noexcept final
        {
            ZRHI_FNPFX(ReleaseSampler)(m_NativeInstance, sampler);
        }

        [[nodiscard]] DescriptorHeap* CreateDescriptorHeap(DescriptorHeapType heapType, const PipelineLayoutDesc& pipelineLayoutDesc,
                                                           const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateDescriptorHeap)(m_NativeInstance, heapType, pipelineLayoutDesc, name);
        }

        [[nodiscard]] DescriptorHeap* CloneDescriptorHeap(DescriptorHeap* src, const char* name) noexcept final
        {
            return ZRHI_FNPFX(CloneDescriptorHeap)(m_NativeInstance, src, name);
        }

        void ReleaseDescriptorHeap(DescriptorHeap* heap) noexcept final
        {
            ZRHI_FNPFX(ReleaseDescriptorHeap)(m_NativeInstance, heap);
        }

        [[nodiscard]] QueryHeap* CreateQueryHeap(QueryHeapType heapType, uint64_t queryCount, const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateQueryHeap)(m_NativeInstance, heapType, queryCount, name);
        }

        void ReleaseQueryHeap(QueryHeap* heap) noexcept final
        {
            ZRHI_FNPFX(ReleaseQueryHeap)(m_NativeInstance, heap);
        }

        [[nodiscard]] Framebuffer* CreateFramebuffer(size_t renderTargetsCount, Texture2D* const* renderTargets, Texture2D* depthStencil,
                                                     const char* name) noexcept final
        {
            return ZRHI_FNPFX(CreateFramebuffer)(m_NativeInstance, renderTargetsCount, renderTargets, depthStencil, name);
        }

        void ReleaseFramebuffer(Framebuffer* framebuffer) noexcept final
        {
            ZRHI_FNPFX(ReleaseFramebuffer)(m_NativeInstance, framebuffer);
        }

        void InitializeSRV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                           const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeSRV_B)(m_NativeInstance, buffer, desc, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeSRV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeSRV_T2D)(m_NativeInstance, texture, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeSRV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeSRV_T3D)(m_NativeInstance, texture, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeUAV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                           const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeUAV_B)(m_NativeInstance, buffer, desc, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeUAV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeUAV_T2D)(m_NativeInstance, texture, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeUAV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeUAV_T3D)(m_NativeInstance, texture, descriptorLocationsCount, descriptorLocations);
        }

        void InitializeCBV(Buffer* buffer, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            ZRHI_FNPFX(InitializeCBV)(m_NativeInstance, buffer, descriptorLocationsCount, descriptorLocations);
        }

        void StartRecording() noexcept final
        {
            ZRHI_FNPFX(StartRecording)(m_NativeInstance);
        }

        void StopRecording() noexcept final
        {
            ZRHI_FNPFX(StopRecording)(m_NativeInstance);
        }

        void UploadBuffer(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept final
        {
            ZRHI_FNPFX(UploadBuffer)(m_NativeInstance, buffer, data, size, offset);
        }

        void UploadBufferSlow(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept final
        {
            ZRHI_FNPFX(UploadBufferSlow)(m_NativeInstance, buffer, data, size, offset);
        }

        void UploadTextureSlow(Texture3D* texture, const TextureData& uploadData) noexcept final
        {
            ZRHI_FNPFX(UploadTextureSlow)(m_NativeInstance, texture, uploadData);
        }

        void GetBufferData(Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset) noexcept final
        {
            ZRHI_FNPFX(GetBufferData)(m_NativeInstance, buffer, destination, size, srcOffset);
        }

        void GetBufferDataImmediately(Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset) noexcept final
        {
            ZRHI_FNPFX(GetBufferDataImmediately)(m_NativeInstance, buffer, destination, size, srcOffset);
        }

        void Dispatch(const DispatchDesc& desc) noexcept final
        {
            ZRHI_FNPFX(Dispatch)(m_NativeInstance, desc);
        }

        void DrawIndexedInstanced(const DrawIndexedInstancedDesc& desc) noexcept final
        {
            ZRHI_FNPFX(DrawIndexedInstanced)(m_NativeInstance, desc);
        }

        void DrawInstanced(const DrawInstancedDesc& desc) noexcept final
        {
            ZRHI_FNPFX(DrawInstanced)(m_NativeInstance, desc);
        }

        void DispatchIndirect(const DispatchIndirectDesc& desc) noexcept final
        {
            ZRHI_FNPFX(DispatchIndirect)(m_NativeInstance, desc);
        }

        void DrawInstancedIndirect(const DrawInstancedIndirectDesc& desc) noexcept final
        {
            ZRHI_FNPFX(DrawInstancedIndirect)(m_NativeInstance, desc);
        }

        void DrawIndexedInstancedIndirect(const DrawIndexedInstancedIndirectDesc& desc) noexcept final
        {
            ZRHI_FNPFX(DrawIndexedInstancedIndirect)(m_NativeInstance, desc);
        }

        void CopyBufferRegion(Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept final
        {
            ZRHI_FNPFX(CopyBufferRegion)(m_NativeInstance, dstBuffer, dstOffset, srcBuffer, srcOffset, size);
        }

        void ClearFormattedBuffer(Buffer* buffer, uint32_t bufferSize, uint8_t clearValue, const DescriptorLocation& descriptorLocation) noexcept final
        {
            ZRHI_FNPFX(ClearFormattedBuffer)(m_NativeInstance, buffer, bufferSize, clearValue, descriptorLocation);
        }

        void SubmitQuery(QueryHeap* queryHeap, uint64_t queryIndex, const QueryDesc& queryDesc) noexcept final
        {
            ZRHI_FNPFX(SubmitQuery)(m_NativeInstance, queryHeap, queryIndex, queryDesc);
        }

        bool ResolveQuery(QueryHeap* queryHeap, QueryType queryType, uint64_t offset, uint64_t count, void* result, size_t size) noexcept final
        {
            return ZRHI_FNPFX(ResolveQuery)(m_NativeInstance, queryHeap, queryType, offset, count, result, size);
        }

        [[nodiscard]] uint64_t GetTimestampFrequency() const noexcept final
        {
            return ZRHI_FNPFX(GetTimestampFrequency)(m_NativeInstance);
        }

        [[nodiscard]] TimestampCalibrationResult GetTimestampCalibration() const noexcept final
        {
            return ZRHI_FNPFX(GetTimestampCalibration)(m_NativeInstance);
        }

        [[nodiscard]] uint64_t GetCPUTimestamp() const noexcept final
        {
            return ZRHI_FNPFX(GetCPUTimestamp)(m_NativeInstance);
        }

        void StartDebugRegion(const char* regionName) noexcept final
        {
            ZRHI_FNPFX(StartDebugRegion)(m_NativeInstance, regionName);
        }

        void EndDebugRegion() noexcept final
        {
            ZRHI_FNPFX(EndDebugRegion)(m_NativeInstance);
        }

        void StartFrameCapture(const char* captureName) noexcept final
        {
            ZRHI_FNPFX(StartFrameCapture)(m_NativeInstance, captureName);
        }

        void StopFrameCapture() noexcept final
        {
            ZRHI_FNPFX(StopFrameCapture)(m_NativeInstance);
        }

    private:
        ZRHIRuntimeHandle m_NativeInstance = nullptr;
    };
} // namespace ZRHI_NS::CAPI

#undef ZRHI_FNPFX
#pragma endregion RHIRuntime

#pragma region RHIFactory
#define ZRHI_FNPFX(name) Zibra_RHI_RHIFactory_##name

typedef void (*ZRHI_PFN(ZRHI_FNPFX(Destruct)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(SetGFXAPI)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, ZRHI_NS::GFXAPI type) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(UseGFXCore)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, void* engineInterfaceVTable) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(UseForcedAdapter)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, int32_t adapterIndex) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(UseAutoSelectedAdapter)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(ForceSoftwareDevice)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(ForceEnableDebugLayer)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(Create)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, ZRHI_NS::CAPI::ZRHIRuntimeHandle* outInstance) noexcept;
typedef void (*ZRHI_PFN(ZRHI_FNPFX(Release)))(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;

#ifndef ZRHI_NO_STATIC_API_DECL
ZRHI_API_IMPORT void ZRHI_FNPFX(Destruct)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(SetGFXAPI)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, ZRHI_NS::GFXAPI type) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(UseGFXCore)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, void* engineInterfaceVTable) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(UseForcedAdapter)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, int32_t adapterIndex) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(UseAutoSelectedAdapter)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(ForceSoftwareDevice)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(ForceEnableDebugLayer)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(Create)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance, ZRHI_NS::CAPI::ZRHIRuntimeHandle* outInstance) noexcept;
ZRHI_API_IMPORT void ZRHI_FNPFX(Release)(ZRHI_NS::CAPI::ZRHIFactoryHandle instance) noexcept;
#else
extern ZRHI_PFN(ZRHI_FNPFX(Destruct)) ZRHI_FNPFX(Destruct);
extern ZRHI_PFN(ZRHI_FNPFX(SetGFXAPI)) ZRHI_FNPFX(SetGFXAPI);
extern ZRHI_PFN(ZRHI_FNPFX(UseGFXCore)) ZRHI_FNPFX(UseGFXCore);
extern ZRHI_PFN(ZRHI_FNPFX(UseForcedAdapter)) ZRHI_FNPFX(UseForcedAdapter);
extern ZRHI_PFN(ZRHI_FNPFX(UseAutoSelectedAdapter)) ZRHI_FNPFX(UseAutoSelectedAdapter);
extern ZRHI_PFN(ZRHI_FNPFX(ForceSoftwareDevice)) ZRHI_FNPFX(ForceSoftwareDevice);
extern ZRHI_PFN(ZRHI_FNPFX(ForceEnableDebugLayer)) ZRHI_FNPFX(ForceEnableDebugLayer);
extern ZRHI_PFN(ZRHI_FNPFX(Create)) ZRHI_FNPFX(Create);
extern ZRHI_PFN(ZRHI_FNPFX(Release)) ZRHI_FNPFX(Release);
#endif


namespace ZRHI_NS::CAPI
{
    class RHIFactoryDLLProxy final : public RHIFactory
    {
        friend inline ReturnCode CreateRHIFactory(RHIFactory** outFactory) noexcept;
    private:
        explicit RHIFactoryDLLProxy(ZRHIFactoryHandle handle)
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode SetGFXAPI(GFXAPI type) noexcept final
        {
            return ZRHI_FNPFX(SetGFXAPI)(m_NativeInstance, type);
        }
        ReturnCode UseGFXCore(Integration::GFXCore* engineInterface) noexcept final
        {
            using namespace Integration;
            switch (engineInterface->GetGFXAPI())
            {
#if ZRHI_SUPPORT_D3D11
            case GFXAPI::D3D11: {
                auto vtable = VTConvert(static_cast<D3D11GFXCore*>(engineInterface));
                return ZRHI_FNPFX(UseGFXCore)(m_NativeInstance, &vtable);
            }
#endif // ZRHI_SUPPORT_D3D11
#if ZRHI_SUPPORT_D3D12
            case GFXAPI::D3D12: {
                auto vtable = VTConvert(static_cast<D3D12GFXCore*>(engineInterface));
                return ZRHI_FNPFX(UseGFXCore)(m_NativeInstance, &vtable);
            }
#endif // ZRHI_SUPPORT_D3D12
#if ZRHI_SUPPORT_VULKAN
            case GFXAPI::Vulkan: {
                auto vtable = VTConvert(static_cast<VulkanGFXCore*>(engineInterface));
                return ZRHI_FNPFX(UseGFXCore)(m_NativeInstance, &vtable);
            }
#endif // ZRHI_SUPPORT_D3D12
#if ZRHI_SUPPORT_METAL
            case GFXAPI::Metal:
                return ZRHI_ERROR_NOT_SUPPORTED;
#endif // ZRHI_SUPPORT_D3D12
            default:
                return ZRHI_ERROR_NOT_SUPPORTED;
            }
        }
        ReturnCode UseForcedAdapter(int32_t adapterIndex) noexcept final
        {
            return ZRHI_FNPFX(UseForcedAdapter)(m_NativeInstance, adapterIndex);
        }
        ReturnCode UseAutoSelectedAdapter() noexcept final
        {
            return ZRHI_FNPFX(UseAutoSelectedAdapter)(m_NativeInstance);
        }
        ReturnCode ForceSoftwareDevice() noexcept final
        {
            return ZRHI_FNPFX(ForceSoftwareDevice)(m_NativeInstance);
        }
        ReturnCode ForceEnableDebugLayer() noexcept final
        {
            return ZRHI_FNPFX(ForceEnableDebugLayer)(m_NativeInstance);
        }
        ReturnCode Create(RHIRuntime** outInstance) noexcept final
        {
            ZRHIRuntimeHandle nativeHandle;
            auto status = ZRHI_FNPFX(Create)(m_NativeInstance, &nativeHandle);
            if (status != ZRHI_SUCCESS)
                return status;
            *outInstance = new RHIInterfaceDLLProxy{nativeHandle};
            return ZRHI_SUCCESS;
        }
        void Release() noexcept final
        {
            ZRHI_FNPFX(Release)(m_NativeInstance);
            delete this;
        }

    private:
        ZRHIFactoryHandle m_NativeInstance;
    };
} // namespace ZRHI_NS::CAPI

#undef ZRHI_FNPFX
#pragma endregion RHIFactory

#pragma region Functions
#define ZRHI_FNPFX(name) Zibra_RHI_##name

typedef ZRHI_NS::Version (*ZRHI_PFN(ZRHI_FNPFX(GetVersion)))() noexcept;
typedef ZRHI_NS::ReturnCode (*ZRHI_PFN(ZRHI_FNPFX(CreateRHIFactory)))(ZRHI_NS::CAPI::ZRHIFactoryHandle* outInstance) noexcept;

#ifndef ZRHI_NO_STATIC_API_DECL
ZRHI_API_IMPORT ZRHI_NS::Version ZRHI_FNPFX(GetVersion)() noexcept;
ZRHI_API_IMPORT ZRHI_NS::ReturnCode ZRHI_FNPFX(CreateRHIFactory)(ZRHI_NS::CAPI::ZRHIFactoryHandle* outInstance) noexcept;
#else
extern ZRHI_PFN(ZRHI_FNPFX(GetVersion)) ZRHI_FNPFX(GetVersion);
extern ZRHI_PFN(ZRHI_FNPFX(CreateRHIFactory)) ZRHI_FNPFX(CreateRHIFactory);
#endif

namespace ZRHI_NS::CAPI
{
    inline Version GetVersion()
    {
        return ZRHI_FNPFX(GetVersion)();
    }

    inline ReturnCode CreateRHIFactory(RHIFactory** outFactory) noexcept
    {
        if (!outFactory)
            return ZRHI_ERROR_INVALID_ARGUMENTS;
        ZRHIFactoryHandle nativeHandle;
        auto status = ZRHI_FNPFX(CreateRHIFactory)(&nativeHandle);
        if (status != ZRHI_SUCCESS)
            return status;
        *outFactory = new RHIFactoryDLLProxy{nativeHandle};
        return ZRHI_SUCCESS;
    }
} // namespace ZRHI_NS::CAPI

#undef ZRHI_FNPFX
#pragma endregion Functions

#pragma region ConsumerBridge
namespace ZRHI_NS::CAPI::ConsumerBridge
{
    struct RHIRuntimeVTable
    {
        void* obj;
        void (*Initialize)(void*);
        void (*Release)(void*);
        bool (*GarbageCollect)(void*);

        GFXAPI (*GetGFXAPI)(void*);

        bool (*QueryFeatureSupport)(void*, Feature feature);
        void (*SetStablePowerState)(void*, bool enable);

        ComputePSO* (*CompileComputePSO)(void*, const ComputePSODesc& desc);
        GraphicsPSO* (*CompileGraphicPSO)(void*, const GraphicsPSODesc& desc);

        void (*ReleaseComputePSO)(void*, ComputePSO* pso);
        void (*ReleaseGraphicPSO)(void*, GraphicsPSO* pso);

        Buffer* (*RegisterBuffer)(void*, void* resourceHandle, const char* name);
        Texture2D* (*RegisterTexture2D)(void*, void* resourceHandle, const char* name);
        Texture3D* (*RegisterTexture3D)(void*, void* resourceHandle, const char* name);

        Buffer* (*CreateBuffer)(void*, size_t size, ResourceHeapType heapType, ResourceUsage usage, uint32_t structuredStride, const char* name);
        Texture2D* (*CreateTexture2D)(void*, size_t width, size_t height, size_t mips, TextureFormat format, ResourceUsage usage, const char* name);
        Texture3D* (*CreateTexture3D)(void*, size_t width, size_t height, size_t depth, size_t mips, TextureFormat format, ResourceUsage usage,
                                      const char* name);
        Sampler* (*CreateSampler)(void*, SamplerAddressMode addressMode, SamplerFilter filter, size_t descriptorLocationsCount,
                                  const DescriptorLocation* descriptorLocations, const char* name);

        void (*ReleaseBuffer)(void*, Buffer* buffer);
        void (*ReleaseTexture2D)(void*, Texture2D* texture);
        void (*ReleaseTexture3D)(void*, Texture3D* texture);
        void (*ReleaseSampler)(void*, Sampler* sampler);

        DescriptorHeap* (*CreateDescriptorHeap)(void*, DescriptorHeapType heapType, const PipelineLayoutDesc& pipelineLayoutDesc, const char* name);
        DescriptorHeap* (*CloneDescriptorHeap)(void*, DescriptorHeap* src, const char* name);
        void (*ReleaseDescriptorHeap)(void*, DescriptorHeap* heap);

        QueryHeap* (*CreateQueryHeap)(void*, QueryHeapType heapType, uint64_t queryCount, const char* name);
        void (*ReleaseQueryHeap)(void*, QueryHeap* heap);

        Framebuffer* (*CreateFramebuffer)(void*, size_t renderTargetsCount, Texture2D* const* renderTargets, Texture2D* depthStencil,
                                          const char* name);
        void (*ReleaseFramebuffer)(void*, Framebuffer* framebuffer);

        void (*InitializeSRVB)(void*, Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                               const DescriptorLocation* descriptorLocations);
        void (*InitializeSRVT2D)(void*, Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations);
        void (*InitializeSRVT3D)(void*, Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations);

        void (*InitializeUAVB)(void*, Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                               const DescriptorLocation* descriptorLocations);
        void (*InitializeUAVT2D)(void*, Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations);
        void (*InitializeUAVT3D)(void*, Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations);

        void (*InitializeCBV)(void*, Buffer* buffer, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations);

        void (*StartRecording)(void*);
        void (*StopRecording)(void*);

        void (*UploadBuffer)(void*, Buffer* buffer, const void* data, uint32_t size, uint32_t offset);
        void (*UploadBufferSlow)(void*, Buffer* buffer, const void* data, uint32_t size, uint32_t offset);
        void (*UploadTextureSlow)(void*, Texture3D* texture, const TextureData& uploadData);

        void (*GetBufferData)(void*, Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset);
        void (*GetBufferDataImmediately)(void*, Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset);

        void (*Dispatch)(void*, const DispatchDesc& desc);
        void (*DrawIndexedInstanced)(void*, const DrawIndexedInstancedDesc& desc);
        void (*DrawInstanced)(void*, const DrawInstancedDesc& desc);
        void (*DispatchIndirect)(void*, const DispatchIndirectDesc& desc);
        void (*DrawInstancedIndirect)(void*, const DrawInstancedIndirectDesc& desc);
        void (*DrawIndexedInstancedIndirect)(void*, const DrawIndexedInstancedIndirectDesc& desc);

        void (*CopyBufferRegion)(void*, Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size);
        void (*ClearFormattedBuffer)(void*, Buffer* buffer, uint32_t bufferSize, uint8_t clearValue, const DescriptorLocation& descriptorLocation);

        void (*SubmitQuery)(void*, QueryHeap* queryHeap, uint64_t queryIndex, const QueryDesc& queryDesc);
        bool (*ResolveQuery)(void*, QueryHeap* queryHeap, QueryType queryType, uint64_t offset, uint64_t count, void* result, size_t size);
        uint64_t (*GetTimestampFrequency)(void*);
        TimestampCalibrationResult (*GetTimestampCalibration)(void*);
        uint64_t (*GetCPUTimestamp)(void*);

        void (*StartDebugRegion)(void*, const char* regionName);
        void (*EndDebugRegion)(void*);
        void (*StartFrameCapture)(void*, const char* captureName);
        void (*StopFrameCapture)(void*);
    };

    inline RHIRuntimeVTable VTConvert(RHI::RHIRuntime* obj) noexcept
    {
        using T = RHI::RHIRuntime;
        RHIRuntimeVTable vt{};
        vt.obj = obj;

        vt.Initialize = [](void* o) { return static_cast<T*>(o)->Initialize(); };
        vt.Release = [](void* o) { return static_cast<T*>(o)->Release(); };
        vt.GarbageCollect = [](void* o) { return static_cast<T*>(o)->GarbageCollect(); };

        vt.GetGFXAPI = [](void* o) { return static_cast<T*>(o)->GetGFXAPI(); };

        vt.QueryFeatureSupport = [](void* o, Feature f) { return static_cast<T*>(o)->QueryFeatureSupport(f); };
        vt.SetStablePowerState = [](void* o, bool e) { return static_cast<T*>(o)->SetStablePowerState(e); };

        vt.CompileComputePSO = [](void* o, const ComputePSODesc& d) { return static_cast<T*>(o)->CompileComputePSO(d); };
        vt.CompileComputePSO = [](void* o, const ComputePSODesc& d) { return static_cast<T*>(o)->CompileComputePSO(d); };
        vt.ReleaseComputePSO = [](void* o, ComputePSO* p) { return static_cast<T*>(o)->ReleaseComputePSO(p); };
        vt.ReleaseGraphicPSO = [](void* o, GraphicsPSO* p) { return static_cast<T*>(o)->ReleaseGraphicPSO(p); };

        vt.RegisterBuffer = [](void* o, void* r, const char* n) { return static_cast<T*>(o)->RegisterBuffer(r, n); };
        vt.RegisterTexture2D = [](void* o, void* r, const char* n) { return static_cast<T*>(o)->RegisterTexture2D(r, n); };
        vt.RegisterTexture3D = [](void* o, void* r, const char* n) { return static_cast<T*>(o)->RegisterTexture3D(r, n); };

        vt.CreateBuffer = [](void* o, size_t s, ResourceHeapType h, ResourceUsage u, uint32_t ss, const char* n) {
            return static_cast<T*>(o)->CreateBuffer(s, h, u, ss, n);
        };
        vt.CreateTexture2D = [](void* o, size_t w, size_t h, size_t m, TextureFormat f, ResourceUsage u, const char* n) {
            return static_cast<T*>(o)->CreateTexture2D(w, h, m, f, u, n);
        };
        vt.CreateTexture3D = [](void* o, size_t w, size_t h, size_t d, size_t m, TextureFormat f, ResourceUsage u, const char* n) {
            return static_cast<T*>(o)->CreateTexture3D(w, h, d, m, f, u, n);
        };
        vt.CreateSampler = [](void* o, SamplerAddressMode a, SamplerFilter f, size_t lc, const DescriptorLocation* l, const char* n) {
            return static_cast<T*>(o)->CreateSampler(a, f, lc, l, n);
        };

        vt.ReleaseBuffer = [](void* o, Buffer* b) { return static_cast<T*>(o)->ReleaseBuffer(b); };
        vt.ReleaseTexture2D = [](void* o, Texture2D* t) { return static_cast<T*>(o)->ReleaseTexture2D(t); };
        vt.ReleaseTexture3D = [](void* o, Texture3D* t) { return static_cast<T*>(o)->ReleaseTexture3D(t); };
        vt.ReleaseSampler = [](void* o, Sampler* s) { return static_cast<T*>(o)->ReleaseSampler(s); };

        vt.CreateDescriptorHeap = [](void* o, DescriptorHeapType t, const PipelineLayoutDesc& d, const char* n) {
            return static_cast<T*>(o)->CreateDescriptorHeap(t, d, n);
        };
        vt.CloneDescriptorHeap = [](void* o, DescriptorHeap* s, const char* n) { return static_cast<T*>(o)->CloneDescriptorHeap(s, n); };
        vt.ReleaseDescriptorHeap = [](void* o, DescriptorHeap* h) { return static_cast<T*>(o)->ReleaseDescriptorHeap(h); };

        vt.CreateQueryHeap = [](void* o, QueryHeapType t, uint64_t c, const char* n) { return static_cast<T*>(o)->CreateQueryHeap(t, c, n); };
        vt.ReleaseQueryHeap = [](void* o, QueryHeap* h) { return static_cast<T*>(o)->ReleaseQueryHeap(h); };

        vt.CreateFramebuffer = [](void* o, size_t tc, Texture2D* const* t, Texture2D* ds, const char* n) {
            return static_cast<T*>(o)->CreateFramebuffer(tc, t, ds, n);
        };
        vt.ReleaseFramebuffer = [](void* o, Framebuffer* f) { return static_cast<T*>(o)->ReleaseFramebuffer(f); };

        vt.InitializeSRVB = [](void* o, Buffer* b, InitializeViewDesc d, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeSRV(b, d, lc, l);
        };
        vt.InitializeSRVT2D = [](void* o, Texture2D* t, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeSRV(t, lc, l);
        };
        vt.InitializeSRVT3D = [](void* o, Texture3D* t, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeSRV(t, lc, l);
        };
        vt.InitializeUAVB = [](void* o, Buffer* b, InitializeViewDesc d, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeUAV(b, d, lc, l);
        };
        vt.InitializeUAVT2D = [](void* o, Texture2D* t, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeUAV(t, lc, l);
        };
        vt.InitializeUAVT3D = [](void* o, Texture3D* t, size_t lc, const DescriptorLocation* l) {
            return static_cast<T*>(o)->InitializeUAV(t, lc, l);
        };
        vt.InitializeCBV = [](void* o, Buffer* b, size_t lc, const DescriptorLocation* l) { return static_cast<T*>(o)->InitializeCBV(b, lc, l); };

        vt.StartRecording = [](void* o) { return static_cast<T*>(o)->StartRecording(); };
        vt.StopRecording = [](void* o) { return static_cast<T*>(o)->StopRecording(); };

        vt.UploadBuffer = [](void* o, Buffer* b, const void* d, uint32_t s, uint32_t of) { return static_cast<T*>(o)->UploadBuffer(b, d, s, of); };
        vt.UploadBufferSlow = [](void* o, Buffer* b, const void* d, uint32_t s, uint32_t of) {
            return static_cast<T*>(o)->UploadBufferSlow(b, d, s, of);
        };
        vt.UploadTextureSlow = [](void* o, Texture3D* t, const TextureData& d) { return static_cast<T*>(o)->UploadTextureSlow(t, d); };

        vt.GetBufferData = [](void* o, Buffer* b, void* d, std::size_t s, std::size_t of) { return static_cast<T*>(o)->GetBufferData(b, d, s, of); };
        vt.GetBufferDataImmediately = [](void* o, Buffer* b, void* d, std::size_t s, std::size_t of) {
            return static_cast<T*>(o)->GetBufferDataImmediately(b, d, s, of);
        };

        vt.Dispatch = [](void* o, const DispatchDesc& d) { return static_cast<T*>(o)->Dispatch(d); };
        vt.DrawIndexedInstanced = [](void* o, const DrawIndexedInstancedDesc& d) { return static_cast<T*>(o)->DrawIndexedInstanced(d); };
        vt.DrawInstanced = [](void* o, const DrawInstancedDesc& d) { return static_cast<T*>(o)->DrawInstanced(d); };
        vt.DispatchIndirect = [](void* o, const DispatchIndirectDesc& d) { return static_cast<T*>(o)->DispatchIndirect(d); };
        vt.DrawInstancedIndirect = [](void* o, const DrawInstancedIndirectDesc& d) { return static_cast<T*>(o)->DrawInstancedIndirect(d); };
        vt.DrawIndexedInstancedIndirect = [](void* o, const DrawIndexedInstancedIndirectDesc& d) {
            return static_cast<T*>(o)->DrawIndexedInstancedIndirect(d);
        };

        vt.CopyBufferRegion = [](void* o, Buffer* db, uint32_t dof, Buffer* sb, uint32_t sof, uint32_t s) {
            return static_cast<T*>(o)->CopyBufferRegion(db, dof, sb, sof, s);
        };
        vt.ClearFormattedBuffer = [](void* o, Buffer* b, uint32_t s, uint8_t v, const DescriptorLocation& l) {
            return static_cast<T*>(o)->ClearFormattedBuffer(b, s, v, l);
        };

        vt.SubmitQuery = [](void* o, QueryHeap* h, uint64_t i, const QueryDesc& d) { return static_cast<T*>(o)->SubmitQuery(h, i, d); };
        vt.ResolveQuery = [](void* o, QueryHeap* h, QueryType t, uint64_t of, uint64_t c, void* r, size_t s) {
            return static_cast<T*>(o)->ResolveQuery(h, t, of, c, r, s);
        };
        vt.GetTimestampFrequency = [](void* o) { return static_cast<T*>(o)->GetTimestampFrequency(); };
        vt.GetTimestampCalibration = [](void* o) { return static_cast<T*>(o)->GetTimestampCalibration(); };
        vt.GetCPUTimestamp = [](void* o) { return static_cast<T*>(o)->GetCPUTimestamp(); };

        vt.StartDebugRegion = [](void* o, const char* n) { return static_cast<T*>(o)->StartDebugRegion(n); };
        vt.EndDebugRegion = [](void* o) { return static_cast<T*>(o)->EndDebugRegion(); };
        vt.StartFrameCapture = [](void* o, const char* n) { return static_cast<T*>(o)->StartFrameCapture(n); };
        vt.StopFrameCapture = [](void* o) { return static_cast<T*>(o)->StopFrameCapture(); };
        return vt;
    }

    class RHIInterfaceVTProxy final : public RHIRuntime
    {
    public:
        explicit RHIInterfaceVTProxy(const RHIRuntimeVTable& vt) noexcept
            : m_VT{vt}
        {
        }

    public:
        void Initialize() noexcept final
        {
            m_VT.Initialize(m_VT.obj);
        }
        void Release() noexcept final
        {
            m_VT.Release(m_VT.obj);
        }
        bool GarbageCollect() noexcept final
        {
            return m_VT.GarbageCollect(m_VT.obj);
        }
        [[nodiscard]] GFXAPI GetGFXAPI() const noexcept final
        {
            return m_VT.GetGFXAPI(m_VT.obj);
        }
        bool QueryFeatureSupport(Feature feature) const noexcept final
        {
            return m_VT.QueryFeatureSupport(m_VT.obj, feature);
        }
        void SetStablePowerState(bool enable) noexcept final
        {
            m_VT.SetStablePowerState(m_VT.obj, enable);
        }
        [[nodiscard]] ComputePSO* CompileComputePSO(const ComputePSODesc& desc) noexcept final
        {
            return m_VT.CompileComputePSO(m_VT.obj, desc);
        }
        [[nodiscard]] GraphicsPSO* CompileGraphicPSO(const GraphicsPSODesc& desc) noexcept final
        {
            return m_VT.CompileGraphicPSO(m_VT.obj, desc);
        }
        void ReleaseComputePSO(ComputePSO* pso) noexcept final
        {
            m_VT.ReleaseComputePSO(m_VT.obj, pso);
        }
        void ReleaseGraphicPSO(GraphicsPSO* pso) noexcept final
        {
            m_VT.ReleaseGraphicPSO(m_VT.obj, pso);
        }
        [[nodiscard]] Buffer* RegisterBuffer(void* resourceHandle, const char* name) noexcept final
        {
            return m_VT.RegisterBuffer(m_VT.obj, resourceHandle, name);
        }
        [[nodiscard]] Texture2D* RegisterTexture2D(void* resourceHandle, const char* name) noexcept final
        {
            return m_VT.RegisterTexture2D(m_VT.obj, resourceHandle, name);
        }
        [[nodiscard]] Texture3D* RegisterTexture3D(void* resourceHandle, const char* name) noexcept final
        {
            return m_VT.RegisterTexture3D(m_VT.obj, resourceHandle, name);
        }
        [[nodiscard]] Buffer* CreateBuffer(size_t size, ResourceHeapType heapType, ResourceUsage usage, uint32_t structuredStride,
                                           const char* name) noexcept final
        {
            return m_VT.CreateBuffer(m_VT.obj, size, heapType, usage, structuredStride, name);
        }
        [[nodiscard]] Texture2D* CreateTexture2D(size_t width, size_t height, size_t mips, TextureFormat format, ResourceUsage usage,
                                                 const char* name) noexcept final
        {
            return m_VT.CreateTexture2D(m_VT.obj, width, height, mips, format, usage, name);
        }
        [[nodiscard]] Texture3D* CreateTexture3D(size_t width, size_t height, size_t depth, size_t mips, TextureFormat format, ResourceUsage usage,
                                                 const char* name) noexcept final
        {
            return m_VT.CreateTexture3D(m_VT.obj, width, height, depth, mips, format, usage, name);
        }
        [[nodiscard]] Sampler* CreateSampler(SamplerAddressMode addressMode, SamplerFilter filter, size_t descriptorLocationsCount,
                                             const DescriptorLocation* descriptorLocations, const char* name) noexcept final
        {
            return m_VT.CreateSampler(m_VT.obj, addressMode, filter, descriptorLocationsCount, descriptorLocations, name);
        }
        void ReleaseBuffer(Buffer* buffer) noexcept final
        {
            m_VT.ReleaseBuffer(m_VT.obj, buffer);
        }
        void ReleaseTexture2D(Texture2D* texture) noexcept final
        {
            m_VT.ReleaseTexture2D(m_VT.obj, texture);
        }
        void ReleaseTexture3D(Texture3D* texture) noexcept final
        {
            m_VT.ReleaseTexture3D(m_VT.obj, texture);
        }
        void ReleaseSampler(Sampler* sampler) noexcept final
        {
            m_VT.ReleaseSampler(m_VT.obj, sampler);
        }
        [[nodiscard]] DescriptorHeap* CreateDescriptorHeap(DescriptorHeapType heapType, const PipelineLayoutDesc& pipelineLayoutDesc,
                                                           const char* name) noexcept final
        {
            return m_VT.CreateDescriptorHeap(m_VT.obj, heapType, pipelineLayoutDesc, name);
        }
        [[nodiscard]] DescriptorHeap* CloneDescriptorHeap(DescriptorHeap* src, const char* name) noexcept final
        {
            return m_VT.CloneDescriptorHeap(m_VT.obj, src, name);
        }
        void ReleaseDescriptorHeap(DescriptorHeap* heap) noexcept final
        {
            m_VT.ReleaseDescriptorHeap(m_VT.obj, heap);
        }
        QueryHeap* CreateQueryHeap(QueryHeapType heapType, uint64_t queryCount, const char* name) noexcept final
        {
            return m_VT.CreateQueryHeap(m_VT.obj, heapType, queryCount, name);
        }
        void ReleaseQueryHeap(QueryHeap* heap) noexcept final
        {
            m_VT.ReleaseQueryHeap(m_VT.obj, heap);
        }
        [[nodiscard]] Framebuffer* CreateFramebuffer(size_t renderTargetsCount, Texture2D* const* renderTargets, Texture2D* depthStencil,
                                                     const char* name) noexcept final
        {
            return m_VT.CreateFramebuffer(m_VT.obj, renderTargetsCount, renderTargets, depthStencil, name);
        }
        void ReleaseFramebuffer(Framebuffer* framebuffer) noexcept final
        {
            m_VT.ReleaseFramebuffer(m_VT.obj, framebuffer);
        }
        void InitializeSRV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                           const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeSRVB(m_VT.obj, buffer, desc, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeSRV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeSRVT2D(m_VT.obj, texture, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeSRV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeSRVT3D(m_VT.obj, texture, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeUAV(Buffer* buffer, InitializeViewDesc desc, size_t descriptorLocationsCount,
                           const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeUAVB(m_VT.obj, buffer, desc, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeUAV(Texture2D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeUAVT2D(m_VT.obj, texture, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeUAV(Texture3D* texture, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeUAVT3D(m_VT.obj, texture, descriptorLocationsCount, descriptorLocations);
        }
        void InitializeCBV(Buffer* buffer, size_t descriptorLocationsCount, const DescriptorLocation* descriptorLocations) noexcept final
        {
            m_VT.InitializeCBV(m_VT.obj, buffer, descriptorLocationsCount, descriptorLocations);
        }
        void StartRecording() noexcept final
        {
            m_VT.StartRecording(m_VT.obj);
        }
        void StopRecording() noexcept final
        {
            m_VT.StartRecording(m_VT.obj);
        }
        void UploadBuffer(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept final
        {
            m_VT.UploadBuffer(m_VT.obj, buffer, data, size, offset);
        }
        void UploadBufferSlow(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) noexcept final
        {
            m_VT.UploadBufferSlow(m_VT.obj, buffer, data, size, offset);
        }
        void UploadTextureSlow(Texture3D* texture, const TextureData& uploadData) noexcept final
        {
            m_VT.UploadTextureSlow(m_VT.obj, texture, uploadData);
        }
        void GetBufferData(Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset) noexcept final
        {
            m_VT.GetBufferData(m_VT.obj, buffer, destination, size, srcOffset);
        }
        void GetBufferDataImmediately(Buffer* buffer, void* destination, std::size_t size, std::size_t srcOffset) noexcept final
        {
            m_VT.GetBufferDataImmediately(m_VT.obj, buffer, destination, size, srcOffset);
        }
        void Dispatch(const DispatchDesc& desc) noexcept final
        {
            m_VT.Dispatch(m_VT.obj, desc);
        }
        void DrawIndexedInstanced(const DrawIndexedInstancedDesc& desc) noexcept final
        {
            m_VT.DrawIndexedInstanced(m_VT.obj, desc);
        }
        void DrawInstanced(const DrawInstancedDesc& desc) noexcept final
        {
            m_VT.DrawInstanced(m_VT.obj, desc);
        }
        void DispatchIndirect(const DispatchIndirectDesc& desc) noexcept final
        {
            m_VT.DispatchIndirect(m_VT.obj, desc);
        }
        void DrawInstancedIndirect(const DrawInstancedIndirectDesc& desc) noexcept final
        {
            m_VT.DrawInstancedIndirect(m_VT.obj, desc);
        }
        void DrawIndexedInstancedIndirect(const DrawIndexedInstancedIndirectDesc& desc) noexcept final
        {
            m_VT.DrawIndexedInstancedIndirect(m_VT.obj, desc);
        }
        void CopyBufferRegion(Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) noexcept final
        {
            m_VT.CopyBufferRegion(m_VT.obj, dstBuffer, dstOffset, srcBuffer, srcOffset, size);
        }
        void ClearFormattedBuffer(Buffer* buffer, uint32_t bufferSize, uint8_t clearValue,
                                  const DescriptorLocation& descriptorLocation) noexcept final
        {
            m_VT.ClearFormattedBuffer(m_VT.obj, buffer, bufferSize, clearValue, descriptorLocation);
        }
        void SubmitQuery(QueryHeap* queryHeap, uint64_t queryIndex, const QueryDesc& queryDesc) noexcept final
        {
            m_VT.SubmitQuery(m_VT.obj, queryHeap, queryIndex, queryDesc);
        }
        bool ResolveQuery(QueryHeap* queryHeap, QueryType queryType, uint64_t offset, uint64_t count, void* result, size_t size) noexcept final
        {
            return m_VT.ResolveQuery(m_VT.obj, queryHeap, queryType, offset, count, result, size);
        }
        [[nodiscard]] uint64_t GetTimestampFrequency() const noexcept final
        {
            return m_VT.GetTimestampFrequency(m_VT.obj);
        }
        [[nodiscard]] TimestampCalibrationResult GetTimestampCalibration() const noexcept final
        {
            return m_VT.GetTimestampCalibration(m_VT.obj);
        }
        [[nodiscard]] uint64_t GetCPUTimestamp() const noexcept final
        {
            return m_VT.GetCPUTimestamp(m_VT.obj);
        }
        void StartDebugRegion(const char* regionName) noexcept final
        {
            m_VT.StartDebugRegion(m_VT.obj, regionName);
        }
        void EndDebugRegion() noexcept final
        {
            m_VT.EndDebugRegion(m_VT.obj);
        }
        void StartFrameCapture(const char* captureName) noexcept final
        {
            m_VT.StartFrameCapture(m_VT.obj, captureName);
        }
        void StopFrameCapture() noexcept final
        {
            m_VT.StopFrameCapture(m_VT.obj);
        }

    private:
        RHIRuntimeVTable m_VT;
    };
} // namespace ZRHI_NS::CAPI::ConsumerBridge
#pragma endregion ConsumerBridge

#endif //ZRHI_NO_CAPI_IMPL

#undef ZRHI_NS
#undef ZRHI_PFN
#undef ZRHI_CONCAT_HELPER
#undef ZRHI_API_IMPORT
#pragma endregion CAPI
