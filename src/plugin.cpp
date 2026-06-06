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
#include <endstone/inventory/item_stack.h>
#include <endstone/inventory/player_inventory.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

class ChestFormListener {
public:
    void onPacketSend(endstone::PacketSendEvent& event) {
        if (event.getPacketId() == 137) { // ItemRegistryPacket
            ChestFormManager::getInstance().handlePacketSend(nullptr, event.getPacketId(), event.getPayload());
        } else if (event.getPlayer()) {
            ChestFormManager::getInstance().handlePacketSend(event.getPlayer(), event.getPacketId(), event.getPayload());
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

        if (command.getName() == "chestinvsee") {
            auto* player = sender.asPlayer();
            if (!player) {
                sender.sendErrorMessage("This command can only be executed by a player.");
                return true;
            }
            if (args.empty()) {
                sender.sendErrorMessage("Usage: /chestinvsee <player>");
                return true;
            }
            auto* target = getServer().getPlayer(args[0]);
            if (!target) {
                sender.sendErrorMessage("Player not found: " + args[0]);
                return true;
            }
            openInventorySeeForm(*player, *target);
            return true;
        }

        if (command.getName() == "chestendersee") {
            auto* player = sender.asPlayer();
            if (!player) {
                sender.sendErrorMessage("This command can only be executed by a player.");
                return true;
            }
            if (args.empty()) {
                sender.sendErrorMessage("Usage: /chestendersee <player>");
                return true;
            }
            auto* target = getServer().getPlayer(args[0]);
            if (!target) {
                sender.sendErrorMessage("Player not found: " + args[0]);
                return true;
            }
            openEnderSeeForm(*player, *target);
            return true;
        }

        return false;
    }

private:
    void setupAndSendTestForm(endstone::Player& player) {
        ChestForm form(*this, "Test Chest Form", ChestSize::Double);

        FormItem filler;
        filler.type_id = "minecraft:black_stained_glass_pane";
        filler.aux = 15;
        filler.display_name = " ";

        for (int i = 0; i < 54; ++i) {
            bool is_border = (i < 9) || (i >= 45) || (i % 9 == 0) || (i % 9 == 8);
            if (is_border) {
                form.setSlot(i, filler);
            }
        }

        FormItem give_kit;
        give_kit.type_id = "minecraft:chest";
        give_kit.display_name = "§aGive Kit";
        give_kit.lore = {"§7Click to receive a kit!"};
        form.setSlot(13, give_kit, [](endstone::Player& p, int slot) {
            p.sendMessage("§a[ChestForm] You received the builder kit!");
            p.getInventory().addItem(endstone::ItemStack("minecraft:apple", 1));
            p.getInventory().addItem(endstone::ItemStack("minecraft:wooden_sword", 1));
        });

        FormItem reopen;
        reopen.type_id = "minecraft:emerald";
        reopen.display_name = "§eReopen";
        reopen.lore = {"§7Click to reopen the form!"};
        form.setSlot(22, reopen, [this](endstone::Player& p, int slot) {
            p.sendMessage("§e[ChestForm] Reopening form in 1 tick...");
            getServer().getScheduler().runTaskLater(*this, [this, &p]() {
                setupAndSendTestForm(p);
            }, 1);
        });

        FormItem close;
        close.type_id = "minecraft:redstone_block";
        close.display_name = "§cClose";
        close.lore = {"§7Click to close!"};
        form.setSlot(31, close, [](endstone::Player& p, int slot) {
            ChestFormManager::getInstance().closeForm(p);
        });

        form.sendTo(player);
    }

    void openInventorySeeForm(endstone::Player& admin, endstone::Player& target) {
        auto form = std::make_shared<ChestForm>(*this, "Inv: " + target.getName(), ChestSize::Double);
        auto& target_inv = target.getInventory();

        FormItem filler;
        filler.type_id = "minecraft:black_stained_glass_pane";
        filler.aux = 15;
        filler.display_name = " ";

        for (int i = 36; i <= 44; ++i) {
            form->setSlot(i, filler);
        }
        form->setSlot(0, filler);
        for (int i = 5; i <= 8; ++i) {
            form->setSlot(i, filler);
        }

        auto helmet = target_inv.getHelmet();
        auto chestplate = target_inv.getChestplate();
        auto leggings = target_inv.getLeggings();
        auto boots = target_inv.getBoots();

        auto addArmorSlot = [this, form, &target](int virtual_slot, std::optional<endstone::ItemStack> item, int target_slot, std::string placeholder_name) {
            if (item && item->getType().getId() != "minecraft:air") {
                FormItem f_item;
                f_item.type_id = item->getType().getId();
                f_item.amount = item->getAmount();
                f_item.aux = item->getData();
                f_item.display_name = "§f" + f_item.type_id;
                form->setSlot(virtual_slot, f_item, [this, &target, target_slot](endstone::Player& p, int slot) {
                    openManagementMenu(p, target, target_slot, false);
                });
            } else {
                FormItem placeholder;
                placeholder.type_id = "minecraft:light_gray_stained_glass_pane";
                placeholder.aux = 8;
                placeholder.display_name = placeholder_name;
                form->setSlot(virtual_slot, placeholder);
            }
        };

        addArmorSlot(1, helmet, 36, "§7Empty Helmet");
        addArmorSlot(2, chestplate, 37, "§7Empty Chestplate");
        addArmorSlot(3, leggings, 38, "§7Empty Leggings");
        addArmorSlot(4, boots, 39, "§7Empty Boots");

        for (int i = 9; i <= 35; ++i) {
            auto item = target_inv.getItem(i);
            if (item && item->getType().getId() != "minecraft:air") {
                FormItem f_item;
                f_item.type_id = item->getType().getId();
                f_item.amount = item->getAmount();
                f_item.aux = item->getData();
                form->setSlot(i, f_item, [this, &target, i](endstone::Player& p, int slot) {
                    openManagementMenu(p, target, i, false);
                });
            } else {
                form->clearSlot(i);
            }
        }

        for (int i = 0; i <= 8; ++i) {
            auto item = target_inv.getItem(i);
            int virtual_slot = 45 + i;
            if (item && item->getType().getId() != "minecraft:air") {
                FormItem f_item;
                f_item.type_id = item->getType().getId();
                f_item.amount = item->getAmount();
                f_item.aux = item->getData();
                form->setSlot(virtual_slot, f_item, [this, &target, i](endstone::Player& p, int slot) {
                    openManagementMenu(p, target, i, false);
                });
            } else {
                form->clearSlot(virtual_slot);
            }
        }

        form->sendTo(admin);
    }

    void openEnderSeeForm(endstone::Player& admin, endstone::Player& target) {
        auto form = std::make_shared<ChestForm>(*this, "Ender: " + target.getName(), ChestSize::Single);
        auto& ender_chest = getOrCreateVirtualEnderChest(target.getUniqueId().str());

        for (int i = 0; i < 27; ++i) {
            auto& item = ender_chest[i];
            if (!item.type_id.empty() && item.type_id != "minecraft:air") {
                form->setSlot(i, item, [this, &target, i](endstone::Player& p, int slot) {
                    openManagementMenu(p, target, i, true);
                });
            } else {
                form->clearSlot(i);
            }
        }

        form->sendTo(admin);
    }

    void openManagementMenu(endstone::Player& admin, endstone::Player& target, int target_slot, bool is_ender) {
        auto form = std::make_shared<ChestForm>(*this, "Manage Slot " + std::to_string(target_slot), ChestSize::Single);

        FormItem preview_item;
        bool has_item = false;

        if (is_ender) {
            auto& chest = getOrCreateVirtualEnderChest(target.getUniqueId().str());
            if (target_slot >= 0 && target_slot < 27) {
                preview_item = chest[target_slot];
                has_item = (!preview_item.type_id.empty() && preview_item.type_id != "minecraft:air");
            }
        } else {
            auto& target_inv = target.getInventory();
            std::optional<endstone::ItemStack> target_item;
            if (target_slot == 36) target_item = target_inv.getHelmet();
            else if (target_slot == 37) target_item = target_inv.getChestplate();
            else if (target_slot == 38) target_item = target_inv.getLeggings();
            else if (target_slot == 39) target_item = target_inv.getBoots();
            else target_item = target_inv.getItem(target_slot);

            if (target_item && target_item->getType().getId() != "minecraft:air") {
                preview_item.type_id = target_item->getType().getId();
                preview_item.amount = target_item->getAmount();
                preview_item.aux = target_item->getData();
                has_item = true;
            }
        }

        if (!has_item) {
            admin.sendMessage("§cThat slot is empty!");
            if (is_ender) openEnderSeeForm(admin, target);
            else openInventorySeeForm(admin, target);
            return;
        }

        form->setSlot(4, preview_item);

        FormItem glass;
        glass.type_id = "minecraft:black_stained_glass_pane";
        glass.aux = 15;
        glass.display_name = " ";
        for (int i = 0; i < 27; ++i) {
            if (i != 4 && i != 11 && i != 13 && i != 15) {
                form->setSlot(i, glass);
            }
        }

        FormItem clone_btn;
        clone_btn.type_id = "minecraft:emerald";
        clone_btn.display_name = "§aClone Item";
        clone_btn.lore = {"§7Click to copy this item stack", "§7to your own inventory."};
        form->setSlot(11, clone_btn, [this, &target, preview_item, target_slot, is_ender](endstone::Player& p, int slot) {
            p.getInventory().addItem(endstone::ItemStack(preview_item.type_id, preview_item.amount, preview_item.aux));
            p.sendMessage("§a[ChestForm] Cloned item to your inventory.");
            if (is_ender) openEnderSeeForm(p, target);
            else openInventorySeeForm(p, target);
        });

        FormItem delete_btn;
        delete_btn.type_id = "minecraft:redstone_block";
        delete_btn.display_name = "§cDelete Item";
        delete_btn.lore = {"§7Click to clear this item", "§7from the player's inventory."};
        form->setSlot(13, delete_btn, [this, &target, target_slot, is_ender](endstone::Player& p, int slot) {
            if (is_ender) {
                auto& chest = getOrCreateVirtualEnderChest(target.getUniqueId().str());
                if (target_slot >= 0 && target_slot < 27) {
                    chest[target_slot] = FormItem{};
                }
            } else {
                auto& target_inv = target.getInventory();
                if (target_slot == 36) target_inv.setHelmet(std::nullopt);
                else if (target_slot == 37) target_inv.setChestplate(std::nullopt);
                else if (target_slot == 38) target_inv.setLeggings(std::nullopt);
                else if (target_slot == 39) target_inv.setBoots(std::nullopt);
                else target_inv.setItem(target_slot, std::nullopt);
            }
            p.sendMessage("§a[ChestForm] Deleted item from slot.");
            if (is_ender) openEnderSeeForm(p, target);
            else openInventorySeeForm(p, target);
        });

        FormItem back_btn;
        back_btn.type_id = "minecraft:arrow";
        back_btn.display_name = "§eGo Back";
        back_btn.lore = {"§7Return to the inventory view."};
        form->setSlot(15, back_btn, [this, &target, is_ender](endstone::Player& p, int slot) {
            if (is_ender) openEnderSeeForm(p, target);
            else openInventorySeeForm(p, target);
        });

        form->sendTo(admin);
    }

    std::vector<FormItem>& getOrCreateVirtualEnderChest(const std::string& uuid) {
        auto it = virtual_ender_chests_.find(uuid);
        if (it != virtual_ender_chests_.end()) {
            return it->second;
        }
        std::vector<FormItem> chest(27);
        FormItem item1;
        item1.type_id = "minecraft:diamond";
        item1.amount = 16;
        item1.display_name = "§bEnder Diamond";
        chest[0] = item1;

        FormItem item2;
        item2.type_id = "minecraft:gold_ingot";
        item2.amount = 32;
        chest[8] = item2;

        virtual_ender_chests_[uuid] = std::move(chest);
        return virtual_ender_chests_[uuid];
    }

    std::unique_ptr<ChestFormListener> listener_;
    std::unordered_map<std::string, std::vector<FormItem>> virtual_ender_chests_;
};

ENDSTONE_PLUGIN("chestform_api", "1.0.8", ChestFormPlugin) {
    prefix = "ChestFormPlugin";
    description = "Native C++ ChestFormAPI for fake chest inventory forms";
    website = "https://github.com/GlacieTeam/ChestFormAPI";
    authors = {"Endstone Developers"};

    command("chestformtest")
        .description("Test fake chest form")
        .usages("/chestformtest")
        .permissions("chestformapi.command.test");

    command("chestinvsee")
        .description("View and manage player inventory")
        .usages("/chestinvsee <player>")
        .permissions("chestformapi.command.invsee");

    command("chestendersee")
        .description("View and manage player ender chest")
        .usages("/chestendersee <player>")
        .permissions("chestformapi.command.endersee");

    permission("chestformapi.command.test")
        .description("Allow testing fake chest forms")
        .default_(endstone::PermissionDefault::Operator);

    permission("chestformapi.command.invsee")
        .description("Allow viewing other player inventories")
        .default_(endstone::PermissionDefault::Operator);

    permission("chestformapi.command.endersee")
        .description("Allow viewing other player ender chests")
        .default_(endstone::PermissionDefault::Operator);
}
