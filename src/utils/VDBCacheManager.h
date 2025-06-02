#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <openvdb/openvdb.h>

namespace Zibra::Utils
{
    class VDBCacheManager
    {
    public:
        static VDBCacheManager& GetInstance();
        
        // Get or create cached VDB file for given source and frame
        std::string GetOrCreateVDBCache(const std::string& sourceFile, int frameIndex, const openvdb::GridPtrVec& vdbGrids);
        
        // Register node to track its cache files
        void RegisterNode(void* nodePtr, const std::string& nodeId);
        
        // Unregister node and cleanup its cache files
        void UnregisterNode(void* nodePtr);
        
        // Update frame for a node (cleanup old frame cache if needed)
        void UpdateNodeFrame(void* nodePtr, const std::string& sourceFile, int frameIndex);
        
        // Cleanup all temporary VDB files
        void CleanupAllTempFiles();
        
        // Check if cache file exists and is valid
        bool IsCacheValid(const std::string& sourceFile, int frameIndex) const;
        
    private:
        VDBCacheManager() = default;
        ~VDBCacheManager();
        
        // Generate cache file path
        std::string GenerateCacheFilePath(const std::string& sourceFile, int frameIndex) const;
        
        // Write VDB grids to file
        bool WriteVDBFile(const std::string& filePath, const openvdb::GridPtrVec& vdbGrids);
        
        // Remove specific cache file
        void RemoveCacheFile(const std::string& filePath);
        
        struct NodeCacheInfo
        {
            std::string nodeId;
            std::string currentSourceFile;
            int currentFrame = -1;
            std::unordered_set<std::string> managedFiles;
        };
        
        std::unordered_map<void*, NodeCacheInfo> m_nodeCache;
        std::unordered_set<std::string> m_allManagedFiles;
        mutable std::mutex m_mutex;
    };
}