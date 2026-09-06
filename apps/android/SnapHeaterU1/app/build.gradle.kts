plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.alphastudio.snapheateru1"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.alphastudio.snapheateru1"
        minSdk = 26
        targetSdk = 35
        versionCode = 4
        val localPreview = providers.gradleProperty("localPreview").orNull == "true"
        versionName = if (localPreview) "0.9.9-preview" else "0.9.9"
        buildConfigField("boolean", "ENABLE_VISUAL_PREVIEW", localPreview.toString())
    }

    buildFeatures {
        buildConfig = true
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.material.icons)

    debugImplementation(libs.androidx.compose.ui.tooling)
}
