import os
import sys
import glob
import ctypes
from typing import Dict, List, Callable, Optional, Any

# ==========================================
# 1. C-ABI Shared Library Dynamic Loader
# ==========================================

lib = None
candidates = [
    "plugins/endstone_chestform_api.dll",
    "plugins/endstone_chestform_api.so",
    "endstone_chestform_api.dll",
    "endstone_chestform_api.so",
]

# Add wildcard searches for the .local directory where Endstone might copy and load the C++ plugin
for root in ["plugins", "."]:
    candidates.extend(glob.glob(os.path.join(root, ".local", "endstone_chestform_api*")))
    candidates.extend(glob.glob(os.path.join(root, "endstone_chestform_api*")))

for c in candidates:
    if os.path.exists(c) and os.path.isfile(c):
        try:
            lib = ctypes.CDLL(os.path.abspath(c))
            if lib and hasattr(lib, "chest_form_create"):
                break
        except Exception:
            pass

if lib is None:
    # Try system PATH / loaded modules lookup
    for name in ["endstone_chestform_api", "endstone_chestform_api.dll", "endstone_chestform_api.so"]:
        try:
            lib = ctypes.CDLL(name)
            if lib and hasattr(lib, "chest_form_create"):
                break
        except Exception:
            pass

# ==========================================
# 2. Setup C-ABI Function Signatures
# ==========================================

if lib is not None:
    lib.chest_form_register_callback_handler.argtypes = [ctypes.c_void_p]
    lib.chest_form_register_callback_handler.restype = None

    lib.chest_form_create.argtypes = [ctypes.c_char_p, ctypes.c_int]
    lib.chest_form_create.restype = ctypes.c_void_p

    lib.chest_form_destroy.argtypes = [ctypes.c_void_p]
    lib.chest_form_destroy.restype = None

    lib.chest_form_set_slot.argtypes = [
        ctypes.c_void_p,                  # form_ptr
        ctypes.c_int,                     # slot
        ctypes.c_char_p,                  # type_id
        ctypes.c_int,                     # amount
        ctypes.c_int,                     # aux
        ctypes.c_char_p,                  # display_name
        ctypes.POINTER(ctypes.c_char_p),  # lore
        ctypes.c_int,                     # lore_size
        ctypes.c_char_p,                  # enchants_str
        ctypes.c_char_p,                  # custom_nbt_snbt
        ctypes.c_void_p                   # callback_id
    ]
    lib.chest_form_set_slot.restype = None

    lib.chest_form_fill_slots.argtypes = [
        ctypes.c_void_p,                  # form_ptr
        ctypes.c_char_p,                  # type_id
        ctypes.c_int,                     # amount
        ctypes.c_int,                     # aux
        ctypes.c_char_p,                  # display_name
        ctypes.POINTER(ctypes.c_char_p),  # lore
        ctypes.c_int,                     # lore_size
        ctypes.c_char_p,                  # enchants_str
        ctypes.c_char_p                   # custom_nbt_snbt
    ]
    lib.chest_form_fill_slots.restype = None

    lib.chest_form_clear_slot.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.chest_form_clear_slot.restype = None

    lib.chest_form_set_title.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.chest_form_set_title.restype = None

    lib.chest_form_set_size.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.chest_form_set_size.restype = None

    lib.chest_form_send_to.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.chest_form_send_to.restype = None

    lib.chest_form_close.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.chest_form_close.restype = None

    lib.chest_form_update.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.chest_form_update.restype = None

# ==========================================
# 3. Callback Handlers & Registry
# ==========================================

callback_registry = {}
next_callback_id = 1
PythonCallbackType = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)

@PythonCallbackType
def global_callback_handler(player_name, slot, callback_id):
    player_name_str = player_name.decode("utf-8")
    entry = callback_registry.get(callback_id)
    if entry:
        plugin, callback_fn = entry
        player = plugin.server.get_player(player_name_str)
        if player:
            try:
                callback_fn(player, slot)
            except Exception as e:
                plugin.logger.error(f"Error in Python slot callback: {e}")

_global_callback_handler_ref = global_callback_handler

if lib is not None:
    lib.chest_form_register_callback_handler(ctypes.cast(_global_callback_handler_ref, ctypes.c_void_p))

# ==========================================
# 4. Public API Wrapper Classes
# ==========================================

class ChestSize:
    SINGLE = 27
    DOUBLE = 54

class FormItem:
    def __init__(self) -> None:
        self.type_id: str = ""
        self.amount: int = 1
        self.aux: int = 0
        self.display_name: str = ""
        self.lore: List[str] = []
        self.enchants: Dict[str, int] = {}
        self.custom_nbt_snbt: str = ""

class ChestForm:
    def __init__(self, plugin: Any, title: str, size: int = ChestSize.DOUBLE) -> None:
        if lib is None:
            raise NotImplementedError(
                "ChestFormAPI C++ backend is not loaded. Please make sure that "
                "endstone_chestform_api.dll or endstone_chestform_api.so is placed in your "
                "server's plugins/ directory, and that the server has started successfully."
            )
        self._plugin = plugin
        title_bytes = title.encode('utf-8')
        self._form_ptr = lib.chest_form_create(title_bytes, size)
        if not self._form_ptr:
            raise RuntimeError("Failed to create native ChestForm object")

    def __del__(self) -> None:
        if hasattr(self, "_form_ptr") and self._form_ptr and lib is not None:
            try:
                lib.chest_form_destroy(self._form_ptr)
            except Exception:
                pass
            self._form_ptr = None

    def setSlot(self, slot: int, item: FormItem, callback: Optional[Callable[[Any, int], None]] = None) -> 'ChestForm':
        if not self._form_ptr:
            return self
        
        type_id_bytes = item.type_id.encode('utf-8') if item.type_id else b"minecraft:air"
        display_name_bytes = item.display_name.encode('utf-8') if item.display_name else b""
        
        lore_bytes = [line.encode('utf-8') for line in item.lore] if item.lore else []
        lore_arr = (ctypes.c_char_p * len(lore_bytes))(*lore_bytes) if lore_bytes else None
        lore_len = len(lore_bytes)
        
        enchants_str = ",".join(f"{k}:{v}" for k, v in item.enchants.items()).encode('utf-8') if item.enchants else b""
        custom_nbt = item.custom_nbt_snbt.encode('utf-8') if item.custom_nbt_snbt else b""
        
        callback_id = None
        if callback is not None:
            global next_callback_id
            callback_id = next_callback_id
            next_callback_id += 1
            callback_registry[callback_id] = (self._plugin, callback)
            
        lib.chest_form_set_slot(
            self._form_ptr,
            slot,
            type_id_bytes,
            item.amount,
            item.aux,
            display_name_bytes,
            lore_arr,
            lore_len,
            enchants_str,
            custom_nbt,
            callback_id
        )
        return self

    def fillSlots(self, item: FormItem) -> 'ChestForm':
        if not self._form_ptr:
            return self
        
        type_id_bytes = item.type_id.encode('utf-8') if item.type_id else b"minecraft:air"
        display_name_bytes = item.display_name.encode('utf-8') if item.display_name else b""
        
        lore_bytes = [line.encode('utf-8') for line in item.lore] if item.lore else []
        lore_arr = (ctypes.c_char_p * len(lore_bytes))(*lore_bytes) if lore_bytes else None
        lore_len = len(lore_bytes)
        
        enchants_str = ",".join(f"{k}:{v}" for k, v in item.enchants.items()).encode('utf-8') if item.enchants else b""
        custom_nbt = item.custom_nbt_snbt.encode('utf-8') if item.custom_nbt_snbt else b""
        
        lib.chest_form_fill_slots(
            self._form_ptr,
            type_id_bytes,
            item.amount,
            item.aux,
            display_name_bytes,
            lore_arr,
            lore_len,
            enchants_str,
            custom_nbt
        )
        return self

    def clearSlot(self, slot: int) -> 'ChestForm':
        if self._form_ptr:
            lib.chest_form_clear_slot(self._form_ptr, slot)
        return self

    def setTitle(self, title: str) -> 'ChestForm':
        if self._form_ptr:
            lib.chest_form_set_title(self._form_ptr, title.encode('utf-8'))
        return self

    def setSize(self, size: int) -> 'ChestForm':
        if self._form_ptr:
            lib.chest_form_set_size(self._form_ptr, size)
        return self

    def sendTo(self, player: Any) -> None:
        if self._form_ptr and player:
            lib.chest_form_send_to(self._form_ptr, player.name.encode('utf-8'))

    def close(self, player: Any) -> None:
        if self._form_ptr and player:
            lib.chest_form_close(self._form_ptr, player.name.encode('utf-8'))

    def update(self, player: Any) -> None:
        if self._form_ptr and player:
            lib.chest_form_update(self._form_ptr, player.name.encode('utf-8'))

    # Snake case equivalents
    def set_slot(self, slot: int, item: FormItem, callback: Optional[Callable[[Any, int], None]] = None) -> 'ChestForm':
        return self.setSlot(slot, item, callback)

    def fill_slots(self, item: FormItem) -> 'ChestForm':
        return self.fillSlots(item)

    def clear_slot(self, slot: int) -> 'ChestForm':
        return self.clearSlot(slot)

    def set_title(self, title: str) -> 'ChestForm':
        return self.setTitle(title)

    def set_size(self, size: int) -> 'ChestForm':
        return self.setSize(size)

    def send_to(self, player: Any) -> None:
        self.sendTo(player)

__all__ = ["ChestSize", "FormItem", "ChestForm"]
