# ChestFormAPI Developer & Usage Guide

`ChestFormAPI` is a native C++23 Endstone library/plugin designed to open virtual chest GUIs entirely server-side using Bedrock packet-spoofing. It requires **no client-side resource packs or addons** and allows you to create interactive menus, custom buttons, and mirror player containers (e.g. Ender Chests and inventories).

---

## 1. Adding as a Dependency

To use `ChestFormAPI` in your own Endstone C++ plugin, add it to your `CMakeLists.txt`.

### CMake Configuration

If you're compiling against it, include the API headers and link the library:

```cmake
# Include ChestFormAPI headers
target_include_directories(my_plugin PRIVATE path/to/ChestformAPI/include)

# Link against the ChestFormAPI shared library
target_link_libraries(my_plugin PRIVATE chest_form_cpp)
```

---

## 2. Basic Usage (27 or 54 Slots, Custom Title)

You can choose between a **Single Chest (27 slots)** or a **Double Chest (54 slots)** using the `ChestSize` enum, and configure any title string you want.

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

---

## 3. Dynamic Updates (Renaming & Changing Size)

The API supports modifying the layout on the fly. If you modify the slots, call `form.update(player)` to push changes to the open GUI. If you change the title or size, calling `update` will automatically recreate the window.

```cpp
// Change title dynamically
form.setTitle("§bUpdated Title");

// Change size to double chest
form.setSize(ChestSize::Double);

// Re-send or update for the player
form.update(player);
```

---

## 4. Building Interactive Buttons

Use the slot callbacks to create buttons that trigger actions, close the inventory, or transition to other pages.

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

---

## 5. Mirroring Containers (Ender Chests & Inventories)

You can use the API to let players view and interact with real container inventories, such as their **Ender Chest**. Since the chest is virtual, you must handle slot click/take/deposit actions inside the callback and sync the changes back to the actual container.

Here is a full example showing how to mirror a player's **Ender Chest** into a virtual chest GUI:

```cpp
#include "chest_form_api/chest_form.h"
#include "chest_form_api/chest_form_manager.h"
#include <endstone/player.h>
#include <endstone/inventory/inventory.h>
#include <endstone/inventory/item_stack.h>

void openEnderChestMirror(endstone::Plugin& plugin, endstone::Player& player) {
    // Ender Chests have 27 slots
    auto form = std::make_shared<ChestForm>(plugin, "§5Your Ender Chest", ChestSize::Single);
    
    // Reference the player's actual ender chest inventory
    // Note: Assuming Endstone provides access to the player's Ender Chest inventory.
    // If Ender Chest is not directly exposed as an Endstone inventory type yet, 
    // you can use the player's main inventory, or manage a virtual custom container map.
    auto& ender_chest = player.getInventory(); // Example: Using main inventory for demonstration

    // 1. Populate the virtual ChestForm slots from the real container
    for (int i = 0; i < 27; ++i) {
        auto* item = ender_chest.getItem(i);
        if (item && item->getType().getId() != "minecraft:air") {
            FormItem form_item;
            form_item.type_id = item->getType().getId();
            form_item.amount = item->getAmount();
            form_item.aux = item->getData();
            // Optional: copy display name/lore if available in Endstone API
            
            // Set the slot and define click handler
            form->setSlot(i, form_item, [form, &ender_chest](endstone::Player& p, int clicked_slot) {
                // Get the clicked item in the real inventory
                auto* real_item = ender_chest.getItem(clicked_slot);
                if (real_item) {
                    p.sendMessage("§d[Ender Chest] You clicked item: " + real_item->getType().getId());
                    
                    // Example: Transfer the item to the player's active cursor/main inventory
                    p.getInventory().addItem(*real_item);
                    
                    // Remove from the Ender Chest
                    ender_chest.clear(clicked_slot);
                    
                    // Remove from our virtual form layout
                    form->clearSlot(clicked_slot);
                    
                    // Push the updated container state to the player's client GUI
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

---

## 6. Creating Dynamic Multi-Page Menus

You can implement paging by clearing slots and repopulating them inside page-button callbacks, then calling `update`.

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

    // Populate Page-Specific Content
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
