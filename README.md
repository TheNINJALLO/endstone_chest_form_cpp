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

The compiled Endstone plugin shared object (`endstone_chestform_api.so` on Linux or `endstone_chestform_api.dll` on Windows) will be generated inside the `build` directory. Move it to your Endstone server's `plugins/` directory to load it.

## Integration Options for Other C++ Plugins

### Option 1: Using FetchContent (CMake-based)
You can declare it in your plugin's `CMakeLists.txt` using CMake's standard fetch API:
```cmake
include(FetchContent)
FetchContent_Declare(
    endstone_chestform_api
    GIT_REPOSITORY https://github.com/TheNINJALLO/endstone_chestform_api.git
    GIT_TAG master
)
FetchContent_MakeAvailable(endstone_chestform_api)

# Link against the target in your plugin
target_link_libraries(your_plugin PRIVATE chestform_api)
```

### Option 2: Using find_package (Local Library)
If installed on the target machine, you can find and link against the library target directly:
```cmake
find_package(endstone_chestform_api CONFIG REQUIRED)
target_link_libraries(your_plugin PRIVATE endstone_chestform_api::chestform_api)
```

---

## Python Integration (Wheel Distribution)

Endstone plugins can be written in C++ or Python. To make the ChestFormAPI accessible to Python plugins, you can compile it as a Python extension module using `pybind11`.

### 1. Declaring Pybind11 Bindings (C++)

Add the following binding code to your C++ codebase (e.g., in `src/bindings.cpp`):

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <chest_form_api/chest_form.h>
#include <chest_form_api/form_item.h>
#include <chest_form_api/chest_form_manager.h>

namespace py = pybind11;

PYBIND11_MODULE(endstone_chestform_api, m) {
    m.doc() = "Python bindings for Endstone ChestFormAPI";

    py::enum_<ChestSize>(m, "ChestSize")
        .value("SINGLE", ChestSize::Single)
        .value("DOUBLE", ChestSize::Double)
        .export_values();

    py::class_<FormItem>(m, "FormItem")
        .def(py::init<>())
        .def_readwrite("type_id", &FormItem::type_id)
        .def_readwrite("amount", &FormItem::amount)
        .def_readwrite("aux", &FormItem::aux)
        .def_readwrite("display_name", &FormItem::display_name)
        .def_readwrite("lore", &FormItem::lore)
        .def_readwrite("enchants", &FormItem::enchants)
        .def_readwrite("custom_nbt_snbt", &FormItem::custom_nbt_snbt);

    py::class_<ChestForm>(m, "ChestForm")
        .def(py::init<endstone::Plugin&, std::string, ChestSize>(),
             py::arg("plugin"), py::arg("title"), py::arg("size") = ChestSize::Double)
        .def("set_slot", [](ChestForm& self, int slot, FormItem item, std::function<void(endstone::Player&, int)> callback) {
            self.setSlot(slot, item, callback);
            return &self;
        }, py::arg("slot"), py::arg("item"), py::arg("callback") = nullptr, py::return_value_policy::reference)
        .def("fill_slots", &ChestForm::fillSlots, py::return_value_policy::reference)
        .def("clear_slot", &ChestForm::clearSlot, py::return_value_policy::reference)
        .def("send_to", &ChestForm::sendTo)
        .def("close", &ChestForm::close);
}
```

### 2. Packaging as a Python Wheel (`.whl`)

Using `scikit-build-core` or `setuptools`, you can bundle the compiled binary as a `.whl` package. Add a `pyproject.toml` to compile the CMake project as a Python wheel:

```toml
[build-system]
requires = ["scikit-build-core>=0.5.0", "pybind11>=2.11.0"]
build-backend = "scikit-build-core.build"

[project]
name = "endstone-chestform-api"
version = "2.0.0"
description = "Python wrapper for Endstone C++ ChestFormAPI"
readme = "README.md"
requires-python = ">=3.9"
dependencies = [
    "endstone>=0.11.0"
]
```

Build the wheel using:
```bash
pip install build
python -m build --wheel
```

This generates a `.whl` package in the `dist/` directory, which can be installed in any Endstone server environment using:
```bash
pip install dist/endstone_chestform_api-2.0.0-*.whl
```

### 3. Example Python API Usage

Once the wheel is installed, you can import and use `endstone_chestform_api` in any Python Endstone plugin:

```python
from endstone.plugin import Plugin
from endstone.player import Player
from endstone_chestform_api import ChestForm, FormItem, ChestSize

class MyPythonPlugin(Plugin):
    def on_enable(self) -> None:
        self.register_events(self)
        self.logger.info("Python ChestForm example loaded!")

    def open_selector(self, player: Player):
        # 1. Create the Chest Form
        form = ChestForm(self, "§bPython Item Selector", ChestSize.DOUBLE)

        # 2. Add an item with custom properties and a callback
        diamond = FormItem()
        diamond.type_id = "minecraft:diamond"
        diamond.display_name = "§bFree Diamond"
        diamond.lore = ["§7Click to claim your diamond!"]
        
        # Callbacks are fully supported using Python functions or lambdas
        def on_click(p: Player, slot: int):
            p.send_message("§a[ChestForm] You clicked the diamond!")
            p.inventory.add_item(p.server.create_item_stack("minecraft:diamond", 1))

        form.set_slot(13, diamond, on_click)

        # 3. Add a close button
        barrier = FormItem()
        barrier.type_id = "minecraft:barrier"
        barrier.display_name = "§cClose Menu"
        form.set_slot(31, barrier, lambda p, slot: form.close(p))

        # 4. Open UI for player
        form.send_to(player)
```

---

## Example C++ API Usage

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

---

## Test Commands

The plugin includes a test command `/chestformtest` registered for operators.
Executing this command opens a demo menu showing different item styles (fillers, custom names, lore, and click triggers).
See [compatibility_notes.md](compatibility_notes.md) for more details on Bedrock network packet specifications.
