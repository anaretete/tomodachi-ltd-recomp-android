// ==============================================================================
// android/app/build.gradle.kts
// Application Module Build Configuration for tomodachi-ltd-recomp
// ==============================================================================

plugins {
    id("com.android.application")
}

android {
    namespace = "com.recomp.tomodachiltd"
    compileSdk = 34
    // ndkVersion will auto-detect from ANDROID_NDK_HOME or setup-ndk

    defaultConfig {
        applicationId = "com.recomp.tomodachiltd"
        minSdk = 29
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0-arm64"

        ndk {
            abiFilters.add("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_ARM_NEON=ON",
                    "-DANDROID_PLATFORM=android-29",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
                cppFlags("-std=c++20", "-O3", "-fvisibility=hidden")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
        debug {
            isDebuggable = true
            isJniDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
        jniLibs {
            useLegacyPackaging = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    // Pure NativeActivity APK - Zero heavyweight JVM dependencies required
}
