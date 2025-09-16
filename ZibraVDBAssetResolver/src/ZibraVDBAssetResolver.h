#pragma once

#include "DecompressionHelper.h"

#ifdef _WIN32
    #define ZIB_RESOLVER_API __declspec(dllexport)
#else
    #define ZIB_RESOLVER_API
#endif

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    ZIBRAVDBRESOLVER_RESOLVER,
    ZIBRAVDBRESOLVER_RESOLVER_CONTEXT
);

class ZIB_RESOLVER_API ZibraVDBResolver final : public ArResolver
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

    ArResolverContext _CreateDefaultContext() const final;

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

    mutable Zibra::AssetResolver::DecompressionHelper m_decompressionManager;
};

PXR_NAMESPACE_CLOSE_SCOPE