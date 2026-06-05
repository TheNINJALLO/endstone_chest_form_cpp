#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "export.h"

struct CHEST_FORM_API FormItem {
    std::string type_id; // e.g., "minecraft:diamond_block"
    int amount = 1;
    int aux = 0;
    std::string display_name;
    std::vector<std::string> lore;
    std::unordered_map<std::string, int> enchants; // enchantment ID -> level
    std::optional<std::string> custom_nbt_snbt;
};
