import os
from sesiweb import SesiWeb

def get_secret(secret_name):
    try:
        return os.environ[secret_name]
    except KeyError:
        raise Exception(f"Missing secret: {secret_name}")
    
def query_houdini_builds(product, version, platforms, allow_daily = False, verbose = False):
    HOUDINI_CLIENT_ID = get_secret("HOUDINI_CLIENT_ID")
    HOUDINI_SECRET_KEY = get_secret("HOUDINI_SECRET_KEY")

    if verbose:
        print(f"Selecting build to use for {product} {version}")

    api = SesiWeb(client_secret=HOUDINI_SECRET_KEY, client_id=HOUDINI_CLIENT_ID)

    all_builds = dict()

    for platform in platforms:
        build_selection = {"product": product, "platform": platform, "version": version}

        build_list = api.get_latest_builds(prodinfo=build_selection, only_production=(not allow_daily))
        if not build_list:
            raise Exception(f"No builds found for {product} {version} on {platform}")
        
        if verbose:
            print(f"Builds available for {product} {version} {platform}:")
            for build in build_list:
                print(f"    {version}.{build.build} {build.platform}")
            print()
        
        detected_builds = set()

        for build in build_list:
            if build.build in detected_builds:
                raise Exception(f"Detected duplicate build {product} {version}.{build} on {platform}")
            detected_builds.add(build.build)

        for build in build_list:
            if build.build not in all_builds:
                all_builds[build.build] = 0
            all_builds[build.build] = all_builds[build.build] + 1

    valid_builds = []

    for build in all_builds:
        if all_builds[build] == len(platforms):
            valid_builds.append(build)
            if verbose:
                print(f"Considering {product} {version}.{build}")
        else:
            if verbose:
                print(f"{product} {version}.{build} is not available on all platforms")

    # Sort valid builds in descending order
    valid_builds.sort(key=lambda b: int(b), reverse=True)
    return valid_builds