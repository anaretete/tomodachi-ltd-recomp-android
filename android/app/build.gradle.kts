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

    defaultConfig {
        applicationId = "com.recomp.tomodachiltd"
        minSdk = 29
        targetSdk = 34
        versionCode = 4
        versionName = "1.0.3"

        ndk {
            abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a", "x86_64", "x86"))
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_PLATFORM=android-29",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
                cppFlags("-std=c++20", "-O3")
            }
        }
    }

    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
            isUniversalApk = true
        }
    }

    signingConfigs {
        getByName("debug") {
            // Standard Android debug certificate
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
    implementation("androidx.documentfile:documentfile:1.0.1")
}
