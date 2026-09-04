from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain
from conan.tools.files import copy, apply_conandata_patches, export_conandata_patches
from conan.tools.scm import Git
from conan.errors import ConanInvalidConfiguration

import os
from pathlib import Path

class OpenvpnPtAndroid(ConanFile):
    name = "openvpn-pt-android"
    version = "1.0.0"
    package_type = "shared-library"
    settings = "os", "arch", "build_type", "compiler"

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def build_requirements(self):
        self.tool_requires("swig/4.1.1")
        self.tool_requires("go/1.23.12")
        self.tool_requires("cmake/[>=3.4.1 <4]")

    def validate(self):
        if self.settings.os != "Android":
            raise ConanInvalidConfiguration(f"{self.name} only supports Android, got {self.settings.os}")

    # AresVPN Client - PINNED TO A COMMIT (AresProject #D191). Upstream clones the moving branch
    # `update-ovpn3`, and GPL/AGPL section 6 wants the Corresponding Source of the binary we
    # convey: a branch does not identify it. `cmake/aresvpn/source-revisions.py` (#D185) refuses a
    # GIT-BRANCH and exits 1 on `--os android`, which is what made this the last thing standing.
    #
    # The escape hatch #D185 designed - record the resolved commit and let the script read it back
    # out of the conan cache - CANNOT FIRE: conan deletes the source and build folders once a
    # package is built, so the clone it reads is gone before anything can ask (measured inside the
    # container that produced the green APK: every openv* package folder holds d/e/es/p and no
    # s or b).
    #
    # THE COST OF PINNING HERE IS THE ONE THING THAT WAS ASSUMED AND IS NOT TRUE. A fork-changed
    # recipe normally forfeits its prebuilt on artifactory.amnezia.org, which we can read and never
    # write - but asked directly, `openvpn-pt-android/1.0.0` publishes ZERO binary packages there,
    # and so does `awg-android/3.1.20260814`. Both Android backends are already source-built on
    # every machine, so this pin costs no build time at all. What it does cost is #D177 rule 3's
    # merge surface on one small, low-churn upstream file, and that is the trade taken.
    #
    # `--branch` takes a branch or a tag and NOT an arbitrary sha, so the pin is a clone followed by
    # a checkout, and the submodules are initialised afterwards because the checkout moves them.
    OPENVPN_PT_ANDROID_COMMIT = "8f5821696b7d8d0ae6c6509306bccfb2c15fe6f6"  # update-ovpn3, 2026-09-05

    def source(self):
        git = Git(self)
        git.clone(
            url="https://github.com/amnezia-vpn/openvpn-pt-android.git",
            target="."
        )
        git.checkout(self.OPENVPN_PT_ANDROID_COMMIT)
        git.run("submodule update --init --recursive")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        apply_conandata_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build(target=["ck_ovpn_plugin_go", "ovpn3", "ovpnutil", "rsapss"])

    def package(self):
        copy(self, "*.h", src=self.build_folder, dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.so", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "amnezia::openvpn-pt-android")
        self.cpp_info.libs = [ "ovpn3", "ovpnutil", "rsapss" ]
        self.cpp_info.set_property("cmake_extra_variables", {
            "OPENVPN_PT_ANDROID_LIBCK_OVPN_PLUGIN_PATH": Path(self.package_folder, "lib", "libck-ovpn-plugin.so").as_posix()
        })
