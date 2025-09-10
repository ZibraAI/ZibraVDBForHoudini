#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "SOP/DecompressorManager/DecompressorManager.h"
#include "pxr/pxr.h"
#include "pxr/base/tf/debug.h"

PXR_NAMESPACE_OPEN_SCOPE

class ZibraVDBDecompressionManager
{
public:
    ZibraVDBDecompressionManager();
    ~ZibraVDBDecompressionManager();

    std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0);
    void CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile);
    static void CleanupAllDecompressedFilesStatic();
    static void CleanupAllDecompressorManagers();
    void AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile);

private:
    Zibra::Helpers::DecompressorManager* GetOrCreateDecompressorManager(const std::string& compressedFile);
    void RestoreFileMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::MetaMap& fileMetadata);
    void RestoreGridMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec& vdbGrids);
    bool CheckLicenseAndLoadLib();

    static std::unordered_map<std::string, std::unordered_set<std::string>> g_DecompressedFilesDict;
    static std::mutex g_DecompressedFilesMutex;
    static std::unordered_map<std::string, std::unique_ptr<Zibra::Helpers::DecompressorManager>> g_DecompressorManagers;
    static std::mutex g_DecompressorManagersMutex;
};

PXR_NAMESPACE_CLOSE_SCOPE