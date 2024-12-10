# ZibraVDB FileCache HDA

## Exporting

### Window: New SOP Digital Asset

| _Property_             | _Value_                                |
|------------------------|----------------------------------------|
| __Asset Label__        | ```Labs ZibraVDB File Cache (Alpha)``` |
| __TAB Submenu path__   | ```ZibraVDB, Labs/FX/Pyro```           |
| __Internal Name__      | ```labs::zibravdb_filecache::0.2```    |
| __Filename (Save To)__ | ```zibravdb_filecache.0.2.hda```       |

### Window: Edit Operator Type Properties

#### Tab: Basic

| _Property_                 | _Value_            |
|----------------------------|--------------------|
| __Icon__                   | ```zibravdb.svg``` |
| __Version__                | ```0.2```          |
| __Embed Icon in Operator__ | :x:                |

#### Tab: Node

| _Property_        | _Value_                                           |
|-------------------|---------------------------------------------------|
| __Message Nodes__ | ```rop_zibravdb_compress1 zibravdb_decompress1``` |

#### Tab: Input/Output

| _Property_         | _Value_                        |
|--------------------|--------------------------------|
| __Input 1 Label__  | ```Volumes to Cache to Disk``` |
| __Output 1 Labal__ | ```Cached VDBs```              |

#### Tab: Scripts
- OnCreated
  > ```
  > import platform
  >
  > kwargs['node'].setColor(hou.Color(0.9, 0.8, 0.55))
  >  
  > if platform.system() != "Windows":
  >     msg = "Error: ZibraVDB for Houdini is currently only supported on Windows."
  >     kwargs['node'].setComment(msg)
  >     kwargs['node'].setGenericFlag(hou.nodeFlag.DisplayComment, True)
  >     print(msg)
  > ```
