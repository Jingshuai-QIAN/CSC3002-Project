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
│   ├── App.h
│   ├── App.cpp
│   ├── Character/               # Player character module
│   │   ├── Character.h
│   │   ├── Character.cpp
│   │   ├── CharacterConfig.h
│   │   └── CharacterConfig.cpp
│   ├── Config/                  # Configuration manager
│   │   ├── ConfigManager.h
│   │   └── ConfigManager.cpp
│   ├── Input/                   # Input handling
│   │   ├── InputManager.h
│   │   └── InputManager.cpp
│   ├── MapLoader/               # Map loading and TMJ parsing
│   │   ├── MapLoader.h
│   │   ├── MapLoader.cpp
│   │   ├── MapObjects.h
│   │   ├── TileSetManager.h
│   │   ├── TileSetManager.cpp
│   │   ├── TileLayer.h
│   │   ├── TileLayer.cpp
│   │   ├── TMJMap.cpp
│   │   └── TMJMap.h
│   ├── Renderer/                # Rendering subsystem
│   │   ├── Renderer.h
│   │   ├── Renderer.cpp
│   │   ├── TextRenderer.h
│   │   └── TextRenderer.cpp
│   └── Utils/                   # Utility helpers
│       ├── Logger.h
│       ├── FileUtils.h
│       └── StringUtils.h
├── config/                      # Configuration files
│   ├── app_config.json
│   ├── render_config.json
│   └── character_config.json
├── fonts/
│   └── arial.ttf
├── maps/
│   ├── bus.tmj
│   ├── canteen.tmj
│   ├── LG_campus_map.tmj
│   ├── library.tmj
│   ├── lower_campus_map.tmj
│   ├── shaw.tmj
│   ├── teaching_building.tmj
│   └── spawns.json
└── tiles/
    ├── exterior.png
    ├── F_01.png
    ├── free_overview.png
    ├── Interior.png
    ├── Modern_City.png
    ├── roguelikeIndoor_transparent.png
    ├── Small_Battle.png
    ├── tile_modern.png
    └── tilemap_packed.png
```
