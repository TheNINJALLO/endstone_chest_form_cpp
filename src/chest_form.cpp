#include "chest_form_api/chest_form.h"
#include "chest_form_api/chest_form_manager.h"
#include <endstone/player.h>

ChestForm::ChestForm(endstone::Plugin& plugin, std::string title, ChestSize size)
    : plugin_(plugin), title_(std::move(title)), size_(size) {}

ChestForm& ChestForm::setSlot(int slot, FormItem item, SlotCallback callback) {
    if (slot >= 0 && slot < static_cast<int>(size_)) {
        items_[slot] = std::move(item);
        if (callback) {
            callbacks_[slot] = std::move(callback);
        } else {
            callbacks_.erase(slot);
        }
    }
    return *this;
}

ChestForm& ChestForm::fillSlots(FormItem item) {
    for (int i = 0; i < static_cast<int>(size_); ++i) {
        setSlot(i, item, nullptr);
    }
    return *this;
}

ChestForm& ChestForm::clearSlot(int slot) {
    items_.erase(slot);
    callbacks_.erase(slot);
    return *this;
}

void ChestForm::sendTo(endstone::Player& player) {
    ChestFormManager::getInstance().openForm(player, *this);
}

void ChestForm::close(endstone::Player& player) {
    ChestFormManager::getInstance().closeForm(player);
}
