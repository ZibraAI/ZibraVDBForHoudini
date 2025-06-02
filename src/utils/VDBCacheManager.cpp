#include "PrecompiledHeader.h"
#include "VDBCacheManager.h"

#include <openvdb/io/File.h>
#include <filesystem>
#include <sstream>
#include <mutex>

namespace Zibra::Utils
{
    VDBCacheManager& VDBCacheManager::GetInstance()
    {
        static VDBCacheManager instance;
        return instance;
    }
    
    VDBCacheManager::~VDBCacheManager()
    {
        CleanupAllTempFiles();
    }
    
    std::string VDBCacheManager::GetOrCreateVDBCache(const std::string& sourceFile, int frameIndex, const openvdb::GridPtrVec& vdbGrids)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string cacheFilePath = GenerateCacheFilePath(sourceFile, frameIndex);
        
        if (IsCacheValid(sourceFile, frameIndex))
        {
            return cacheFilePath;
        }
        
        if (WriteVDBFile(cacheFilePath, vdbGrids))
        {
            m_allManagedFiles.insert(cacheFilePath);
            return cacheFilePath;
        }
        
        return "";
    }
    
    void VDBCacheManager::RegisterNode(void* nodePtr, const std::string& nodeId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        NodeCacheInfo& info = m_nodeCache[nodePtr];
        info.nodeId = nodeId;
    }
    
    void VDBCacheManager::UnregisterNode(void* nodePtr)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_nodeCache.find(nodePtr);
        if (it != m_nodeCache.end())
        {
            // Cleanup all files managed by this node
            for (const std::string& filePath : it->second.managedFiles)
            {
                RemoveCacheFile(filePath);
                m_allManagedFiles.erase(filePath);
            }
            
            m_nodeCache.erase(it);
        }
    }
    
    void VDBCacheManager::UpdateNodeFrame(void* nodePtr, const std::string& sourceFile, int frameIndex)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_nodeCache.find(nodePtr);
        if (it != m_nodeCache.end())
        {
            NodeCacheInfo& info = it->second;
            
            // If frame changed, cleanup old frame cache
            if (info.currentFrame != frameIndex || info.currentSourceFile != sourceFile)
            {
                // Remove old frame cache file
                if (info.currentFrame != -1 && !info.currentSourceFile.empty())
                {
                    std::string oldCacheFile = GenerateCacheFilePath(info.currentSourceFile, info.currentFrame);
                    if (info.managedFiles.count(oldCacheFile))
                    {
                        RemoveCacheFile(oldCacheFile);
                        info.managedFiles.erase(oldCacheFile);
                        m_allManagedFiles.erase(oldCacheFile);
                    }
                }
                
                info.currentSourceFile = sourceFile;
                info.currentFrame = frameIndex;
            }
            
            // Add current frame cache to managed files
            std::string currentCacheFile = GenerateCacheFilePath(sourceFile, frameIndex);
            info.managedFiles.insert(currentCacheFile);
        }
    }
    
    void VDBCacheManager::CleanupAllTempFiles()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const std::string& filePath : m_allManagedFiles)
        {
            RemoveCacheFile(filePath);
        }
        
        m_allManagedFiles.clear();
        m_nodeCache.clear();
    }
    
    bool VDBCacheManager::IsCacheValid(const std::string& sourceFile, int frameIndex) const
    {
        std::string cacheFilePath = GenerateCacheFilePath(sourceFile, frameIndex);
        
        if (!std::filesystem::exists(cacheFilePath))
            return false;
            
        if (std::filesystem::file_size(cacheFilePath) == 0)
            return false;
            
        if (std::filesystem::exists(sourceFile))
        {
            auto sourceTime = std::filesystem::last_write_time(sourceFile);
            auto cacheTime = std::filesystem::last_write_time(cacheFilePath);
            if (sourceTime > cacheTime)
                return false;
        }
        
        return true;
    }
    
    std::string VDBCacheManager::GenerateCacheFilePath(const std::string& sourceFile, int frameIndex) const
    {
        std::filesystem::path sourcePath(sourceFile);
        std::string folder = sourcePath.parent_path().string();
        std::string baseName = sourcePath.stem().string();
        
        std::ostringstream oss;
        oss << folder << "/" << baseName << "_frame_" << frameIndex << ".tmp.vdb";
        return oss.str();
    }
    
    bool VDBCacheManager::WriteVDBFile(const std::string& filePath, const openvdb::GridPtrVec& vdbGrids)
    {
        if (vdbGrids.empty())
            return false;
            
        try
        {
            std::filesystem::path outputPath(filePath);
            std::filesystem::create_directories(outputPath.parent_path());
            
            openvdb::io::File file(filePath);
            file.write(vdbGrids);
            file.close();
            
            return std::filesystem::exists(filePath) && std::filesystem::file_size(filePath) > 0;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
    
    void VDBCacheManager::RemoveCacheFile(const std::string& filePath)
    {
        try
        {
            if (std::filesystem::exists(filePath))
            {
                std::filesystem::remove(filePath);
            }
        }
        catch (const std::exception&)
        {
            // Ignore errors during cleanup
        }
    }
}