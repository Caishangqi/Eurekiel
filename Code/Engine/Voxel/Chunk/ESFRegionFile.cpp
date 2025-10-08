#include "ESFRegionFile.hpp"
#include "RLECompressor.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace enigma::voxel
{
    // Static members initialization
    std::unique_ptr<ESFRegionFile> ChunkFileManager::s_cachedRegionFile = nullptr;
    int32_t                        ChunkFileManager::s_cachedRegionX    = INT32_MAX;
    int32_t                        ChunkFileManager::s_cachedRegionY    = INT32_MAX;

    // ESFRegionFile implementation
    ESFRegionFile::ESFRegionFile(const std::string& filePath, int32_t regionX, int32_t regionY)
        : m_filePath(filePath)
          , m_regionX(regionX)
          , m_regionY(regionY)
          , m_isValid(false)
          , m_lastError(ESFError::None)
          , m_isDirty(false)
    {
        // Initialize index
        std::memset(m_index, 0, sizeof(m_index));

        // Try to open existing file or create new one
        if (std::filesystem::exists(filePath))
        {
            m_file.open(filePath, std::ios::binary | std::ios::in | std::ios::out);
            if (m_file.is_open())
            {
                m_lastError = LoadHeader();
                if (m_lastError == ESFError::None)
                {
                    m_lastError = LoadIndex();
                    if (m_lastError == ESFError::None)
                    {
                        m_isValid = true;
                    }
                }
            }
            else
            {
                m_lastError = ESFError::FileIOError;
            }
        }
        else
        {
            // Create new region file
            m_lastError = CreateNewFile();
            m_isValid   = (m_lastError == ESFError::None);
        }
    }

    ESFRegionFile::~ESFRegionFile()
    {
        Close();
    }

    ESFError ESFRegionFile::ReadChunk(int32_t  localChunkX, int32_t localChunkY,
                                      uint8_t* outputData, size_t   outputSize, size_t& bytesRead)
    {
        bytesRead = 0;

        if (!m_isValid || !ValidateCoordinates(localChunkX, localChunkY))
            return ESFError::InvalidCoordinates;

        size_t                    chunkIndex = GetChunkIndex(localChunkX, localChunkY);
        const ESFChunkIndexEntry& entry      = m_index[chunkIndex];

        if (entry.offset == 0 || entry.size == 0)
            return ESFError::ChunkNotFound;

        // Seek to chunk data
        m_file.seekg(entry.offset);
        if (!m_file.good())
            return ESFError::FileIOError;

        // Read chunk data header
        ESFChunkDataHeader chunkHeader;
        m_file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ESFChunkDataHeader));
        if (!m_file.good() || !chunkHeader.IsValid())
            return ESFError::CorruptedHeader;

        // Read compressed data
        std::vector<uint8_t> compressedData(chunkHeader.compressedSize);
        m_file.read(reinterpret_cast<char*>(compressedData.data()), chunkHeader.compressedSize);
        if (!m_file.good())
            return ESFError::FileIOError;

        // Decompress if needed
        if (chunkHeader.compressionType == 0) // RLE compression
        {
            std::vector<uint32_t> decompressed(chunkHeader.uncompressedSize / sizeof(uint32_t));
            size_t                decompressedCount = RLECompressor::Decompress(
                compressedData.data(), compressedData.size(),
                decompressed.data(), decompressed.size()
            );

            if (decompressedCount == 0)
                return ESFError::CompressionError;

            size_t copySize = std::min(outputSize, static_cast<size_t>(chunkHeader.uncompressedSize));
            std::memcpy(outputData, decompressed.data(), copySize);
            bytesRead = copySize;
        }
        else
        {
            // No compression, copy directly
            size_t copySize = std::min(outputSize, static_cast<size_t>(chunkHeader.compressedSize));
            std::memcpy(outputData, compressedData.data(), copySize);
            bytesRead = copySize;
        }

        return ESFError::None;
    }

    ESFError ESFRegionFile::WriteChunk(int32_t        localChunkX, int32_t localChunkY,
                                       const uint8_t* inputData, size_t    inputSize)
    {
        if (!m_isValid || !ValidateCoordinates(localChunkX, localChunkY))
            return ESFError::InvalidCoordinates;

        if (!inputData || inputSize == 0)
            return ESFError::InvalidChunkIndex;

        // Prepare data for compression
        const uint32_t* blockData  = reinterpret_cast<const uint32_t*>(inputData);
        size_t          blockCount = inputSize / sizeof(uint32_t);

        std::vector<uint8_t> compressedData(RLECompressor::CalculateMaxCompressedSize(blockCount));
        size_t               compressedSize  = 0;
        uint8_t              compressionType = 0;

        // Try RLE compression
        if (RLECompressor::ShouldCompress(blockData, blockCount))
        {
            compressedSize = RLECompressor::Compress(
                blockData, blockCount,
                compressedData.data(), compressedData.size()
            );
            compressionType = 0; // RLE
        }

        // If compression failed or wasn't beneficial, store uncompressed
        if (compressedSize == 0)
        {
            compressedData.resize(inputSize);
            std::memcpy(compressedData.data(), inputData, inputSize);
            compressedSize  = inputSize;
            compressionType = 255; // No compression
        }

        // Create chunk data header
        ESFChunkDataHeader chunkHeader;
        chunkHeader.uncompressedSize = static_cast<uint32_t>(inputSize);
        chunkHeader.compressedSize   = static_cast<uint32_t>(compressedSize);
        chunkHeader.compressionType  = compressionType;

        // Find write position (append to end for now)
        m_file.seekp(0, std::ios::end);
        size_t writeOffset = m_file.tellp();

        // Write chunk data header
        m_file.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ESFChunkDataHeader));
        if (!m_file.good())
            return ESFError::FileIOError;

        // Write compressed data
        m_file.write(reinterpret_cast<const char*>(compressedData.data()), compressedSize);
        if (!m_file.good())
            return ESFError::FileIOError;

        // Update index
        size_t              chunkIndex = GetChunkIndex(localChunkX, localChunkY);
        ESFChunkIndexEntry& entry      = m_index[chunkIndex];

        // If this is a new chunk, increment count
        if (entry.offset == 0)
        {
            m_header.chunkCount++;
        }

        entry.offset = static_cast<uint32_t>(writeOffset);
        entry.size   = static_cast<uint32_t>(sizeof(ESFChunkDataHeader) + compressedSize);

        m_isDirty = true;
        return ESFError::None;
    }

    bool ESFRegionFile::HasChunk(int32_t localChunkX, int32_t localChunkY) const
    {
        if (!ValidateCoordinates(localChunkX, localChunkY))
            return false;

        size_t chunkIndex = GetChunkIndex(localChunkX, localChunkY);
        return m_index[chunkIndex].offset != 0;
    }

    ESFError ESFRegionFile::Flush()
    {
        if (!m_isDirty)
            return ESFError::None;

        ESFError error = SaveHeader();
        if (error != ESFError::None)
            return error;

        error = SaveIndex();
        if (error != ESFError::None)
            return error;

        m_file.flush();
        m_isDirty = false;
        return ESFError::None;
    }

    void ESFRegionFile::Close()
    {
        if (m_file.is_open())
        {
            if (m_isDirty)
            {
                Flush(); // Try to save pending changes
            }
            m_file.close();
        }
        m_isValid = false;
    }

    ESFError ESFRegionFile::ValidateFile()
    {
        if (!m_isValid)
            return ESFError::FileIOError;

        // Basic header validation
        if (!m_header.IsValid())
            return ESFError::CorruptedHeader;

        // Check file size consistency
        m_file.seekg(0, std::ios::end);
        size_t fileSize = m_file.tellg();

        if (!ESFLayout::ValidateFileSize(fileSize, m_header.chunkCount))
            return ESFError::CorruptedHeader;

        return ESFError::None;
    }

    // Private methods
    ESFError ESFRegionFile::LoadHeader()
    {
        m_file.seekg(0);
        m_file.read(reinterpret_cast<char*>(&m_header), sizeof(ESFHeader));

        if (!m_file.good())
            return ESFError::FileIOError;

        if (!m_header.IsValid())
            return ESFError::CorruptedHeader;

        return ESFError::None;
    }

    ESFError ESFRegionFile::SaveHeader()
    {
        m_header.UpdateTimestamp();

        m_file.seekp(0);
        m_file.write(reinterpret_cast<const char*>(&m_header), sizeof(ESFHeader));

        return m_file.good() ? ESFError::None : ESFError::FileIOError;
    }

    ESFError ESFRegionFile::LoadIndex()
    {
        m_file.seekg(ESF_HEADER_SIZE);
        m_file.read(reinterpret_cast<char*>(m_index), ESF_INDEX_SIZE);

        return m_file.good() ? ESFError::None : ESFError::FileIOError;
    }

    ESFError ESFRegionFile::SaveIndex()
    {
        m_file.seekp(ESF_HEADER_SIZE);
        m_file.write(reinterpret_cast<const char*>(m_index), ESF_INDEX_SIZE);

        return m_file.good() ? ESFError::None : ESFError::FileIOError;
    }

    ESFError ESFRegionFile::CreateNewFile()
    {
        // Create directory if needed
        std::filesystem::path dirPath = std::filesystem::path(m_filePath).parent_path();
        if (!dirPath.empty())
        {
            std::filesystem::create_directories(dirPath);
        }

        // Create new file
        m_file.open(m_filePath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!m_file.is_open())
            return ESFError::FileIOError;

        // Initialize header
        m_header            = ESFHeader();
        m_header.chunkCount = 0;
        m_header.UpdateTimestamp();

        // Write initial header and index
        ESFError error = SaveHeader();
        if (error != ESFError::None)
            return error;

        error = SaveIndex();
        if (error != ESFError::None)
            return error;

        return ESFError::None;
    }

    size_t ESFRegionFile::GetChunkIndex(int32_t localChunkX, int32_t localChunkY) const
    {
        return ESFLayout::ChunkToIndex(localChunkX, localChunkY);
    }

    bool ESFRegionFile::ValidateCoordinates(int32_t localChunkX, int32_t localChunkY) const
    {
        return localChunkX >= 0 && localChunkX < ESF_REGION_SIZE &&
            localChunkY >= 0 && localChunkY < ESF_REGION_SIZE;
    }

    // ChunkFileManager implementation
    ESFError ChunkFileManager::SaveChunk(const std::string& worldPath, int32_t chunkX, int32_t chunkY,
                                         const uint8_t*     chunkData, size_t  dataSize)
    {
        int32_t regionX, regionY;
        ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

        ESFRegionFile* regionFile = GetOrCreateRegionFile(worldPath, regionX, regionY);
        if (!regionFile || !regionFile->IsValid())
            return ESFError::FileIOError;

        int32_t localX, localY;
        ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);

        ESFError error = regionFile->WriteChunk(localX, localY, chunkData, dataSize);
        if (error == ESFError::None)
        {
            error = regionFile->Flush();
        }

        return error;
    }

    ESFError ChunkFileManager::LoadChunk(const std::string& worldPath, int32_t chunkX, int32_t     chunkY,
                                         uint8_t*           outputData, size_t outputSize, size_t& bytesRead)
    {
        int32_t regionX, regionY;
        ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

        ESFRegionFile* regionFile = GetOrCreateRegionFile(worldPath, regionX, regionY);
        if (!regionFile || !regionFile->IsValid())
            return ESFError::FileIOError;

        int32_t localX, localY;
        ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);

        return regionFile->ReadChunk(localX, localY, outputData, outputSize, bytesRead);
    }

    bool ChunkFileManager::ChunkExists(const std::string& worldPath, int32_t chunkX, int32_t chunkY)
    {
        int32_t regionX, regionY;
        ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

        std::string regionPath = GetRegionFilePath(worldPath, regionX, regionY);
        if (!std::filesystem::exists(regionPath))
            return false;

        ESFRegionFile* regionFile = GetOrCreateRegionFile(worldPath, regionX, regionY);
        if (!regionFile || !regionFile->IsValid())
            return false;

        int32_t localX, localY;
        ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);

        return regionFile->HasChunk(localX, localY);
    }

    std::string ChunkFileManager::GetRegionFilePath(const std::string& worldPath, int32_t regionX, int32_t regionY)
    {
        std::string regionFileName = ESFLayout::GenerateRegionFileName(regionX, regionY);
        return worldPath + "/" + regionFileName;
    }

    ESFRegionFile* ChunkFileManager::GetOrCreateRegionFile(const std::string& worldPath,
                                                           int32_t            regionX, int32_t regionY)
    {
        // Simple caching: if we already have the right region file, use it
        if (s_cachedRegionFile && s_cachedRegionX == regionX && s_cachedRegionY == regionY)
        {
            return s_cachedRegionFile.get();
        }

        // Close current cached file and open new one
        s_cachedRegionFile.reset();

        std::string regionPath = GetRegionFilePath(worldPath, regionX, regionY);
        s_cachedRegionFile     = std::make_unique<ESFRegionFile>(regionPath, regionX, regionY);
        s_cachedRegionX        = regionX;
        s_cachedRegionY        = regionY;

        return s_cachedRegionFile.get();
    }

    void ChunkFileManager::CloseAllRegionFiles()
    {
        s_cachedRegionFile.reset();
        s_cachedRegionX = INT32_MAX;
        s_cachedRegionY = INT32_MAX;
    }
}
