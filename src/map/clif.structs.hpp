#ifndef CLIF_STRUCTS
#define CLIF_STRUCTS

# include <cstdint>

// these need better names
// the only one that is fully accurate is DEAD
enum class BeingRemoveType
{
    NEGATIVE = -1,
    ZERO = 0,
    DEAD = 1,
    QUIT = 2,
    WARP = 3,
    DISGUISE = 9,
};
enum class PickupFail : uint8_t
{
    OKAY = 0,
    BAD_ITEM = 1,
    TOO_HEAVY = 2,
    TOO_FAR = 3,
    INV_FULL = 4,
    STACK_FULL = 5,
    DROP_STEAL = 6,
};

enum class ArrowFail : uint16_t
{
    NO_AMMO = 0,
    EQUIPPING = 3,
};

#endif //CLIF_STRUCTS
