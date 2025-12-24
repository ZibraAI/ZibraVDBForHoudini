import os
import argparse
import houdini_install_shared

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Houdini installer")
    arg_parser.add_argument("--platform", type=str, help="Houdini platform to install", required=True)
    arg_parser.add_argument("--version", type=str, help="Houdini version to install", required=True)
    arg_parser.add_argument("--build", type=str, help="Houdini build to install", required=True)
    arg_parser.add_argument("--install-path", type=str, help="Houdini installation path", required=True)
    args = arg_parser.parse_args()

    if houdini_install_shared.get_install_path(args.version, args.build) != args.install_path:
        # Workflow uses this path outside of this script as well
        # Need to make sure that we can correctly determine path of old houdini installation
        raise Exception(f"Provided install path {args.install_path} does not match expected path {houdini_install_shared.get_install_path(args.version, args.build)}")

    need_install = True

    cache_path = houdini_install_shared.get_cache_path(args.version, args.build)
    if not os.path.exists(cache_path):
        print("Didn't detect installed Houdini version")
    else:
        print(f"Houdini {args.version}.{args.build} is already installed")
        if not os.path.exists(args.install_path):
            print(f"Installation path {args.install_path} does not exist, installation may be corrupted")
        else:
            print(f"Skipping installation")
            need_install = False
        
    out_file_path = os.getenv('GITHUB_OUTPUT')

    with open(out_file_path, "a") as out_file:
        out_file.write(f"need_install_houdini={'true' if need_install else 'false'}")