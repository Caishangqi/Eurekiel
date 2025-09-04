#include "ImageLoader.hpp"
#include "../../Core/ErrorWarningAssert.hpp"
#include "../../Core/StringUtils.hpp"
#include "../ResourceMetadata.hpp"

// Include stb_image for loading from memory
#define STB_IMAGE_IMPLEMENTATION_INCLUDED  // Prevent double definition since Image.cpp already includes it
#include "ThirdParty/stb/stb_image.h"

namespace enigma::resource
{
    ImageLoader::ImageLoader()
    {
        // Initialize supported extensions
        m_supportedExtensions = {
            ".png", ".jpg", ".jpeg", ".bmp", ".tga"
        };
    }

    ResourcePtr ImageLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
    {
        if (data.empty())
        {
            ERROR_AND_DIE(Stringf("ImageLoader: No data provided for image resource '%s'",
                metadata.location.ToString().c_str()));
        }

        try
        {
            // Load image from memory data
            std::unique_ptr<Image> image = LoadImageFromData(data, metadata.location.ToString());

            if (!image)
            {
                ERROR_AND_DIE(Stringf("ImageLoader: Failed to load image from data for resource '%s'",
                    metadata.location.ToString().c_str()));
            }

            // Create ImageResource
            auto imageResource = std::make_shared<ImageResource>(metadata, std::move(image));

            return std::static_pointer_cast<IResource>(imageResource);
        }
        catch (const std::exception& e)
        {
            ERROR_AND_DIE(Stringf("ImageLoader: Exception loading image '%s': %s",
                metadata.location.ToString().c_str(), e.what()));
        }
    }

    std::set<std::string> ImageLoader::GetSupportedExtensions() const
    {
        return m_supportedExtensions;
    }

    std::string ImageLoader::GetLoaderName() const
    {
        return "ImageLoader";
    }

    int ImageLoader::GetPriority() const
    {
        // High priority for image files
        return 100;
    }

    bool ImageLoader::CanLoad(const ResourceMetadata& metadata) const
    {
        const std::string extension = metadata.GetFileExtension();
        return IsImageFormat(extension);
    }

    bool ImageLoader::IsImageFormat(const std::string& extension) const
    {
        std::string lowerExt = extension;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return m_supportedExtensions.find(lowerExt) != m_supportedExtensions.end();
    }

    std::unique_ptr<Image> ImageLoader::LoadImageFromData(const std::vector<uint8_t>& data, const std::string& debugName) const
    {
        if (data.empty())
        {
            return nullptr;
        }

        // Use stb_image to load from memory
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1); // Match existing Image class behavior

        unsigned char* pixels = stbi_load_from_memory(
            data.data(),
            static_cast<int>(data.size()),
            &width,
            &height,
            &channels,
            0 // Desired channels (0 = original)
        );

        if (!pixels)
        {
            const char* error = stbi_failure_reason();
            ERROR_AND_DIE(Stringf("ImageLoader: stb_image failed to load '%s': %s",
                debugName.c_str(), error ? error : "Unknown error"));
        }

        try
        {
            // Create Image object with loaded dimensions
            auto image = std::make_unique<Image>(IntVec2(width, height), Rgba8::WHITE);

            // Copy pixel data into Image format
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int pixelIndex = (y * width + x) * channels;

                    unsigned char r = pixels[pixelIndex + 0];
                    unsigned char g = channels > 1 ? pixels[pixelIndex + 1] : r;
                    unsigned char b = channels > 2 ? pixels[pixelIndex + 2] : r;
                    unsigned char a = channels > 3 ? pixels[pixelIndex + 3] : 255;

                    image->SetTexelColor(IntVec2(x, y), Rgba8(r, g, b, a));
                }
            }

            // Clean up stb_image memory
            stbi_image_free(pixels);

            return image;
        }
        catch (...)
        {
            // Clean up stb_image memory on exception
            stbi_image_free(pixels);
            throw;
        }
    }
}
