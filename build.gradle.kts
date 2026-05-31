// Top-level build file where you can add configuration options common to all sub-projects/modules.
plugins {
    // Upgraded to 8.3.2 to fix the NativeModelBuilder C++ crash
    id("com.android.application") version "8.3.2" apply false
    id("com.android.library") version "8.3.2" apply false
    id("org.jetbrains.kotlin.android") version "1.9.22" apply false
}