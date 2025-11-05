#include "PrecompiledHeader.h"

#include "ZibraVDBAssetResolver.h"

#include "decompression/DecompressionHelper.h"
#include "utils/Helpers.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDBRESOLVER_RESOLVER, "Print debug output during ZibraVDB path resolution");
}

AR_DEFINE_RESOLVER(ZibraVDBResolver, ArResolver);

ZibraVDBResolver::ZibraVDBResolver()
{
    UT_Exit::addExitCallback([](void*) { Zibra::AssetResolver::DecompressionHelper::GetInstance().Cleanup(); }, nullptr);
}

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    auto assetURI = URI(assetPath);
    if (!assetURI.isValid || !Zibra::Helpers::IsZibraVDBFile(assetURI))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Asset not handled by ZibraVDB resolver: '%s'\n", assetPath.c_str());
        return assetPath;
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB asset detected: '%s'\n", assetPath.c_str());

    if (TfIsRelativePath(assetURI.path.string()) && anchorAssetPath)
    {
        std::string anchorDir = TfGetPathName(anchorAssetPath);
        std::string resolvedPath = TfStringCatPaths(anchorDir, assetURI.path.string());
        std::string normalizedPath = TfNormPath(resolvedPath);

        std::string resolvedURI = normalizedPath;
        auto frameIt = assetURI.queryParams.find("frame");
        if (frameIt == assetURI.queryParams.end())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBResolver::CreateIdentifier - Missing mandatory 'frame' parameter for ZibraVDB file: '%s'\n",
                     assetPath.c_str());
            return {};
        }

        int frameIndex;
        if (!Zibra::Helpers::TryParseInt(frameIt->second, frameIndex))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBResolver::CreateIdentifier - Invalid 'frame' parameter value for ZibraVDB file: '%s'\n",
                     frameIt->second.c_str());
            return {};
        }

        resolvedURI += "?frame=" + frameIt->second;
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Resolved ZibraVDB asset path to: '%s'\n", resolvedURI.c_str());
        return resolvedURI;
    }

    return TfNormPath(assetPath);
}

std::string ZibraVDBResolver::_CreateIdentifierForNewAsset(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // ZibraVDB asset resolver only handles decompression of existing .zibravdb files.
    if (Zibra::Helpers::IsZibraVDBFile(URI(assetPath)))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset "
                 "creation.\n");
        return {};
    }

    return assetPath;
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    auto assetURI = URI(assetPath);
    if (!assetURI.isValid || !Zibra::Helpers::IsZibraVDBFile(assetURI))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Asset not handled by ZibraVDB resolver: '%s'\n", assetPath.c_str());
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    if (!TfPathExists(assetURI.path.string()))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", assetURI.path.c_str());
        return {};
    }

    auto frameIt = assetURI.queryParams.find("frame");
    if (frameIt == assetURI.queryParams.end())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Missing mandatory 'frame' parameter for ZibraVDB file: '%s'\n", assetPath.c_str());
        return {};
    }

    int frameIndex;
    if (!Zibra::Helpers::TryParseInt(frameIt->second, frameIndex))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Invalid 'frame' parameter value for ZibraVDB file: '%s'\n", frameIt->second.c_str());
        return {};
    }

    if (frameIndex <= 0)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Invalid frame parameter for ZibraVDB file: '%s', frame: %d\n", assetPath.c_str(),
                 frameIndex);
        return {};
    }

    auto& decompressionHelper = Zibra::AssetResolver::DecompressionHelper::GetInstance();
    std::string decompressedPath = decompressionHelper.DecompressZibraVDBFile(assetURI.path.string(), frameIndex);
    if (decompressedPath.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager "
                 "initialization, frame availability, or compression format)\n",
                 assetURI.path.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
    return ArResolvedPath(decompressedPath);
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    // ZibraVDB asset resolver only handles decompression of existing .zibravdb files.
    if (Zibra::Helpers::IsZibraVDBFile(URI(assetPath)))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_ResolveForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset "
                 "creation.\n");
        return {};
    }

    return ArDefaultResolver().ResolveForNewAsset(assetPath);
}

std::shared_ptr<ArAsset> ZibraVDBResolver::_OpenAsset(const ArResolvedPath& resolvedPath) const
{
    return ArFilesystemAsset::Open(resolvedPath);
}

std::shared_ptr<ArWritableAsset> ZibraVDBResolver::_OpenAssetForWrite(const ArResolvedPath& resolvedPath, WriteMode writeMode) const
{
    return ArFilesystemWritableAsset::Create(resolvedPath, writeMode);
}

PXR_NAMESPACE_CLOSE_SCOPE