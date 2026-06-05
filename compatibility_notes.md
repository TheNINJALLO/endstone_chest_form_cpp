# Compatibility Notes

This document describes the Minecraft Bedrock protocol specifications and IDs used by the `endstone_chest_form_cpp` plugin to construct fake chest forms.

## Tested Protocol Configuration

*   **Target Minecraft Bedrock Edition Version**: `1.20.80` to `1.21.x`
*   **Protocol Library**: `SculkCatalystMC/Protocol` (C++23)
*   **Target Compiler**: Clang 18+ on Linux (`libc++`)

## Mapped Network Packets

The plugin captures and serializes the following Minecraft Bedrock packets to simulate interactive chest inventories:

| Packet Name | Bedrock ID (Dec) | Bedrock ID (Hex) | Direction | Usage in ChestFormAPI |
| :--- | :--- | :--- | :--- | :--- |
| `UpdateBlockPacket` | `21` | `0x15` | Server -> Client | Spawns fake chest blocks at relative coords, and restores original blocks upon close. |
| `BlockActorDataPacket` | `56` | `0x38` | Server -> Client | Sets custom name (Title) and links two chests into a double chest. |
| `ContainerOpenPacket` | `46` | `0x2E` | Server -> Client | Prompts the client to open the container GUI mapping to a specific block position. |
| `InventoryContentPacket` | `49` | `0x31` | Server -> Client | Syncs the items inside the fake chest container slots and reverts illegal clicks. |
| `ContainerClosePacket` | `47` | `0x2F` | Client <-> Server | Fired when either party closes the chest GUI. Triggers cleanup. |
| `ItemStackRequestPacket` | `147` | `0x93` | Client -> Server | Captures item interaction events, checks slot bounds, and invokes registered callbacks. |
| `ItemRegistryPacket` | `137` | `0x89` | Server -> Client | Dynamically intercepts item definitions on login to build name-to-numerical-ID mappings. |

## Dynamic Chest Entity NBT Layout

A chest block entity on Bedrock requires specific tags within its `BlockActorDataPacket` compound tag:

```json
{
  "id": "Chest",
  "x": (int) Chest_X,
  "y": (int) Chest_Y,
  "z": (int) Chest_Z,
  "CustomName": (string) "Chest Title",
  "pairx": (int) Paired_Chest_X, // Only for Double Chests
  "pairz": (int) Paired_Chest_Z  // Only for Double Chests
}
```

## Troubleshooting & Desync Reversion

To prevent players from stealing items from the fake chest form, the plugin interceptor cancels the `ItemStackRequestPacket` by setting the Endstone `PacketReceiveEvent` to cancelled. However, Bedrock clients do not automatically revert the cursor item on cancelled transactions. To resolve this:
1.  We intercept the packet.
2.  We immediately resend the `InventoryContentPacket` for the chest container.
3.  We send the `InventoryContentPacket` for the player's main inventory (Window ID `0`) to force-resync the client-side inventory state.
4.  We cancel the event.
