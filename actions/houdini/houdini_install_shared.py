import sys
    
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
    
def get_cache_path(version, build):
    if sys.platform == "linux":
        return f"/opt/installed_houdini_{version}.{build}.txt"
    elif sys.platform == "darwin":
        return f"/Applications/Houdini/installed_houdini_{version}.{build}.txt"
    elif sys.platform == "win32":
        return f"C:/Houdini/installed_houdini_{version}.{build}.txt"
    else:
        print(sys.platform)
        raise Exception("Unexpected platform")