#pragma once

#include <string>

namespace Zibra
{
    class LicenseManager
    {
    public:
        // Must match list of products in ZibraVDB SDK
        // Or at least not exceed it
        enum class Product
        {
            Compression,
            Decompression,
            Count
        };

        // Listed in order of priority
        // When 2 different activation methods gave different status codes and failed
        // The one that is higher in this list will be reported
        enum class Status
        {
            OK,
            ValidationError,
            InvalidKeyFormat,
            NoLicense,
            LibraryError,
            Uninitialized,
        };

        // Listed in order of priority
        enum class ActivationType
        {
            Offline,
            LicenseServer,
            Online,
            // Must be last
            None
        };

        // Listed in order of priority
        enum class LicensePathType
        {
            EnvVar,
            UserPrefDir,
            HSite,
            HQRoot,
            // Must be last
            None
        };

        // Singleton
        static LicenseManager& GetInstance();

        bool IsAnyLicenseValid() const;
        Status GetLicenseStatus(Product product) const;
        int GetLicenseTier(Product product) const;
        const char* GetLicenseType(Product product) const;
        ActivationType GetActivationType() const;
        LicensePathType GetLicensePathType() const;
        const std::string& GetLicensePath() const;

        void CheckoutLicense();
        // Will NOT remove license from HSITE or HQROOT
        void RemoveLicense();

        std::string GetLicenseKey() const;
        std::string GetOfflineLicense() const;
        std::string GetLicenseServerAddress() const;

        void SetLicenseKey(const char* key);
        void SetOfflineLicense(const char* offlineLicense);
        void SetLicenseServer(const char* licenseServerAddress);

        void CopyLicenseFile(const std::string& destFolder);

        bool CheckLicense(Product product);
        const std::string& GetActivationError() const;

    private:
        static const char* const ms_DefaultLicenseKeyFileName;
        static const char* const ms_DefaultOfflineLicenseFileName;
        static const char* const ms_DefaultLicenseServerFileName;

        Status m_Status[size_t(Product::Count)] = {Status::Uninitialized, Status::Uninitialized};
        std::string m_ActivationError;
        ActivationType m_Type = ActivationType::None;
        LicensePathType m_LicensePathType = LicensePathType::None;
        std::string m_LicensePath;
        std::string m_LicenseKey;
        std::string m_OfflineLicense;
        std::string m_LicenseServerAddress;

        static std::string SanitizePath(const std::string& path);
        static std::string SanitizeKey(const std::string& key);
        static std::string SanitizeOfflineLicense(const std::string& offlineLicense);

        static std::string GetKeyPath(LicensePathType pathType);
        static std::string GetOfflineLicensePath(LicensePathType pathType);
        static std::string GetLicenseServerAddressPath(LicensePathType pathType);

        static std::string ReadKeyFromFile(const std::string& path);
        static std::string ReadOfflineLicenseFromFile(const std::string& path);
        static std::string ReadLicenseServerAddressFromFile(const std::string& path);

        bool IsLicenseValid(Product product) const;
        Status TryCheckoutLicense(ActivationType type, LicensePathType pathType);

        void SetStatusFromZibraVDBRuntime();
        void SetStatusForAllProducts(Status status);
    };
} // namespace Zibra