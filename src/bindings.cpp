#include "bindings.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "chest_form_api/chest_form.h"
#include "chest_form_api/form_item.h"
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <iostream>

namespace py = pybind11;

void register_python_bindings(endstone::Plugin& plugin) {
    if (!Py_IsInitialized()) {
        plugin.getLogger().warning("Python interpreter not initialized. Skipping python bindings registration.");
        return;
    }

    py::gil_scoped_acquire acquire;

    try {
        // Create the module dynamically using the recommended pybind11 API
        static PyModuleDef def = {
            PyModuleDef_HEAD_INIT,
            "endstone_chestform_api",
            "Native C++ ChestFormAPI bindings for Python plugins",
            -1,
            nullptr, nullptr, nullptr, nullptr, nullptr
        };
        auto m = py::module_::create_extension_module("endstone_chestform_api", "Native C++ ChestFormAPI bindings for Python plugins", &def);

        // Bind ChestSize enum
        py::enum_<ChestSize>(m, "ChestSize")
            .value("SINGLE", ChestSize::Single)
            .value("DOUBLE", ChestSize::Double)
            .export_values();

        // Bind FormItem struct
        py::class_<FormItem>(m, "FormItem")
            .def(py::init<>())
            .def_readwrite("type_id", &FormItem::type_id)
            .def_readwrite("amount", &FormItem::amount)
            .def_readwrite("aux", &FormItem::aux)
            .def_readwrite("display_name", &FormItem::display_name)
            .def_readwrite("lore", &FormItem::lore)
            .def_readwrite("enchants", &FormItem::enchants)
            .def_readwrite("custom_nbt_snbt", &FormItem::custom_nbt_snbt);

        // Bind ChestForm class
        py::class_<ChestForm, std::shared_ptr<ChestForm>>(m, "ChestForm")
            .def(py::init<endstone::Plugin&, std::string, ChestSize>(),
                 py::arg("plugin"), py::arg("title"), py::arg("size") = ChestSize::Double)
            // C++ camelCase names
            .def("setSlot", [](ChestForm& self, int slot, const FormItem& item, py::object callback) {
                if (callback.is_none()) {
                    self.setSlot(slot, item, nullptr);
                } else {
                    auto safe_callback = [callback](endstone::Player& p, int slot_idx) {
                        py::gil_scoped_acquire acquire_gil;
                        try {
                            callback(std::ref(p), slot_idx);
                        } catch (const py::error_already_set& e) {
                            std::cerr << "Error in Python slot callback: " << e.what() << std::endl;
                        }
                    };
                    self.setSlot(slot, item, safe_callback);
                }
                return &self;
            }, py::arg("slot"), py::arg("item"), py::arg("callback") = nullptr, py::return_value_policy::reference_internal)
            .def("fillSlots", &ChestForm::fillSlots, py::arg("item"), py::return_value_policy::reference_internal)
            .def("clearSlot", &ChestForm::clearSlot, py::arg("slot"), py::return_value_policy::reference_internal)
            .def("setTitle", &ChestForm::setTitle, py::arg("title"), py::return_value_policy::reference_internal)
            .def("setSize", &ChestForm::setSize, py::arg("size"), py::return_value_policy::reference_internal)
            .def("sendTo", &ChestForm::sendTo, py::arg("player"))
            .def("close", &ChestForm::close, py::arg("player"))
            .def("update", &ChestForm::update, py::arg("player"))
            // Python snake_case equivalents
            .def("set_slot", [](ChestForm& self, int slot, const FormItem& item, py::object callback) {
                if (callback.is_none()) {
                    self.setSlot(slot, item, nullptr);
                } else {
                    auto safe_callback = [callback](endstone::Player& p, int slot_idx) {
                        py::gil_scoped_acquire acquire_gil;
                        try {
                            callback(std::ref(p), slot_idx);
                        } catch (const py::error_already_set& e) {
                            std::cerr << "Error in Python slot callback: " << e.what() << std::endl;
                        }
                    };
                    self.setSlot(slot, item, safe_callback);
                }
                return &self;
            }, py::arg("slot"), py::arg("item"), py::arg("callback") = nullptr, py::return_value_policy::reference_internal)
            .def("fill_slots", &ChestForm::fillSlots, py::arg("item"), py::return_value_policy::reference_internal)
            .def("clear_slot", &ChestForm::clearSlot, py::arg("slot"), py::return_value_policy::reference_internal)
            .def("set_title", &ChestForm::setTitle, py::arg("title"), py::return_value_policy::reference_internal)
            .def("set_size", &ChestForm::setSize, py::arg("size"), py::return_value_policy::reference_internal)
            .def("send_to", &ChestForm::sendTo, py::arg("player"));

        // Inject the module into sys.modules
        auto sys_modules = py::module_::import("sys").attr("modules");
        sys_modules["endstone_chestform_api"] = m;

    } catch (const std::exception& e) {
        plugin.getLogger().error(std::string("Error registering Python bindings: ") + e.what());
    }
}
