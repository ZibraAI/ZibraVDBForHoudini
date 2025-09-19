#include "PrecompiledHeader.h"

#include "ZibraVDBAssetResolver.h"

#include "DecompressionHelper.h"
#include "utils/Helpers.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDBRESOLVER_RESOLVER, "Print debug output during ZibraVDB path resolution");
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT, "Print debug output during ZibraVDB context creating and modification");
}

class ZibraVDBResolverContext
{
public:
    ZibraVDBResolverContext()
    {
        m_TmpDir = TfGetenv("HOUDINI_TEMP_DIR");
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg("ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
    }

    ZibraVDBResolverContext(const std::string& tmpdir)
        : m_TmpDir(tmpdir.empty() ? TfGetenv("HOUDINI_TEMP_DIR") : tmpdir)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg("ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
    }

    bool operator<(const ZibraVDBResolverContext& rhs) const
    {
        return m_TmpDir < rhs.m_TmpDir;
    }

    bool operator==(const ZibraVDBResolverContext& rhs) const
    {
        return m_TmpDir == rhs.m_TmpDir;
    }

    bool operator!=(const ZibraVDBResolverContext& rhs) const
    {
        return m_TmpDir != rhs.m_TmpDir;
    }

    const std::string& getTmpDir() const
    {
        return m_TmpDir;
    }

private:
    std::string m_TmpDir;
};

size_t hash_value(const ZibraVDBResolverContext& context)
{
    size_t hash = SYShash(context.getTmpDir());
    return hash;
}

AR_DECLARE_RESOLVER_CONTEXT(ZibraVDBResolverContext);

AR_DEFINE_RESOLVER(ZibraVDBResolver, ArResolver);

ZibraVDBResolver::ZibraVDBResolver()
{
    UT_Exit::addExitCallback(
        [](void*) {
            Zibra::AssetResolver::DecompressionHelper::CleanupAllDecompressorManagers();
            Zibra::AssetResolver::DecompressionHelper::CleanupAllDecompressedFilesStatic();
        },
        nullptr);
}

ZibraVDBResolver::~ZibraVDBResolver() = default;

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    std::unordered_map<std::string, std::string> parseResult;
    if (!Zibra::Helpers::ParseZibraVDBURI(assetPath, parseResult))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Failed to parse ZibraVDB URI: '%s'\n", assetPath.c_str());
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    std::string extractedPath = parseResult["path"] + "/" + parseResult["name"];
    auto frameIt = parseResult.find("frame");
    if (frameIt == parseResult.end() || frameIt->second.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Missing or empty frame parameter\n");
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    int frame = 0;
    if (!Zibra::Helpers::TryParseInt(frameIt->second, frame))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Invalid frame parameter '%s'.\n", frameIt->second.c_str());
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
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
    std::unordered_map<std::string, std::string> parseResult;
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
    std::unordered_map<std::string, std::string> parseResult;
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

    const auto* ctx = _GetCurrentContextObject<ZibraVDBResolverContext>();
    if (!ctx || ctx->getTmpDir().empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - No valid context or temp directory found\n");
        return {};
    }

    std::string tmpDir = ctx->getTmpDir();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Using temp directory: '%s'\n", tmpDir.c_str());

    std::string decompressedPath = Zibra::AssetResolver::DecompressionHelper::DecompressZibraVDBFile(actualFilePath, tmpDir, frame);
    if (decompressedPath.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager "
                 "initialization, frame availability, or compression format)\n",
                 actualFilePath.c_str());
        return ArResolvedPath();
    }

    Zibra::AssetResolver::DecompressionHelper::CleanupOldFiles(actualFilePath, decompressedPath);
    Zibra::AssetResolver::DecompressionHelper::AddDecompressedFile(actualFilePath, decompressedPath);

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

ArResolverContext ZibraVDBResolver::_CreateContextFromString(const std::string& contextStr) const
{
    return ArResolverContext(ZibraVDBResolverContext(contextStr));
}

ArResolverContext ZibraVDBResolver::_CreateDefaultContext() const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg("ZibraVDBResolver::_CreateDefaultContext - Creating default resolver context\n");
    return ArResolverContext(ZibraVDBResolverContext());
}

bool ZibraVDBResolver::_IsContextDependentPath(const std::string& path) const
{
    std::unordered_map<std::string, std::string> parseResult;
    return Zibra::Helpers::ParseZibraVDBURI(path, parseResult);
}

PXR_NAMESPACE_CLOSE_SCOPE