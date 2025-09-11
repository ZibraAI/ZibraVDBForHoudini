#pragma once

#include <memory>
#include <string>

#include "ZibraVDBDecompressionManager.h"
#include "pxr/pxr.h"
#include "pxr/base/tf/debug.h"
#include "pxr/usd/ar/resolver.h"

#define ZIBRAVDBRESOLVER_API

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    ZIBRAVDBRESOLVER_RESOLVER,
    ZIBRAVDBRESOLVER_RESOLVER_CONTEXT
);

class ZIBRAVDBRESOLVER_API ZibraVDBResolver : public ArResolver
{
public:
    ZibraVDBResolver();
    virtual ~ZibraVDBResolver();

    static void CleanupAllDecompressedFilesStatic();
    static void CleanupAllDecompressorManagers();
protected:
    std::string _CreateIdentifier(
        const std::string& assetPath,
        const ArResolvedPath& anchorAssetPath) const final;

    std::string _CreateIdentifierForNewAsset(
        const std::string& assetPath,
        const ArResolvedPath& anchorAssetPath) const final;

    ArResolvedPath _Resolve(
        const std::string& assetPath) const final;

    ArResolvedPath _ResolveForNewAsset(
        const std::string& assetPath) const final;

    ArResolverContext _CreateContextFromString(
        const std::string& contextStr) const final;

    bool _IsContextDependentPath(
        const std::string &assetPath) const final;


    std::shared_ptr<ArAsset> _OpenAsset(
        const ArResolvedPath& resolvedPath) const final;

    std::shared_ptr<ArWritableAsset> _OpenAssetForWrite(
        const ArResolvedPath& resolvedPath,
        WriteMode writeMode) const final;

private:

    bool IsZibraVDBPath(const std::string& path) const;
    std::string ParseZibraVDBURI(const std::string& uri, int& frame) const;

    mutable ZibraVDBDecompressionManager m_decompressionManager;
};

class ZIBRAVDBRESOLVER_API ZibraVDBResolver_Linux final : public ZibraVDBResolver
{
public:
    ZibraVDBResolver_Linux() = default;
    virtual ~ZibraVDBResolver_Linux() = default;
};

class ZIBRAVDBRESOLVER_API ZibraVDBResolver_Mac final : public ZibraVDBResolver
{
public:
    ZibraVDBResolver_Mac() = default;
    virtual ~ZibraVDBResolver_Mac() = default;
};

class ZIBRAVDBRESOLVER_API ZibraVDBResolver_Win final : public ZibraVDBResolver
{
public:
    ZibraVDBResolver_Win() = default;
    virtual ~ZibraVDBResolver_Win() = default;
};

#ifdef ZIB_TARGET_OS_WIN
    #define ZIBRAVDB_RESOLVER_CLASS_NAME ZibraVDBResolver_Win
#elif defined(ZIB_TARGET_OS_MAC)
    #define ZIBRAVDB_RESOLVER_CLASS_NAME ZibraVDBResolver_Mac
#else
    #define ZIBRAVDB_RESOLVER_CLASS_NAME ZibraVDBResolver_Linux
#endif

PXR_NAMESPACE_CLOSE_SCOPE