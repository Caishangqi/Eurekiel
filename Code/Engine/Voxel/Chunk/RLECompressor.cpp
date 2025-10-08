#include "RLECompressor.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <algorithm>
#include <cstring>

namespace enigma::voxel
{
    size_t RLECompressor::Compress(const uint32_t* inputData, size_t  inputSize,
                                   uint8_t*        outputData, size_t outputSize)
    {
        if (!inputData || inputSize == 0 || !outputData || outputSize < sizeof(RLEHeader))
            return 0;

        // Check if compression is beneficial
        if (!ShouldCompress(inputData, inputSize))
        {
            // Store uncompressed data with special header
            RLEHeader header;
            header.magic        = 0x524C;
            header.version      = 1;
            header.originalSize = static_cast<uint32_t>(inputSize * sizeof(uint32_t));

            size_t totalSize = sizeof(RLEHeader) + header.originalSize;
            if (outputSize < totalSize)
                return 0;

            // Write header indicating uncompressed data
            std::memcpy(outputData, &header, sizeof(RLEHeader));

            // Write raw data
            std::memcpy(outputData + sizeof(RLEHeader), inputData, header.originalSize);

            return totalSize;
        }

        // Perform RLE compression
        std::vector<RLERunEntry> runs;
        runs.reserve(inputSize); // Worst case: every block is different

        size_t runCount = CompressRuns(inputData, inputSize, runs.data(), runs.capacity());
        if (runCount == 0)
            return 0;

        return WriteCompressedData(runs.data(), runCount, inputSize * sizeof(uint32_t),
                                   outputData, outputSize);
    }

    size_t RLECompressor::Decompress(const uint8_t* inputData, size_t  inputSize,
                                     uint32_t*      outputData, size_t outputSize)
    {
        if (!inputData || inputSize < sizeof(RLEHeader) || !outputData || outputSize == 0)
            return 0;

        RLEHeader header;
        if (!ReadHeader(inputData, inputSize, header))
            return 0;

        size_t expectedElements = header.originalSize / sizeof(uint32_t);
        if (expectedElements > outputSize)
            return 0;

        // Check if data was stored uncompressed
        size_t dataOffset = sizeof(RLEHeader);
        if (inputSize == sizeof(RLEHeader) + header.originalSize)
        {
            // Data is uncompressed, copy directly
            std::memcpy(outputData, inputData + dataOffset, header.originalSize);
            return expectedElements;
        }

        // Decompress RLE runs
        return ReadRuns(inputData, inputSize, dataOffset, outputData, outputSize);
    }

    size_t RLECompressor::CalculateMaxCompressedSize(size_t inputSize)
    {
        // Worst case: every block is different
        // Header + (inputSize * RLERunEntry)
        return sizeof(RLEHeader) + (inputSize * sizeof(RLERunEntry));
    }

    float RLECompressor::EstimateCompressionRatio(const uint32_t* inputData, size_t inputSize)
    {
        if (!inputData || inputSize == 0)
            return 1.0f;

        size_t   runCount  = 0;
        uint32_t lastValue = inputData[0];

        for (size_t i = 1; i < inputSize; ++i)
        {
            if (inputData[i] != lastValue)
            {
                runCount++;
                lastValue = inputData[i];
            }
        }
        runCount++; // Count the final run

        size_t originalSize   = inputSize * sizeof(uint32_t);
        size_t compressedSize = sizeof(RLEHeader) + (runCount * sizeof(RLERunEntry));

        return static_cast<float>(compressedSize) / static_cast<float>(originalSize);
    }

    bool RLECompressor::ShouldCompress(const uint32_t* inputData, size_t inputSize)
    {
        // Compress if we can achieve at least 10% reduction
        return EstimateCompressionRatio(inputData, inputSize) < 0.9f;
    }

    bool RLECompressor::ValidateCompressedData(const uint8_t* compressedData, size_t compressedSize,
                                               size_t         expectedOriginalSize)
    {
        if (!compressedData || compressedSize < sizeof(RLEHeader))
            return false;

        RLEHeader header;
        if (!ReadHeader(compressedData, compressedSize, header))
            return false;

        return header.originalSize == expectedOriginalSize;
    }

    // Private implementation methods
    size_t RLECompressor::CompressRuns(const uint32_t* inputData, size_t inputSize,
                                       RLERunEntry*    runs, size_t      maxRuns)
    {
        if (!inputData || inputSize == 0 || !runs || maxRuns == 0)
            return 0;

        size_t runCount = 0;
        size_t i        = 0;

        while (i < inputSize && runCount < maxRuns)
        {
            uint32_t currentValue = inputData[i];
            uint16_t runLength    = 1;

            // Count consecutive identical values
            while (i + runLength < inputSize &&
                inputData[i + runLength] == currentValue &&
                runLength < UINT16_MAX)
            {
                runLength++;
            }

            // Store the run
            runs[runCount].runLength    = runLength;
            runs[runCount].blockStateID = currentValue;
            runCount++;

            i += runLength;
        }

        return runCount;
    }

    size_t RLECompressor::WriteCompressedData(const RLERunEntry* runs, size_t           runCount,
                                              size_t             originalSize, uint8_t* outputData, size_t outputSize)
    {
        size_t totalSize = sizeof(RLEHeader) + (runCount * sizeof(RLERunEntry));
        if (outputSize < totalSize)
            return 0;

        // Write header
        RLEHeader header;
        header.magic        = 0x524C;
        header.version      = 1;
        header.originalSize = static_cast<uint32_t>(originalSize);

        std::memcpy(outputData, &header, sizeof(RLEHeader));

        // Write runs
        std::memcpy(outputData + sizeof(RLEHeader), runs, runCount * sizeof(RLERunEntry));

        return totalSize;
    }

    bool RLECompressor::ReadHeader(const uint8_t* inputData, size_t inputSize, RLEHeader& header)
    {
        if (!inputData || inputSize < sizeof(RLEHeader))
            return false;

        std::memcpy(&header, inputData, sizeof(RLEHeader));
        return header.IsValid();
    }

    size_t RLECompressor::ReadRuns(const uint8_t* inputData, size_t  inputSize, size_t dataOffset,
                                   uint32_t*      outputData, size_t outputSize)
    {
        if (dataOffset >= inputSize)
            return 0;

        size_t bytesRemaining = inputSize - dataOffset;
        size_t runCount       = bytesRemaining / sizeof(RLERunEntry);

        if (runCount == 0)
            return 0;

        const RLERunEntry* runs        = reinterpret_cast<const RLERunEntry*>(inputData + dataOffset);
        size_t             outputIndex = 0;

        for (size_t runIndex = 0; runIndex < runCount && outputIndex < outputSize; ++runIndex)
        {
            const RLERunEntry& run = runs[runIndex];

            if (!run.IsValid())
                return 0; // Invalid run data

            // Expand the run
            for (uint16_t i = 0; i < run.runLength && outputIndex < outputSize; ++i)
            {
                outputData[outputIndex++] = run.blockStateID;
            }
        }

        return outputIndex;
    }
}
