import com.android.build.gradle.internal.api.BaseVariantOutputImpl

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.serialization)
    id("property-delegate")
}

kotlin {
    jvmToolchain(17)
}

// get values from gradle or local properties
val qtTargetSdkVersion: String by gradleProperties
val qtTargetAbiList: String by gradleProperties
val outputBaseName: String by gradleProperties

android {
    namespace = "org.amnezia.vpn"

    buildFeatures {
        buildConfig = true
        viewBinding = true
    }

    androidResources {
        // don't compress Qt binary resources file
        noCompress += "rcc"
    }

    packaging {
        // compress .so binary libraries
        jniLibs.useLegacyPackaging = true
    }

    val abiList = qtTargetAbiList.split(",")

    defaultConfig {
        applicationId = "org.aresvpn.client"
        targetSdk = qtTargetSdkVersion.toInt()

        // keeps language resources for only the locales specified below
        resourceConfigurations += listOf("en", "ru", "b+zh+Hans")
        // ndk.abiFilters is only used for single-ABI builds; multi-ABI uses splits below
        if (abiList.size == 1) {
            ndk.abiFilters += abiList
        }
    }

    signingConfigs {
        register("release") {
            storeFile = providers.environmentVariable("QT_ANDROID_KEYSTORE_PATH").orNull?.let { file(it) }
            storePassword = providers.environmentVariable("QT_ANDROID_KEYSTORE_STORE_PASS").orNull
            keyAlias = providers.environmentVariable("QT_ANDROID_KEYSTORE_ALIAS").orNull
            keyPassword = providers.environmentVariable("QT_ANDROID_KEYSTORE_STORE_PASS").orNull
        }
    }

    buildTypes {
        release {
            // exclude coroutine debug resource from release build
            packaging {
                resources.excludes += "DebugProbesKt.bin"
            }
            signingConfig = signingConfigs["release"]
        }

        create("fdroid") {
            initWith(getByName("release"))
            signingConfig = null
            matchingFallbacks += "release"
        }
    }

    flavorDimensions += "billing"

    productFlavors {
        create("oss") {
            dimension = "billing"
            buildConfigField("boolean", "IS_PLAY_BUILD", "false")
        }
        create("play") {
            dimension = "billing"
            buildConfigField("boolean", "IS_PLAY_BUILD", "true")
        }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            java.setSrcDirs(listOf("src"))
            res.setSrcDirs(listOf("res"))
            // androyddeployqt creates the folders below
            assets.setSrcDirs(listOf("assets"))
            jniLibs.setSrcDirs(listOf("libs"))
        }

        getByName("oss") {
            java.setSrcDirs(listOf("oss"))
        }

        getByName("play") {
            java.setSrcDirs(listOf("play"))
        }
    }

    splits {
        abi {
            // splits only make sense for multi-ABI builds; single-ABI uses ndk.abiFilters
            isEnable = abiList.size > 1
            reset()
            include(*abiList.toTypedArray())
            isUniversalApk = false
        }
    }

    // fix for Qt Creator to allow deploying the application to a device
    // to enable this fix, add the line outputBaseName=android-build to local.properties
    if (outputBaseName.isNotEmpty()) {
        applicationVariants.all {
            outputs.map { it as BaseVariantOutputImpl }
                .forEach { output ->
                    if (output.outputFileName.endsWith(".apk")) {
                        output.outputFileName = "$outputBaseName-${buildType.name}.apk"
                    }
                }
        }
    }

    // androiddeployqt expects:
    //   APK: build/outputs/apk/{base}-{buildType}[-unsigned].apk  (no flavor subdir)
    //   AAB: build/outputs/bundle/{buildType}/{base}-{buildType}.aab (no flavor subdir)
    // where {base} = outputBaseName (set by Qt Creator) or "android-build" (CI fallback).
    // Release APK gets -unsigned suffix (Qt cmake signs it); debug does not.
    // Copy only oss flavor to the flat output dir that androiddeployqt/Qt Creator expect.
    // Play flavor is built via android_play_apk/android_play_aab cmake targets and uses
    // its native Gradle output paths directly.
    applicationVariants.all {
        val flavorName = productFlavors.firstOrNull()?.name ?: ""
        val buildTypeName = buildType.name
        if (flavorName == "oss") {
            val base = outputBaseName.ifEmpty { "android-build" }
            val unsignedSuffix = if (buildTypeName == "release") "-unsigned" else ""

            packageApplicationProvider.configure {
                doLast {
                    val srcDir = layout.buildDirectory.dir("outputs/apk/oss/$buildTypeName").get().asFile
                    val dstDir = layout.buildDirectory.dir("outputs/apk").get().asFile
                    dstDir.mkdirs()
                    srcDir.listFiles()?.filter { it.name.endsWith(".apk") }?.forEach { apk ->
                        apk.copyTo(File(dstDir, "$base-$buildTypeName$unsignedSuffix.apk"), overwrite = true)
                    }
                }
            }

            val variantName = name
            tasks.named("bundle${variantName.replaceFirstChar { it.uppercase() }}") {
                doLast {
                    val srcDir = layout.buildDirectory.dir("outputs/bundle/$variantName").get().asFile
                    val dstDir = layout.buildDirectory.dir("outputs/bundle/$buildTypeName").get().asFile
                    dstDir.mkdirs()
                    srcDir.listFiles()?.filter { it.name.endsWith(".aab") }?.forEach { aab ->
                        aab.copyTo(File(dstDir, "$base-$buildTypeName.aab"), overwrite = true)
                    }
                }
            }
        }
    }

    lint {
        disable += "InvalidFragmentVersionForActivityResult"
    }
}

dependencies {
    implementation(project(":qt"))
    implementation(project(":utils"))
    implementation(project(":protocolApi"))
    implementation(project(":wireguard"))
    implementation(project(":awg"))
    implementation(project(":openvpn"))
    implementation(project(":xray"))
    implementation(libs.androidx.core)
    implementation(libs.androidx.activity)
    implementation(libs.androidx.fragment)
    implementation(libs.kotlinx.coroutines)
    implementation(libs.kotlinx.serialization.protobuf)
    // ML KIT AND THE CAMERA STACK ARE GONE (AresProject #D178, #D187).
    //
    // `com.google.mlkit:barcode-scanning` is closed-source and not a system library, so a GPL-3
    // binary that links it cannot be conveyed - #D178 makes "no Android binary while ML Kit is
    // linked" a release condition rather than a preference. It existed for ONE feature: reading a
    // configuration out of a QR code.
    //
    // Under #D187 that feature is unreachable, and this was measured rather than assumed. The
    // chain is PageSetupWizardConfigSource -> PageSetupWizardQrReader ->
    // android_controller.cpp::startQrCodeReader -> CameraActivity -> ML Kit, and the config-source
    // chooser has exactly two callers - ConnectionTypeSelectionDrawer and PageSetupWizardStart -
    // NEITHER of which is referenced by anything any more: the drawer by nothing at all, the start
    // page by nothing since #D182 made our login the first screen. The tab bar's "+" was the last
    // route and #D187 removed it.
    //
    // A rent arrives by signing in with id + password + idx. There is no configuration to scan.
    //
    // CAMERAX STAYS, and taking it out with ML Kit was an over-reach corrected by the build. The
    // licence condition #D178 names is ML Kit alone - closed-source, not a system library; CameraX
    // is Apache-2.0 and carries no such problem. And `AmneziaApplication.kt` implements
    // `CameraXConfig.Provider`, so removing the dependency broke the Kotlin compile of a file that
    // has nothing to do with the QR feature. Dropping a dependency because it is NEAR the one that
    // must go is how a fork acquires edits it cannot justify at merge time (#D177 rule 3).
    implementation(libs.bundles.androidx.camera)
    implementation(libs.androidx.datastore)
    implementation(libs.androidx.biometric)

    playImplementation(project(":billing"))
}

fun DependencyHandler.playImplementation(dependency: Any): Dependency? =
    add("playImplementation", dependency)
