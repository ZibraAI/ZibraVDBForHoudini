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
    UT_Exit::addExitCallback(
        [](void*) {
            Zibra::AssetResolver::DecompressionHelper::GetInstance().Cleanup();
        },
        nullptr);
}

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    auto zibraURI = Zibra::Helpers::ParseZibraVDBURI(assetPath);
    
    // If not a ZibraVDB file, use default resolver
    if (!zibraURI.isZibraVDB)
    {
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    // If ZibraVDB file but malformatted, return empty (don't process)
    if (!zibraURI.isValid)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI is malformatted: '%s'\n", assetPath.c_str());
        return {};
    }

    if (TfIsRelativePath(zibraURI.filepath.string()) && anchorAssetPath)
    {
        std::string anchorDir = TfGetPathName(anchorAssetPath);
        std::string resolvedPath = TfStringCatPaths(anchorDir, zibraURI.filepath.string());
        std::string normalizedPath = TfNormPath(resolvedPath);

        std::string resolvedURI = normalizedPath;
        if (zibraURI.frame > 0)
        {
            resolvedURI += "?frame=" + std::to_string(zibraURI.frame);
        }

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Resolved ZibraVDB URI to: '%s'\n", resolvedURI.c_str());
        return resolvedURI;
    }

    return TfNormPath(assetPath);
}

std::string ZibraVDBResolver::_CreateIdentifierForNewAsset(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // ZibraVDB asset resolver only handles decompression of existing .zibravdb files.
    if (Zibra::Helpers::ParseZibraVDBURI(assetPath).isZibraVDB)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset creation.\n");
        return {};
    }

    return ArDefaultResolver().CreateIdentifierForNewAsset(assetPath, anchorAssetPath);
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    auto zibraURI = Zibra::Helpers::ParseZibraVDBURI(assetPath);
    if (!zibraURI.isZibraVDB)
    {
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    if (!zibraURI.isValid)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - ZibraVDB URI is malformatted: '%s'\n", assetPath.c_str());
        return {};
    }

    if (!TfPathExists(zibraURI.filepath.string()))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", zibraURI.filepath.c_str());
        return {};
    }

    if (zibraURI.frame <= 0)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Invalid frame parameter\n");
        return {};
    }

    auto& decompressionHelper = Zibra::AssetResolver::DecompressionHelper::GetInstance();
    std::string decompressedPath = decompressionHelper.DecompressZibraVDBFile(zibraURI.filepath.string(), zibraURI.frame);
    if (decompressedPath.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager "
                 "initialization, frame availability, or compression format)\n",
                 zibraURI.filepath.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
    return ArResolvedPath(decompressedPath);
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    // ZibraVDB asset resolver only handles decompression of existing .zibravdb files.
    if (Zibra::Helpers::ParseZibraVDBURI(assetPath).isZibraVDB)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_ResolveForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset creation.\n");
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