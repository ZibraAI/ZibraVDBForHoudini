import os
import argparse
import json
import houdini_version_query

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")
    
HOUDINI_PRODUCT = "houdini"
HOUDINI_VERSIONS = ["20.0", "20.5", "21.0"]
HOUDINI_PLATFORMS = ["win64-vc143", "macosx_arm64", "macosx_x86_64", "linux_x86_64_gcc11.2"]
    
def windows_x64_entry(version, build):
    return {
               "name": f"Windows x64 {version}.{build}",
               "runner": "windows-latest",
               "generator": "Visual Studio 17 2022",
               "executable-extension": ".exe",
               "houdini-version": version,
               "houdini-build": build,
               "houdini-platform": "win64-vc143",
               "houdini-install-path": f"C:\\Houdini\\{version}.{build}",
               "hfs-path": f"C:\\Houdini\\{version}.{build}",
               "python-command": "python",
               "python-venv-activate-path": "Scripts/Activate.ps1",
               "additional-config-args": None
           }

def linux_x64_entry(version, build):
    return {
               "name": f"Linux x64 {version}.{build}",
               "runner": [
               "self-hosted",
               "Linux",
               "X64",
               "houdini"
               ],
               "generator": "Ninja Multi-Config",
               "executable-extension": None,
               "houdini-version": version,
               "houdini-build": build,
               "houdini-platform": "linux_x86_64_gcc11.2",
               "houdini-install-path": f"/opt/hfs{version}.{build}",
               "hfs-path": f"/opt/hfs{version}.{build}",
               "python-command": "python3",
               "python-venv-activate-path": "bin/Activate.ps1",
               "additional-config-args": None
           }

def macos_x64_entry(version, build):
    return {
               "name": f"macOS x64 {version}.{build}",
               "runner": "macos-14",
               "generator": "Xcode",
               "executable-extension": None,
               "houdini-version": version,
               "houdini-build": build,
               "houdini-platform": "macosx_x86_64",
               "houdini-install-path": f"/Applications/Houdini/Houdini{version}.{build}",
               "hfs-path": f"/Applications/Houdini/Houdini{version}.{build}/Frameworks/Houdini.framework/Versions/Current/Resources",
               "python-command": "python3",
               "python-venv-activate-path": "bin/Activate.ps1",
               "additional-config-args": "-DCMAKE_OSX_ARCHITECTURES=x86_64"
           }

def macos_arm64_entry(version, build):
    return {
               "name": f"macOS arm64 {version}.{build}",
               "runner": "macos-14",
               "generator": "Xcode",
               "executable-extension": None,
               "houdini-version": version,
               "houdini-build": build,
               "houdini-platform": "macosx_arm64",
               "houdini-install-path": f"/Applications/Houdini/Houdini{version}.{build}",
               "hfs-path": f"/Applications/Houdini/Houdini{version}.{build}/Frameworks/Houdini.framework/Versions/Current/Resources",
               "python-command": "python3",
               "python-venv-activate-path": "bin/Activate.ps1",
               "additional-config-args": "-DCMAKE_OSX_ARCHITECTURES=arm64"
           }
    
def append_builds_to_matrix(matrix, version, builds, full_build):
    for build in builds:
        matrix["include"].append(linux_x64_entry(version, build))
        if full_build:
            matrix["include"].append(windows_x64_entry(version, build))
            matrix["include"].append(macos_x64_entry(version, build))
            matrix["include"].append(macos_arm64_entry(version, build))

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Houdini build selector")
    arg_parser.add_argument("--full-build", action='store_true')
    args = arg_parser.parse_args()

    matrix = {"include": []}
    for version in HOUDINI_VERSIONS:
        valid_builds = houdini_version_query.query_houdini_builds(
            product=HOUDINI_PRODUCT,
            version=version,
            platforms=HOUDINI_PLATFORMS,
            allow_daily=False,
            verbose=False
        )
        if not valid_builds:
            raise Exception(f"No common builds found for {HOUDINI_PRODUCT} {version} on platforms {HOUDINI_PLATFORMS}")
        if not args.full_build:
            valid_builds = valid_builds[:1]
        for build in valid_builds:
            matrix["include"].append(linux_x64_entry(version, build))
            if args.full_build:
                matrix["include"].append(windows_x64_entry(version, build))
                matrix["include"].append(macos_x64_entry(version, build))
                matrix["include"].append(macos_arm64_entry(version, build))

    out_file_path = os.getenv('GITHUB_OUTPUT')

    with open(out_file_path, "a") as out_file:
        out_file.write(f"build_matrix={json.dumps(matrix)}")
