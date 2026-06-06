#include "chest_form_api/chest_form.h"
#include "chest_form_api/chest_form_manager.h"
#include <endstone/plugin/plugin.h>
#include <endstone/event/server/packet_send_event.h>
#include <endstone/event/server/packet_receive_event.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/command/command_sender.h>
#include <endstone/command/command.h>
#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/scheduler/scheduler.h>
#include <memory>
#include <vector>
#include <string>

class ChestFormListener {
public:
    void onPacketSend(endstone::PacketSendEvent& event) {
        if (event.getPlayer()) {
            ChestFormManager::getInstance().handlePacketSend(*event.getPlayer(), event.getPacketId(), event.getPayload());
        }
    }

    void onPacketReceive(endstone::PacketReceiveEvent& event) {
        if (event.getPlayer()) {
            bool allow = ChestFormManager::getInstance().handlePacketReceive(*event.getPlayer(), event.getPacketId(), event.getPayload());
            if (!allow) {
                event.setCancelled(true);
            }
        }
    }

    void onPlayerQuit(endstone::PlayerQuitEvent& event) {
        ChestFormManager::getInstance().closeForm(event.getPlayer(), false);
    }
};

class ChestFormPlugin : public endstone::Plugin {
public:
    void onEnable() override {
        listener_ = std::make_unique<ChestFormListener>();
        registerEvent(&ChestFormListener::onPacketSend, *listener_);
        registerEvent(&ChestFormListener::onPacketReceive, *listener_);
        registerEvent(&ChestFormListener::onPlayerQuit, *listener_);

        ChestFormManager::getInstance().init(*this);
        getLogger().info("ChestFormPlugin (C++) enabled!");
    }

    void onDisable() override {
        ChestFormManager::getInstance().shutdown();
        listener_.reset();
        getLogger().info("ChestFormPlugin (C++) disabled!");
    }

    bool onCommand(endstone::CommandSender& sender, const endstone::Command& command, const std::vector<std::string>& args) override {
        if (command.getName() == "chestformtest") {
            auto* player = sender.asPlayer();
            if (!player) {
                sender.sendErrorMessage("This command can only be executed by a player.");
                return true;
            }

            setupAndSendTestForm(*player);
            return true;
        }
        return false;
    }

private:
    void setupAndSendTestForm(endstone::Player& player) {
        // Create a 54-slot chest form
        ChestForm form(*this, "Test Chest Form", ChestSize::Double);

        // Fill borders with glass panes
        FormItem filler;
        filler.type_id = "minecraft:stained_glass_pane";
        filler.aux = 15; // Black colored pane
        filler.display_name = " ";

        for (int i = 0; i < 54; ++i) {
            bool is_border = (i < 9) || (i >= 45) || (i % 9 == 0) || (i % 9 == 8);
            if (is_border) {
                form.setSlot(i, filler);
            }
        }

        // Slot 13: Give Kit
        FormItem give_kit;
        give_kit.type_id = "minecraft:diamond_block";
        give_kit.display_name = "§aGive Kit";
        give_kit.lore = {"§7Click to receive a kit!"};
        form.setSlot(13, give_kit, [](endstone::Player& p, int slot) {
            p.sendMessage("§a[ChestForm] You received the builder kit!");
        });

        // Slot 22: Reopen
        FormItem reopen;
        reopen.type_id = "minecraft:emerald";
        reopen.display_name = "§eReopen";
        reopen.lore = {"§7Click to reopen the form!"};
        form.setSlot(22, reopen, [this](endstone::Player& p, int slot) {
            p.sendMessage("§e[ChestForm] Reopening form in 1 tick...");
            // Reopen form in the next server tick to let the current inventory action settle
            getServer().getScheduler().runTaskLater(*this, [this, &p]() {
                setupAndSendTestForm(p);
            }, 1);
        });

        // Slot 31: Close
        FormItem close;
        close.type_id = "minecraft:barrier";
        close.display_name = "§cClose";
        close.lore = {"§7Click to close!"};
        form.setSlot(31, close, [](endstone::Player& p, int slot) {
            ChestFormManager::getInstance().closeForm(p);
        });

        form.sendTo(player);
    }

    std::unique_ptr<ChestFormListener> listener_;
};

ENDSTONE_PLUGIN("chest_form_cpp", "1.0.3", ChestFormPlugin) {
    prefix = "ChestFormPlugin";
    description = "Native C++ ChestFormAPI for fake chest inventory forms";
    website = "https://github.com/GlacieTeam/ChestFormAPI";
    authors = {"Endstone Developers"};

    command("chestformtest")
        .description("Test fake chest form")
        .usages("/chestformtest")
        .permissions("chestformapi.command.test");

    permission("chestformapi.command.test")
        .description("Allow testing fake chest forms")
        .default_(endstone::PermissionDefault::Operator);
}
