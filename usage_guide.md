# ChestFormAPI Developer & Usage Guide

`ChestFormAPI` is a native C++23 Endstone library/plugin designed to open virtual chest GUIs entirely server-side using Bedrock packet-spoofing. It requires **no client-side resource packs or addons** and allows you to create interactive menus, custom buttons, and mirror player containers (e.g. Ender Chests and inventories).

This guide covers usage in both **C++** and **Python**.

---

## 1. Installation & Dependencies

To use `ChestFormAPI` in your plugin project, you must set it up as a dependency.

### C++ Plugins (`CMakeLists.txt`)
Include the headers and link the library:
```cmake
# Include ChestFormAPI headers
target_include_directories(my_plugin PRIVATE path/to/ChestformAPI/include)

# Link against the ChestFormAPI shared library
target_link_libraries(my_plugin PRIVATE chestform_api)
```

### Python Plugins (`pyproject.toml`)
Add `endstone-chestform-api` to your dependencies. You can install it via PyPI or directly from the GitHub repository:
```toml
dependencies = [
    "endstone>=0.11.0",
    "endstone-chestform-api"
]
```

---

## 2. Basic Usage (27 or 54 Slots, Custom Title)

You can choose between a **Single Chest (27 slots)** or a **Double Chest (54 slots)** using the `ChestSize` enum, and configure any title string you want.

### C++
```cpp
#include "chest_form_api/chest_form.h"
#include <endstone/player.h>

void openSimpleForm(endstone::Plugin& plugin, endstone::Player& player) {
    // Create a 27-slot chest form with a customized title
    ChestForm form(plugin, "§dMy Virtual Vault", ChestSize::Single);

    // Set slot 13 (center of single chest) to a Golden Apple
    FormItem apple;
    apple.type_id = "minecraft:golden_apple";
    apple.display_name = "§6Golden Apple";
    apple.lore = {"§7Tastes delicious!", "§eClick to eat!"};
    
    form.setSlot(13, apple, [](endstone::Player& p, int slot) {
        p.sendMessage("§aYou clicked the golden apple!");
    });

    // Display the chest form to the player
    form.sendTo(player);
}
```

### Python
```python
from endstone.player import Player
from endstone.plugin import Plugin
from endstone_chestform_api import ChestForm, FormItem, ChestSize

def open_simple_form(plugin: Plugin, player: Player):
    # Create a 27-slot chest form with a customized title
    form = ChestForm(plugin, "§dMy Virtual Vault", ChestSize.SINGLE)

    # Set slot 13 (center of single chest) to a Golden Apple
    apple = FormItem()
    apple.type_id = "minecraft:golden_apple"
    apple.display_name = "§6Golden Apple"
    apple.lore = ["§7Tastes delicious!", "§eClick to eat!"]

    def on_click(p: Player, slot: int):
        p.send_message("§aYou clicked the golden apple!")

    form.set_slot(13, apple, on_click)

    # Display the chest form to the player
    form.send_to(player)
```

---

## 3. Dynamic Updates (Renaming & Changing Size)

The API supports modifying the layout on the fly. If you modify the slots, call `update` to push changes to the open GUI. If you change the title or size, calling `update` will automatically recreate the window.

### C++
```cpp
// Change title dynamically
form.setTitle("§bUpdated Title");

// Change size to double chest
form.setSize(ChestSize::Double);

// Re-send or update for the player
form.update(player);
```

### Python
```python
# Change title dynamically
form.set_title("§bUpdated Title")

# Change size to double chest
form.set_size(ChestSize.DOUBLE)

# Re-send or update for the player
form.update(player)
```

---

## 4. Building Interactive Buttons

Use slot callbacks to create buttons that trigger actions, close the inventory, or transition to other pages.

### C++
```cpp
// 1. Close Button
FormItem close_button;
close_button.type_id = "minecraft:barrier";
close_button.display_name = "§cClose Menu";
form.setSlot(8, close_button, [](endstone::Player& p, int slot) {
    ChestFormManager::getInstance().closeForm(p);
});

// 2. Command Trigger Button
FormItem spawn_teleport;
spawn_teleport.type_id = "minecraft:ender_pearl";
spawn_teleport.display_name = "§bTeleport to Spawn";
form.setSlot(4, spawn_teleport, [](endstone::Player& p, int slot) {
    p.performCommand("spawn");
    ChestFormManager::getInstance().closeForm(p);
});
```

### Python
```python
from endstone_chest_form import ChestFormManager

# 1. Close Button
close_button = FormItem()
close_button.type_id = "minecraft:barrier"
close_button.display_name = "§cClose Menu"
form.set_slot(8, close_button, lambda p, slot: form.close(p))

# 2. Command Trigger Button
spawn_teleport = FormItem()
spawn_teleport.type_id = "minecraft:ender_pearl"
spawn_teleport.display_name = "§bTeleport to Spawn"

def on_teleport(p: Player, slot: int):
    p.perform_command("spawn")
    form.close(p)

form.set_slot(4, spawn_teleport, on_teleport)
```

---

## 5. Mirroring Containers (Ender Chests & Inventories)

You can use the API to let players view and interact with real container inventories, such as their **Ender Chest**. Since the chest is virtual, you must handle slot click/take/deposit actions inside the callback and sync the changes back to the actual container.

### C++
```cpp
#include "chest_form_api/chest_form.h"
#include "chest_form_api/chest_form_manager.h"
#include <endstone/player.h>
#include <endstone/inventory/inventory.h>
#include <endstone/inventory/item_stack.h>

void openEnderChestMirror(endstone::Plugin& plugin, endstone::Player& player) {
    auto form = std::make_shared<ChestForm>(plugin, "§5Your Ender Chest", ChestSize::Single);
    auto& ender_chest = player.getInventory(); // Example: Using main inventory for demonstration

    // Populate the virtual ChestForm slots from the real container
    for (int i = 0; i < 27; ++i) {
        auto* item = ender_chest.getItem(i);
        if (item && item->getType().getId() != "minecraft:air") {
            FormItem form_item;
            form_item.type_id = item->getType().getId();
            form_item.amount = item->getAmount();
            form_item.aux = item->getData();
            
            form->setSlot(i, form_item, [form, &ender_chest](endstone::Player& p, int clicked_slot) {
                auto* real_item = ender_chest.getItem(clicked_slot);
                if (real_item) {
                    p.sendMessage("§d[Ender Chest] You clicked item: " + real_item->getType().getId());
                    p.getInventory().addItem(*real_item);
                    ender_chest.clear(clicked_slot);
                    form->clearSlot(clicked_slot);
                    form->update(p);
                }
            });
        } else {
            form->clearSlot(i);
        }
    }

    form->sendTo(player);
}
```

### Python
```python
from endstone.player import Player
from endstone.plugin import Plugin
from endstone.inventory import ItemStack
from endstone_chestform_api import ChestForm, FormItem, ChestSize

def open_ender_chest_mirror(plugin: Plugin, player: Player):
    form = ChestForm(plugin, "§5Your Ender Chest", ChestSize.SINGLE)
    ender_chest = player.inventory # Example: Using main inventory for demonstration

    for i in range(27):
        item = ender_chest.get_item(i)
        if item and item.type.id != "minecraft:air":
            form_item = FormItem()
            form_item.type_id = item.type.id
            form_item.amount = item.amount
            form_item.aux = item.data

            # Capture references using defaults in lambda or local functions
            def make_callback(slot_index):
                def callback(p: Player, slot: int):
                    real_item = ender_chest.get_item(slot_index)
                    if real_item:
                        p.send_message(f"§d[Ender Chest] You clicked: {real_item.type.id}")
                        p.inventory.add_item(real_item)
                        ender_chest.clear(slot_index)
                        form.clear_slot(slot_index)
                        form.update(p)
                return callback

            form.set_slot(i, form_item, make_callback(i))
        else:
            form.clear_slot(i)

    form.send_to(player)
```

---

## 6. Creating Dynamic Multi-Page Menus

You can implement paging by clearing slots and repopulating them inside page-button callbacks, then calling `update`.

### C++
```cpp
void openPaginatedMenu(endstone::Plugin& plugin, endstone::Player& player, int page) {
    auto form = std::make_shared<ChestForm>(plugin, "Menu - Page " + std::to_string(page), ChestSize::Double);

    // Border Fillers
    FormItem glass;
    glass.type_id = "minecraft:stained_glass_pane";
    glass.aux = 15;
    glass.display_name = " ";
    for (int i = 0; i < 9; ++i) form->setSlot(i, glass);
    for (int i = 45; i < 54; ++i) form->setSlot(i, glass);

    // Page Navigation Buttons
    if (page > 1) {
        FormItem prev;
        prev.type_id = "minecraft:arrow";
        prev.display_name = "§e<- Previous Page";
        form->setSlot(45, prev, [&plugin, page](endstone::Player& p, int slot) {
            openPaginatedMenu(plugin, p, page - 1);
        });
    }

    FormItem next;
    next.type_id = "minecraft:arrow";
    next.display_name = "§eNext Page ->";
    form->setSlot(53, next, [&plugin, page](endstone::Player& p, int slot) {
        openPaginatedMenu(plugin, p, page + 1);
    });

    if (page == 1) {
        FormItem item1;
        item1.type_id = "minecraft:diamond";
        item1.display_name = "§bDiamond Page 1";
        form->setSlot(22, item1);
    } else if (page == 2) {
        FormItem item2;
        item2.type_id = "minecraft:emerald";
        item2.display_name = "§aEmerald Page 2";
        form->setSlot(22, item2);
    }

    form->sendTo(player);
}
```

### Python
```python
def open_paginated_menu(plugin: Plugin, player: Player, page: int):
    form = ChestForm(plugin, f"Menu - Page {page}", ChestSize.DOUBLE)

    # Border Fillers
    glass = FormItem()
    glass.type_id = "minecraft:stained_glass_pane"
    glass.aux = 15
    glass.display_name = " "
    for i in range(9):
        form.set_slot(i, glass)
    for i in range(45, 54):
        form.set_slot(i, glass)

    # Page Navigation Buttons
    if page > 1:
        prev_btn = FormItem()
        prev_btn.type_id = "minecraft:arrow"
        prev_btn.display_name = "§e<- Previous Page"
        form.set_slot(45, prev_btn, lambda p, slot: open_paginated_menu(plugin, p, page - 1))

    next_btn = FormItem()
    next_btn.type_id = "minecraft:arrow"
    next_btn.display_name = "§eNext Page ->"
    form.set_slot(53, next_btn, lambda p, slot: open_paginated_menu(plugin, p, page + 1))

    if page == 1:
        item1 = FormItem()
        item1.type_id = "minecraft:diamond"
        item1.display_name = "§bDiamond Page 1"
        form.set_slot(22, item1)
    elif page == 2:
        item2 = FormItem()
        item2.type_id = "minecraft:emerald"
        item2.display_name = "§aEmerald Page 2"
        form.set_slot(22, item2)

    form.send_to(player)

---

## 7. Advanced: Clone & Delete in Container Mirroring

When viewing player inventories or Ender Chests, you can set up options to copy (clone) item stacks or delete them from the container by opening a sub-menu when clicking on a slot.

### C++
```cpp
void openManagementMenu(endstone::Plugin& plugin, endstone::Player& admin, endstone::Player& target, int target_slot) {
    auto form = std::make_shared<ChestForm>(plugin, "Manage Slot " + std::to_string(target_slot), ChestSize::Single);

    auto& target_inv = target.getInventory();
    auto target_item = target_inv.getItem(target_slot);
    if (!target_item || target_item->getType().getId() == "minecraft:air") {
        admin.sendMessage("§cThat slot is empty!");
        return;
    }

    // Preview slot item (Slot 4)
    FormItem preview;
    preview.type_id = target_item->getType().getId();
    preview.amount = target_item->getAmount();
    preview.aux = target_item->getData();
    form->setSlot(4, preview);

    // Clone button (Slot 11)
    FormItem clone_btn;
    clone_btn.type_id = "minecraft:emerald";
    clone_btn.display_name = "§aClone Item";
    clone_btn.lore = {"§7Copy item stack to your inventory"};
    form->setSlot(11, clone_btn, [preview](endstone::Player& p, int slot) {
        p.getInventory().addItem(endstone::ItemStack(preview.type_id, preview.amount, preview.aux));
        p.sendMessage("§a[ChestForm] Cloned item.");
    });

    // Delete button (Slot 13)
    FormItem delete_btn;
    delete_btn.type_id = "minecraft:redstone_block";
    delete_btn.display_name = "§cDelete Item";
    delete_btn.lore = {"§7Remove item from player's inventory"};
    form->setSlot(13, delete_btn, [&target_inv, target_slot](endstone::Player& p, int slot) {
        target_inv.setItem(target_slot, std::nullopt);
        p.sendMessage("§a[ChestForm] Deleted item.");
    });

    form->sendTo(admin);
}
```

### Python
```python
def open_management_menu(plugin: Plugin, admin: Player, target: Player, target_slot: int):
    form = ChestForm(plugin, f"Manage Slot {target_slot}", ChestSize.SINGLE)

    target_inv = target.inventory
    target_item = target_inv.get_item(target_slot)
    if not target_item or target_item.type.id == "minecraft:air":
        admin.send_message("§cThat slot is empty!")
        return

    # Preview slot item (Slot 4)
    preview = FormItem()
    preview.type_id = target_item.type.id
    preview.amount = target_item.amount
    preview.aux = target_item.data
    form.set_slot(4, preview)

    # Clone button (Slot 11)
    clone_btn = FormItem()
    clone_btn.type_id = "minecraft:emerald"
    clone_btn.display_name = "§aClone Item"
    clone_btn.lore = ["§7Copy item stack to your inventory"]
    
    def on_clone(p: Player, slot: int):
        p.inventory.add_item(ItemStack(preview.type_id, preview.amount, preview.aux))
        p.send_message("§a[ChestForm] Cloned item.")
        
    form.set_slot(11, clone_btn, on_clone)

    # Delete button (Slot 13)
    delete_btn = FormItem()
    delete_btn.type_id = "minecraft:redstone_block"
    delete_btn.display_name = "§cDelete Item"
    delete_btn.lore = ["§7Remove item from player's inventory"]
    
    def on_delete(p: Player, slot: int):
        target_inv.clear(target_slot) # Or set_item(target_slot, None)
        p.send_message("§a[ChestForm] Deleted item.")
        
    form.set_slot(13, delete_btn, on_delete)

    form.send_to(admin)
```

```
