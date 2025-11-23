# CSC3002-Project


## Compile and run

### Compile for the first time

```bash
cd path_to_navigation_folder
make
./codes/main.exe
```

### Compile again

```bash
cd path_to_navigation_folder
make clean
make VERBOSE=1
./codes/main.exe
```


## Structure

```plain text
navigation/
├── README.md
├── Makefile
├── codes/
│   ├── main.cpp
│   ├── Character/               # 主角角色模块
│   │   ├── Character.h
│   │   ├── Character.cpp
│   │   ├── CharacterConfig.h
│   │   └── CharacterConfig.cpp
│   ├── Config/                  # 配置模块
│   │   ├── ConfigManager.h
│   │   └── ConfigManager.cpp
│   ├── Input/                   # 输入处理模块  
│   │   ├── InputManager.h
│   │   └── InputManager.cpp
│   ├── MapLoader/               # 地图加载模块
│   │   ├── MapLoader.h
│   │   ├── MapLoader.cpp
│   │   ├── MapObjects.h
│   │   ├── TileSetManager.h
│   │   ├── TileSetManager.cpp
│   │   ├── TileLayer.h
│   │   ├── TileLayer.cpp
│   │   ├── TMJMap.cpp
│   │   └── TMJMap.h
│   ├── Renderer/                # 渲染模块
│   │   ├── Renderer.h
│   │   ├── Renderer.cpp
│   │   ├── TextRenderer.cpp
│   │   └── TextRenderer.h
│   └── Utils/                   # 工具模块
│   │   ├── Logger.h
│   │   ├── FileUtils.h
│   │   └── StringUtils.h
├── config/                      # 配置目录
│   ├── app_config.json          # 主配置文件
│   └── render_config.json       # 渲染配置文件
│   └── character_config.json    # 主角配置文件
├── fonts
│   └── arial.ttf
├── maps/
│   ├── bus.tmj
│   ├── canteen.tmj
│   ├── LG_campus_map.
│   ├── library.tmj
│   ├── lower_campus_map.tmj
│   ├── shaw.tmj
│   ├── spawns.
│   └── teaching_building.tmj
└── tiles/
    ├── F_01.png
    ├── Modern_City.png
    ├── Small_Battle.
    └── tile_modern.png
```
