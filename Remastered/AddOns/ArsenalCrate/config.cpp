// Ammo Crates (Arsenal) - a stock-asset ammo crate that offers UNLIMITED weapons
// and magazines. The crate itself is an ordinary ReammoBox (stock model/assets);
// the Poseidon inventory engine detects a crate of class "ArsenalCrate" nearby
// and shows an infinite "one of everything" arsenal page (open with TAB).
//
// Parsed straight from disk by the engine (Globals::Init in GameState.cpp).
// ReammoBox lives in the base bin/config.bin, so no addon dependency is needed.
class CfgPatches
{
    class ArsenalCrate
    {
        units[] = {"ArsenalCrate"};
        weapons[] = {};
        requiredVersion = 1.0;
        requiredAddons[] = {};
    };
};

class CfgVehicles
{
    class ReammoBox;                       // external stock ammo-crate base (base config)
    class ArsenalCrate : ReammoBox
    {
        scope = 2;                         // shown in the editor object list
        displayName = "Ammo Crates (Arsenal)";
        vehicleClass = "Ammo";             // same "Ammo Crates" editor category
    };
};
