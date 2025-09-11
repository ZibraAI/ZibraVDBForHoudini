#include "ZibraVDBAssetResolver.h"

#include <SYS/SYS_Hash.h>
#include <UT/UT_Exit.h>
#include <csignal>
#include <pxr/usd/ar/defaultResolver.h>
#include <string>

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/defineResolverContext.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"

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
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg(
            "ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
    }

    ZibraVDBResolverContext(const std::string &tmpdir)
        : m_TmpDir(tmpdir.empty() ? TfGetenv("HOUDINI_TEMP_DIR") : tmpdir)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg(
            "ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
    }

    bool operator<(const ZibraVDBResolverContext& rhs) const
    { return m_TmpDir < rhs.m_TmpDir; }

    bool operator==(const ZibraVDBResolverContext& rhs) const
    { return m_TmpDir == rhs.m_TmpDir; }

    bool operator!=(const ZibraVDBResolverContext& rhs) const
    { return m_TmpDir != rhs.m_TmpDir; }

    const std::string &getTmpDir() const
    { return m_TmpDir; }

private:
    std::string m_TmpDir;
};

size_t
hash_value(const ZibraVDBResolverContext& context)
{
    size_t hash = SYShash(context.getTmpDir());
    return hash;
}

AR_DECLARE_RESOLVER_CONTEXT(ZibraVDBResolverContext);

AR_DEFINE_RESOLVER(ZIBRAVDB_RESOLVER_CLASS_NAME, ArResolver);

ZibraVDBResolver::ZibraVDBResolver()
{
    UT_Exit::addExitCallback([](void*){
        CleanupAllDecompressorManagers();
        CleanupAllDecompressedFilesStatic();
    }, nullptr);
}

ZibraVDBResolver::~ZibraVDBResolver() = default;

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string extractedPath = ParseZibraVDBURI(assetPath, frame);

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
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifierForNewAsset(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset('%s', '%s')\n", assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    return _CreateIdentifier(assetPath, anchorAssetPath);
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string actualFilePath = ParseZibraVDBURI(assetPath, frame);

    if (!TfPathExists(actualFilePath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", actualFilePath.c_str());
        return {};
    }

    const auto* ctx = _GetCurrentContextObject<ZibraVDBResolverContext>();
    if (!ctx || ctx->getTmpDir().empty()) {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - No valid context or temp directory found\n");
        return {};
    }

    std::string tmpDir = ctx->getTmpDir();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_Resolve - Using temp directory: '%s'\n", tmpDir.c_str());

    std::string decompressedPath = m_decompressionManager.DecompressZibraVDBFile(actualFilePath, tmpDir, frame);
    if (!decompressedPath.empty())
    {
        m_decompressionManager.CleanupUnneededDecompressedFiles(actualFilePath, decompressedPath);
        m_decompressionManager.AddDecompressedFile(actualFilePath, decompressedPath);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
        return ArResolvedPath(decompressedPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager initialization, frame availability, or compression format)\n", actualFilePath.c_str());
    return ArResolvedPath();
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
    return IsZibraVDBPath(path);
}

bool ZibraVDBResolver::IsZibraVDBPath(const std::string& path) const
{
    return path.find(ZIB_ZIBRAVDB_EXT) != std::string::npos;
}

std::string ZibraVDBResolver::ParseZibraVDBURI(const std::string& uri, int& frame) const
{
    frame = 0;

    size_t questionMarkPos = uri.find('?');
    if (questionMarkPos == std::string::npos)
    {
        return uri;
    }

    if (uri.find('?', questionMarkPos + 1) != std::string::npos)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ParseZibraVDBURI - Multiple '?' characters found in URI: '%s'\n", uri.c_str());
        return uri.substr(0, questionMarkPos);
    }

    std::string path = uri.substr(0, questionMarkPos);
    std::string queryString = uri.substr(questionMarkPos + 1);

    std::string frameParam;
    bool foundFrame = false;

    size_t start = 0;
    size_t ampPos;
    do
    {
        ampPos = queryString.find('&', start);
        std::string param = (ampPos == std::string::npos) ? queryString.substr(start) : queryString.substr(start, ampPos - start);

        if (param.substr(0, 6) == "frame=")
        {
            std::string frameValue = param.substr(6);
            if (foundFrame)
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ParseZibraVDBURI - Multiple 'frame' parameters found, using last one: '%s'\n", frameValue.c_str());
            }
            frameParam = frameValue;
            foundFrame = true;
        }

        start = ampPos + 1;
    } while (ampPos != std::string::npos);

    if (foundFrame && !frameParam.empty())
    {
        frame = std::stoi(frameParam);
    }
    return path;
}

void ZibraVDBResolver::CleanupAllDecompressedFilesStatic()
{
    Zibra::AssetResolver::DecompressionHelper::CleanupAllDecompressedFilesStatic();
}

void ZibraVDBResolver::CleanupAllDecompressorManagers()
{
    Zibra::AssetResolver::DecompressionHelper::CleanupAllDecompressorManagers();
}

PXR_NAMESPACE_CLOSE_SCOPE