import os
import sys
import argparse
    
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
    arg_parser.add_argument("--install-path", type=str, help="Houdini installation path", required=True)
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

    need_install = True

    if cached_build == args.build:
        print(f"Houdini {args.version}.{args.build} is already installed")
        if not os.path.exists(args.install_path):
            print(f"Installation path {args.install_path} does not exist, installation may be corrupted")
        else:
            print(f"Skipping installation")
            need_install = False
    elif cached_build == "":
        print("Didn't detect installed Houdini version")
    else:
        print(f"Previously installed Houdini version was {args.version}.{cached_build} but {args.version}.{args.build} is requested.")
        
    out_file_path = os.getenv('GITHUB_OUTPUT')

    with open(out_file_path, "a") as out_file:
        out_file.write(f"need_install_houdini={'true' if need_install else 'false'}")