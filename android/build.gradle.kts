// ==============================================================================
// android/build.gradle.kts
// Top-Level Build File for tomodachi-ltd-recomp Android Project
// ==============================================================================

plugins {
    id("com.android.application") version "8.4.1" apply false
}

tasks.register("clean", Delete::class) {
    delete(rootProject.layout.buildDirectory)
}
