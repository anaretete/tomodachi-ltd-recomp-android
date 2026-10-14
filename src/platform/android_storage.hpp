// ==============================================================================
// src/platform/android_storage.hpp
// Android Storage Virtualization & RomFS Asset Provisioning
// ==============================================================================

#pragma once

#include <string>
#include <vector>
#include <android_native_app_glue.h>

namespace Tomodachi::Platform {

struct StoragePaths {
    std::string internal_data_dir;   // /data/user/0/<pkg>/files
    std::string external_data_dir;   // /storage/emulated/0/Android/data/<pkg>/files
    std::string romfs_dir;           // Active resolved RomFS path
    std::string save_dir;            // Active save directory
    std::string cache_dir;           // Shader and decompilation cache
};

class AndroidStorage {
public:
    static AndroidStorage& Instance();

    // Initialize paths from ANativeActivity
    void Initialize(struct android_app* app);

    // Verify whether game assets (RomFS, ExeFS) are provisioned
    bool VerifyAssets(std::string& outMissingPath);

    // Show interactive on-device installer UI to pick prod.keys and NSP/RomFS
    void ShowInstallerInterface();

    // Check presence of keys and game files
    bool HasProdKeys() const;
    bool HasGameData() const;

    // Show native Android modal dialog via JNI
    void ShowFatalAssetDialog(const std::string& targetPath);
    void ShowFatalDialog(const std::string& title, const std::string& message);

    // Accessors
    const StoragePaths& GetPaths() const { return m_paths; }
    const std::string& GetRomFsPath() const { return m_paths.romfs_dir; }
    const std::string& GetSavePath() const { return m_paths.save_dir; }
    const std::string& GetCachePath() const { return m_paths.cache_dir; }

    // Path resolution helper for virtual filesystem redirect
    std::string ResolveVirtualPath(const std::string& relativePath) const;

private:
    AndroidStorage() = default;
    ~AndroidStorage() = default;
    AndroidStorage(const AndroidStorage&) = delete;
    AndroidStorage& operator=(const AndroidStorage&) = delete;

    bool DirectoryExists(const std::string& path) const;
    bool EnsureDirectoryExists(const std::string& path);

    struct android_app* m_app = nullptr;
    StoragePaths m_paths;
    bool m_initialized = false;
};

} // namespace Tomodachi::Platform
