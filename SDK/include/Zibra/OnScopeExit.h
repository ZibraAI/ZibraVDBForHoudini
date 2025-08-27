#pragma once

#include <utility>

#include "Helpers.h"

namespace Zibra
{
    template <typename T>
    class ScopeExitHelper
    {
    public:
        explicit ScopeExitHelper(T&& lambda)
            : m_Lambda(std::move(lambda))
        {
        }

        ScopeExitHelper(const ScopeExitHelper&) = delete;
        ScopeExitHelper& operator=(const ScopeExitHelper&) = delete;
        ScopeExitHelper(ScopeExitHelper&&) = delete;
        ScopeExitHelper& operator=(ScopeExitHelper&&) = delete;

        ~ScopeExitHelper()
        {
            m_Lambda();
        }

    private:
        T m_Lambda;
    };
} // namespace Zibra

#define ZIB_ON_SCOPE_EXIT(lambda) auto ZIB_CONCAT(zibScopeExit, __LINE__) = ::Zibra::ScopeExitHelper(lambda)
