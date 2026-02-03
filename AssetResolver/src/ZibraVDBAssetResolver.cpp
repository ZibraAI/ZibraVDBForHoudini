#include "PrecompiledHeader.h"

#include "ZibraVDBAssetResolver.h"

#include "decompression/DecompressionHelper.h"
#include "utils/Helpers.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDB_RESOLVER, "ZibraVDB asset resolver debugging");
}

AR_DEFINE_RESOLVER(ZibraVDBResolver, ArResolver);

ZibraVDBResolver::ZibraVDBResolver()
{
    UT_Exit::addExitCallback([](void*) {
        if (Zibra::AssetResolver::DecompressionHelper::HasInstance())
        {
            Zibra::AssetResolver::DecompressionHelper::GetInstance().Cleanup();
        }
    }, nullptr);
}

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    URI assetURI = URI(assetPath);
    if (!assetURI.isValid)
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Invalid URI: '%s'\n", assetPath.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDB_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB asset detected: '%s'\n", assetPath.c_str());

    auto frameIt = assetURI.queryParams.find("frame");
    if (frameIt == assetURI.queryParams.end())
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Missing mandatory 'frame' parameter for ZibraVDB file: '%s'\n",
                 assetPath.c_str());
        return {};
    }

    int frameIndex;
    if (!Zibra::Helpers::TryParseInt(frameIt->second, frameIndex))
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Invalid 'frame' parameter value for ZibraVDB file: '%s'\n",
                 frameIt->second.c_str());
        return {};
    }

    std::string filePath = assetURI.path;
    if (TfIsRelativePath(filePath) && anchorAssetPath)
    {
        std::string anchorDir = TfGetPathName(anchorAssetPath);
        std::string resolvedPath = TfStringCatPaths(anchorDir, filePath);
        filePath = TfNormPath(resolvedPath);
    }
    else
    {
        filePath = TfNormPath(filePath);
    }

    std::string resolvedURI = std::string(ZIB_ZIBRAVDB_SCHEME) + "://" + filePath + "?frame=" + frameIt->second;
    TF_DEBUG(ZIBRAVDB_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifier - Resolved ZibraVDB asset path to: '%s'\n", resolvedURI.c_str());
    return resolvedURI;
}

std::string ZibraVDBResolver::_CreateIdentifierForNewAsset(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    TF_DEBUG(ZIBRAVDB_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset "
             "creation: '%s'\n", assetPath.c_str());
    return {};
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    URI assetURI = URI(assetPath);
    if (!assetURI.isValid)
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Invalid URI: '%s'\n", assetPath.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDB_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    if (!TfPathExists(assetURI.path))
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER).Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", assetURI.path.c_str());
        return {};
    }

    auto frameIt = assetURI.queryParams.find("frame");
    if (frameIt == assetURI.queryParams.end())
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Missing mandatory 'frame' parameter for ZibraVDB file: '%s'\n", assetPath.c_str());
        return {};
    }

    int frameIndex;
    if (!Zibra::Helpers::TryParseInt(frameIt->second, frameIndex))
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Invalid 'frame' parameter value for ZibraVDB file: '%s'\n", frameIt->second.c_str());
        return {};
    }

    auto& decompressionHelper = Zibra::AssetResolver::DecompressionHelper::GetInstance();
    std::string decompressedPath = decompressionHelper.DecompressZibraVDBFile(assetURI.path, frameIndex);
    if (decompressedPath.empty())
    {
        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for frame %i in file '%s' (check temp directory permissions and "
                 "sequence frame range)\n",
                 frameIndex, assetURI.path.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDB_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
    return ArResolvedPath(decompressedPath);
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    TF_DEBUG(ZIBRAVDB_RESOLVER)
        .Msg("ZibraVDBResolver::_ResolveForNewAsset - ZibraVDB asset resolver only handles existing .zibravdb files, not asset "
             "creation: '%s'\n", assetPath.c_str());
    return {};
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