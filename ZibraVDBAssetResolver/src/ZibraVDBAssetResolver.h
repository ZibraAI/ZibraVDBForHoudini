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

#ifdef ZIB_TARGET_OS_WIN
    #ifdef ZIBRAVDBRESOLVER_EXPORTS
    #define ZIBRAVDBRESOLVER_API __declspec(dllexport)
    #else
    #define ZIBRAVDBRESOLVER_API __declspec(dllimport)
    #endif
#else
    #define ZIBRAVDBRESOLVER_API
#endif

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

    static void _CleanupAllDecompressedFilesStatic();
    static void _CleanupAllDecompressorManagers();
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

    //    AR_API
    //    ArTimestamp _GetModificationTimestamp(
    //        const std::string& assetPath,
    //        const ArResolvedPath& resolvedPath) const final;

    std::shared_ptr<ArAsset> _OpenAsset(
        const ArResolvedPath& resolvedPath) const final;

    std::shared_ptr<ArWritableAsset> _OpenAssetForWrite(
        const ArResolvedPath& resolvedPath,
        WriteMode writeMode) const final;

private:
    static constexpr const char* ZIBRAVDB_EXTENSION = ".zibravdb";

    bool _IsZibraVDBPath(const std::string& path) const;
    std::string _ParseZibraVDBURI(const std::string& uri, int& frame) const;
    std::string _DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0) const;
    void _RestoreFileMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::MetaMap& fileMetadata) const;
    void _RestoreGridMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec& vdbGrids) const;

    bool CheckLicenseAndLoadLib() const;

    void _AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile) const;
    void _CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile) const;
    Zibra::Helpers::DecompressorManager* _GetOrCreateDecompressorManager(const std::string& compressedFile) const;

    static std::unordered_map<std::string, std::unordered_set<std::string>> s_globalDecompressedFilesDict;
    static std::mutex s_globalDecompressedFilesMutex;
    static std::unordered_map<std::string, std::unique_ptr<Zibra::Helpers::DecompressorManager>> s_decompressorManagers;
    static std::mutex s_decompressorManagersMutex;
};

PXR_NAMESPACE_CLOSE_SCOPE