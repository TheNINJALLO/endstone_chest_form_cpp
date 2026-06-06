#include "chest_form_api/chest_form_manager.h"
#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/plugin/plugin.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/level/dimension.h>
#include <endstone/block/block.h>
#include <endstone/block/block_data.h>
#include <cmath>
#include <cstring>
#include <vector>

// SculkCatalystMC/Protocol Headers
#include <sculk/protocol/codec/packet/UpdateBlockPacket.hpp>
#include <sculk/protocol/codec/packet/BlockActorDataPacket.hpp>
#include <sculk/protocol/codec/packet/ContainerOpenPacket.hpp>
#include <sculk/protocol/codec/packet/ContainerClosePacket.hpp>
#include <sculk/protocol/codec/packet/InventoryContentPacket.hpp>
#include <sculk/protocol/codec/packet/ItemStackRequestPacket.hpp>
#include <sculk/protocol/codec/packet/ItemRegistryPacket.hpp>
#include <sculk/protocol/utility/BinaryStream.hpp>
#include <sculk/protocol/utility/ReadOnlyBinaryStream.hpp>
#include <sculk/protocol/codec/nbt/CompoundTag.hpp>
#include <sculk/protocol/codec/nbt/TagVariant.hpp>
#include <sculk/protocol/codec/nbt/ValueTag.hpp>

static void sendPacketHelper(endstone::Player& player, const sculk::protocol::IPacket& packet) {
    std::vector<std::byte> buffer;
    sculk::protocol::BinaryStream stream(buffer);
    packet.write(stream);
    std::string payload(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    player.sendPacket(static_cast<int>(packet.getId()), payload);
}

static sculk::protocol::NetworkItemStackDescriptor serializeFormItem(const FormItem& item) {
    sculk::protocol::NetworkItemStackDescriptor desc;
    desc.mId = ChestFormManager::getInstance().getItemId(item.type_id);
    if (desc.mId == 0) {
        desc.mStackSize = 0;
        desc.mAux = 0;
        desc.mBlockRuntimeId = 0;
        return desc;
    }

    desc.mStackSize = static_cast<std::uint16_t>(item.amount);
    desc.mAux = static_cast<std::uint32_t>(item.aux);
    desc.mBlockRuntimeId = 0;

    // Display Name and Lore
    sculk::protocol::CompoundTag display_compound;
    if (!item.display_name.empty()) {
        display_compound.mValue["Name"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{item.display_name}};
    }
    if (!item.lore.empty()) {
        sculk::protocol::ListTag lore_list;
        lore_list.mType = sculk::protocol::TagType::String;
        for (const auto& line : item.lore) {
            lore_list.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::StringTag{line}});
        }
        display_compound.mValue["Lore"] = sculk::protocol::TagVariant{lore_list};
    }

    sculk::protocol::CompoundTag main_compound;
    if (!display_compound.mValue.empty()) {
        main_compound.mValue["display"] = sculk::protocol::TagVariant{display_compound};
    }

    // Enchantments
    if (!item.enchants.empty()) {
        sculk::protocol::ListTag enchant_list;
        enchant_list.mType = sculk::protocol::TagType::Compound;
        for (const auto& [id_str, lvl] : item.enchants) {
            sculk::protocol::CompoundTag enchant_tag;
            short id = 0;
            try {
                id = static_cast<short>(std::stoi(id_str));
            } catch (...) {
                // If it is a string representation like "protection", default to 0
            }
            enchant_tag.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::ShortTag{id}};
            enchant_tag.mValue["lvl"] = sculk::protocol::TagVariant{sculk::protocol::ShortTag{static_cast<short>(lvl)}};
            enchant_list.mValue.push_back(sculk::protocol::TagVariant{enchant_tag});
        }
        main_compound.mValue["ench"] = sculk::protocol::TagVariant{enchant_list};
    }

    // Serialize NBT to UserData
    std::vector<std::byte> user_bytes;
    sculk::protocol::BinaryStream user_stream(user_bytes);
    if (!main_compound.mValue.empty()) {
        user_stream.writeSignedShort(-1); // NBT count indicator
        main_compound.serialize(user_stream);
    } else {
        user_stream.writeSignedShort(0);  // No NBT
    }
    user_stream.writeSignedInt(0); // can_place_on block count
    user_stream.writeSignedInt(0); // can_destroy block count

    desc.mUserData = std::string(user_stream.asStringView());
    return desc;
}

static sculk::protocol::CompoundTag serializeFormItemToNbt(const FormItem& item, std::uint8_t slot) {
    sculk::protocol::CompoundTag nbt;
    if (item.type_id.empty() || item.type_id == "minecraft:air") {
        return nbt;
    }

    nbt.mValue["Slot"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{static_cast<std::int8_t>(slot)}};
    nbt.mValue["Name"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{item.type_id}};
    nbt.mValue["Count"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{static_cast<std::int8_t>(item.amount)}};
    nbt.mValue["Damage"] = sculk::protocol::TagVariant{sculk::protocol::ShortTag{static_cast<std::int16_t>(item.aux)}};
    nbt.mValue["WasPickedUp"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};

    // Display Name and Lore
    sculk::protocol::CompoundTag display_compound;
    if (!item.display_name.empty()) {
        display_compound.mValue["Name"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{item.display_name}};
    }
    if (!item.lore.empty()) {
        sculk::protocol::ListTag lore_list;
        lore_list.mType = sculk::protocol::TagType::String;
        for (const auto& line : item.lore) {
            lore_list.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::StringTag{line}});
        }
        display_compound.mValue["Lore"] = sculk::protocol::TagVariant{lore_list};
    }

    sculk::protocol::CompoundTag main_compound;
    if (!display_compound.mValue.empty()) {
        main_compound.mValue["display"] = sculk::protocol::TagVariant{display_compound};
    }

    // Enchantments
    if (!item.enchants.empty()) {
        sculk::protocol::ListTag enchant_list;
        enchant_list.mType = sculk::protocol::TagType::Compound;
        for (const auto& [id_str, lvl] : item.enchants) {
            sculk::protocol::CompoundTag enchant_tag;
            short id = 0;
            try {
                id = static_cast<short>(std::stoi(id_str));
            } catch (...) {}
            enchant_tag.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::ShortTag{id}};
            enchant_tag.mValue["lvl"] = sculk::protocol::TagVariant{sculk::protocol::ShortTag{static_cast<short>(lvl)}};
            enchant_list.mValue.push_back(sculk::protocol::TagVariant{enchant_tag});
        }
        main_compound.mValue["ench"] = sculk::protocol::TagVariant{enchant_list};
    }

    if (!main_compound.mValue.empty()) {
        nbt.mValue["tag"] = sculk::protocol::TagVariant{main_compound};
    }

    return nbt;
}


ChestFormManager& ChestFormManager::getInstance() {
    static ChestFormManager instance;
    return instance;
}

void ChestFormManager::init(endstone::Plugin& plugin) {
    plugin_ = &plugin;
    
    auto& server = plugin_->getServer();
    
    auto chest_data = server.createBlockData("minecraft:chest");
    if (chest_data) {
        chest_runtime_id_ = chest_data->getRuntimeId();
    } else {
        plugin_->getLogger().error("Failed to query runtime ID for minecraft:chest");
    }

    auto air_data = server.createBlockData("minecraft:air");
    if (air_data) {
        air_runtime_id_ = air_data->getRuntimeId();
    } else {
        plugin_->getLogger().error("Failed to query runtime ID for minecraft:air");
    }
}

void ChestFormManager::shutdown() {
    // Force close active sessions
    std::vector<std::string> uuids_to_close;
    for (const auto& [uuid, session] : active_sessions_) {
        uuids_to_close.push_back(uuid);
    }
    for (const auto& uuid : uuids_to_close) {
        auto* player = plugin_->getServer().getPlayer(uuid);
        if (player) {
            closeForm(*player, false);
        }
    }
    active_sessions_.clear();
    plugin_ = nullptr;
}

void ChestFormManager::openForm(endstone::Player& player, const ChestForm& form) {
    auto uuid = player.getUniqueId().str();

    // Close any already open fake chest forms
    closeForm(player, false);

    ChestFormSession session;
    session.player_uuid = uuid;
    session.title = form.getTitle();
    session.size = form.getSize();
    session.window_id = next_window_id_++;
    if (next_window_id_ > 99) {
        next_window_id_ = 50;
    }

    // Place the fake block at a safe coordinate near the player
    auto location = player.getLocation();
    auto& dimension = player.getDimension();
    session.chest_x = static_cast<int>(std::floor(location.getX()));

    // Place the chest directly below the player's feet to guarantee the block is within reach.
    int target_y = static_cast<int>(std::floor(location.getY())) - 1;
    auto dim_name = dimension.getName();
    int min_y = -64;
    if (dim_name == "Nether" || dim_name == "The End") {
        min_y = 0;
    }
    if (target_y < min_y) {
        target_y = min_y;
    }
    session.chest_y = target_y;
    session.chest_z = static_cast<int>(std::floor(location.getZ()));

    // Store original blocks to restore them later
    auto block1 = dimension.getBlockAt(session.chest_x, session.chest_y, session.chest_z);
    session.original_block_runtime_id_1 = (block1 && block1->getData()) ? block1->getData()->getRuntimeId() : air_runtime_id_;

    bool is_double = (session.size == ChestSize::Double);
    int pair_x = session.chest_x + 1;
    int pair_z = session.chest_z;

    if (is_double) {
        auto block2 = dimension.getBlockAt(pair_x, session.chest_y, pair_z);
        session.original_block_runtime_id_2 = (block2 && block2->getData()) ? block2->getData()->getRuntimeId() : air_runtime_id_;
    } else {
        session.original_block_runtime_id_2 = air_runtime_id_;
    }

    session.callbacks = form.getCallbacks();
    session.slots = form.getItems();
    session.is_open = true;

    active_sessions_[uuid] = session;

    if (plugin_) {
        plugin_->getLogger().info("Opening Fake Chest Form for " + player.getName() + 
                                  " (Window ID: " + std::to_string(session.window_id) +
                                  ", Item Registry Size: " + std::to_string(item_name_to_id_.size()) + ")");
    }

    // Place fake chest block(s)
    sculk::protocol::UpdateBlockPacket update1;
    update1.mBlockPosition = {session.chest_x, session.chest_y, session.chest_z};
    update1.mRuntimeId = chest_runtime_id_;
    update1.mFlag = 3;
    update1.mLayer = 0;
    sendPacketHelper(player, update1);

    if (is_double) {
        sculk::protocol::UpdateBlockPacket update2;
        update2.mBlockPosition = {pair_x, session.chest_y, pair_z};
        update2.mRuntimeId = chest_runtime_id_;
        update2.mFlag = 3;
        update2.mLayer = 0;
        sendPacketHelper(player, update2);
    }

    // Initialize block actor data for titles/double-chest setup
    sculk::protocol::BlockActorDataPacket actor1;
    actor1.mBlockPosition = {session.chest_x, session.chest_y, session.chest_z};

    sculk::protocol::CompoundTag compound1;
    compound1.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{"Chest"}};
    compound1.mValue["x"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_x}};
    compound1.mValue["y"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_y}};
    compound1.mValue["z"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_z}};
    compound1.mValue["Findable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
    compound1.mValue["isMovable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};

    sculk::protocol::ListTag items1;
    items1.mType = sculk::protocol::TagType::Compound;
    for (int i = 0; i < 27; ++i) {
        auto slot_it = session.slots.find(i);
        if (slot_it != session.slots.end()) {
            items1.mValue.push_back(sculk::protocol::TagVariant{serializeFormItemToNbt(slot_it->second, i)});
        } else {
            items1.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::CompoundTag{}});
        }
    }
    compound1.mValue["Items"] = sculk::protocol::TagVariant{items1};

    if (is_double) {
        compound1.mValue["pairx"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_x}};
        compound1.mValue["pairz"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_z}};
        compound1.mValue["pairlead"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};
    }
    compound1.mValue["CustomName"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{session.title}};
    actor1.mActorDataTags = std::move(compound1);
    sendPacketHelper(player, actor1);

    if (is_double) {
        sculk::protocol::BlockActorDataPacket actor2;
        actor2.mBlockPosition = {pair_x, session.chest_y, pair_z};

        sculk::protocol::CompoundTag compound2;
        compound2.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{"Chest"}};
        compound2.mValue["x"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_x}};
        compound2.mValue["y"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_y}};
        compound2.mValue["z"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_z}};
        compound2.mValue["Findable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
        compound2.mValue["isMovable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};

        sculk::protocol::ListTag items2;
        items2.mType = sculk::protocol::TagType::Compound;
        for (int i = 0; i < 27; ++i) {
            auto slot_it = session.slots.find(i + 27);
            if (slot_it != session.slots.end()) {
                items2.mValue.push_back(sculk::protocol::TagVariant{serializeFormItemToNbt(slot_it->second, i)});
            } else {
                items2.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::CompoundTag{}});
            }
        }
        compound2.mValue["Items"] = sculk::protocol::TagVariant{items2};

        compound2.mValue["pairx"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_x}};
        compound2.mValue["pairz"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_z}};
        compound2.mValue["pairlead"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
        compound2.mValue["CustomName"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{session.title}};
        actor2.mActorDataTags = std::move(compound2);
        sendPacketHelper(player, actor2);
    }

    // Send open container packet after a small delay to let client construct the block actor first
    if (plugin_) {
        auto player_uuid = player.getUniqueId();
        plugin_->getServer().getScheduler().runTaskLater(*plugin_, [this, player_uuid, uuid, window_id = session.window_id, chest_x = session.chest_x, chest_y = session.chest_y, chest_z = session.chest_z, size = session.size]() {
            auto* current_player = plugin_->getServer().getPlayer(player_uuid);
            if (!current_player) return;

            auto it = active_sessions_.find(uuid);
            if (it == active_sessions_.end() || it->second.window_id != window_id || !it->second.is_open) {
                return;
            }

            sculk::protocol::ContainerOpenPacket open;
            open.mContainerId = static_cast<sculk::protocol::ContainerID>(window_id);
            open.mContainerType = sculk::protocol::ContainerType::Container;
            open.mPosition = {chest_x, chest_y, chest_z};
            open.mTargetActorId = -1;
            sendPacketHelper(*current_player, open);

            // Populate container slots
            sculk::protocol::InventoryContentPacket content;
            content.mInventoryId = window_id;
            int total_slots = static_cast<int>(size);
            content.mSlots.resize(total_slots);
            for (int i = 0; i < total_slots; ++i) {
                auto slot_it = it->second.slots.find(i);
                if (slot_it != it->second.slots.end()) {
                    content.mSlots[i] = serializeFormItem(slot_it->second);
                } else {
                    sculk::protocol::NetworkItemStackDescriptor empty_desc;
                    empty_desc.mId = 0;
                    content.mSlots[i] = empty_desc;
                }
            }
            sendPacketHelper(*current_player, content);
        }, 10);
    }
}

void ChestFormManager::closeForm(endstone::Player& player, bool client_initiated) {
    auto uuid = player.getUniqueId().str();
    auto it = active_sessions_.find(uuid);
    if (it == active_sessions_.end()) {
        return;
    }

    auto session = it->second;
    active_sessions_.erase(uuid);

    if (plugin_) {
        plugin_->getLogger().info("Closing Fake Chest Form for " + player.getName() + 
                                  " (Window ID: " + std::to_string(session.window_id) +
                                  ", Client Initiated: " + (client_initiated ? "true" : "false") + ")");
    }

    if (!client_initiated) {
        sculk::protocol::ContainerClosePacket close_packet;
        close_packet.mContainerId = static_cast<sculk::protocol::ContainerID>(session.window_id);
        close_packet.mContainerType = sculk::protocol::ContainerType::Container;
        close_packet.mServerInitiatedClose = true;
        sendPacketHelper(player, close_packet);
    }

    // Revert the client's block state to the original environment blocks
    sculk::protocol::UpdateBlockPacket restore1;
    restore1.mBlockPosition = {session.chest_x, session.chest_y, session.chest_z};
    restore1.mRuntimeId = session.original_block_runtime_id_1;
    restore1.mFlag = 3;
    restore1.mLayer = 0;
    sendPacketHelper(player, restore1);

    if (session.size == ChestSize::Double) {
        sculk::protocol::UpdateBlockPacket restore2;
        restore2.mBlockPosition = {session.chest_x + 1, session.chest_y, session.chest_z};
        restore2.mRuntimeId = session.original_block_runtime_id_2;
        restore2.mFlag = 3;
        restore2.mLayer = 0;
        sendPacketHelper(player, restore2);
    }
}

void ChestFormManager::updateForm(endstone::Player& player, const ChestForm& form) {
    auto uuid = player.getUniqueId().str();
    auto it = active_sessions_.find(uuid);
    if (it == active_sessions_.end() || !it->second.is_open) {
        openForm(player, form);
        return;
    }

    auto& session = it->second;

    if (session.size != form.getSize() || session.title != form.getTitle()) {
        openForm(player, form);
        return;
    }

    session.slots = form.getItems();
    session.callbacks = form.getCallbacks();

    bool is_double = (session.size == ChestSize::Double);
    int pair_x = session.chest_x + 1;
    int pair_z = session.chest_z;

    sculk::protocol::BlockActorDataPacket actor1;
    actor1.mBlockPosition = {session.chest_x, session.chest_y, session.chest_z};

    sculk::protocol::CompoundTag compound1;
    compound1.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{"Chest"}};
    compound1.mValue["x"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_x}};
    compound1.mValue["y"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_y}};
    compound1.mValue["z"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_z}};
    compound1.mValue["Findable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
    compound1.mValue["isMovable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};

    sculk::protocol::ListTag items1;
    items1.mType = sculk::protocol::TagType::Compound;
    for (int i = 0; i < 27; ++i) {
        auto slot_it = session.slots.find(i);
        if (slot_it != session.slots.end()) {
            items1.mValue.push_back(sculk::protocol::TagVariant{serializeFormItemToNbt(slot_it->second, i)});
        } else {
            items1.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::CompoundTag{}});
        }
    }
    compound1.mValue["Items"] = sculk::protocol::TagVariant{items1};

    if (is_double) {
        compound1.mValue["pairx"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_x}};
        compound1.mValue["pairz"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_z}};
        compound1.mValue["pairlead"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};
    }
    compound1.mValue["CustomName"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{session.title}};
    actor1.mActorDataTags = std::move(compound1);
    sendPacketHelper(player, actor1);

    if (is_double) {
        sculk::protocol::BlockActorDataPacket actor2;
        actor2.mBlockPosition = {pair_x, session.chest_y, pair_z};

        sculk::protocol::CompoundTag compound2;
        compound2.mValue["id"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{"Chest"}};
        compound2.mValue["x"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_x}};
        compound2.mValue["y"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_y}};
        compound2.mValue["z"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{pair_z}};
        compound2.mValue["Findable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
        compound2.mValue["isMovable"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{1}};

        sculk::protocol::ListTag items2;
        items2.mType = sculk::protocol::TagType::Compound;
        for (int i = 0; i < 27; ++i) {
            auto slot_it = session.slots.find(i + 27);
            if (slot_it != session.slots.end()) {
                items2.mValue.push_back(sculk::protocol::TagVariant{serializeFormItemToNbt(slot_it->second, i)});
            } else {
                items2.mValue.push_back(sculk::protocol::TagVariant{sculk::protocol::CompoundTag{}});
            }
        }
        compound2.mValue["Items"] = sculk::protocol::TagVariant{items2};

        compound2.mValue["pairx"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_x}};
        compound2.mValue["pairz"] = sculk::protocol::TagVariant{sculk::protocol::IntTag{session.chest_z}};
        compound2.mValue["pairlead"] = sculk::protocol::TagVariant{sculk::protocol::ByteTag{0}};
        compound2.mValue["CustomName"] = sculk::protocol::TagVariant{sculk::protocol::StringTag{session.title}};
        actor2.mActorDataTags = std::move(compound2);
        sendPacketHelper(player, actor2);
    }

    sculk::protocol::InventoryContentPacket content;
    content.mInventoryId = session.window_id;
    int total_slots = static_cast<int>(session.size);
    content.mSlots.resize(total_slots);
    for (int i = 0; i < total_slots; ++i) {
        auto slot_it = session.slots.find(i);
        if (slot_it != session.slots.end()) {
            content.mSlots[i] = serializeFormItem(slot_it->second);
        } else {
            sculk::protocol::NetworkItemStackDescriptor empty_desc;
            empty_desc.mId = 0;
            content.mSlots[i] = empty_desc;
        }
    }
    sendPacketHelper(player, content);
}

void ChestFormManager::handlePacketSend(endstone::Player* player, int packet_id, std::string_view payload) {
    if (packet_id == static_cast<int>(sculk::protocol::MinecraftPacketIds::ItemRegistry) || packet_id == 137 || packet_id == 162) {
        sculk::protocol::ReadOnlyBinaryStream stream(payload);
        sculk::protocol::ItemRegistryPacket packet;
        if (packet.read(stream)) {
            if (plugin_) {
                plugin_->getLogger().info("Successfully parsed ItemRegistryPacket with " + 
                                          std::to_string(packet.mItemData.size()) + " items.");
            }
            for (const auto& item : packet.mItemData) {
                item_name_to_id_[item.mName] = item.mId;
            }
        } else {
            if (plugin_) {
                plugin_->getLogger().error("Failed to parse ItemRegistryPacket!");
            }
        }
    }
}

bool ChestFormManager::handlePacketReceive(endstone::Player& player, int packet_id, std::string_view payload) {
    auto uuid = player.getUniqueId().str();
    auto session_it = active_sessions_.find(uuid);
    if (session_it == active_sessions_.end()) {
        return true;
    }

    auto& session = session_it->second;

    // 47 is ContainerClosePacket
    if (packet_id == 47) {
        sculk::protocol::ReadOnlyBinaryStream stream(payload);
        sculk::protocol::ContainerClosePacket packet;
        if (packet.read(stream)) {
            if (plugin_) {
                plugin_->getLogger().info("Received ContainerClosePacket for " + player.getName() + 
                                          " (Packet Window ID: " + std::to_string(static_cast<int>(packet.mContainerId)) + 
                                          ", Active Session Window ID: " + std::to_string(session.window_id) + ")");
            }
            if (static_cast<std::uint8_t>(packet.mContainerId) == session.window_id) {
                closeForm(player, true);
            }
        }
        return true;
    }

    // 147 is ItemStackRequestPacket
    if (packet_id == 147) {
        sculk::protocol::ReadOnlyBinaryStream stream(payload);
        sculk::protocol::ItemStackRequestPacket packet;
        if (packet.read(stream)) {
            bool matches_our_chest = false;
            int clicked_slot = -1;

            for (const auto& req_data : packet.mRequest.mRequests) {
                for (const auto& action : req_data.mActions) {
                    if (action.mActionType == sculk::protocol::ItemStackRequestAction::Type::Take ||
                        action.mActionType == sculk::protocol::ItemStackRequestAction::Type::Place ||
                        action.mActionType == sculk::protocol::ItemStackRequestAction::Type::Swap) {

                        if (std::holds_alternative<sculk::protocol::ItemStackRequestAction::TransferBase>(action.mVariant)) {
                            const auto& transfer = std::get<sculk::protocol::ItemStackRequestAction::TransferBase>(action.mVariant);
                            if (transfer.mSource.mFullContainerName.mContainerEnumName == sculk::protocol::ContainerEnumName::DynamicContainer &&
                                transfer.mSource.mFullContainerName.mDynamicId == session.window_id) {
                                matches_our_chest = true;
                                clicked_slot = transfer.mSource.mSlot;
                            }
                            if (transfer.mDestination.mFullContainerName.mContainerEnumName == sculk::protocol::ContainerEnumName::DynamicContainer &&
                                transfer.mDestination.mFullContainerName.mDynamicId == session.window_id) {
                                matches_our_chest = true;
                                clicked_slot = transfer.mDestination.mSlot;
                            }
                        } else if (std::holds_alternative<sculk::protocol::ItemStackRequestAction::Swap>(action.mVariant)) {
                            const auto& swap = std::get<sculk::protocol::ItemStackRequestAction::Swap>(action.mVariant);
                            if (swap.mSource.mFullContainerName.mContainerEnumName == sculk::protocol::ContainerEnumName::DynamicContainer &&
                                swap.mSource.mFullContainerName.mDynamicId == session.window_id) {
                                matches_our_chest = true;
                                clicked_slot = swap.mSource.mSlot;
                            }
                            if (swap.mDestination.mFullContainerName.mContainerEnumName == sculk::protocol::ContainerEnumName::DynamicContainer &&
                                swap.mDestination.mFullContainerName.mDynamicId == session.window_id) {
                                matches_our_chest = true;
                                clicked_slot = swap.mDestination.mSlot;
                            }
                        }
                    }
                }
            }

            if (matches_our_chest) {
                if (plugin_) {
                    plugin_->getLogger().info("Click on Fake Chest Form for " + player.getName() + 
                                              " (Slot: " + std::to_string(clicked_slot) + ")");
                }

                if (clicked_slot >= 0 && clicked_slot < static_cast<int>(session.size)) {
                    auto callback_it = session.callbacks.find(clicked_slot);
                    if (callback_it != session.callbacks.end() && callback_it->second) {
                        try {
                            callback_it->second(player, clicked_slot);
                        } catch (const std::exception& e) {
                            if (plugin_) {
                                plugin_->getLogger().error("Error executing slot callback: " + std::string(e.what()));
                            }
                        } catch (...) {
                            if (plugin_) {
                                plugin_->getLogger().error("Unknown error executing slot callback.");
                            }
                        }
                    }
                }

                // Resync inventory contents immediately to revert client item stack modifications
                sculk::protocol::InventoryContentPacket content;
                content.mInventoryId = session.window_id;
                int total_slots = static_cast<int>(session.size);
                content.mSlots.resize(total_slots);
                for (int i = 0; i < total_slots; ++i) {
                    auto it = session.slots.find(i);
                    if (it != session.slots.end()) {
                        content.mSlots[i] = serializeFormItem(it->second);
                    } else {
                        sculk::protocol::NetworkItemStackDescriptor empty_desc;
                        empty_desc.mId = 0;
                        content.mSlots[i] = empty_desc;
                    }
                }
                sendPacketHelper(player, content);

                // Resync player main inventory to prevent cursor or local slot mismatch
                auto& inv = player.getInventory();
                sculk::protocol::InventoryContentPacket inv_content;
                inv_content.mInventoryId = 0;
                inv_content.mSlots.resize(inv.getSize());
                for (int i = 0; i < inv.getSize(); ++i) {
                    auto item_ptr = inv.getItem(i);
                    if (item_ptr) {
                        sculk::protocol::NetworkItemStackDescriptor desc;
                        desc.mId = getItemId(item_ptr->getType().getId());
                        desc.mStackSize = static_cast<std::uint16_t>(item_ptr->getAmount());
                        desc.mAux = static_cast<std::uint32_t>(item_ptr->getData());
                        inv_content.mSlots[i] = desc;
                    } else {
                        sculk::protocol::NetworkItemStackDescriptor empty_desc;
                        empty_desc.mId = 0;
                        inv_content.mSlots[i] = empty_desc;
                    }
                }
                sendPacketHelper(player, inv_content);

                return false; // Cancel processing this packet on the server
            }
        }
    }

    return true;
}

std::int16_t ChestFormManager::getItemId(const std::string& name) const {
    if (name.empty() || name == "minecraft:air") {
        return 0;
    }
    auto it = item_name_to_id_.find(name);
    if (it != item_name_to_id_.end()) {
        return it->second;
    }

    static const std::unordered_map<std::string, std::int16_t> fallback_ids = {
        {"minecraft:stained_glass_pane", 160},
        {"minecraft:black_stained_glass_pane", 160},
        {"minecraft:light_gray_stained_glass_pane", 160},
        {"minecraft:diamond_block", 57},
        {"minecraft:emerald", 388},
        {"minecraft:barrier", 416},
        {"minecraft:stone", 1},
        {"minecraft:diamond", 264},
        {"minecraft:gold_ingot", 266},
        {"minecraft:iron_ingot", 265},
        {"minecraft:chest", 54}
    };
    auto fallback_it = fallback_ids.find(name);
    if (fallback_it != fallback_ids.end()) {
        return fallback_it->second;
    }

    // Attempt parsing raw numerical value from string if name is digits
    try {
        return static_cast<std::int16_t>(std::stoi(name));
    } catch (...) {}

    return 1; // Fallback to stone if identifier is completely unresolvable
}
