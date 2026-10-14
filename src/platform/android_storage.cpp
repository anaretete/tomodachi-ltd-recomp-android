// ==============================================================================
// src/platform/android_storage.cpp
// Android Storage Virtualization & RomFS Asset Provisioning Implementation
// ==============================================================================

#include "android_storage.hpp"

#include <android/log.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <jni.h>

#define LOG_TAG "TomodachiStorage"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Tomodachi::Platform {

AndroidStorage& AndroidStorage::Instance() {
    static AndroidStorage s_instance;
    return s_instance;
}

bool AndroidStorage::DirectoryExists(const std::string& path) const {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool AndroidStorage::EnsureDirectoryExists(const std::string& path) {
    if (DirectoryExists(path)) {
        return true;
    }
    // Recursive mkdir
    char tmp[1024];
    char* p = nullptr;
    size_t len = strnlen(path.c_str(), sizeof(tmp) - 1);
    if (len == 0 || len >= sizeof(tmp) - 1) {
        return false;
    }
    memcpy(tmp, path.c_str(), len);
    tmp[len] = '\0';

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0775);
            *p = '/';
        }
    }
    return mkdir(tmp, 0775) == 0 || DirectoryExists(path);
}

void AndroidStorage::Initialize(struct android_app* app) {
    m_app = app;
    if (!m_app || !m_app->activity) {
        LOGE("Failed to initialize AndroidStorage: invalid android_app context.");
        return;
    }

    // Retrieve internal and external sandbox paths
    if (m_app->activity->internalDataPath) {
        m_paths.internal_data_dir = m_app->activity->internalDataPath;
    } else {
        m_paths.internal_data_dir = "/data/data/com.recomp.tomodachiltd/files";
    }

    if (m_app->activity->externalDataPath) {
        m_paths.external_data_dir = m_app->activity->externalDataPath;
    } else {
        m_paths.external_data_dir = "/storage/emulated/0/Android/data/com.recomp.tomodachiltd/files";
    }

    // Set POSIX environment variables to virtualize desktop paths
    setenv("HOME", m_paths.internal_data_dir.c_str(), 1);
    setenv("XDG_DATA_HOME", m_paths.internal_data_dir.c_str(), 1);
    setenv("XDG_CONFIG_HOME", m_paths.internal_data_dir.c_str(), 1);
    setenv("TMPDIR", (m_paths.internal_data_dir + "/cache").c_str(), 1);

    m_paths.save_dir = m_paths.internal_data_dir + "/saves";
    m_paths.cache_dir = m_paths.internal_data_dir + "/cache";

    EnsureDirectoryExists(m_paths.save_dir);
    EnsureDirectoryExists(m_paths.cache_dir);

    // Resolve RomFS search order:
    // 1. External App Directory (/storage/emulated/0/Android/data/<pkg>/files/romfs)
    // 2. Internal App Directory (/data/data/<pkg>/files/romfs)
    // 3. Shared Public Fallback (/storage/emulated/0/TomodachiRecomp/romfs)
    std::string candidate_ext = m_paths.external_data_dir + "/romfs";
    std::string candidate_int = m_paths.internal_data_dir + "/romfs";
    std::string candidate_public = "/storage/emulated/0/TomodachiRecomp/romfs";

    if (DirectoryExists(candidate_ext)) {
        m_paths.romfs_dir = candidate_ext;
        LOGI("RomFS resolved from external app storage: %s", candidate_ext.c_str());
    } else if (DirectoryExists(candidate_int)) {
        m_paths.romfs_dir = candidate_int;
        LOGI("RomFS resolved from internal app storage: %s", candidate_int.c_str());
    } else if (DirectoryExists(candidate_public)) {
        m_paths.romfs_dir = candidate_public;
        LOGI("RomFS resolved from shared public directory: %s", candidate_public.c_str());
    } else {
        // Default target where the user will be directed to copy files
        m_paths.romfs_dir = candidate_ext;
        LOGW("No existing RomFS directory found. Target path will be: %s", candidate_ext.c_str());
    }

    m_initialized = true;
    LOGI("Storage initialized. Internal: %s, External: %s", 
         m_paths.internal_data_dir.c_str(), m_paths.external_data_dir.c_str());
}

bool AndroidStorage::VerifyAssets(std::string& outMissingPath) {
    if (!m_initialized) {
        outMissingPath = "Uninitialized Storage System";
        return false;
    }

    if (!DirectoryExists(m_paths.romfs_dir)) {
        outMissingPath = m_paths.romfs_dir;
        return false;
    }

    // Check directory emptiness: must contain at least one valid entry besides . and ..
    DIR* dir = opendir(m_paths.romfs_dir.c_str());
    if (!dir) {
        outMissingPath = m_paths.romfs_dir;
        return false;
    }

    struct dirent* entry = nullptr;
    int valid_entries = 0;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            valid_entries++;
            break;
        }
    }
    closedir(dir);

    if (valid_entries == 0) {
        outMissingPath = m_paths.romfs_dir + " (Directory is empty)";
        return false;
    }

    return true;
}

std::string AndroidStorage::ResolveVirtualPath(const std::string& relativePath) const {
    if (relativePath.rfind("romfs/", 0) == 0) {
        return m_paths.romfs_dir + "/" + relativePath.substr(6);
    }
    if (relativePath.rfind("save/", 0) == 0) {
        return m_paths.save_dir + "/" + relativePath.substr(5);
    }
    return m_paths.internal_data_dir + "/" + relativePath;
}

void AndroidStorage::ShowFatalAssetDialog(const std::string& targetPath) {
    std::string titleStr = "Missing Game Assets (RomFS)";
    std::string messageStr = 
        "Tomodachi Life Recomp requires dumped Nintendo Switch RomFS assets to launch.\n\n"
        "Please extract and place your 'romfs' directory at:\n" + 
        targetPath + "\n\n"
        "Then restart the application.";
    ShowFatalDialog(titleStr, messageStr);
}

void AndroidStorage::ShowFatalDialog(const std::string& title, const std::string& message) {
    if (!m_app || !m_app->activity || !m_app->activity->vm) {
        LOGE("Cannot show fatal dialog: JNI environment unavailable.");
        return;
    }

    JavaVM* vm = m_app->activity->vm;
    JNIEnv* env = nullptr;
    jint res = vm->AttachCurrentThread(&env, nullptr);
    if (res != JNI_OK || !env) {
        LOGE("Failed to attach JNI thread for fatal alert dialog.");
        return;
    }

    LOGE("Showing fatal dialog: [%s] %s", title.c_str(), message.c_str());

    jclass activityClass = env->FindClass("com/recomp/tomodachiltd/TomodachiActivity");
    if (activityClass) {
        jmethodID showModal = env->GetStaticMethodID(activityClass, "showModalDialog", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (showModal) {
            jstring jTitle = env->NewStringUTF(title.c_str());
            jstring jMessage = env->NewStringUTF(message.c_str());
            env->CallStaticVoidMethod(activityClass, showModal, jTitle, jMessage);
            env->DeleteLocalRef(jTitle);
            env->DeleteLocalRef(jMessage);
        }
        env->DeleteLocalRef(activityClass);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    vm->DetachCurrentThread();
}

} // namespace Tomodachi::Platform
