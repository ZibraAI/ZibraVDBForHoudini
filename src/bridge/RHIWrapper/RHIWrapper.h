#pragma once

#include <Zibra/RHI.h>
#include "openvdb/OpenVDBEncoder.h"

namespace Zibra
{
    class RHIWrapper
    {
    public:
        void Initialize() noexcept;

        void Release() noexcept;

        void AllocateExternalBuffers(CE::Decompression::DecompressorResourcesRequirements requirements) noexcept;

        void FreeExternalBuffers() noexcept;

        void StartRecording() const noexcept;
        void StopRecording() const noexcept;
        void GarbageCollect() noexcept;

        RHI::RHIRuntime* const GetRHIRuntime() const noexcept;

        const CE::Decompression::DecompressorResources& GetDecompressorResources() const noexcept;

        OpenVDBSupport::DecompressedFrameData GetDecompressedFrameData(const CE::Decompression::FrameInfo& frameInfo) const noexcept;

    private:
        RHI::RHIFactory* m_RHIFactory = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;

        CE::Decompression::DecompressorResources m_decompressorResources;
    };


} // namespace Zibra