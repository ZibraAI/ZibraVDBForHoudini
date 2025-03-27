import os
import sys
import requests
import subprocess
from sesiweb import SesiWeb
from sesiweb.model.service import ProductBuild

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")

def get_platforms():
    if sys.platform == "linux":
        return ["linux_x86_64_gcc11.2"]
    elif sys.platform == "darwin":
        return ["macosx"]
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

HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

PLATFORMS = get_platforms()
PRODUCTS = ["houdini"]
VERSIONS = ["20.5"]

if __name__ == "__main__":
    sw = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    for product in PRODUCTS:
        for platform in PLATFORMS:
            for version in VERSIONS:
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
                    raise Exception("Unimplemented")
                elif sys.platform == "darwin":
                    raise Exception("Unimplemented")
                elif sys.platform == "win32":
                    subprocess.run([file_name, "/S", f"/InstallDir=C:\\{product}_{platform}_{version}", "/acceptEULA=2021-10-13"], check=True)
                else:
                    raise Exception("Unexpected platform")
