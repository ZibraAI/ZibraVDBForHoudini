#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "SOP/DecompressorManager/DecompressorManager.h"
#include "pxr/pxr.h"
#include "pxr/base/tf/debug.h"
#include "pxr/usd/ar/resolver.h"
#include "utils/Helpers.h"
#include "Globals.h"

#define ZIBRAVDBRESOLVER_API

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    ZIBRAVDBRESOLVER_RESOLVER,
    ZIBRAVDBRESOLVER_RESOLVER_CONTEXT
);

class ZIBRAVDBRESOLVER_API ZibraVDBResolver final : public ArResolver
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
    std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0) const;
    void RestoreFileMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::MetaMap& fileMetadata) const;
    void RestoreGridMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec& vdbGrids) const;

    bool CheckLicenseAndLoadLib() const;

    void AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile) const;
    void CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile) const;
    Zibra::Helpers::DecompressorManager* GetOrCreateDecompressorManager(const std::string& compressedFile) const;

    static std::unordered_map<std::string, std::unordered_set<std::string>> g_DecompressedFilesDict;
    static std::mutex g_DecompressedFilesMutex;
    static std::unordered_map<std::string, std::unique_ptr<Zibra::Helpers::DecompressorManager>> g_DecompressorManagers;
    static std::mutex g_DecompressorManagersMutex;
};

PXR_NAMESPACE_CLOSE_SCOPE