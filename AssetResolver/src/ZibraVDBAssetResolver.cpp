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
    std::map<std::string, std::string> parseResult;
    if (!Zibra::Helpers::ParseZibraVDBURI(assetPath, parseResult))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Failed to parse ZibraVDB URI: '%s'\n", assetPath.c_str());
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    std::string extractedPath = parseResult["path"] + "/" + parseResult["name"];
    int frame = 1;

    if (const auto frameIt = parseResult.find("frame"); frameIt != parseResult.end())
    {
        if (!Zibra::Helpers::TryParseInt(frameIt->second, frame))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBResolver::CreateIdentifier - Invalid frame parameter '%s'. Falling back to frame #1\n",
                     frameIt->second.c_str());
        }
    }

    if (TfIsRelativePath(extractedPath) && anchorAssetPath)
    {
        std::string anchorDir = TfGetPathName(anchorAssetPath);
        std::string resolvedPath = TfStringCatPaths(anchorDir, extractedPath);
        std::string normalizedPath = TfNormPath(resolvedPath);

        std::string resolvedURI = normalizedPath;
        if (frame != 0)
        {
            resolvedURI += "?frame=" + std::to_string(frame);
        }

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Resolved ZibraVDB URI to: '%s'\n", resolvedURI.c_str());
        return resolvedURI;
    }

    return TfNormPath(assetPath);
}

std::string ZibraVDBResolver::_CreateIdentifierForNewAsset(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    std::map<std::string, std::string> parseResult;
    if (!Zibra::Helpers::ParseZibraVDBURI(assetPath, parseResult))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset - Failed to parse ZibraVDB URI: '%s'\n", assetPath.c_str());
        return ArDefaultResolver().CreateIdentifierForNewAsset(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset('%s', '%s')\n", assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    return _CreateIdentifier(assetPath, anchorAssetPath);
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    std::map<std::string, std::string> parseResult;
    if (!Zibra::Helpers::ParseZibraVDBURI(assetPath, parseResult))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Failed to parse ZibraVDB URI: '%s'\n", assetPath.c_str());
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    std::string actualFilePath = parseResult["path"] + "/" + parseResult["name"];
    auto frameIt = parseResult.find("frame");
    if (frameIt == parseResult.end() || frameIt->second.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Missing or empty frame parameter\n");
        return ArDefaultResolver().Resolve(assetPath);
    }

    int frame = 0;
    if (!Zibra::Helpers::TryParseInt(frameIt->second, frame))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Invalid frame parameter '%s'\n", frameIt->second.c_str());
        return ArDefaultResolver().Resolve(assetPath);
    }

    if (!TfPathExists(actualFilePath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", actualFilePath.c_str());
        return {};
    }

    auto& decompressionHelper = Zibra::AssetResolver::DecompressionHelper::GetInstance();
    std::string decompressedPath = decompressionHelper.DecompressZibraVDBFile(actualFilePath, frame);
    if (decompressedPath.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager "
                 "initialization, frame availability, or compression format)\n",
                 actualFilePath.c_str());
        return {};
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
    return ArResolvedPath(decompressedPath);
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_ResolveForNewAsset('%s')\n", assetPath.c_str());
    return _Resolve(assetPath);
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