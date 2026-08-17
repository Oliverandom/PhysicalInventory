#include "InventoryModel.hpp"
#include "../../World/Entities/Weapons/ItemMass.hpp"

#include <Poseidon/World/Entities/Infantry/Person.hpp>
#include <Poseidon/World/Entities/Infantry/ManActs.hpp>
#include <Poseidon/World/Entities/Infantry/SoldierOld.hpp>
#include <Poseidon/World/Entities/Weapons/Weapons.hpp>
#include <Poseidon/World/Terrain/Landscape.hpp>
#include <Poseidon/AI/VehicleAI.hpp>
#include <Poseidon/AI/EntityAI.hpp>
#include <Poseidon/AI/EntityAIType.hpp>
#include <Poseidon/AI/AIUnit.hpp>
#include <Poseidon/AI/AIGroup.hpp>
#include <Poseidon/AI/AICenter.hpp>
#include <Poseidon/World/Scene/Object.hpp>   // TargetSide
#include <Poseidon/Network/Network.hpp>
#include <Poseidon/World/World.hpp>
#include <Poseidon/Foundation/platform.hpp>   // strcmpi

#include <cstring>   // strcmp
#include <cstdio>    // fopen (settings file)

namespace Poseidon
{

// --- Weapon hotkeys & inventory settings ----------------------------------

// A hotkey binding. `cls` is a KIND-QUALIFIED key ("w:M16" / "m:HandGrenade") so a
// weapon and its like-named magazine don't collide (OFP gives them the same class
// name). `inst` is the specific clicked item pointer (WeaponType*/Magazine*) - used
// only so the number badge shows on the ONE item clicked, not every copy.
struct HotkeyBind { RString cls; const void *inst = nullptr; };
static HotkeyBind g_weaponHotkeys[10];
static int  g_invOpenKey  = 18;      // SDL_SCANCODE_O (matches the UAInventory default)
static bool g_invEditMenu = false;

// --- Global inventory UI settings (edit menu; persisted to the cfg file) -----
// Defaults live in one place so "Reset to default" is a straight copy-back.
struct InvUISettings
{
    float iconScale   = 1.00f;   //!< multiplies EVERY icon's drawn size (0.40..2.00)
    int   gridCols    = 7;       //!< inventory grid columns (1..12)
    float volCapOverride = 0.0f; //!< 0 = use per-side default; else litres (5..150)
    bool  accentTheme  = false;  //!< tint panel backgrounds/headers with the accent
    bool  accentBorder = false;  //!< tint borders/outlines with the accent
    float accentR = 0.35f;       //!< accent colour (also used for highlights)
    float accentG = 0.62f;
    float accentB = 1.00f;
    float vicW = 1.00f;          //!< vicinity panel width  scale (0.50..1.60)
    float vicH = 1.00f;          //!< vicinity panel height scale
    float invW = 1.00f;          //!< inventory panel width  scale
    float invH = 1.00f;          //!< inventory panel height scale
    float mouseSensFloor = 0.18f;//!< min mouse-look scale at max weight (0.01 frozen..1.0 off)
    bool  showBars    = true;    //!< show the on-screen health + stamina bars
    float barOpacity  = 0.90f;   //!< opacity of the health + stamina bars (0.05..1.0)
    bool  showFuelHud = true;    //!< show the in-vehicle fuel + speed HUD
    float fuelHudOpacity = 0.90f;//!< opacity of the in-vehicle fuel + speed HUD (0.05..1.0)
    bool  weaponsInBackpack = false; //!< allow rifles/launchers in the grid (as 3x2 tiles)
    bool  infiniteWeight  = false;   //!< ignore the carry-weight cap
    bool  infiniteVolume  = false;   //!< ignore the volume cap
    bool  infiniteStamina = false;   //!< never drain stamina
};
static InvUISettings g_ui;

float InventoryGlobalIconScale()            { return g_ui.iconScale; }
void  SetInventoryGlobalIconScale(float v)  { g_ui.iconScale = v < 0.40f ? 0.40f : (v > 2.00f ? 2.00f : v); }
int   InventoryGridCols()                   { return g_ui.gridCols; }
void  SetInventoryGridCols(int c)           { g_ui.gridCols = c < 1 ? 1 : (c > 12 ? 12 : c); }
float InventoryVolumeCapOverride()          { return g_ui.volCapOverride; }
void  SetInventoryVolumeCapOverride(float v){ g_ui.volCapOverride = v < 0.0f ? 0.0f : (v > 150.0f ? 150.0f : v); }
bool  InventoryAccentTheming()              { return g_ui.accentTheme; }
void  SetInventoryAccentTheming(bool b)     { g_ui.accentTheme = b; }
bool  InventoryAccentBorders()              { return g_ui.accentBorder; }
void  SetInventoryAccentBorders(bool b)     { g_ui.accentBorder = b; }
void  InventoryAccentColor(float &r, float &g, float &b) { r = g_ui.accentR; g = g_ui.accentG; b = g_ui.accentB; }
void  SetInventoryAccentColor(float r, float g, float b)
{
    auto cl = [](float x){ return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); };
    g_ui.accentR = cl(r); g_ui.accentG = cl(g); g_ui.accentB = cl(b);
}
float InventoryVicWinScaleW() { return g_ui.vicW; }
float InventoryVicWinScaleH() { return g_ui.vicH; }
float InventoryInvWinScaleW() { return g_ui.invW; }
float InventoryInvWinScaleH() { return g_ui.invH; }
static float ClampWin(float v) { return v < 0.50f ? 0.50f : (v > 1.60f ? 1.60f : v); }
void  SetInventoryVicWinScaleW(float v) { g_ui.vicW = ClampWin(v); }
void  SetInventoryVicWinScaleH(float v) { g_ui.vicH = ClampWin(v); }
void  SetInventoryInvWinScaleW(float v) { g_ui.invW = ClampWin(v); }
void  SetInventoryInvWinScaleH(float v) { g_ui.invH = ClampWin(v); }
float InventoryMouseSensFloor()             { return g_ui.mouseSensFloor; }
void  SetInventoryMouseSensFloor(float v)   { g_ui.mouseSensFloor = v < 0.01f ? 0.01f : (v > 1.0f ? 1.0f : v); }
bool  InventoryShowBars()                   { return g_ui.showBars; }
void  SetInventoryShowBars(bool b)          { g_ui.showBars = b; }
float InventoryBarOpacity()                 { return g_ui.barOpacity; }
void  SetInventoryBarOpacity(float v)       { g_ui.barOpacity = v < 0.05f ? 0.05f : (v > 1.0f ? 1.0f : v); }
bool  InventoryShowFuelHud()                { return g_ui.showFuelHud; }
void  SetInventoryShowFuelHud(bool b)       { g_ui.showFuelHud = b; }
float InventoryFuelHudOpacity()             { return g_ui.fuelHudOpacity; }
void  SetInventoryFuelHudOpacity(float v)   { g_ui.fuelHudOpacity = v < 0.05f ? 0.05f : (v > 1.0f ? 1.0f : v); }
bool  InventoryWeaponsInBackpack()          { return g_ui.weaponsInBackpack; }
void  SetInventoryWeaponsInBackpack(bool b) { g_ui.weaponsInBackpack = b; }
bool  InventoryInfiniteWeight()             { return g_ui.infiniteWeight; }
void  SetInventoryInfiniteWeight(bool b)    { g_ui.infiniteWeight = b; }
bool  InventoryInfiniteVolume()             { return g_ui.infiniteVolume; }
void  SetInventoryInfiniteVolume(bool b)    { g_ui.infiniteVolume = b; }
bool  InventoryInfiniteStamina()            { return g_ui.infiniteStamina; }
void  SetInventoryInfiniteStamina(bool b)   { g_ui.infiniteStamina = b; }
void  ResetInventorySettingsToDefault() { g_ui = InvUISettings(); }

void SetWeaponHotkey(int slot, RString weaponName, const void *inst)
{
    if (slot >= 0 && slot < 10)
    {
        g_weaponHotkeys[slot].cls = weaponName;
        g_weaponHotkeys[slot].inst = inst;
    }
}

RString GetWeaponHotkey(int slot)
{
    return (slot >= 0 && slot < 10) ? g_weaponHotkeys[slot].cls : RString();
}

int WeaponHotkeyBadgeSlot(const char *key, const void *inst)
{
    if (!key || !*key)
        return -1;
    const bool isMag = (key[0] == 'm' && key[1] == ':');
    for (int s = 0; s < 10; s++)
    {
        const HotkeyBind &b = g_weaponHotkeys[s];
        if (b.cls.GetLength() == 0 || strcmp((const char *)b.cls, key) != 0)
            continue;
        // Throwables share a class, so a magazine badge only lights the exact clicked
        // instance - not every grenade of that type.
        if (isMag && b.inst && inst && b.inst != inst)
            continue;
        return s;
    }
    return -1;
}

bool ApplyWeaponHotkey(Person *player, int slot)
{
    if (!player)
        return false;
    RString wn = GetWeaponHotkey(slot);
    if (wn.GetLength() == 0)
        return false;
    const char *key = (const char *)wn;

    // A slot binding ("slot:0/1/2") selects whatever weapon currently occupies the
    // primary / secondary / handgun loadout slot - so the key follows the slot, not a
    // specific weapon, and keeps working after the player swaps guns.
    if (key[0] == 's' && key[1] == 'l' && key[2] == 'o' && key[3] == 't' && key[4] == ':')
    {
        const int kind = key[5] - '0';
        const int mask = (kind == 0) ? MaskSlotPrimary
                       : (kind == 1) ? MaskSlotSecondary
                                     : MaskSlotHandGun;
        for (int i = 0; i < player->NMagazineSlots(); i++)
        {
            const MagazineSlot &ms = player->GetMagazineSlot(i);
            if (ms._weapon && (ms._weapon->_weaponType & mask))
            {
                player->SelectWeapon(i, true);
                return true;
            }
        }
        return false;
    }

    // Bindings are kind-qualified ("w:<weapon>" / "m:<magazine>") because a weapon and
    // its magazine share a class name in OFP. Strip the prefix; tolerate legacy
    // unprefixed keys by trying the weapon path then the throwable path.
    const bool isMag = (key[0] == 'm' && key[1] == ':');
    const bool isWpn = (key[0] == 'w' && key[1] == ':');
    const char *name = (isMag || isWpn) ? key + 2 : key;

    // First-aid kit: pressing its hotkey USES the kit (full self-heal, one use)
    // exactly like the "Use first aid kit" scroll action - it is never wielded.
    // Route an ATUseMagazine action through the player's unit so the medic
    // animation, heal, and depletion all run as they do from the action menu
    // (see Man::StartActionProcessing / EntityAI::PerformAction).
    if (!isWpn && IsFirstAidKitMagazine(name))
    {
        for (int m = 0; m < player->NMagazines(); m++)
        {
            Magazine *mag = player->GetMagazine(m);
            if (!mag || !mag->_type || mag->_ammo <= 0 ||
                strcmp((const char *)mag->_type->GetName(), name) != 0)
                continue;
            UIAction act;
            act.type = ATUseMagazine;
            act.target = player;
            act.param = mag->_creator;
            act.param2 = mag->_id;
            act.priority = 0.5f;
            act.showWindow = false;
            act.hideOnUse = true;
            player->StartActionProcessing(act, player->Brain());
            return true;
        }
        return false;
    }

    // IMPORTANT: EntityAI::SelectWeapon() indexes MAGAZINE SLOTS (muzzles), not the
    // weapon-systems list. The old code passed a weapon-systems index straight to
    // SelectWeapon(), so anything whose slot index differed (notably the launcher)
    // selected the wrong slot and the keypress fell through to the command menu.

    // A real weapon (rifle / launcher / pistol): match the slot's muzzle weapon.
    if (!isMag)
    {
        for (int i = 0; i < player->NMagazineSlots(); i++)
        {
            const MagazineSlot &ms = player->GetMagazineSlot(i);
            if (ms._weapon && strcmp((const char *)ms._weapon->GetName(), name) == 0)
            {
                player->SelectWeapon(i, true);
                return true;
            }
        }
    }

    // A throwable (grenade / mine / satchel): the binding is a MAGAZINE class.
    if (!isWpn)
    {
        // Already the magazine loaded in some Throw/Put slot -> just select it.
        // Clear any residual reload timers so it reads ready (green) at once and the
        // placing action isn't gated behind a reload.
        for (int i = 0; i < player->NMagazineSlots(); i++)
        {
            const MagazineSlot &ms = player->GetMagazineSlot(i);
            if (ms._magazine && ms._magazine->_type &&
                strcmp((const char *)ms._magazine->_type->GetName(), name) == 0)
            {
                ms._magazine->_reload = 0;
                ms._magazine->_reloadMagazine = 0;
                player->SelectWeapon(i, true);
                return true;
            }
        }
        // Otherwise (e.g. smoke while frag is loaded, or a mine/satchel not yet in
        // hand): find the carried magazine of that class and the muzzle slot that can
        // use it, swap it in, then select it. ReloadMagazineTimed(afterAnimation=true)
        // does an instant magazine swap, but it still primes the per-shot reload timer
        // (that's the red/amber/green flash and what was blocking the place animation),
        // so we zero those timers right after to make the item instantly ready.
        for (int m = 0; m < player->NMagazines(); m++)
        {
            Magazine *mag = player->GetMagazine(m);
            if (!mag || !mag->_type ||
                strcmp((const char *)mag->_type->GetName(), name) != 0)
                continue;
            for (int i = 0; i < player->NMagazineSlots(); i++)
            {
                const MagazineSlot &ms = player->GetMagazineSlot(i);
                if (ms._muzzle && ms._muzzle->CanUse(mag->_type))
                {
                    player->ReloadMagazineTimed(i, m, true);   // instant magazine swap
                    mag->_reload = 0;                          // no per-shot reload flash
                    mag->_reloadMagazine = 0;
                    player->SelectWeapon(i, true);
                    return true;
                }
            }
        }
    }
    return false;
}

int  InventoryOpenKey()          { return g_invOpenKey; }
bool InventoryEditMenuVisible()  { return g_invEditMenu; }
void SetInventoryOpenKey(int sc) { g_invOpenKey = sc; }
void SetInventoryEditMenuVisible(bool s) { g_invEditMenu = s; }

void LoadInventorySettings()
{
    FILE *f = fopen("Remastered\\inventory_settings.cfg", "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char key[128];
        double val;
        if (sscanf(line, " %127[^= ] = %lf", key, &val) != 2)
            continue;

        if (strstr(key, "inventoryKey"))            g_invOpenKey = (int)val;
        // showEditMenu is intentionally NOT restored: the debug/edit menu always
        // starts hidden and is only turned on in-session via its bound key, so it
        // never pops up by default even if a tuning session left it enabled.
        else if (strstr(key, "iconScale"))          SetInventoryGlobalIconScale((float)val);
        else if (strstr(key, "gridCols"))           SetInventoryGridCols((int)val);
        else if (strstr(key, "volCapOverride"))      SetInventoryVolumeCapOverride((float)val);
        else if (strstr(key, "accentTheme"))         SetInventoryAccentTheming(val != 0.0);
        else if (strstr(key, "accentBorder"))        SetInventoryAccentBorders(val != 0.0);
        else if (strstr(key, "accentR"))             g_ui.accentR = (float)val;
        else if (strstr(key, "accentG"))             g_ui.accentG = (float)val;
        else if (strstr(key, "accentB"))             g_ui.accentB = (float)val;
        else if (strstr(key, "vicW"))                SetInventoryVicWinScaleW((float)val);
        else if (strstr(key, "vicH"))                SetInventoryVicWinScaleH((float)val);
        else if (strstr(key, "invW"))                SetInventoryInvWinScaleW((float)val);
        else if (strstr(key, "invH"))                SetInventoryInvWinScaleH((float)val);
        else if (strstr(key, "mouseSensFloor"))      SetInventoryMouseSensFloor((float)val);
        else if (strstr(key, "showFuelHud"))         SetInventoryShowFuelHud(val != 0.0);
        else if (strstr(key, "fuelHudOpacity"))      SetInventoryFuelHudOpacity((float)val);
        else if (strstr(key, "showBars"))            SetInventoryShowBars(val != 0.0);
        else if (strstr(key, "barOpacity"))          SetInventoryBarOpacity((float)val);
        else if (strstr(key, "weaponsInBackpack"))   SetInventoryWeaponsInBackpack(val != 0.0);
        else if (strstr(key, "infiniteWeight"))      SetInventoryInfiniteWeight(val != 0.0);
        else if (strstr(key, "infiniteVolume"))      SetInventoryInfiniteVolume(val != 0.0);
        else if (strstr(key, "infiniteStamina"))     SetInventoryInfiniteStamina(val != 0.0);
    }
    fclose(f);
}

void SaveInventorySettings()
{
    FILE *f = fopen("Remastered\\inventory_settings.cfg", "w");
    if (!f)
        return;
    fprintf(f, "// Inventory settings (written by the in-game edit menu).\n");
    fprintf(f, "// inventoryKey = SDL scancode (O=18, K=14, I=12, TAB=43, ...). showEditMenu = 0/1.\n");
    fprintf(f, "inventoryKey=%d\n", g_invOpenKey);
    fprintf(f, "showEditMenu=%d\n", g_invEditMenu ? 1 : 0);
    fprintf(f, "// --- edit-menu UI settings ---\n");
    fprintf(f, "iconScale=%.4f\n",      g_ui.iconScale);
    fprintf(f, "gridCols=%d\n",         g_ui.gridCols);
    fprintf(f, "volCapOverride=%.4f\n", g_ui.volCapOverride);
    fprintf(f, "accentTheme=%d\n",      g_ui.accentTheme ? 1 : 0);
    fprintf(f, "accentBorder=%d\n",     g_ui.accentBorder ? 1 : 0);
    fprintf(f, "accentR=%.4f\n",        g_ui.accentR);
    fprintf(f, "accentG=%.4f\n",        g_ui.accentG);
    fprintf(f, "accentB=%.4f\n",        g_ui.accentB);
    fprintf(f, "vicW=%.4f\n",           g_ui.vicW);
    fprintf(f, "vicH=%.4f\n",           g_ui.vicH);
    fprintf(f, "invW=%.4f\n",           g_ui.invW);
    fprintf(f, "invH=%.4f\n",           g_ui.invH);
    fprintf(f, "mouseSensFloor=%.4f\n", g_ui.mouseSensFloor);
    fprintf(f, "showBars=%d\n",          g_ui.showBars ? 1 : 0);
    fprintf(f, "barOpacity=%.4f\n",      g_ui.barOpacity);
    fprintf(f, "showFuelHud=%d\n",       g_ui.showFuelHud ? 1 : 0);
    fprintf(f, "fuelHudOpacity=%.4f\n",  g_ui.fuelHudOpacity);
    fprintf(f, "weaponsInBackpack=%d\n", g_ui.weaponsInBackpack ? 1 : 0);
    fprintf(f, "infiniteWeight=%d\n",    g_ui.infiniteWeight ? 1 : 0);
    fprintf(f, "infiniteVolume=%d\n",    g_ui.infiniteVolume ? 1 : 0);
    fprintf(f, "infiniteStamina=%d\n",   g_ui.infiniteStamina ? 1 : 0);
    fclose(f);
}

// --- Per-icon tuning (debug tool + runtime overrides) ---------------------
namespace
{
struct TuneRec { RStringB name; IconTune t; };
static AutoArray<TuneRec> g_tunes;
}

static IconTune *FindIconTuneExact(const char *key)
{
    if (!key || !*key)
        return nullptr;
    for (int i = 0; i < g_tunes.Size(); i++)
        if (strcmp((const char *)g_tunes[i].name, key) == 0)
            return &g_tunes[i].t;
    return nullptr;
}

IconTune *FindIconTune(const char *key)
{
    if (IconTune *e = FindIconTuneExact(key))
        return e;
    // Legacy fallback: tunes saved before weapon/mag keys were split by kind were
    // keyed by the bare class name. Keep applying those until the item is re-edited
    // (which writes a kind-qualified key and stops using this fallback).
    const char *bare = (key && key[0] && key[1] == ':') ? key + 2 : nullptr;
    return bare ? FindIconTuneExact(bare) : nullptr;
}

IconTune &EditIconTune(const char *key)
{
    if (IconTune *e = FindIconTuneExact(key))   // exact only - never edit via the legacy fallback
        return *e;
    TuneRec r;
    r.name = RStringB(key);
    // Seed a freshly split "w:<class>"/"m:<class>" entry from any legacy bare-class
    // tune, so the previously-tuned value carries over as the starting point.
    if (key && key[0] && key[1] == ':')
        if (const IconTune *legacy = FindIconTuneExact(key + 2))
            r.t = *legacy;
    g_tunes.Add(r);
    return g_tunes[g_tunes.Size() - 1].t;
}

// Kind-qualified tuning key ("w:<class>" / "m:<class>") so a weapon and its
// same-named magazine (a launcher and its rocket) tune independently.
RString IconTuneKey(const InvCell &cell)
{
    const char *pre = (cell.kind == InvItemKind::Weapon) ? "w:" : "m:";
    return RString(pre) + RString((const char *)cell.ClassName());
}

// The laser designator is a weapon whose "magazine" is really just its battery.
// We don't want that battery listed as a separate inventory item or given an ammo
// counter, so it reads as a tool that simply emits a laser.
static bool IsLaserDesignatorClass(const char *cn)
{
    return cn && (strcmp(cn, "LaserDesignator") == 0 || strcmp(cn, "LaserDesignatorOH") == 0);
}

void KeepLaserDesignatorLoaded(Person *player)
{
    if (!player)
        return;
    for (int i = 0; i < player->NMagazineSlots(); i++)
    {
        const MagazineSlot &slot = player->GetMagazineSlot(i);
        Magazine *m = slot._magazine.GetRef();   // Ref<Magazine> pointee is mutable
        if (!m || !m->_type)
            continue;
        if (!IsLaserDesignatorClass((const char *)m->_type->GetName()))
            continue;
        const int maxA = m->_type->_maxAmmo;
        if ((int)m->_ammo < maxA)
            m->_ammo = maxA;   // refill the battery so the laser never depletes
    }
}

// Baked-in default icon tuning (sizes / nudges / footprints) from the tuning pass.
// IconTune field order: size, ox, oy, gw, gh, vsize, vox, voy, vw, lsize, lox, loy.
// These apply even with no invicons\_tuning.txt; the file, if present, overrides
// individual entries (so the debug tool can still tweak on top).
namespace
{
struct DefaultTune { const char *key; IconTune t; };
static const DefaultTune kDefaultTunes[] = {
    {"HandGrenade", {0.9843f,0.0400f,0.0100f,1,1,0.9843f,0.0400f,0.0100f,1,0.9843f,0.0400f,0.0100f}},
    {"M16", {1.0000f,0.0000f,0.0000f,1,1,1.0000f,0.0000f,0.0000f,1,1.0000f,0.0000f,0.0000f}},
    {"M21", {1.0000f,0.0000f,0.0000f,4,1,1.0000f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"LAWLauncher", {0.9936f,0.0000f,0.0000f,2,1,0.9936f,0.0000f,0.0000f,2,0.9936f,0.0000f,0.0000f}},
    {"CarlGustavLauncher", {1.0879f,0.0000f,0.0000f,4,1,1.0879f,0.0000f,0.0000f,2,1.0879f,0.0000f,0.0000f}},
    {"m:LAWLauncher", {0.9936f,0.0000f,0.0000f,2,1,0.9936f,0.0000f,0.0000f,1,0.9936f,0.0000f,0.0000f}},
    {"w:LAWLauncher", {1.0251f,0.0000f,0.0000f,2,1,1.0251f,0.0000f,0.0000f,2,1.0251f,0.0000f,0.0000f}},
    {"w:M16", {1.0000f,0.0000f,0.0000f,1,1,1.0000f,0.0000f,0.0000f,2,1.0093f,0.0000f,0.0000f}},
    {"m:Mortar", {1.0408f,0.0000f,0.0000f,2,1,1.0408f,0.0000f,0.0000f,1,1.0408f,0.0000f,0.0000f}},
    {"w:NVGoggles", {0.7893f,0.0100f,0.0000f,3,1,0.7893f,0.0100f,0.0000f,2,0.7893f,0.0100f,0.0000f}},
    {"m:M60", {1.0722f,0.0000f,0.0000f,2,1,1.0093f,0.0000f,0.0000f,2,1.0722f,0.0000f,0.0000f}},
    {"w:M60", {1.0722f,0.0000f,0.0000f,4,1,1.3708f,0.0000f,0.0000f,3,1.0722f,0.0000f,0.0000f}},
    {"w:CarlGustavLauncher", {1.0565f,0.0000f,0.0000f,4,1,1.0565f,0.0000f,0.0000f,2,1.0565f,0.0000f,0.0000f}},
    {"m:CarlGustavLauncher", {1.1164f,-0.0100f,0.0200f,3,1,1.1036f,0.0000f,-0.0300f,2,1.1164f,-0.0100f,0.0200f}},
    {"w:AALauncher", {1.0565f,0.0000f,0.0000f,4,1,1.0565f,0.0000f,0.0000f,2,1.0565f,0.0000f,0.0000f}},
    {"m:AALauncher", {1.1007f,0.0000f,0.0000f,4,1,1.0408f,0.0000f,0.0000f,2,1.1007f,0.0000f,0.0000f}},
    {"w:M21", {1.1792f,0.0000f,0.0000f,4,1,1.6065f,0.0000f,0.0000f,3,1.1792f,0.0000f,0.0000f}},
    {"m:M21", {0.9622f,0.0000f,0.0000f,1,1,0.9622f,0.0000f,0.0000f,1,0.9622f,0.0000f,0.0000f}},
    {"w:Binocular", {0.9121f,0.0000f,0.0000f,2,1,0.8679f,0.0000f,0.0000f,2,0.9121f,0.0000f,0.0000f}},
    {"m:HK", {1.0000f,0.0000f,0.0000f,1,1,1.0000f,0.0000f,0.0000f,1,1.0000f,0.0000f,0.0000f}},
    {"m:AK74", {0.9936f,0.0000f,0.0000f,1,1,0.9936f,0.0000f,0.0000f,1,0.9936f,0.0000f,0.0000f}},
    {"w:AK74GrenadeLauncher", {1.0251f,0.0000f,0.0000f,4,1,1.0251f,0.0000f,0.0000f,2,1.0251f,0.0000f,0.0000f}},
    {"w:AK74SU", {1.0565f,0.0000f,0.0000f,4,1,0.9622f,0.0000f,0.0000f,2,1.0565f,0.0000f,0.0000f}},
    {"m:PK", {1.0000f,0.0000f,0.0000f,2,1,1.0093f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:PK", {0.9936f,0.0000f,0.0000f,4,1,1.5436f,0.0000f,0.0000f,3,0.9936f,0.0000f,0.0000f}},
    {"m:AT4Launcher", {0.9779f,0.0000f,0.0000f,2,1,0.9779f,0.0000f,0.0000f,1,0.9779f,0.0000f,0.0000f}},
    {"m:RPGLauncher", {0.9779f,0.0000f,-0.0300f,2,1,1.0000f,0.0000f,0.0000f,1,0.9779f,0.0000f,-0.0300f}},
    {"w:9K32Launcher", {1.1036f,0.0000f,0.0500f,4,1,1.4965f,0.0000f,0.0500f,3,1.1036f,0.0000f,0.0500f}},
    {"w:AT4Launcher", {1.0000f,0.0000f,0.0000f,4,1,1.0000f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:RPGLauncher", {1.0408f,0.0000f,0.0300f,4,1,1.0408f,0.0000f,0.0300f,2,1.0408f,0.0000f,0.0300f}},
    {"m:9K32Launcher", {0.9936f,0.0000f,-0.0200f,6,1,1.0000f,0.0000f,-0.0200f,3,0.9936f,0.0000f,-0.0200f}},
    {"w:SVDDragunov", {1.1036f,0.0000f,0.0000f,4,1,1.5751f,0.0000f,0.0000f,3,1.1036f,0.0000f,0.0000f}},
    {"m:SVDDragunov", {0.9308f,0.0000f,0.0000f,1,1,0.9308f,0.0000f,0.0000f,1,0.9308f,0.0000f,0.0000f}},
    {"m:6G30Magazine", {1.6851f,0.0000f,0.0000f,3,1,1.7000f,0.0000f,0.0000f,2,1.6851f,0.0000f,0.0000f}},
    {"m:MM1Magazine", {1.5751f,0.0000f,-0.0200f,3,1,1.6222f,0.0000f,-0.0200f,2,1.5751f,0.0000f,-0.0200f}},
    {"m:RevolverMag", {1.0093f,0.0600f,0.0500f,1,1,1.0093f,0.0600f,0.0500f,1,1.0093f,0.0600f,0.0500f}},
    {"m:PipeBomb", {1.2451f,-0.0100f,0.0300f,2,1,1.1979f,0.0000f,0.0000f,2,1.2451f,-0.0100f,0.0300f}},
    {"w:Skorpion", {1.0000f,0.0000f,0.0000f,3,1,1.0000f,0.0000f,0.0000f,2,1.1508f,0.0000f,0.0000f}},
    {"w:Tokarev", {1.0565f,0.0000f,0.0000f,3,1,0.9622f,0.0000f,0.0000f,2,1.1508f,0.0000f,0.0000f}},
    {"w:Kozlice", {1.1351f,0.0000f,0.0000f,4,1,1.7000f,0.0000f,0.0000f,3,1.1351f,0.0000f,0.0000f}},
    {"w:Glock", {0.7265f,0.0000f,0.0000f,3,1,0.6793f,0.0000f,0.0000f,2,0.7265f,0.0000f,0.0000f}},
    {"w:Beretta", {1.1665f,0.0000f,0.0000f,2,1,0.8365f,0.0000f,0.0000f,2,1.1665f,0.0000f,0.0000f}},
    {"m:TokarevMag", {0.8993f,0.0000f,0.0000f,1,1,0.8993f,0.0000f,0.0000f,1,0.8993f,0.0000f,0.0000f}},
    {"w:CZ75", {1.0250f,0.0000f,0.0000f,2,1,1.0251f,0.0000f,0.0000f,2,1.2608f,0.0000f,0.0000f}},
    {"w:Ingram", {1.0000f,0.0000f,0.0000f,2,1,1.1351f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:Revolver", {1.0000f,0.0000f,0.0000f,2,1,1.0000f,0.0000f,0.0000f,2,1.2136f,0.0000f,0.0000f}},
    {"w:GlockS", {1.0000f,0.0000f,0.0000f,2,1,1.0000f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"m:Mine", {1.0565f,0.0000f,0.0000f,2,1,1.0565f,0.0000f,0.0000f,2,1.0565f,0.0000f,0.0000f}},
    {"m:MineE", {1.0000f,0.0000f,0.0000f,2,1,1.0000f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:M4", {0.9465f,0.0000f,0.0000f,4,1,0.9465f,0.0000f,0.0000f,2,1.0250f,0.0000f,0.0000f}},
    {"m:HandGrenade", {0.9151f,0.0300f,0.0100f,1,1,0.9151f,0.0300f,0.0100f,1,0.9151f,0.0300f,0.0100f}},
    {"m:GrenadeLauncher", {1.1036f,0.0000f,0.0000f,1,1,1.0000f,0.0000f,0.0000f,1,1.1036f,0.0000f,0.0000f}},
    {"w:6G30", {1.0565f,0.0000f,0.0000f,4,1,1.0000f,0.0000f,0.0000f,2,1.0565f,0.0000f,0.0000f}},
    {"w:LaserDesignator", {0.9779f,0.0000f,0.0000f,4,1,0.9465f,0.0000f,0.0000f,2,0.9779f,0.0000f,0.0000f}},
    {"w:HuntingRifle", {1.0000f,0.0000f,0.0000f,5,1,1.7000f,0.0000f,0.0000f,3,1.1665f,0.0000f,0.0000f}},
    {"w:AK47GrenadeLauncher", {1.0000f,0.0000f,0.0000f,4,1,1.0251f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:FAL", {1.0000f,0.0000f,0.0000f,4,1,1.0408f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:HKG3", {1.0000f,0.0000f,0.0000f,4,1,1.5908f,0.0000f,0.0000f,3,1.1193f,0.0000f,0.0000f}},
    {"w:Steyr", {1.0000f,0.0000f,0.0000f,4,1,1.0879f,0.0000f,0.0000f,2,1.0000f,0.0000f,0.0000f}},
    {"w:G36a", {1.0000f,0.0000f,0.0000f,4,1,1.1164f,0.0000f,0.0000f,2,1.1164f,0.0000f,0.0000f}},
    // First-aid kits: baked from the tuning pass so they default to their current
    // on-screen size even with no invicons\_tuning.txt present.
    {"m:US_FirstAidKit",  {0.8624f,0.0200f,0.0000f,1,1,0.8817f,0.0000f,0.0000f,1,1.0000f,0.0000f,0.0000f}},
    {"m:AI2_FirstAidKit", {0.8624f,0.0000f,0.0000f,1,1,0.9010f,0.0000f,0.0000f,1,1.0000f,0.0000f,0.0000f}},
};
}

void LoadIconTuning()
{
    g_tunes.Clear();

    // 1) Baked-in defaults (the finalized tuning pass) - apply even with no file.
    for (const DefaultTune &d : kDefaultTunes)
    {
        TuneRec r;
        r.name = RStringB(d.key);
        r.t = d.t;
        g_tunes.Add(r);
    }

    // 2) Optional override file (the debug tool writes this). Upsert on top of the
    //    baked defaults so tweaks still work; delete the file to revert to defaults.
    FILE *f = fopen("invicons\\_tuning.txt", "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char nm[128];
        float sz, ox, oy, vsz, vox, voy, lsz, lox, loy;
        int gw, gh, vw;
        // Field growth over time: 7 -> 10 (adds vicinity) -> 13 (adds equipped).
        int n = sscanf(line, "%127s %f %f %f %d %d %d %f %f %f %f %f %f",
                       nm, &sz, &ox, &oy, &gw, &gh, &vw, &vsz, &vox, &voy, &lsz, &lox, &loy);
        if (n < 7)
            continue;
        if (n < 10) { vsz = sz; vox = ox; voy = oy; }
        if (n < 13) { lsz = sz; lox = ox; loy = oy; }
        IconTune nt;
        nt.size = sz; nt.ox = ox; nt.oy = oy; nt.gw = gw; nt.gh = gh;
        nt.vsize = vsz; nt.vox = vox; nt.voy = voy; nt.vw = vw;
        nt.lsize = lsz; nt.lox = lox; nt.loy = loy;
        if (IconTune *ex = FindIconTuneExact(nm))
            *ex = nt;
        else
        {
            TuneRec r; r.name = RStringB(nm); r.t = nt;
            g_tunes.Add(r);
        }
    }
    fclose(f);
}

void SaveIconTuning()
{
    FILE *f = fopen("invicons\\_tuning.txt", "w");
    if (!f)
        return;
    for (int i = 0; i < g_tunes.Size(); i++)
    {
        const IconTune &t = g_tunes[i].t;
        fprintf(f, "%s %.4f %.4f %.4f %d %d %d %.4f %.4f %.4f %.4f %.4f %.4f\n",
                (const char *)g_tunes[i].name, t.size, t.ox, t.oy, t.gw, t.gh, t.vw,
                t.vsize, t.vox, t.voy, t.lsize, t.lox, t.loy);
    }
    fclose(f);
}

// Grid footprint with any per-class override applied (gw>0 means "use override").
static GridSize TunedFootprint(const char *cn, GridSize def)
{
    if (const IconTune *t = FindIconTune(cn))
        if (t->gw > 0)
            return {t->gw, t->gh > 0 ? t->gh : def.h};
    return def;
}

// Vanilla take/drop feedback: shows the on-screen title (e.g. "Taking ammo")
// and plays the pickup sound from CfgCutScenes. Defined in Transport.cpp; the
// engine's own take/drop code forward-declares it the same way.
void CutScene(const char *name);

// ---------------------------------------------------------------------------
// InvCell
// ---------------------------------------------------------------------------

// Effective magazine type: the loaded instance's type, or (arsenal) the bare type.
static const MagazineType *CellMagType(const InvCell &c)
{
    if (c.magazine && c.magazine->_type) return c.magazine->_type;
    return c.magType;
}

RString InvCell::DisplayName() const
{
    switch (kind)
    {
        case InvItemKind::Weapon:
            return weapon ? weapon->GetDisplayName() : RString();
        case InvItemKind::Magazine:
        {
            const MagazineType *mt = CellMagType(*this);
            if (mt)
            {
                // The AP mine (MineE) and AT mine (Mine) share the config display
                // name "Mine"; relabel them so they read distinctly in the inventory.
                if (strcmp((const char *)mt->GetName(), "MineE") == 0)
                    return RString("AP Mine");
                if (strcmp((const char *)mt->GetName(), "Mine") == 0)
                    return RString("AT Mine");
            }
            return mt ? RString(mt->GetDisplayName()) : RString();
        }
        default:
            return RString();
    }
}

RStringB InvCell::ClassName() const
{
    switch (kind)
    {
        case InvItemKind::Weapon:
            return weapon ? weapon->GetName() : RStringB();
        case InvItemKind::Magazine:
        {
            const MagazineType *mt = CellMagType(*this);
            return mt ? mt->GetName() : RStringB();
        }
        default:
            return RStringB();
    }
}

RStringB InvCell::PictureName() const
{
    switch (kind)
    {
        case InvItemKind::Weapon:
            return weapon ? weapon->GetPictureName() : RStringB();
        case InvItemKind::Magazine:
        {
            const MagazineType *mt = CellMagType(*this);
            return mt ? mt->GetPictureName() : RStringB();
        }
        default:
            return RStringB();
    }
}

bool InvCell::GetAmmo(int &current, int &capacity) const
{
    if (kind != InvItemKind::Magazine)
    {
        return false;
    }
    const MagazineType *mt = CellMagType(*this);
    if (!mt)
    {
        return false;
    }
    current = magazine ? (int)magazine->_ammo : (int)mt->_maxAmmo;   // arsenal mags are full
    capacity = mt->_maxAmmo;
    return true;
}

float InvCell::Mass() const
{
    switch (kind)
    {
        case InvItemKind::Weapon:   return ItemMass::Of(weapon);
        case InvItemKind::Magazine:
            return magazine ? ItemMass::Of(magazine)
                            : (magType ? ItemMass::Of(magType) : 0.0f);
        default:                    return 0.0f;
    }
}

const void *InvCell::SourceKey() const
{
    static const int kArsenalKey = 0;
    if (fromArsenal)
    {
        return &kArsenalKey;   // all arsenal cells share one page
    }
    return (const void *)(const VehicleSupply *)container;
}

RString InvCell::SourceLabel() const
{
    if (fromArsenal)
    {
        return RString("Arsenal (unlimited)");
    }
    VehicleSupply *c = container;
    if (!c || !c->GetType())
    {
        return RString("Ground");
    }
    RString name(c->GetType()->GetDisplayName());
    return fromBody ? name + RString(" (body)") : name;
}

// ---------------------------------------------------------------------------
// InventoryModel
// ---------------------------------------------------------------------------

InventoryModel::InventoryModel(Person *player)
    : _player(player)
{
    Refresh();
}

static TargetSide PlayerSide(Person *p);   // defined below

float InventoryModel::MaxCarryWeight() const
{
    if (InventoryInfiniteWeight())
        return 1.0e6f;   // debug: effectively unlimited
    // Per-side carry budget (kg): civilians and resistance are less kitted out.
    switch (PlayerSide(_player))
    {
        case TCivilian: return 50.0f;
        case TGuerrila: return 55.0f;
        default:        return 60.0f;   // West / East (and unknown)
    }
}

RString InventoryModel::VicinitySourceLabel() const
{
    const int total = _containers.Size() + _bodies.Size();
    if (total == 0)
    {
        return RString("Nothing nearby");
    }
    RString name;
    if (_bodies.Size() > 0 && _bodies[0])
    {
        // A dead soldier - show its name, tagged as a body.
        name = (_bodies[0]->GetType())
                   ? RString(_bodies[0]->GetType()->GetDisplayName()) + RString(" (body)")
                   : RString("Body");
    }
    else
    {
        VehicleSupply *c0 = _containers[0];
        name = (c0 && c0->GetType()) ? RString(c0->GetType()->GetDisplayName())
                                     : RString("Container");
    }
    if (total > 1)
    {
        return name + RString("  (+more nearby)");
    }
    return name;
}

bool InventoryModel::HasWeaponWithMask(int mask) const
{
    if (!_player)
    {
        return false;
    }
    EntityAI *unit = _player;
    for (int i = 0; i < unit->NWeaponSystems(); i++)
    {
        const WeaponType *w = unit->GetWeaponSystem(i);
        if (w && (w->_weaponType & mask))
        {
            return true;
        }
    }
    return false;
}

// --- Per-side volume capacity (invisible internal budget) ------------------
static TargetSide PlayerSide(Person *p)
{
    if (!p)
        return TSideUnknown;
    AIUnit *u = p->Brain();
    AIGroup *g = u ? u->GetGroup() : nullptr;
    AICenter *c = g ? g->GetCenter() : nullptr;
    return c ? c->GetSide() : TSideUnknown;
}

static float SideVolumeCapacity(TargetSide s)
{
    switch (s)
    {
        case TWest:     return 75.0f;   // US: large ALICE + LBE
        case TEast:     return 36.0f;   // USSR: veshmeshok + belt kit
        case TGuerrila: return 45.0f;   // Resistance / CSLA: vz.85 + webbing
        case TCivilian: return 28.0f;   // civilian everyday batoh
        default:        return 40.0f;
    }
}

static float CellVolume(const InvCell &c)
{
    if (c.kind == InvItemKind::Weapon)
        return ItemMass::VolumeOf(c.weapon);
    const MagazineType *mt = c.magType ? c.magType : (c.magazine ? c.magazine->_type : nullptr);
    return mt ? ItemMass::VolumeOf(mt) : 0.0f;
}

int InventoryModel::PlayerSideIndex() const
{
    switch (PlayerSide(_player))
    {
        case TWest:     return 0;
        case TEast:     return 1;
        case TGuerrila: return 2;
        case TCivilian: return 3;
        default:        return -1;
    }
}

void InventoryModel::Refresh()
{
    _grid.Clear();
    _vicinity.Clear();
    _loadout.Clear();
    _equippedIdx.Clear();
    _containers.Clear();
    _bodies.Clear();
    _commandUnits.Clear();
    _carriedMass = 0.0f;

    if (!_player)
    {
        return;
    }

    ScanLoadout();
    ScanGrid();
    ScanVicinity();

    _carriedMass = ItemMass::TotalCarried(_player);
    _carriedVolume = ItemMass::TotalVolumeCarried(_player);
    // Edit-menu override (litres) wins over the per-side default when non-zero;
    // "infinite volume" wins over everything.
    _volumeCapacity = InventoryInfiniteVolume() ? 1.0e6f
                    : (InventoryVolumeCapOverride() > 0.0f) ? InventoryVolumeCapOverride()
                    : SideVolumeCapacity(PlayerSide(_player));
}

void InventoryModel::ScanGrid()
{
    EntityAI *unit = _player;

    // v3: rifles and launchers ONLY live in the two top loadout slots - never in
    // the storage grid. The grid holds spare PISTOLS (a player can carry more than
    // one handgun and swap which is equipped), binoculars / NVGs (which have no
    // loadout slot), plus magazines.
    for (int i = 0; i < unit->NWeaponSystems(); i++)
    {
        const WeaponType *w = unit->GetWeaponSystem(i);
        if (!w)
        {
            continue;
        }
        // Spare handguns + binoculars/NVGs belong in the grid. Primary/secondary long
        // guns normally live only in the loadout slots - but the debug "Rifles in
        // backpack" option also lets extra (non-wielded) rifles/launchers sit in the
        // grid as 3-wide x 2-tall tiles.
        int gridMasks = MaskSlotHandGun | MaskSlotBinocular;
        if (InventoryWeaponsInBackpack())
            gridMasks |= MaskSlotPrimary | MaskSlotSecondary;
        if ((w->_weaponType & gridMasks) == 0)
        {
            continue;
        }
        // Skip the one already shown in the loadout pistol slot.
        bool equipped = false;
        for (int e = 0; e < _equippedIdx.Size(); e++)
        {
            if (_equippedIdx[e] == i) { equipped = true; break; }
        }
        if (equipped)
        {
            continue;
        }

        InvCell cell;
        cell.pane = InvPane::Grid;
        cell.kind = InvItemKind::Weapon;
        cell.index = i;
        cell.weapon = w;
        const int wt = w->_weaponType;
        const bool laser = IsLaserDesignatorClass((const char *)w->GetName());
        if (wt & MaskSlotBinocular)
        {
            // NVGs share the binocular slot mask but get a wider 2-tile footprint;
            // plain binoculars stay a single tile.
            const char *wn = (const char *)w->GetName();
            if (wn && strcmpi(wn, "NVGoggles") == 0)
            {
                cell.footprint.w = 2;   // night-vision goggles: 2 tiles
                cell.footprint.h = 1;
            }
            else
            {
                cell.footprint.w = 1;   // binoculars: single tile
                cell.footprint.h = 1;
            }
        }
        else if ((wt & (MaskSlotPrimary | MaskSlotSecondary)) && !laser)
        {
            cell.footprint.w = 3;   // rifles/launchers (not the laser): 3 tiles long
            cell.footprint.h = 1;
        }
        else
        {
            cell.footprint = TunedFootprint((const char *)IconTuneKey(cell), ItemFootprint::Of(w));
        }
        FindPlacement(cell.footprint, cell.gx, cell.gy);
        _grid.Add(cell);
    }

    // The grid is otherwise magazines-only storage: every carried magazine that
    // isn't currently loaded in a weapon (spare rifle/pistol mags plus grenades).
    for (int i = 0; i < unit->NMagazines(); i++)
    {
        const Magazine *m = unit->GetMagazine(i);
        if (!m)
        {
            continue;
        }
        if (m->_type && IsLaserDesignatorClass((const char *)m->_type->GetName()))
        {
            continue;   // designator battery isn't shown as a carried item
        }

        // Skip a magazine that's currently loaded in a weapon - it's counted as
        // that weapon's ammo (shown on hover), not as a separate inventory item.
        // EXCEPTION: throwables (grenades/mines/satchels) stay listed even while
        // "loaded" in the Throw/Put muzzle - selecting one loads it into that muzzle,
        // and we don't want the item to vanish from the inventory when that happens.
        bool loaded = false;
        for (int k = 0; k < unit->NMagazineSlots(); k++)
        {
            if (unit->GetMagazineSlot(k)._magazine == m)
            {
                loaded = true;
                break;
            }
        }
        if (loaded && !IsThrowable(m->_type))
        {
            continue;
        }

        InvCell cell;
        cell.pane = InvPane::Grid;
        cell.kind = InvItemKind::Magazine;
        cell.index = i;
        cell.magazine = m;
        if (m->_type && strcmp((const char *)m->_type->GetName(), "JerryCan") == 0)
        {
            cell.footprint.w = 2;   // jerry can: 2 wide x 3 tall
            cell.footprint.h = 3;
        }
        else
        {
            cell.footprint = TunedFootprint((const char *)IconTuneKey(cell),
                                            m->_type ? ItemFootprint::Of(m->_type) : GridSize{1, 1});
        }
        FindPlacement(cell.footprint, cell.gx, cell.gy);
        _grid.Add(cell);
    }

    // NOTE: this re-derives grid position from scratch on every Refresh(),
    // which means a manual MoveWithinGrid() arrangement gets clobbered the
    // next time a pickup/drop triggers a rescan. If you want arrangements to
    // stick, key a small gx/gy cache off (kind, index) - or off the
    // Magazine's _id (EntityAIType.hpp:227), which is stable per-instance -
    // and consult it here before calling FindPlacement.
}

bool InventoryModel::IsThrowableItem(const InvCell &cell) const
{
    if (cell.kind != InvItemKind::Magazine)
        return false;
    const MagazineType *mt = cell.magType ? cell.magType
                                          : (cell.magazine ? cell.magazine->_type : nullptr);
    return IsThrowable(mt);
}

bool InventoryModel::IsThrowable(const MagazineType *mt) const
{
    if (!_player || !mt)
    {
        return false;
    }
    EntityAI *unit = _player;
    for (int i = 0; i < unit->NWeaponSystems(); i++)
    {
        const WeaponType *w = unit->GetWeaponSystem(i);
        if (!w)
        {
            continue;
        }
        // Only the "Throw"/"Put" muzzle-only pseudo-weapons (no real slot).
        if ((w->_weaponType & (MaskSlotPrimary | MaskSlotSecondary | MaskSlotHandGun)) != 0)
        {
            continue;
        }
        for (int j = 0; j < w->_muzzles.Size(); j++)
        {
            if (w->_muzzles[j] && w->_muzzles[j]->CanUse(mt))
            {
                return true;
            }
        }
    }
    return false;
}

int InventoryModel::SpareMagCount(const WeaponType *w) const
{
    if (!_player || !w)
    {
        return 0;
    }
    EntityAI *unit = _player;
    int count = 0;
    for (int i = 0; i < unit->NMagazines(); i++)
    {
        const Magazine *m = unit->GetMagazine(i);
        if (!m || !m->_type)
        {
            continue;
        }
        // Not the magazine currently loaded in a weapon.
        bool loaded = false;
        for (int k = 0; k < unit->NMagazineSlots(); k++)
        {
            if (unit->GetMagazineSlot(k)._magazine == m)
            {
                loaded = true;
                break;
            }
        }
        if (loaded)
        {
            continue;
        }
        for (int j = 0; j < w->_muzzles.Size(); j++)
        {
            if (w->_muzzles[j] && w->_muzzles[j]->CanUse(m->_type))
            {
                count++;
                break;
            }
        }
    }
    return count;
}

void InventoryModel::ScanLoadout()
{
    EntityAI *unit = _player;
    if (!unit)
    {
        return;
    }

    struct SlotDef { LoadoutSlot::Kind kind; int mask; };
    const SlotDef defs[3] = {
        { LoadoutSlot::Primary,   MaskSlotPrimary },
        { LoadoutSlot::Secondary, MaskSlotSecondary },
        { LoadoutSlot::Handgun,   MaskSlotHandGun },
    };

    for (int s = 0; s < 3; s++)
    {
        LoadoutSlot slot;
        slot.kind = defs[s].kind;

        const WeaponType *found = nullptr;
        int foundIdx = -1;
        for (int i = 0; i < unit->NWeaponSystems(); i++)
        {
            const WeaponType *w = unit->GetWeaponSystem(i);
            if (w && (w->_weaponType & defs[s].mask))
            {
                found = w;
                foundIdx = i;
                break;
            }
        }

        if (found)
        {
            slot.hasItem = true;
            slot.cell.pane = InvPane::Grid;   // carried item -> DropToGround works
            slot.cell.kind = InvItemKind::Weapon;
            slot.cell.index = foundIdx;
            slot.cell.weapon = found;
            slot.cell.footprint = ItemFootprint::Of(found);
            int cur = 0, cap = 0;
            // No ammo readout for the laser designator - it just emits a laser.
            if (!IsLaserDesignatorClass((const char *)found->GetName()) &&
                WeaponLoadedAmmo(found, cur, cap))
            {
                slot.count = cur;
            }
            slot.spare = SpareMagCount(found);
            _equippedIdx.Add(foundIdx);
        }
        _loadout.Add(slot);
    }
}

bool InventoryModel::WeaponLoadedAmmo(const WeaponType *w, int &cur, int &cap) const
{
    if (!_player || !w)
    {
        return false;
    }
    for (int j = 0; j < w->_muzzles.Size(); j++)
    {
        const MuzzleType *mz = w->_muzzles[j];
        for (int i = 0; i < _player->NMagazineSlots(); i++)
        {
            const MagazineSlot &slot = _player->GetMagazineSlot(i);
            if (slot._muzzle == mz && slot._magazine)
            {
                cur = slot._magazine->_ammo;
                cap = (slot._magazine->_type) ? slot._magazine->_type->_maxAmmo : cur;
                return true;
            }
        }
    }
    return false;
}

// A ground-drop holder (spawned by DropToTarget) is not a "container" the player
// walked up to - it's loose gear lying on the ground, so it belongs on the single
// shared "Ground" page rather than getting a page of its own.
static bool IsGroundHolderType(const VehicleSupply *c)
{
    if (!c || !c->GetType())
    {
        return false;
    }
    const char *n = (const char *)c->GetType()->GetName();
    return strcmp(n, "WeaponHolder") == 0 || strcmp(n, "SecondaryWeaponHolder") == 0;
}

// True if `m` is a living soldier the player commands: same group, player is the
// group leader, and it isn't the player. Used to expose squad-mates' inventories.
static bool IsCommandedByPlayer(const Person *player, const Man *m)
{
    if (!player || !m || (const Person *)m == player)
        return false;
    AIUnit *pu = player->Brain();
    AIUnit *mu = m->Brain();
    if (!pu || !mu || !pu->IsGroupLeader())
        return false;
    AIGroup *pg = pu->GetGroup();
    return pg && mu->GetGroup() == pg;
}

void InventoryModel::CollectContainers()
{
    if (!_player)
    {
        return;
    }

    const Vector3 pos = _player->Position();
    const float radius = VicinityRadius();

    // VERIFY: see the identical VERIFY note in the previous pass regarding
    // exact Landscape accessor names (GLOB_LAND, LandGrid). Left unchanged
    // here since it's unrelated to the grid/weight redesign.
    Landscape *land = GLOB_LAND;
    if (!land)
    {
        return;
    }

    const float cell = LandGrid;
    // Widen the CELL search window generously so large objects (APCs, the field
    // hospital) whose model ORIGIN sits a cell or two away are still visited. The
    // precise per-object reach test below (player radius + the object's own size)
    // decides what actually qualifies, so small objects still require true nearness.
    const float scan = radius + 40.0f;
    const int xMin = toIntFloor((pos.X() - scan) / cell);
    const int xMax = toIntFloor((pos.X() + scan) / cell);
    const int zMin = toIntFloor((pos.Z() - scan) / cell);
    const int zMax = toIntFloor((pos.Z() + scan) / cell);

    for (int z = zMin; z <= zMax; z++)
    {
        for (int x = xMin; x <= xMax; x++)
        {
            const ObjectList &list = land->GetObjects(z, x);
            for (int i = 0; i < list.Size(); i++)
            {
                Object *obj = list[i];
                if (!obj || obj == _player)
                {
                    continue;
                }
                // Reach = base radius + the object's own bounding size, so you can
                // access a vehicle from anywhere along its body (not just near its
                // model origin) and a big installation like the field hospital from
                // anywhere in its footprint - fixing the "only near one spot" issue.
                const float reach = radius + obj->GetRadius();
                if (obj->Position().Distance2(pos) > reach * reach)
                {
                    continue;
                }

                // Dead soldier? Its gear (worn weapons/magazines) is lootable.
                Man *man = dyn_cast<Man>(obj);
                if (man && man->IsDead())
                {
                    _bodies.Add(man);
                    continue;
                }
                // Living squad-mate you command? Its inventory is accessible for
                // give/take (you are the group leader and it is in your group).
                if (man && !man->IsDead() && IsCommandedByPlayer(_player, man))
                {
                    // Tight 2 m transfer range for squad-mates (you hand gear directly).
                    if (obj->Position().Distance2(pos) <= 2.0f * 2.0f)
                        _commandUnits.Add(man);
                    continue;
                }

                VehicleSupply *supply = dyn_cast<VehicleSupply>(obj);
                if (!supply)
                {
                    continue;
                }

                const EntityAIType *type = supply->GetType();
                if (!type)
                {
                    continue;
                }
                // Show the container if it CAN hold cargo, or if it currently DOES:
                // medic vehicles and the field hospital have no configured cargo
                // capacity but carry the first-aid kits stocked into them at spawn.
                const bool hasCargoCap = (type->GetMaxMagazinesCargo() > 0 || type->_maxWeaponsCargo > 0);
                const bool hasCargoNow = (supply->GetMagazineCargoSize() > 0 || supply->GetWeaponCargoSize() > 0);
                if (!hasCargoCap && !hasCargoNow)
                {
                    continue;
                }

                // Ambulances use a tighter access range: the vehicle body plus a small
                // (half of the default) 2 m margin. Measured from the body size, not a
                // fraction of the whole reach, so you can still reach the ENDS of a long
                // hull (halving the total reach put the vehicle's own rear out of range).
                {
                    const char *cn = (const char *)type->GetName();
                    if (strcmp(cn, "M113Ambul") == 0 || strcmp(cn, "BMPAmbul") == 0)
                    {
                        const float ambReach = obj->GetRadius() + radius * 0.5f;
                        if (obj->Position().Distance2(pos) > ambReach * ambReach)
                        {
                            continue;
                        }
                    }
                    else if (strcmp(cn, "MASH") == 0 || strcmp(cn, "hospital") == 0)
                    {
                        // Half the (generous, body-size-based) field-hospital reach.
                        const float mashReach = (radius + obj->GetRadius()) * 0.5f;
                        if (obj->Position().Distance2(pos) > mashReach * mashReach)
                        {
                            continue;
                        }
                    }
                }

                _containers.Add(supply);
            }
        }
    }
}

void InventoryModel::ScanVicinity()
{
    CollectContainers();

    // Dead soldiers: their worn weapons and magazines are lootable. Person is a
    // VehicleSupply, but its gear lives in weapon systems / magazines, not cargo.
    for (int b = 0; b < _bodies.Size(); b++)
    {
        Person *body = _bodies[b];
        if (!body)
        {
            continue;
        }
        for (int i = 0; i < body->NWeaponSystems(); i++)
        {
            const WeaponType *w = body->GetWeaponSystem(i);
            if (!w)
            {
                continue;
            }
            // Skip the Throw/Put muzzle pseudo-weapons.
            if ((w->_weaponType & (MaskSlotPrimary | MaskSlotSecondary | MaskSlotHandGun)) == 0)
            {
                continue;
            }
            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Weapon;
            cell.index = i;
            cell.container = body;   // Person is a VehicleSupply
            cell.fromBody = true;
            cell.weapon = w;
            _vicinity.Add(cell);
        }
        for (int i = 0; i < body->NMagazines(); i++)
        {
            const Magazine *m = body->GetMagazine(i);
            if (!m)
            {
                continue;
            }
            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Magazine;
            cell.index = i;
            cell.container = body;
            cell.fromBody = true;
            cell.magazine = m;
            _vicinity.Add(cell);
        }
    }

    // Living squad-mates you command: same person-gear handling as a body (fromBody),
    // but their page also accepts drops (give), and taking works too.
    for (int u = 0; u < _commandUnits.Size(); u++)
    {
        Person *mate = _commandUnits[u];
        if (!mate)
        {
            continue;
        }
        for (int i = 0; i < mate->NWeaponSystems(); i++)
        {
            const WeaponType *w = mate->GetWeaponSystem(i);
            if (!w ||
                (w->_weaponType & (MaskSlotPrimary | MaskSlotSecondary | MaskSlotHandGun)) == 0)
            {
                continue;
            }
            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Weapon;
            cell.index = i;
            cell.container = mate;
            cell.fromBody = true;
            cell.weapon = w;
            _vicinity.Add(cell);
        }
        for (int i = 0; i < mate->NMagazines(); i++)
        {
            const Magazine *m = mate->GetMagazine(i);
            if (!m)
            {
                continue;
            }
            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Magazine;
            cell.index = i;
            cell.container = mate;
            cell.fromBody = true;
            cell.magazine = m;
            _vicinity.Add(cell);
        }
    }

    for (int c = 0; c < _containers.Size(); c++)
    {
        VehicleSupply *container = _containers[c];
        if (!container)
        {
            continue;
        }
        // The arsenal crate is an infinite source, not storage: show only the clean
        // arsenal (added below), never the ReammoBox default cargo it inherits.
        if (container->GetType() &&
            strcmp((const char *)container->GetType()->GetName(), "ArsenalCrate") == 0)
        {
            continue;
        }

        for (int i = 0; i < container->GetWeaponCargoSize(); i++)
        {
            const WeaponType *w = container->GetWeaponCargo(i);
            if (!w)
            {
                continue;
            }

            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Weapon;
            cell.index = i;
            cell.container = container;
            cell.weapon = w;
            _vicinity.Add(cell);
        }

        for (int i = 0; i < container->GetMagazineCargoSize(); i++)
        {
            const Magazine *m = container->GetMagazineCargo(i);
            if (!m)
            {
                continue;
            }

            InvCell cell;
            cell.pane = InvPane::Ground;
            cell.kind = InvItemKind::Magazine;
            cell.index = i;
            cell.container = container;
            cell.magazine = m;
            _vicinity.Add(cell);
        }
    }

    // The infinite arsenal only appears for the dedicated "Ammo Crates (Arsenal)"
    // object (CfgVehicles class ArsenalCrate) - not for ordinary ammo crates.
    for (int c = 0; c < _containers.Size(); c++)
    {
        VehicleSupply *v = _containers[c];
        if (v && v->GetType() &&
            strcmp((const char *)v->GetType()->GetName(), "ArsenalCrate") == 0)
        {
            AddArsenal();
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Vicinity source list: one page per nearby container/body (even when EMPTY,
    // so a bare car still shows its name so the player knows where items go), the
    // arsenal if present, and finally an always-available "Ground" drop page.
    // DisplayInventory paginates the vicinity strip by this list.
    _vicSources.Clear();

    bool haveArsenal = false;
    for (int c = 0; c < _containers.Size(); c++)
    {
        VehicleSupply *v = _containers[c];
        if (!v || !v->GetType())
        {
            continue;
        }
        if (strcmp((const char *)v->GetType()->GetName(), "ArsenalCrate") == 0)
        {
            haveArsenal = true;
            continue;   // represented by the single arsenal page below
        }
        if (IsGroundHolderType(v))
        {
            continue;   // loose gear -> shown on the shared Ground page, not its own
        }
        VicSource s;
        s.label = RString(v->GetType()->GetDisplayName());
        s.container = v;
        _vicSources.Add(s);
    }

    for (int b = 0; b < _bodies.Size(); b++)
    {
        Person *body = _bodies[b];
        if (!body)
        {
            continue;
        }
        VicSource s;
        s.label = (body->GetType())
                      ? RString(body->GetType()->GetDisplayName()) + RString(" (body)")
                      : RString("Body");
        s.container = (VehicleSupply *)body;
        s.fromBody = true;
        _vicSources.Add(s);
    }

    // Living squad-mates you command: one page each (give + take).
    for (int u = 0; u < _commandUnits.Size(); u++)
    {
        Person *mate = _commandUnits[u];
        if (!mate)
        {
            continue;
        }
        VicSource s;
        s.label = (mate->GetType())
                      ? RString(mate->GetType()->GetDisplayName())
                      : RString("Squad-mate");
        s.container = (VehicleSupply *)mate;
        s.fromBody = true;
        _vicSources.Add(s);
    }

    if (haveArsenal)
    {
        VicSource s;
        s.label = RString("Arsenal (unlimited)");
        s.isArsenal = true;
        _vicSources.Add(s);
    }

    // Ground is always the final page, so items can be dropped even with no
    // container in range.
    {
        VicSource g;
        g.label = RString("Ground");
        g.isGround = true;
        _vicSources.Add(g);
    }
}

bool InventoryModel::CellOnSource(const InvCell &cell, int srcIdx) const
{
    if (srcIdx < 0 || srcIdx >= _vicSources.Size())
    {
        return false;
    }
    const VicSource &s = _vicSources[srcIdx];
    if (s.isGround)
    {
        // The Ground page gathers all loose gear lying on the ground (any
        // WeaponHolder cargo), so a dropped item shows here rather than on its own.
        if (cell.fromArsenal || cell.fromBody)
        {
            return false;
        }
        return IsGroundHolderType((const VehicleSupply *)cell.container);
    }
    if (s.isArsenal)
    {
        return cell.fromArsenal;
    }
    if (cell.fromArsenal)
    {
        return false;
    }
    return (const VehicleSupply *)cell.container == (const VehicleSupply *)s.container;
}

void InventoryModel::AddArsenal()
{
    // Enumerate the type banks only ONCE per open (it isn't free); reuse after.
    if (!_arsenalBuilt)
    {
        _arsenalBuilt = true;

        // Every magazine type already placed anywhere in the arsenal, so pass 2
        // (loose throwables) doesn't re-add a mag that a weapon already showed.
        AutoArray<const MagazineType *> shownMags;

        // 1) Every carryable weapon, each immediately followed by its compatible
        //    magazine(s) - so a weapon and the mag you need for it sit side by side.
        for (int i = 0; i < WeaponTypes.Size(); i++)
        {
            const WeaponType *w = WeaponTypes.Get(i);
            if (!w || w->_scope < 2)
            {
                continue;
            }
            if ((w->_weaponType & (MaskSlotPrimary | MaskSlotSecondary |
                                   MaskSlotHandGun | MaskSlotBinocular)) == 0)
            {
                continue;   // not a carryable weapon
            }
            InvCell wc;
            wc.pane = InvPane::Ground;
            wc.kind = InvItemKind::Weapon;
            wc.index = i;
            wc.weapon = w;
            wc.fromArsenal = true;
            _arsenalCache.Add(wc);

            // this weapon's magazines, right after it (dedup within the weapon)
            AutoArray<const MagazineType *> localMags;
            for (int j = 0; j < w->_muzzles.Size(); j++)
            {
                const MuzzleType *mz = w->_muzzles[j];
                if (!mz)
                {
                    continue;
                }
                for (int k = 0; k < mz->_magazines.Size(); k++)
                {
                    const MagazineType *m = mz->_magazines[k];
                    if (!m || m->_maxAmmo <= 0)
                    {
                        continue;   // skip proxy/uninitialised mags (show as "0")
                    }
                    if (IsLaserDesignatorClass((const char *)m->GetName()))
                    {
                        continue;   // designator battery isn't a real ammo item
                    }
                    bool seen = false;
                    for (int s = 0; s < localMags.Size(); s++)
                    {
                        if (localMags[s] == m) { seen = true; break; }
                    }
                    if (seen)
                    {
                        continue;
                    }
                    localMags.Add(m);

                    bool inGlobal = false;
                    for (int s = 0; s < shownMags.Size(); s++)
                    {
                        if (shownMags[s] == m) { inGlobal = true; break; }
                    }
                    if (!inGlobal)
                    {
                        shownMags.Add(m);
                    }

                    InvCell mc;
                    mc.pane = InvPane::Ground;
                    mc.kind = InvItemKind::Magazine;
                    mc.index = 0;
                    mc.magType = m;
                    mc.fromArsenal = true;
                    _arsenalCache.Add(mc);
                }
            }
        }

        // 2) Infantry throwables/placeables not tied to a carryable weapon (grenades,
        //    satchels, mines) - identified by their item slot bits. Vehicle ammo has
        //    none of these bits, so it is excluded from the arsenal.
        for (int i = 0; i < MagazineTypes.Size(); i++)
        {
            const MagazineType *m = MagazineTypes.Get(i);
            if (!m || m->_scope < 2 || m->_maxAmmo <= 0)
            {
                continue;
            }
            if ((m->_magazineType & (MaskSlotItem | MaskSlotHandGunItem)) == 0)
            {
                continue;   // not a throwable/placeable item (e.g. vehicle ammo)
            }
            if (IsLaserDesignatorClass((const char *)m->GetName()))
            {
                continue;   // designator battery isn't a real ammo item
            }
            bool seen = false;
            for (int s = 0; s < shownMags.Size(); s++)
            {
                if (shownMags[s] == m) { seen = true; break; }
            }
            if (seen)
            {
                continue;   // already shown next to a weapon - don't duplicate it here
            }
            shownMags.Add(m);
            InvCell mc;
            mc.pane = InvPane::Ground;
            mc.kind = InvItemKind::Magazine;
            mc.index = i;
            mc.magType = m;
            mc.fromArsenal = true;
            _arsenalCache.Add(mc);
        }
    }
    for (int i = 0; i < _arsenalCache.Size(); i++)
    {
        _vicinity.Add(_arsenalCache[i]);
    }
}

void InventoryModel::FindPlacement(GridSize footprint, int &outX, int &outY) const
{
    // Row-major scan for the first rect of size footprint with no overlap
    // against existing Grid cells. O(rows * cols * items) - fine for an
    // inventory-sized item count; revisit if this ever needs to hold
    // hundreds of items.
    // Columns are user-adjustable; never fewer than the item's own width or the
    // inner loop could never place it (infinite outer loop).
    int cols = InventoryGridCols();
    if (cols < footprint.w) cols = footprint.w;
    int row = 0;
    for (;; row++)
    {
        for (int col = 0; col + footprint.w <= cols; col++)
        {
            bool free = true;
            for (int i = 0; i < _grid.Size() && free; i++)
            {
                const InvCell &c = _grid[i];
                const bool overlapX = col < c.gx + c.footprint.w && c.gx < col + footprint.w;
                const bool overlapY = row < c.gy + c.footprint.h && c.gy < row + footprint.h;
                if (overlapX && overlapY)
                {
                    free = false;
                }
            }
            if (free)
            {
                outX = col;
                outY = row;
                return;
            }
        }
        // Row full - grid just grows. No failure case, per the design note:
        // grid space is cosmetic and never blocks a weight-legal item.
    }
}

InvTransferResult InventoryModel::CanTransfer(const InvCell &from, InvPane toPane) const
{
    if (!_player || from.IsEmpty())
    {
        return InvTransferResult::Unavailable;
    }
    if (from.pane == toPane)
    {
        return InvTransferResult::SameCell;
    }

    if (toPane == InvPane::Grid)
    {
        // Rifles/launchers normally may ONLY occupy their single loadout slot - never
        // the storage grid. If that slot is already taken, refuse the pickup instead
        // of silently carrying an invisible extra long gun. Handguns are fine (they
        // show in the grid and can be swapped into the pistol slot). The debug "Rifles
        // in backpack" option lifts this restriction so extra long guns are carried in
        // the grid (as 3x2 tiles) instead of being refused.
        if (from.kind == InvItemKind::Weapon && from.weapon && !InventoryWeaponsInBackpack())
        {
            const int wt = from.weapon->_weaponType;
            if ((wt & MaskSlotPrimary) && HasWeaponWithMask(MaskSlotPrimary))
            {
                return InvTransferResult::SlotOccupied;
            }
            if ((wt & MaskSlotSecondary) && HasWeaponWithMask(MaskSlotSecondary))
            {
                return InvTransferResult::SlotOccupied;
            }
        }

        // Weight gate.
        const float afterPickup = _carriedMass + from.Mass();
        if (afterPickup > MaxCarryWeight())
        {
            return InvTransferResult::OverCarryWeight;
        }
        // Volume gate (invisible; per-side capacity). Adding this item must not push
        // the carried packed volume past the side's budget.
        if (_volumeCapacity > 0.0f && _carriedVolume + CellVolume(from) > _volumeCapacity)
        {
            return InvTransferResult::OverVolume;
        }
        return InvTransferResult::Ok;
    }

    // Player -> ground: always legal by weight (you're losing mass, not
    // gaining it), just check droppability.
    if (from.kind == InvItemKind::Weapon && from.weapon && !from.weapon->_canDrop)
    {
        return InvTransferResult::NotDroppable;
    }
    return InvTransferResult::Ok;
}

InvTransferResult InventoryModel::Transfer(const InvCell &from, InvPane toPane)
{
    const InvTransferResult check = CanTransfer(from, toPane);
    if (check != InvTransferResult::Ok)
    {
        return check;
    }

    if (toPane == InvPane::Grid)
    {
        // Arsenal: infinite - take a copy, never deplete the source.
        if (from.fromArsenal)
        {
            if (from.kind == InvItemKind::Weapon && from.weapon)
            {
                _player->AddWeapon(const_cast<WeaponType *>(from.weapon), /*force=*/true);
            }
            else if (from.magType)
            {
                Ref<Magazine> m = new Magazine(const_cast<MagazineType *>(from.magType));
                m->_ammo = from.magType->_maxAmmo;   // arsenal mags come out full, not empty
                _player->AddMagazine(m, /*force=*/true);
            }
            else
            {
                return InvTransferResult::Unavailable;
            }
            if (_player)
            {
                _player->PlayAction(ManActPutDown);
            }
            CutScene(from.kind == InvItemKind::Weapon ? "TakeWeapon" : "TakeMagazine");
            Refresh();
            return InvTransferResult::Ok;
        }

        VehicleSupply *container = from.container;
        if (!container)
        {
            return InvTransferResult::Unavailable;
        }

        // Looting a dead soldier: its gear is in weapon systems / magazines, so
        // take with RemoveWeapon/RemoveMagazine (not the cargo path).
        if (from.fromBody)
        {
            Person *body = dyn_cast<Person, VehicleSupply>(container);
            if (!body)
            {
                return InvTransferResult::Unavailable;
            }
            if (from.kind == InvItemKind::Weapon)
            {
                Ref<WeaponType> w = const_cast<WeaponType *>(from.weapon);
                body->RemoveWeapon(w);
                _player->AddWeapon(w, /*force=*/true);
            }
            else
            {
                Ref<Magazine> m = const_cast<Magazine *>(from.magazine);
                body->RemoveMagazine(m);
                _player->AddMagazine(m, /*force=*/true);
            }
            if (_player)
            {
                _player->PlayAction(ManActPutDown);
            }
            CutScene(from.kind == InvItemKind::Weapon ? "TakeWeapon" : "TakeMagazine");
            Refresh();
            return InvTransferResult::Ok;
        }

        // force=true is what makes this weight-only: it skips CheckWeapon /
        // the slot-mask check entirely (confirmed VehicleAI.cpp:1764-1776 and
        // 1983-2000 - `if (!force) { CheckWeapon(...) }`). AI callers still
        // pass force=false elsewhere, so AI gear logic is untouched.
        //
        // CRITICAL: _magazineCargo is a RefArray<Magazine> (VehicleAI.hpp:33),
        // so RemoveMagazineCargo drops the array's reference and may FREE the
        // Magazine. We must hold our own Ref across remove->add or AddMagazine
        // gets a dangling pointer. This mirrors the engine's own take path,
        // which keeps a Ref<const Magazine> alive across the move
        // (Transport.cpp:805-828).
        if (from.kind == InvItemKind::Weapon)
        {
            Ref<WeaponType> w = const_cast<WeaponType *>(from.weapon);
            container->RemoveWeaponCargo(w);
            _player->AddWeapon(w, /*force=*/true);
            if (GWorld->GetMode() == GModeNetware)
            {
                GetNetworkManager().RemoveWeaponCargo(container, w->GetName());
            }
        }
        else
        {
            Ref<Magazine> m = const_cast<Magazine *>(from.magazine);
            container->RemoveMagazineCargo(m);
            _player->AddMagazine(m, /*force=*/true);
            if (GWorld->GetMode() == GModeNetware)
            {
                // VERIFY signature against Transport.cpp:825 - the magazine
                // variant keys off (container, creator, id).
                GetNetworkManager().RemoveMagazineCargo(container, m->_creator, m->_id);
            }
        }

        if (_player)
        {
            _player->PlayAction(ManActPutDown);   // vanilla gear-handling gesture
        }
        // Vanilla feedback: "Taking weapon/ammo" title + pickup sound.
        CutScene(from.kind == InvItemKind::Weapon ? "TakeWeapon" : "TakeMagazine");
        Refresh();
        return InvTransferResult::Ok;
    }

    return DropToGround(from);
}

void InventoryModel::MoveWithinGrid(InvCell &cell, int newGx, int newGy)
{
    // Pure UI repositioning - no weight implication, always legal. Caller
    // (DisplayInventory) is responsible for collision-checking against
    // other grid cells if you want tiles to refuse to overlap visually;
    // per the design note this is cosmetic only, so a naive implementation
    // that allows overlap is acceptable too - your call on polish level.
    cell.gx = newGx;
    cell.gy = newGy;
}

InvTransferResult InventoryModel::DropToGround(const InvCell &from)
{
    // Legacy behaviour: auto-pick a nearby container, else the ground.
    return DropToTarget(from, nullptr, false);
}

InvTransferResult InventoryModel::DropToSource(const InvCell &from, int sourceIndex)
{
    if (sourceIndex < 0 || sourceIndex >= _vicSources.Size())
    {
        return DropToTarget(from, nullptr, false);
    }
    const VicSource &s = _vicSources[sourceIndex];
    if (s.isGround)
    {
        return DropToTarget(from, nullptr, /*forceGround*/ true);
    }
    if (s.fromBody)
    {
        // A person page (dead body or living squad-mate): hand the item to their gear.
        VehicleSupply *c = s.container;
        Person *person = c ? dyn_cast<Person, VehicleSupply>(c) : nullptr;
        if (person)
        {
            return DropToPerson(from, person);
        }
        return InvTransferResult::NotDroppable;
    }
    if (s.isArsenal)
    {
        // The arsenal is an infinite source, so dumping an item into it just deletes
        // it (the arsenal can hand back an identical one any time).
        return DiscardToArsenal(from);
    }
    return DropToTarget(from, (VehicleSupply *)s.container, false);
}

InvTransferResult InventoryModel::DropToPerson(const InvCell &from, Person *person)
{
    if (!_player || !person || from.IsEmpty())
    {
        return InvTransferResult::Unavailable;
    }
    if (from.pane != InvPane::Grid)   // only the player's own carried items
    {
        return InvTransferResult::SameCell;
    }
    if (from.kind == InvItemKind::Weapon && from.weapon && !from.weapon->_canDrop)
    {
        return InvTransferResult::NotDroppable;
    }

    // Move the item from the player's worn gear to the other person's (dead body OR a
    // living squad-mate). Mirror of the fromBody take path in Transfer(). Hold a Ref
    // across remove->add so the object isn't freed mid-move.
    if (from.kind == InvItemKind::Weapon)
    {
        Ref<WeaponType> w = const_cast<WeaponType *>(from.weapon);
        _player->RemoveWeapon(w);
        person->AddWeapon(w, /*force=*/true);
    }
    else
    {
        Ref<Magazine> m = const_cast<Magazine *>(from.magazine);
        _player->RemoveMagazine(m);
        person->AddMagazine(m, /*force=*/true);
    }

    _player->PlayAction(ManActPutDown);
    CutScene(from.kind == InvItemKind::Weapon ? "TakeWeapon" : "TakeMagazine");
    Refresh();
    return InvTransferResult::Ok;
}

InvTransferResult InventoryModel::DiscardToArsenal(const InvCell &from)
{
    if (!_player || from.IsEmpty())
    {
        return InvTransferResult::Unavailable;
    }
    if (from.pane != InvPane::Grid)   // only the player's own carried items
    {
        return InvTransferResult::SameCell;
    }
    if (from.kind == InvItemKind::Weapon && from.weapon && !from.weapon->_canDrop)
    {
        return InvTransferResult::NotDroppable;
    }

    // Remove from the unit and simply let it go (no container, no ground holder).
    // The arsenal is a single-player preview convenience, so no network routing.
    if (from.kind == InvItemKind::Weapon)
    {
        Ref<WeaponType> w = const_cast<WeaponType *>(from.weapon);
        _player->RemoveWeapon(w);
    }
    else
    {
        Ref<Magazine> m = const_cast<Magazine *>(from.magazine);
        _player->RemoveMagazine(m);
    }

    _player->PlayAction(ManActPutDown);   // vanilla gear-handling gesture
    Refresh();
    return InvTransferResult::Ok;
}

InvTransferResult InventoryModel::DropToTarget(const InvCell &from,
                                               VehicleSupply *preferred,
                                               bool forceGround)
{
    if (!_player || from.IsEmpty())
    {
        return InvTransferResult::Unavailable;
    }
    if (from.pane != InvPane::Grid)
    {
        return InvTransferResult::SameCell;
    }
    if (from.kind == InvItemKind::Weapon && from.weapon && !from.weapon->_canDrop)
    {
        return InvTransferResult::NotDroppable;
    }

    // Find a nearby container with room. If one exists, its AddWeaponCargo/
    // AddMagazineCargo handles everything. If none has room (or none is near),
    // we spawn a WeaponHolder ourselves using the VERIFIED placement sequence
    // from Transport.cpp:1338-1350 - note AddWeaponCargo/AddMagazineCargo also
    // auto-spawn a holder when a container is full (Transport.cpp:1411-1428),
    // so a fallback holder only needs creating when there's no container at all.
    VehicleSupply *target = nullptr;
    const InvItemKind kind = from.kind;
    auto hasRoom = [kind](VehicleSupply *c) {
        return (kind == InvItemKind::Weapon) ? c->GetFreeWeaponCargo() > 0
                                             : c->GetFreeMagazineCargo() > 0;
    };
    if (forceGround)
    {
        // Reuse an existing ground holder with room, so repeated drops don't spawn
        // a pile of separate holders (they all share the one Ground page).
        for (int i = 0; i < _containers.Size(); i++)
        {
            VehicleSupply *c = _containers[i];
            if (c && IsGroundHolderType(c) && hasRoom(c))
            {
                target = c;
                break;
            }
        }
    }
    else if (preferred && hasRoom(preferred))
    {
        // Explicit page target (e.g. the car the player is looking at).
        target = preferred;
    }
    else if (!preferred)
    {
        for (int i = 0; i < _containers.Size(); i++)
        {
            VehicleSupply *c = _containers[i];
            if (c && hasRoom(c))
            {
                target = c;
                break;
            }
        }
    }

    if (!target)
    {
        const bool secondary =
            from.kind == InvItemKind::Weapon &&
            from.weapon &&
            (from.weapon->_weaponType & MaskSlotSecondary) != 0 &&
            (from.weapon->_weaponType & MaskSlotPrimary) == 0;

        Ref<EntityAI> holder = NewVehicle(secondary ? "SecondaryWeaponHolder" : "WeaponHolder");
        Ref<VehicleSupply> container = dyn_cast<VehicleSupply, EntityAI>(holder.GetRef());
        if (!container)
        {
            return InvTransferResult::Unavailable;
        }

        // Verified placement sequence (Transport.cpp:1338-1350).
        Matrix4 transform = _player->Transform();
        transform.SetPosition(_player->Position() + _player->Direction() * 0.5f);
        container->PlaceOnSurface(transform);
        container->SetTransform(transform);
        container->Init(transform);
        GWorld->AddBuilding(container);
        if (GWorld->GetMode() == GModeNetware)
        {
            GetNetworkManager().CreateVehicle(container, VLTBuilding, "", -1);
        }

        target = container;
    }

    // Remove from the unit (holding a Ref so nothing is freed mid-move), then
    // hand to the container. RemoveWeapon(const WeaponType*) and
    // RemoveMagazine(const Magazine*) both confirmed (EntityAI.hpp:864,894).
    if (from.kind == InvItemKind::Weapon)
    {
        Ref<WeaponType> w = const_cast<WeaponType *>(from.weapon);
        _player->RemoveWeapon(w);
        target->AddWeaponCargo(w, 1);
        if (GWorld->GetMode() == GModeNetware)
        {
            GetNetworkManager().AddWeaponCargo(target, w->GetName());
        }
    }
    else
    {
        Ref<Magazine> m = const_cast<Magazine *>(from.magazine);
        _player->RemoveMagazine(m);
        target->AddMagazineCargo(m);
        if (GWorld->GetMode() == GModeNetware)
        {
            GetNetworkManager().AddMagazineCargo(target, m);
        }
    }

    if (_player)
    {
        _player->PlayAction(ManActPutDown);   // vanilla gear-handling gesture
    }
    // Vanilla feedback: "Dropping weapon/ammo" title + drop sound.
    CutScene(from.kind == InvItemKind::Weapon ? "DropWeapon" : "DropMagazine");
    Refresh();
    return InvTransferResult::Ok;
}

} // namespace Poseidon
