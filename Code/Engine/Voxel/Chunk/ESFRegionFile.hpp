#pragma once
#include "ESFFormat.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <string>
#include <fstream>
#include <memory>

namespace enigma::voxel::chunk
{
    /**
     * @brief ESF Region File I/O Manager for chunk serialization
     *
     * Handles reading and writing of .esf region files containing 16x16 chunks.
     * Implements Assignment 2 ESF file format specification.
     */
    class ESFRegionFile
    {
    public:
        /**
         * @brief Open/create a region file for read/write operations
         */
        ESFRegionFile(const std::string& filePath, int32_t regionX, int32_t regionY);
        ~ESFRegionFile();

        /**
         * @brief Check if region file is valid and ready for I/O
         */
        bool IsValid() const { return m_isValid; }

        /**
         * @brief Get last error information
         */
        ESFError GetLastError() const { return m_lastError; }

        /**
         * @brief Read chunk data from region file
         */
        ESFError ReadChunk(int32_t  localChunkX, int32_t localChunkY,
                           uint8_t* outputData, size_t   outputSize, size_t& bytesRead);

        /**
         * @brief Write chunk data to region file
         */
        ESFError WriteChunk(int32_t        localChunkX, int32_t localChunkY,
                            const uint8_t* inputData, size_t    inputSize);

        /**
         * @brief Check if chunk exists in region file
         */
        bool HasChunk(int32_t localChunkX, int32_t localChunkY) const;

        /**
         * @brief Get chunk count in this region
         */
        uint32_t GetChunkCount() const { return m_header.chunkCount; }

        /**
         * @brief Flush all pending writes to disk
         */
        ESFError Flush();

        /**
         * @brief Close the region file
         */
        void Close();

        /**
         * @brief Validate region file integrity
         */
        ESFError ValidateFile();

    private:
        std::string  m_filePath;
        int32_t      m_regionX;
        int32_t      m_regionY;
        std::fstream m_file;
        bool         m_isValid;
        ESFError     m_lastError;
        bool         m_isDirty;

        ESFHeader          m_header;
        ESFChunkIndexEntry m_index[ESF_MAX_CHUNKS];

        // Internal I/O methods
        ESFError LoadHeader();
        ESFError SaveHeader();
        ESFError LoadIndex();
        ESFError SaveIndex();
        ESFError CreateNewFile();

        size_t GetChunkIndex(int32_t localChunkX, int32_t localChunkY) const;
        bool   ValidateCoordinates(int32_t localChunkX, int32_t localChunkY) const;
    };

    /**
     * @brief High-level chunk save/load utilities
     */
    class ChunkFileManager
    {
    public:
        /**
         * @brief Save chunk to appropriate region file
         */
        static ESFError SaveChunk(const std::string& worldPath, int32_t chunkX, int32_t chunkY,
                                  const uint8_t*     chunkData, size_t  dataSize);

        /**
         * @brief Load chunk from region file
         */
        static ESFError LoadChunk(const std::string& worldPath, int32_t chunkX, int32_t     chunkY,
                                  uint8_t*           outputData, size_t outputSize, size_t& bytesRead);

        /**
         * @brief Check if chunk exists in world
         */
        static bool ChunkExists(const std::string& worldPath, int32_t chunkX, int32_t chunkY);

        /**
         * @brief Generate region file path for world coordinates
         */
        static std::string GetRegionFilePath(const std::string& worldPath, int32_t regionX, int32_t regionZ);

    private:
        static ESFRegionFile* GetOrCreateRegionFile(const std::string& worldPath,
                                                    int32_t            regionX, int32_t regionZ);
        static void CloseAllRegionFiles();

        // Region file cache (simple static cache for demo)
        static std::unique_ptr<ESFRegionFile> s_cachedRegionFile;
        static int32_t                        s_cachedRegionX, s_cachedRegionY;
    };
}
