# Endstone ChestFormAPI

A high-performance, native C++23 Endstone plugin and Python wrapper that allows developers to create and manage virtual chest inventory GUIs for Bedrock Edition players. Spoofs inventory screens fully server-side with zero client-side resource packs or downloads required.

---

<p align="center">
  <img src="https://img.shields.io/pypi/v/endstone-chestform-api?style=for-the-badge&color=blue" alt="PyPI Version" />
  <img src="https://img.shields.io/pypi/pyversions/endstone-chestform-api?style=for-the-badge&color=green" alt="Python Versions" />
  <img src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++ Standard" />
  <img src="https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux-lightgrey?style=for-the-badge" alt="Supported Platforms" />
  <img src="https://img.shields.io/badge/License-MPL_2.0-orange?style=for-the-badge" alt="License" />
</p>

---

## 📖 Project Documentation

To help you get started quickly and configure integrations, please refer to the corresponding guides:

*   **[Developer & Usage Guide](usage_guide.md)** — Comprehensive guide with C++ and Python side-by-side examples covering setup, dynamic updates, chest mirroring, and buttons.
*   **[Python Packaging & Deployment Guide](python_wheel_guide.md)** — Learn how to bundle your Python Endstone plugins with `endstone-chestform-api` into standalone wheels (`.whl`).
*   **[Protocol Compatibility Notes](compatibility_notes.md)** — Technical specs detailing packet structures, Bedrock IDs, and network synchronization details.

---

## ✨ Features

*   **Flexible Layouts**: Supports both Single Chest (27 slots) and Double Chest (54 slots) layouts.
*   **Aesthetic Formatting**: Customize slot items with display names, lore strings, enchantments, and custom NBT.
*   **Dynamic UI Refreshes**: Update titles, sizes, or individual slot icons dynamically without desyncing clients.
*   **Interactive Callbacks**: Register lambda/function triggers directly to slots; handles execution when slots are clicked.
*   **Desync & Dupe Protections**: Cancels client-side item movements to prevent duplication or inventory desync.
*   **Auto-Cleanup**: Restores original blocks when a player disconnects, closes the UI, or the plugin disables.

---

## 🚀 Quick Start

### 1. C++ Integration

Add the headers and link the library target:

```cmake
include(FetchContent)
FetchContent_Declare(
    endstone_chestform_api
    GIT_REPOSITORY https://github.com/TheNINJALLO/endstone_chestform_api.git
    GIT_TAG master
)
FetchContent_MakeAvailable(endstone_chestform_api)

# Link the target in your CMakeLists.txt
target_link_libraries(your_plugin PRIVATE chestform_api)
```

**C++ Example:**
```cpp
#include <chest_form_api/chest_form.h>
#include <endstone/player.h>

void openMenu(endstone::Plugin& plugin, endstone::Player& player) {
    ChestForm form(plugin, "§bDeveloper Kit Selector", ChestSize::Double);

    FormItem kit_item;
    kit_item.type_id = "minecraft:diamond_sword";
    kit_item.display_name = "§aWarrior's Blade";
    kit_item.lore = {"§7Click to claim your sword!"};

    form.setSlot(13, kit_item, [](endstone::Player& p, int slot) {
        p.getInventory().addItem(endstone::ItemStack("minecraft:diamond_sword", 1));
        p.sendMessage("§a[ChestForm] Claimed sword!");
    });

    form.sendTo(player);
}
```

### 2. Python Integration

Install the package directly from PyPI:
```bash
pip install endstone-chestform-api
```

**Python Example:**
```python
from endstone.player import Player
from endstone.plugin import Plugin
from endstone_chestform_api import ChestForm, FormItem, ChestSize

def open_menu(plugin: Plugin, player: Player):
    form = ChestForm(plugin, "§bPython Kit Selector", ChestSize.DOUBLE)

    kit_item = FormItem()
    kit_item.type_id = "minecraft:diamond_sword"
    kit_item.display_name = "§aWarrior's Blade"
    kit_item.lore = ["§7Click to claim your sword!"]

    def on_click(p: Player, slot: int):
        p.inventory.add_item(p.server.create_item_stack("minecraft:diamond_sword", 1))
        p.send_message("§a[ChestForm] Claimed sword!")

    form.set_slot(13, kit_item, on_click)
    form.send_to(player)
```

---

## 🛠️ Build from Source

### Linux (Clang 18)
To build on Linux, you must use **Clang 18** and **Ninja**:

```bash
# Configure with Clang 18 and Ninja
CC=clang-18 CXX=clang++-18 cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build targets
cmake --build build
```

The compiled plugin shared object (`endstone_chestform_api.so`) will be output inside the `build/` directory.

### Windows (MSVC 2022)
To build on Windows, use Visual Studio Build Tools:

```powershell
# Configure Visual Studio project
cmake -B build -G "Visual Studio 17 2022" -A x64

# Compile the library
cmake --build build --config Release
```

The compiled DLL (`endstone_chestform_api.dll`) will be output inside the `build/Release/` directory.

---

## 📜 License

This project is licensed under the terms of the **Mozilla Public License, v. 2.0** (MPL-2.0). See the [LICENSE](LICENSE) file for details.
