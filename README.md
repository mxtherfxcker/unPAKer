# unPAKer

![Version](https://img.shields.io/badge/version-v1--dev1-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B17-red.svg)
![Status](https://img.shields.io/badge/status-Development-orange.svg)

Open-source game resource archive extractor. Analyze and extract contents from .pak archives used in various game engines.

## Features

- 🎮 Support for multiple game archive formats
  - Unreal Engine 3/4/5 (fully supported in `v1.2-stable`)
  - Source Engine
  - Generic PAK archives (fully supported in `v1.2-stable`)
- ⚡ Fast and lightweight

## Supported Formats

| Format | Engine | Status |
|--------|--------|--------|
| UE3 PAK | Unreal Engine 3 | ✅ Read |
| UE4/5 PAK | Unreal Engine 4/5 | ❌ Coming in `v1.2-stable` |
| VPK | Source Engine | ✅ Read |
| Generic PAK | Other Engines | ❌ Coming in `v1.2-stable` |

## Requirements

- *Windows 10/11*
- *C++17 compiler* (MSVC 2019+, GCC 8+, Clang 8+)
- *CMake 3.15+*

## Build on Windows

```
> git clone https://github.com/mxtherfxcker/unPAKer.git
> cd unPAKer && mkdir build && cd build
> cmake .. -G "Visual Studio 17 2022" -A x64
> cmake --build . --config Release
```

## Roadmap

### v1-dev1

- [x] `.vpk` archives support
- [x] UE 3 `.pak` support
- [x] Search files and dir's
- [ ] Preview content for:
	- [x] Text files (`.vpk` only yet)
	- [ ] Audio files
	- [ ] Models and textures (coming in `v2.1-stable`)
	- [ ] Other supported files (coming in `v2.5-stable`)

- [ ] Extract files (coming in `v1-stable`)
- [ ] Import and edit files (coming in `v3-stable`)

## Support

- 🐛 Issues: [GitHub Issues](https://github.com/mxtherfxcker/unPAKer/issues)
- ✉️ Email: [ceyoynxy35@gmail.com](mailto:ceyoynxy35@gmail.com)

---

**Made with ❤️ by mxtherfxcker**
