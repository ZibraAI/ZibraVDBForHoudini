#pragma once

#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace Zibra::CE::Addons::FileManagement
{
    template <typename CharType>
    inline std::basic_string_view<CharType> GetZibraVDBFileExtension();

    template <>
    inline std::basic_string_view<char> GetZibraVDBFileExtension()
    {
        return ".VDB";
    }

    template <>
    inline std::basic_string_view<wchar_t> GetZibraVDBFileExtension()
    {
        return L".VDB";
    }

    template <typename CharType>
    inline bool IsDigit(CharType chr);

    template <>
    inline bool IsDigit<char>(char chr)
    {
        return std::isdigit(static_cast<unsigned char>(chr)) != 0;
    }

    template <>
    inline bool IsDigit<wchar_t>(wchar_t chr)
    {
        return std::iswdigit(chr) != 0;
    }

    inline std::vector<std::filesystem::path> CalculateFileList(const std::filesystem::path::string_type& inputMask)
    {
        std::vector<std::filesystem::path> files;

        std::filesystem::path maskPath = inputMask;
        std::filesystem::path folderPath = maskPath.parent_path();
        std::filesystem::path::string_type mask = maskPath.filename().native();

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

                if (fileName.native().find(mask) != 0)
                {
                    continue;
                }

                auto tail = fileName.stem().native().substr(mask.size());
                bool tailIsDigitsOnly = std::all_of(tail.begin(), tail.end(), IsDigit<std::filesystem::path::string_type::value_type>);
                if (!tailIsDigitsOnly)
                {
                    continue;
                }

                std::filesystem::path::string_type fileNameStr = fileName.native();
                std::filesystem::path::string_type fileNameStrUpper = fileNameStr;
                std::transform(fileNameStrUpper.begin(), fileNameStrUpper.end(), fileNameStrUpper.begin(), ::toupper);

                if (fileNameStrUpper.substr(fileNameStrUpper.size() -
                                            GetZibraVDBFileExtension<std::filesystem::path::value_type>().size()) !=
                    GetZibraVDBFileExtension<std::filesystem::path::value_type>())
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
            std::filesystem::path::string_type aStr = a.filename().native().substr(mask.size());
            std::filesystem::path::string_type bStr = b.filename().native().substr(mask.size());
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
