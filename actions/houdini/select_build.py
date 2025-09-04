import os
import argparse
from sesiweb import SesiWeb

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Houdini build selector")
    arg_parser.add_argument("--product", type=str, required=True)
    arg_parser.add_argument("--version", type=str, help="Version within which to find latest common build", required=True)
    arg_parser.add_argument("--platforms", type=str, nargs="*", required=True)
    arg_parser.add_argument("--allow-daily", action="store_true")
    arg_parser.add_argument("--verbose", action="store_true")
    args = arg_parser.parse_args()

    HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
    HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

    if args.verbose:
        print(f"Selecting build to use for {args.product} {args.version}")

    api = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    all_builds = dict()

    for platform in args.platforms:
        build_selection = {"product": args.product, "platform": platform, "version": args.version}

        build_list = api.get_latest_builds(prodinfo=build_selection, only_production=(not args.allow_daily))
        if not build_list:
            raise Exception(f"No builds found for {args.product} {args.version} on {platform}")
        
        if args.verbose:
            print(f"Builds available for {args.product} {args.version} {platform}:")
            for build in build_list:
                print(f"    {args.version}.{build.build} {build.platform}")
            print()
        
        detected_builds = set()

        for build in build_list:
            if build.build in detected_builds:
                raise Exception(f"Detected duplicate build {args.product} {args.version}.{build} on {platform}")
            detected_builds.add(build.build)

        for build in build_list:
            if build.build not in all_builds:
                all_builds[build.build] = 0
            all_builds[build.build] = all_builds[build.build] + 1

    seleted_build = -1

    for build in all_builds:
        if all_builds[build] == len(args.platforms):
            if args.verbose:
                print(f"Considering {args.product} {args.version}.{build}")
            if int(build) > seleted_build:
                seleted_build = int(build)
        else:
            if args.verbose:
                print(f"{args.product} {args.version}.{build} is not available on all platforms")

    out_file_path = os.getenv('GITHUB_OUTPUT')
    out_variable_name = args.version.replace(".", "_")

    with open(out_file_path, "a") as out_file:
        out_file.write(f"houdini_build_{out_variable_name}={seleted_build}")

    print(f"Selected build {args.product} {args.version}.{seleted_build}")
