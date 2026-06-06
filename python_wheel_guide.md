# How-To: Build and Package Python Endstone Plugins using ChestFormAPI

This guide explains how to build, package, and deploy a Python-based Endstone plugin as a Python Wheel (`.whl`) that uses the `ChestFormAPI` to create and handle fake chest inventory GUIs.

---

## Prerequisites

Before starting, ensure that:
1. You have an Endstone server running.
2. The C++ backend plugin (`endstone_chest_form_cpp.so`) is placed in the server's `plugins/` folder.
3. The Python bindings wheel (`endstone_chest_form-1.0.0-*.whl`) is compiled and installed in your server's Python environment:
   ```bash
   pip install endstone_chest_form-1.0.0-py3-none-any.whl
   ```

---

## Step 1: Set Up Your Project Structure

Create a directory layout for your Python plugin project:

```text
my_selector_plugin/
├── pyproject.toml
├── README.md
└── src/
    └── my_selector_plugin/
        ├── __init__.py
        └── plugin.py
```

---

## Step 2: Configure `pyproject.toml`

The `pyproject.toml` file configures the python packaging metadata and declares dependencies. Under `dependencies`, add `endstone` and `endstone_chest_form`:

```toml
[build-system]
requires = ["setuptools>=61.0.0", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "my-selector-plugin"
version = "1.0.0"
description = "A Python Endstone plugin showcasing fake chest GUI forms"
readme = "README.md"
requires-python = ">=3.9"
classifiers = [
    "Programming Language :: Python :: 3",
    "License :: OSI Approved :: MIT License",
    "Operating System :: OS Independent",
]
dependencies = [
    "endstone>=0.11.0",
    "endstone_chest_form>=1.0.0"
]

[project.entry-points."endstone.plugins"]
my_selector_plugin = "my_selector_plugin.plugin:MySelectorPlugin"
```

> [!IMPORTANT]
> The `[project.entry-points."endstone.plugins"]` section tells the Endstone server how to locate your plugin's main entry point class (`MySelectorPlugin` inside `src/my_selector_plugin/plugin.py`).

---

## Step 3: Implement the Python Plugin Code

Define the plugin metadata, register a test command `/menu`, and write the logic to open the fake chest GUI.

### `src/my_selector_plugin/__init__.py`
Leave this blank or add package imports.

### `src/my_selector_plugin/plugin.py`
```python
from endstone.plugin import Plugin
from endstone.command import Command, CommandSender
from endstone.player import Player
from endstone.event import event_handler, PlayerQuitEvent

# Import the ChestForm classes from our compiled bindings module
from endstone_chest_form import ChestForm, FormItem, ChestSize

class MySelectorPlugin(Plugin):
    # Metadata parameters matching the C++ counterpart
    prefix = "SelectorPlugin"
    
    def on_enable(self):
        self.register_events(self)
        self.logger.info("Python Selector Plugin loaded successfully!")

    def on_command(self, sender: CommandSender, command: Command, args: list[str]) -> bool:
        if command.name == "menu":
            # Command can only be executed by players
            player = sender if isinstance(sender, Player) else None
            if not player:
                sender.send_error_message("This command can only be executed by a player.")
                return True

            self.open_custom_menu(player)
            return True
        return False

    def open_custom_menu(self, player: Player):
        # 1. Create a Double Chest UI
        form = ChestForm(self, "§bPython Kit Selector", ChestSize.DOUBLE)

        # 2. Add an interactive item to Slot 13 (Give kit)
        kit_item = FormItem()
        kit_item.type_id = "minecraft:diamond_block"
        kit_item.display_name = "§aClaim Builder Kit"
        kit_item.lore = [
            "§7Click to receive:",
            "§f- 64x Stone",
            "§f- 1x Diamond Axe"
        ]

        def on_kit_click(p: Player, slot: int):
            p.send_message("§a[Selector] Claiming builder kit...")
            # Give items using player inventory API
            p.inventory.add_item(p.server.create_item_stack("minecraft:stone", 64))
            p.inventory.add_item(p.server.create_item_stack("minecraft:diamond_axe", 1))

        form.set_slot(13, kit_item, on_kit_click)

        # 3. Add an interactive item to Slot 22 (Send message)
        stats_item = FormItem()
        stats_item.type_id = "minecraft:paper"
        stats_item.display_name = "§eShow My Stats"
        stats_item.lore = ["§7Click to print stats in chat!"]

        def on_stats_click(p: Player, slot: int):
            p.send_message(f"§e[Stats] Player name: {p.name}")
            p.send_message(f"§e[Stats] Health: {p.health}/{p.max_health}")

        form.set_slot(22, stats_item, on_stats_click)

        # 4. Fill filler glass panes to create a premium frame aesthetic
        filler = FormItem()
        filler.type_id = "minecraft:stained_glass_pane"
        filler.aux = 15  # Black colored pane
        filler.display_name = " "

        for i in range(54):
            # Fill borders
            is_border = (i < 9) or (i >= 45) or (i % 9 == 0) or (i % 9 == 8)
            if is_border:
                form.set_slot(i, filler)

        # 5. Add a close button at the bottom center
        close_btn = FormItem()
        close_btn.type_id = "minecraft:barrier"
        close_btn.display_name = "§cClose Menu"
        form.set_slot(49, close_btn, lambda p, slot: form.close(p))

        # 6. Send/Open the form UI to the player
        form.send_to(player)

    # Automatically handle block cleanups if the player quits while GUI is open
    @event_handler
    def on_player_quit(self, event: PlayerQuitEvent):
        # The C++ manager handles cleanup safely
        pass
```

---

## Step 4: Package Your Plugin into a Python Wheel (`.whl`)

Build the package using the Python `build` module. Run this command in the root folder of your project (`my_selector_plugin/`):

```bash
# Install packaging tools
pip install build

# Compile and build the wheel file
python -m build --wheel
```

This generates a file named `dist/my_selector_plugin-1.0.0-py3-none-any.whl`.

---

## Step 5: Deploy the Wheel to the Endstone Server

Copy the wheel file to your target Endstone server and install it:

```bash
pip install my_selector_plugin-1.0.0-py3-none-any.whl
```

When you start your Endstone Dedicated Server, it will detect the plugin entrypoint, register the `/menu` command, and handle the chest GUI callbacks in pure Python!
