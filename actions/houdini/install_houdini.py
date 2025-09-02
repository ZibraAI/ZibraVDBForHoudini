import os
import sys
import requests
import subprocess
import platform
import argparse
import hashlib
from sesiweb import SesiWeb
from sesiweb.model.service import ProductBuild

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")
    
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
    arg_parser.add_argument("--platform", type=str, help="Houdini platform to install", required=True)
    arg_parser.add_argument("--version", type=str, help="Houdini version to install", required=True)
    arg_parser.add_argument("--build", type=str, help="Houdini build to install", required=False)
    args = arg_parser.parse_args()

    HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
    HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

    platform = args.platform
    product = "houdini"
    version = args.version

    sw = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    build_filter = {}

    if args.build:
        build_filter["build"] = args.build

    build_selection = {"product": product, "platform": platform, "version": version}
    build_list = sw.get_latest_builds(prodinfo=build_selection, prodfilter=build_filter, only_production=False)
    if not build_list:
        raise Exception(f"No builds found for {product} {version} on {platform} with filter {build_filter}")
    build = build_list[0]
    build_dl = sw.get_build_download(prodinfo=ProductBuild(**build.dict()))
    full_version_string = f"{build.version}.{build.build}"
    print(f"Installing {product} {full_version_string} for {platform}...")
    
    r = requests.get(build_dl.download_url)
    if r.status_code != 200:
        raise Exception(f"Download failed with code {r.status_code}")
    if not r.content:
        raise Exception("Downloaded content is empty")
    if len(r.content) != build_dl.size:
        raise Exception(f"Downloaded content size {len(r.content)} does not match expected size {build_dl.size}")
    print(f"Download successful")

    hash_md5 = hashlib.md5()
    hash_md5.update(r.content)
    if hash_md5.hexdigest() != build_dl.hash:
        raise Exception(f"Downloaded file hash {hash_md5.hexdigest()} does not match expected hash {build_dl.hash}")
    print(f"Hash verified")

    with open(build_dl.filename, "wb") as f:
        f.write(r.content)

    print(f"Starting install")
    if sys.platform == "linux":
        # Extract the tarball
        subprocess.run(["tar", "-xvf", build_dl.filename])
        # Find extracted directory that starts with houdini_
        extracted_dir = [d for d in os.listdir() if d.startswith("houdini-") and os.path.isdir(d)][0]
        # Make installer executable
        subprocess.run(["chmod", "+x", f"{extracted_dir}/houdini.install"])
        # If /opt/hfs exists, delete it
        if os.path.exists("/opt/hfs"):
            os.rmtree("/opt/hfs")
        # Run installer
        subprocess.run([f"{extracted_dir}/houdini.install", "--install-houdini", "--no-install-engine-maya", "--no-install-engine-unity", "--no-install-engine-unreal", "--no-install-menus", "--no-install-hfs-symlink", "--no-install-license", "--no-install-avahi", "--no-install-sidefxlabs", "--no-install-hqueue-server", "--no-install-hqueue-client", "--auto-install", "--make-dir", "--accept-EULA", "2021-10-13", "/opt/hfs"])
    elif sys.platform == "darwin":
        # Mount the DMG
        subprocess.run(["hdiutil", "attach", build_dl.filename])
        # Mounted volume is /Volumes/Houdini
        # Run the installer
        subprocess.run(["sudo", "installer", "-pkg", "/Volumes/Houdini/Houdini.pkg", "-target", "/"])
        # Unmount the DMG
        subprocess.run(["hdiutil", "detach", "/Volumes/Houdini"])          
    elif sys.platform == "win32":
        # If C:\Houdini exists delete it
        if os.path.exists("C:\\Houdini"):
            os.rmtree("C:\\Houdini")
        # Run installer
        subprocess.run([build_dl.filename, "/S", f"/InstallDir=C:\\Houdini", "/acceptEULA=2021-10-13"], check=True)
    else:
        raise Exception("Unexpected platform")
