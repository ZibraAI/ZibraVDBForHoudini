#pragma once

#include <filesystem>
#include <vector>

namespace Zibra::CE::Addons::FileManagement
{
    std::vector<std::filesystem::path> CalculateFileList(const std::string& inputMask)
    {
        std::vector<std::filesystem::path> files;

        std::filesystem::path maskPath = inputMask;
        std::filesystem::path folderPath = maskPath.parent_path();
        std::string mask = maskPath.filename().string();

        try
        {

            for (const auto& entry : std::filesystem::directory_iterator(folderPath))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                std::filesystem::path filePath = entry.path();
                std::filesystem::path fileName = filePath.filename();

                if (fileName.string().find(mask) != 0)
                {
                    continue;
                }

                auto tail = fileName.stem().string().substr(mask.size());
                bool tailIsDigitsOnly =
                    std::all_of(tail.begin(), tail.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
                if (!tailIsDigitsOnly)
                {
                    continue;
                }

                std::string fileNameStr = fileName.string();
                std::string fileNameStrUpper = fileNameStr;
                std::transform(fileNameStrUpper.begin(), fileNameStrUpper.end(), fileNameStrUpper.begin(), ::toupper);

                if (!fileNameStrUpper.ends_with(".VDB"))
                {
                    continue;
                }

                files.push_back(filePath);
            }
        }
        catch (std::filesystem::filesystem_error err)
        {
            return {};
        }

        std::sort(files.begin(), files.end(), [&](const std::filesystem::path& a, const std::filesystem::path& b) {
            std::string aStr = a.filename().string().substr(mask.size());
            std::string bStr = b.filename().string().substr(mask.size());
            int frameIndexA = std::stoi(aStr);
            int frameIndexB = std::stoi(bStr);
            return frameIndexA < frameIndexB;
        });

        if (files.empty())
        {
            if (std::filesystem::exists(maskPath))
            {
                files.push_back(maskPath);
            }
        }

        return files;
    }
} // namespace Zibra::CE::Addons::FileManagement
