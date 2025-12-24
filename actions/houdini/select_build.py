import os
import argparse
import houdini_version_query

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

    valid_builds = houdini_version_query.query_houdini_builds(
        product=args.product,
        version=args.version,
        platforms=args.platforms,
        allow_daily=args.allow_daily,
        verbose=args.verbose
    )

    if not valid_builds:
        raise Exception(f"No common builds found for {args.product} {args.version} on platforms {args.platforms}")
