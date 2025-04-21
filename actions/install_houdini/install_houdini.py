import os
import sys
import requests
import subprocess
import platform
import argparse
from sesiweb import SesiWeb
from sesiweb.model.service import ProductBuild

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")

def get_platform_list(architecture):
    if sys.platform == "linux":
        return ["linux_x86_64_gcc11.2"]
    elif sys.platform == "darwin":
        if architecture == "arm64":
            print("Searching for arm64 build")
            return ["macosx_arm64_clang15"]
        else:
            print("Running on x86_64 build")
            return ["macosx_x86_64_clang15"]
    elif sys.platform == "win32":
        return ["win64-vc143"]
    else:
        raise Exception("Unexpected platform")
    
def get_file_extension():
    if sys.platform == "linux":
        return "tar.gz"
    elif sys.platform == "darwin":
        return "dmg"
    elif sys.platform == "win32":
        return "exe"
    else:
        print(platform)
        raise Exception("Unexpected platform")

if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser(description="Install Houdini")
    arg_parser.add_argument("--architecture", type=str, help="macOS architecture to install for (arm64 or x86_64)", default="arm64", choices=["arm64", "x86_64"])
    arg_parser.add_argument("--version", type=str, help="Houdini version to install", default="20.5", choices=["20.5"])
    args = arg_parser.parse_args()

    HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
    HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

    platform_list = get_platform_list(args.architecture)
    product_list = ["houdini"]
    version_list = [args.version]

    sw = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    for platform in platform_list:
        for product in product_list:
            for version in version_list:
                product_build = {"product": product, "platform": platform, "version": version}
                build_select = sw.get_latest_build(prodinfo=product_build, only_production=False)
                build_dl = sw.get_build_download(prodinfo=ProductBuild(**build_select.dict()))
                full_version_string = f"{build_select.version}.{build_select.build}"
                print(f"Installing {product} {full_version_string} for {platform}...")

                r = requests.get(build_dl.download_url)
                file_name = f"{product}_{version}_{platform}.{get_file_extension()}"
                with open(file_name, "wb") as f:
                    f.write(r.content)

                # Run the installer
                if sys.platform == "linux":
                    # Extract the tarball
                    subprocess.run(["tar", "-xvf", file_name])
                    # Find extracted directory that starts with houdini_
                    extracted_dir = [d for d in os.listdir() if d.startswith("houdini-") and os.path.isdir(d)][0]
                    # Make installer executable
                    subprocess.run(["chmod", "+x", f"{extracted_dir}/houdini.install"])
                    # Run installer
                    subprocess.run([f"{extracted_dir}/houdini.install", "--install-houdini", "--no-install-engine-maya", "--no-install-engine-unity", "--no-install-engine-unreal", "--no-install-menus", "--no-install-hfs-symlink", "--no-install-license", "--no-install-avahi", "--no-install-sidefxlabs", "--no-install-hqueue-server", "--no-install-hqueue-client", "--auto-install", "--make-dir", "--accept-EULA", "2021-10-13", "/opt/hfs20.5"])
                elif sys.platform == "darwin":
                    # Mount the DMG
                    subprocess.run(["hdiutil", "attach", file_name])
                    # Mounted volume is /Volumes/Houdini
                    # Run the installer
                    subprocess.run(["sudo", "installer", "-pkg", "/Volumes/Houdini/Houdini.pkg", "-target", "/"])
                    # Unmount the DMG
                    subprocess.run(["hdiutil", "detach", "/Volumes/Houdini"])          
                elif sys.platform == "win32":
                    subprocess.run([file_name, "/S", f"/InstallDir=C:\\{product}_{platform}_{version}", "/acceptEULA=2021-10-13"], check=True)
                else:
                    raise Exception("Unexpected platform")
