#pragma once

#ifdef _WIN32
    #define ZIB_RESOLVER_API __declspec(dllexport)
#else
    #define ZIB_RESOLVER_API
#endif

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    ZIBRAVDBRESOLVER_RESOLVER
);

class ZIB_RESOLVER_API ZibraVDBResolver final : public ArResolver
{
public:
    ZibraVDBResolver();

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

    std::shared_ptr<ArAsset> _OpenAsset(
        const ArResolvedPath& resolvedPath) const final;

    std::shared_ptr<ArWritableAsset> _OpenAssetForWrite(
        const ArResolvedPath& resolvedPath,
        WriteMode writeMode) const final;
};

PXR_NAMESPACE_CLOSE_SCOPE