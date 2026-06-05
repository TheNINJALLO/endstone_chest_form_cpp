# Endstone ChestFormAPI (C++)

A native C++23 Endstone plugin and library for Linux that allows developer plugins (and Python wheel integrations) to open fake chest inventory forms to Bedrock Edition players. It handles custom item slot icons, title headers, double chest sizes, item lock protections, and click routing callbacks.

## Features

*   **Custom Size Support**: Single Chest (27 slots) or Double Chest (54 slots).
*   **Aesthetic Formatting**: Custom display names, item amounts, custom NBT tags, and lore.
*   **Interactive Callbacks**: Register lambda callbacks per-slot to run code when clicked.
*   **Anti-Steal Reversion**: Reverts slot desyncs on cancelled transaction events.
*   **Automatic Cleanup**: Restores original blocks when a player disconnects, closes the UI, or the plugin disables.

## Build Requirements (Linux)

To compile this plugin, you must use **Clang 18** or newer with `libc++` and a C++23-compliant build system.

### Install Dependencies on Ubuntu/Debian

Run the following commands to configure your build environment:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build wget git

# Install Clang 18
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18

# Install libc++ for Clang 18
sudo apt-get install -y libc++-18-dev libc++abi-18-dev
```

## Compilation

Build the shared library plugin using CMake and Ninja:

```bash
# Set compilers and build configurations
CC=clang-18 CXX=clang++-18 cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the targets
cmake --build build
```

The compiled Endstone plugin shared object (`libendstone_chest_form_cpp.so`) will be generated inside the `build` directory. Move it to your Endstone server's `plugins/` directory to load it.

## Integration options for Other C++ Plugins

### Option 1: Using FetchContent (Fallback)
You can declare it in your plugin's `CMakeLists.txt` using CMake's standard fetch API:
```cmake
include(FetchContent)
FetchContent_Declare(
    endstone_chest_form_cpp
    GIT_REPOSITORY https://github.com/GlacieTeam/ChestFormAPI.git # Update to actual repository
    GIT_TAG main
)
FetchContent_MakeAvailable(endstone_chest_form_cpp)
```

### Option 2: Using find_package (Preferred)
If installed on the server, you can link against it directly:
```cmake
find_package(Protocol CONFIG REQUIRED)
target_link_libraries(your_plugin PRIVATE Protocol::Protocol)
```

## Example API Usage

Below is a simple example showing how to create and open a double chest form using the C++ API:

```cpp
#include <chest_form_api/chest_form.h>
#include <endstone/player.h>

void sendCustomForm(endstone::Plugin& plugin, endstone::Player& player) {
    // 1. Initialize a double chest form
    ChestForm form(plugin, "§bDeveloper Kit Selector", ChestSize::Double);

    // 2. Add slot items and click listeners
    FormItem diamond_sword;
    diamond_sword.type_id = "minecraft:diamond_sword";
    diamond_sword.display_name = "§aExcalibur";
    diamond_sword.lore = {"§7Click to receive this legendary sword!"};
    diamond_sword.enchants["9"] = 5; // Sharpness V

    form.setSlot(13, diamond_sword, [](endstone::Player& p, int slot) {
        p.sendMessage("§aYou have chosen Excalibur!");
    });

    // 3. Fill filler border blocks
    FormItem border_glass;
    border_glass.type_id = "minecraft:stained_glass_pane";
    border_glass.aux = 15; // Black
    border_glass.display_name = " ";

    for (int i = 0; i < 9; ++i) form.setSlot(i, border_glass);

    // 4. Send UI to player
    form.sendTo(player);
}
```

## Test Commands

The plugin includes a test command `/chestformtest` registered for operators.
Executing this command opens a demo menu showing different item styles (fillers, custom names, lore, and click triggers).
See [compatibility_notes.md](compatibility_notes.md) for more details on Bedrock network packet specifications.
