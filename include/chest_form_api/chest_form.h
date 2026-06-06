#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "export.h"
#include "form_item.h"

namespace endstone {
class Plugin;
class Player;
}

enum class ChestSize {
    Single = 27,
    Double = 54
};

using SlotCallback = std::function<void(endstone::Player&, int slot)>;

class CHEST_FORM_API ChestForm {
public:
    explicit ChestForm(endstone::Plugin& plugin, std::string title, ChestSize size = ChestSize::Double);
    ~ChestForm() = default;

    ChestForm& setSlot(int slot, FormItem item, SlotCallback callback = nullptr);
    ChestForm& fillSlots(FormItem item);
    ChestForm& clearSlot(int slot);

    ChestForm& setTitle(std::string title);
    ChestForm& setSize(ChestSize size);

    void sendTo(endstone::Player& player);
    void close(endstone::Player& player);
    void update(endstone::Player& player);

    // Getters for manager access
    [[nodiscard]] endstone::Plugin& getPlugin() const { return plugin_; }
    [[nodiscard]] const std::string& getTitle() const { return title_; }
    [[nodiscard]] ChestSize getSize() const { return size_; }
    [[nodiscard]] const std::unordered_map<int, FormItem>& getItems() const { return items_; }
    [[nodiscard]] const std::unordered_map<int, SlotCallback>& getCallbacks() const { return callbacks_; }

private:
    endstone::Plugin& plugin_;
    std::string title_;
    ChestSize size_;
    std::unordered_map<int, FormItem> items_;
    std::unordered_map<int, SlotCallback> callbacks_;
};
