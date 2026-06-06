#include "chest_form_api/chest_form.h"
#include "chest_form_api/form_item.h"
#include "chest_form_api/chest_form_manager.h"
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/server.h>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

// Definition of the Python callback function pointer type
using PythonCallback = void (*)(const char* player_name, int slot, void* callback_id);

static PythonCallback g_python_callback = nullptr;

static void parse_enchants(const char* enchants_str, FormItem& item) {
    if (enchants_str && std::strlen(enchants_str) > 0) {
        std::string s(enchants_str);
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto colon = token.find(':');
            if (colon != std::string::npos) {
                auto id = token.substr(0, colon);
                auto lvl_str = token.substr(colon + 1);
                try {
                    item.enchants[id] = std::stoi(lvl_str);
                } catch (...) {}
            }
        }
    }
}

extern "C" {

CHEST_FORM_API void chest_form_register_callback_handler(PythonCallback callback) {
    g_python_callback = callback;
}

CHEST_FORM_API void* chest_form_create(const char* title, int size) {
    auto* plugin = ChestFormManager::getInstance().getPlugin();
    if (!plugin) return nullptr;
    auto size_enum = static_cast<ChestSize>(size);
    auto* form = new ChestForm(*plugin, title ? title : "", size_enum);
    return form;
}

CHEST_FORM_API void chest_form_destroy(void* form_ptr) {
    if (form_ptr) {
        delete static_cast<ChestForm*>(form_ptr);
    }
}

CHEST_FORM_API void chest_form_set_slot(void* form_ptr, int slot, const char* type_id, int amount, int aux,
                               const char* display_name, const char** lore, int lore_size,
                               const char* enchants_str, const char* custom_nbt_snbt, void* callback_id) {
    if (!form_ptr) return;
    auto* form = static_cast<ChestForm*>(form_ptr);

    FormItem item;
    item.type_id = type_id ? type_id : "minecraft:air";
    item.amount = amount;
    item.aux = aux;
    item.display_name = display_name ? display_name : "";
    if (lore && lore_size > 0) {
        for (int i = 0; i < lore_size; ++i) {
            if (lore[i]) {
                item.lore.push_back(lore[i]);
            }
        }
    }
    parse_enchants(enchants_str, item);
    if (custom_nbt_snbt && std::strlen(custom_nbt_snbt) > 0) {
        item.custom_nbt_snbt = custom_nbt_snbt;
    }

    if (callback_id) {
        auto callback = [callback_id](endstone::Player& p, int slot_idx) {
            if (g_python_callback) {
                g_python_callback(p.getName().c_str(), slot_idx, callback_id);
            }
        };
        form->setSlot(slot, item, callback);
    } else {
        form->setSlot(slot, item, nullptr);
    }
}

CHEST_FORM_API void chest_form_fill_slots(void* form_ptr, const char* type_id, int amount, int aux,
                                const char* display_name, const char** lore, int lore_size,
                                const char* enchants_str, const char* custom_nbt_snbt) {
    if (!form_ptr) return;
    auto* form = static_cast<ChestForm*>(form_ptr);

    FormItem item;
    item.type_id = type_id ? type_id : "minecraft:air";
    item.amount = amount;
    item.aux = aux;
    item.display_name = display_name ? display_name : "";
    if (lore && lore_size > 0) {
        for (int i = 0; i < lore_size; ++i) {
            if (lore[i]) {
                item.lore.push_back(lore[i]);
            }
        }
    }
    parse_enchants(enchants_str, item);
    if (custom_nbt_snbt && std::strlen(custom_nbt_snbt) > 0) {
        item.custom_nbt_snbt = custom_nbt_snbt;
    }

    form->fillSlots(item);
}

CHEST_FORM_API void chest_form_clear_slot(void* form_ptr, int slot) {
    if (form_ptr) {
        static_cast<ChestForm*>(form_ptr)->clearSlot(slot);
    }
}

CHEST_FORM_API void chest_form_set_title(void* form_ptr, const char* title) {
    if (form_ptr) {
        static_cast<ChestForm*>(form_ptr)->setTitle(title ? title : "");
    }
}

CHEST_FORM_API void chest_form_set_size(void* form_ptr, int size) {
    if (form_ptr) {
        static_cast<ChestForm*>(form_ptr)->setSize(static_cast<ChestSize>(size));
    }
}

CHEST_FORM_API void chest_form_send_to(void* form_ptr, const char* player_name) {
    if (!form_ptr || !player_name) return;
    auto* form = static_cast<ChestForm*>(form_ptr);
    auto* player = form->getPlugin().getServer().getPlayer(player_name);
    if (player) {
        form->sendTo(*player);
    }
}

CHEST_FORM_API void chest_form_close(void* form_ptr, const char* player_name) {
    if (!form_ptr || !player_name) return;
    auto* form = static_cast<ChestForm*>(form_ptr);
    auto* player = form->getPlugin().getServer().getPlayer(player_name);
    if (player) {
        form->close(*player);
    }
}

CHEST_FORM_API void chest_form_update(void* form_ptr, const char* player_name) {
    if (!form_ptr || !player_name) return;
    auto* form = static_cast<ChestForm*>(form_ptr);
    auto* player = form->getPlugin().getServer().getPlayer(player_name);
    if (player) {
        form->update(*player);
    }
}

}
