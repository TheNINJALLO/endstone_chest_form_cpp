#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string_view>
#include <cstdint>
#include "export.h"
#include "chest_form.h"

namespace endstone {
class Plugin;
class Player;
}

struct CHEST_FORM_API ChestFormSession {
    std::string player_uuid;
    std::string title;
    ChestSize size;
    std::uint8_t window_id;
    int chest_x;
    int chest_y;
    int chest_z;
    std::uint32_t original_block_runtime_id_1;
    std::uint32_t original_block_runtime_id_2;
    std::unordered_map<int, SlotCallback> callbacks;
    std::unordered_map<int, FormItem> slots;
    bool is_open{false};
};

class CHEST_FORM_API ChestFormManager {
public:
    static ChestFormManager& getInstance();

    void init(endstone::Plugin& plugin);
    void shutdown();

    void openForm(endstone::Player& player, const ChestForm& form);
    void closeForm(endstone::Player& player, bool client_initiated = false);
    void updateForm(endstone::Player& player, const ChestForm& form);

    // Packet interceptors called from Endstone events
    void handlePacketSend(endstone::Player* player, int packet_id, std::string_view payload);
    // Returns true to allow the packet to be processed by the server, false to cancel
    bool handlePacketReceive(endstone::Player& player, int packet_id, std::string_view payload);

    // Queries to resolve items
    [[nodiscard]] std::int16_t getItemId(const std::string& name) const;
    [[nodiscard]] std::int16_t getItemIdWithoutFallback(const std::string& name) const;
    [[nodiscard]] endstone::Plugin* getPlugin() const { return plugin_; }
    [[nodiscard]] std::uint32_t getChestRuntimeId() const { return chest_runtime_id_; }
    [[nodiscard]] std::uint32_t getAirRuntimeId() const { return air_runtime_id_; }

    [[nodiscard]] bool hasSession(const std::string& player_uuid) const {
        return active_sessions_.find(player_uuid) != active_sessions_.end();
    }

private:
    ChestFormManager() = default;
    ~ChestFormManager() = default;

    ChestFormManager(const ChestFormManager&) = delete;
    ChestFormManager& operator=(const ChestFormManager&) = delete;

    endstone::Plugin* plugin_{nullptr};
    std::unordered_map<std::string, ChestFormSession> active_sessions_;
    std::unordered_map<std::string, std::int16_t> item_name_to_id_;
    std::uint32_t chest_runtime_id_{0};
    std::uint32_t air_runtime_id_{0};
    std::uint8_t next_window_id_{50};
};
