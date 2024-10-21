#pragma once
#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport::MathHelpers
{

    bool IsNearlyEqual(float a, float b) noexcept;
    bool IsNearlyEqual(double a, double b) noexcept;
    bool IsNearlyInteger(float value) noexcept;
    bool IsNearlyInteger(double value) noexcept;
    float RoundIfNearlyZero(float value) noexcept;
    int FloorWithEpsilon(float value) noexcept;
    int CeilWithEpsilon(float value) noexcept;
    int ModBlockSize(int value) noexcept;
    int FloorToBlockSize(int value) noexcept;
    int CeilToBlockSize(int value) noexcept;
    int ComputeChannelCountFromMask(uint8 mask) noexcept;
    float Lerp(float a, float b, float t) noexcept;
    double Lerp(double a, double b, double t) noexcept;
    CompressionEngine::ZCE_AABB operator|(const CompressionEngine::ZCE_AABB& a, const CompressionEngine::ZCE_AABB& b) noexcept;
    bool IsEmpty(const CompressionEngine::ZCE_AABB& a) noexcept;
    uint32_t CountBits(uint32_t x) noexcept;

    template <class T>
    inline constexpr void NormalizeRange(T* data, const size_t size, const T minValue, const T maxValue, const T newMinValue,
                                         const T newMaxValue) noexcept
    {
        for (size_t i = 0; i < size; i++)
        {
            data[i] = (data[i] - minValue) / (maxValue - minValue) * (newMaxValue - newMinValue) + newMinValue;
        }
    };
} // namespace Zibra::OpenVDBSupport::MathHelpers