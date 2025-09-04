import os
import shutil
import sys
import requests
import subprocess
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
        print(sys.platform)
        raise Exception("Unexpected platform")
    
def get_install_path(version, build):
    if sys.platform == "linux":
        return f"/opt/hfs{version}.{build}"
    elif sys.platform == "darwin":
        return f"/Applications/Houdini/Houdini{version}.{build}"
    elif sys.platform == "win32":
        return f"C:\\Houdini\\{version}.{build}"
    else:
        print(sys.platform)
        raise Exception("Unexpected platform")
    
def get_cache_path(version):
    if sys.platform == "linux":
        return f"/opt/installed_houdini_{version}.txt"
    elif sys.platform == "darwin":
        return f"~/Library/Application Support/ZibraAI/installed_houdini_{version}.txt"
    elif sys.platform == "win32":
        return f"C:/Houdini/installed_houdini_{version}.txt"
    else:
        print(sys.platform)
        raise Exception("Unexpected platform")

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Houdini installer")
    arg_parser.add_argument("--platform", type=str, help="Houdini platform to install", required=True)
    arg_parser.add_argument("--version", type=str, help="Houdini version to install", required=True)
    arg_parser.add_argument("--build", type=str, help="Houdini build to install", required=True)
    arg_parser.add_argument("--install_path", type=str, help="Houdini installation path", required=True)
    args = arg_parser.parse_args()

    if get_install_path(args.version, args.build) != args.install_path:
        # Workflow uses this path outside of this script as well
        # Need to make sure that we can correctly determine path of old houdini installation
        raise Exception(f"Provided install path {args.install_path} does not match expected path {get_install_path(args.version, args.build)}")

    cached_build = ""
    cache_path = get_cache_path(args.version)
    if os.path.exists(cache_path):
        with open(cache_path, "r") as cache_file:
            cached_build = cache_file.read()

    if cached_build == args.build:
        print(f"Houdini {args.version}.{args.build} is already installed")
        if not os.path.exists(args.install_path):
            print(f"Installation path {args.install_path} does not exist, installation may be corrupted, proceeding with fresh install")
        else:
            print(f"Skipping installation")
            sys.exit(0)
    elif cached_build == "":
        print("Didn't detect installed Houdini version, proceeding with fresh install")
    else:
        print(f"Previously installed Houdini version was {args.version}.{cached_build} but {args.version}.{args.build} is requested.")
        print(f"Overwriting previously installed version.")
        os.remove(cache_path)
        shutil.rmtree(get_install_path(args.version, cached_build))

    HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
    HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

    platform = args.platform
    product = "houdini"
    version = args.version

    sw = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    build_filter = {"build": args.build}

    build_selection = {"product": product, "platform": platform, "version": version}
    build_list = sw.get_latest_builds(prodinfo=build_selection, prodfilter=build_filter, only_production=False)
    if not build_list:
        raise Exception(f"No builds found for {product} {version} on {platform} with filter {build_filter}")
    build = build_list[0]
    build_dl = sw.get_build_download(prodinfo=ProductBuild(**build.dict()))
    full_version_string = f"{build.version}.{build.build}"
    print(f"Installing {product} {full_version_string} for {build.platform}...")
    
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
        subprocess.run(["tar", "-xvf", build_dl.filename], check=True)
        # Find extracted directory that starts with houdini_
        extracted_dir = [d for d in os.listdir() if d.startswith("houdini-") and os.path.isdir(d)][0]
        # Make installer executable
        subprocess.run(["chmod", "+x", f"{extracted_dir}/houdini.install"], check=True)
        # Run installer
        install_process = subprocess.run([f"{extracted_dir}/houdini.install", "--install-houdini", "--no-install-engine-maya", "--no-install-engine-unity", "--no-install-engine-unreal", "--no-install-menus", "--no-install-hfs-symlink", "--no-install-license", "--no-install-avahi", "--no-install-sidefxlabs", "--no-install-hqueue-server", "--no-install-hqueue-client", "--auto-install", "--make-dir", "--accept-EULA", "2021-10-13", args.install_path], check=True)
        # Clean up unpacked archive
        shutil.rmtree(extracted_dir)
    elif sys.platform == "darwin":
        # Mount the DMG
        subprocess.run(["hdiutil", "attach", build_dl.filename], check=True)
        # Mounted volume is /Volumes/Houdini
        # Run the installer
        install_process = subprocess.run(["sudo", "installer", "-pkg", "/Volumes/Houdini/Houdini.pkg", "-target", "/"], check=True)
        # Unmount the DMG
        subprocess.run(["hdiutil", "detach", "/Volumes/Houdini"], check=True)
    elif sys.platform == "win32":
        # Run installer
        install_process = subprocess.run([build_dl.filename, "/S", f"/InstallDir={args.install_path}", "/acceptEULA=2021-10-13"], check=False, capture_output=True, text=True)
    else:
        raise Exception("Unexpected platform")

    if install_process.returncode != 0:
        print(f"Installer failed with code {install_process.returncode}")
        print(f"Installer stdout: {install_process.stdout}")
        print(f"Installer stderr: {install_process.stderr}")
        raise Exception(f"Installation failed. Exit code: {install_process.returncode}")
    
    print(f"Install succeeded, cleaning up")
    os.remove(build_dl.filename)

    os.makedirs(os.path.dirname(cache_path), exist_ok=True)
    with open(cache_path, "w") as cache_file:
        cache_file.write(args.build)
    print(f"Installation of {product} {full_version_string} complete")
