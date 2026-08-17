#include <Poseidon/AI/AI.hpp>
#include <Poseidon/Core/Application.hpp>
#include <Poseidon/Core/Global.hpp>
#include <Poseidon/Core/Config/EngineConfig.hpp>
#include <Poseidon/Core/Config/UserConfig.hpp>
#include <Poseidon/Foundation/Logging/Logging.hpp>
#include <Poseidon/Foundation/Memory/MemFreeReq.hpp>
#include <Poseidon/Foundation/Memory/MemoryBudget.hpp>
#include <Poseidon/Foundation/Framework/Log.hpp>
#include <Poseidon/Foundation/Platform/AppConfig.hpp>
#include <Poseidon/Core/Config/Config.hpp>
#include <Poseidon/Core/ModSystem.hpp>

#include <Poseidon/Foundation/Common/Filenames.hpp>
#include <Poseidon/Foundation/Strings/Bstring.hpp>
#include <Poseidon/IO/Streams/QBStream.hpp>
#include <Poseidon/IO/ParamFile/ParamFile.hpp>
#include <Poseidon/UI/Locale/Stringtable/Stringtable.hpp>

#include <Poseidon/World/World.hpp>
#include <Poseidon/UI/Settings/GameSettingsConfig.hpp>
#include <Poseidon/Core/Progress.hpp>
#include <Poseidon/Game/Chat.hpp>
#include <Poseidon/Input/InputSubsystem.hpp>
#include <Poseidon/Network/Network.hpp>
#include <Poseidon/IO/FileServerMT.hpp>
#include <Poseidon/IO/ParamFileExt.hpp>
#include <Poseidon/UI/Locale/Languages.hpp>
#include <Poseidon/Asset/Addon/AddonInfo.hpp>
#include <Poseidon/Asset/Addon/AddonSystem.hpp>
#include <stdio.h>
#include <string.h>
#include <functional>
#include <string>
#include <Poseidon/Foundation/Containers/RStringArray.hpp>
#include <Poseidon/Foundation/Framework/DebugLog.hpp>
#include <Poseidon/Foundation/Strings/RString.hpp>
#include <Poseidon/Foundation/Time/Time.hpp>
#include <Poseidon/Foundation/Types/Pointers.hpp>
#include <Poseidon/Foundation/platform.hpp>

using Poseidon::AddonInfo;
using Poseidon::AddonSystem;
using Poseidon::CheckAddonContext;
using Poseidon::LoadBanksContext;

#include <Poseidon/Foundation/Common/Win.h>
#include <Poseidon/Foundation/Common/PlayerPrefs.hpp>
#include <Poseidon/Foundation/Common/GamePaths.hpp>
#include <Poseidon/Core/Profile/ProfileService.hpp>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace Poseidon;
static const char* ProductList[] = {"OFP: Cold War Crisis", "OFP: Resistance", "VBS", nullptr};
static bool StringInList(const char* str, const char** list);

namespace Poseidon
{
bool IsAddonMetadataAccepted(const char* product, const char* encryption, const char* formatVersion,
                             bool encryptionRequired, const char** productList)
{
    if (encryptionRequired && (!encryption || *encryption == 0))
    {
        return false;
    }

    if (formatVersion && *formatVersion != 0)
    {
        return false;
    }

    if (product && *product != 0 && !StringInList(product, productList))
    {
        return false;
    }

    return true;
}
} // namespace Poseidon

static bool StringInList(const char* str, const char** list)
{
    while (*list)
    {
        if (!strcmpi(*list, str))
        {
            return true;
        }
        list++;
    }
    return false;
}

static bool CheckProductCallback(QFBank* bank, BankContextBase* context)
{
    CheckAddonContext* cpc = static_cast<CheckAddonContext*>(context);
    RString product = bank->GetProperty("product");
    RString encryption = bank->GetProperty("encryption");
    RString formatVersion = bank->GetProperty("pboVersion");

    return Poseidon::IsAddonMetadataAccepted(product, encryption, formatVersion, cpc->encryptionRequired,
                                             cpc->productList);
}

extern bool GUseFileBanks;
extern void ClearShapes();
extern void DestroyMsgFormats();
extern RString CurrentCampaign;
extern RString CurrentBattle;
extern RString CurrentMission;
#include <Poseidon/Core/Version.hpp>

namespace Poseidon
{
RString GetUserParams();
}
using Poseidon::Foundation::Time;
using Poseidon::Foundation::UITime;

// Non-static so main.cpp can call it.
RString& GetModPathInternal()
{
    static RString ModPath;
    return ModPath;
}

bool EnumModDirectories(ModDirectoryCallback callback, void* context)
{
    RString& ModPath = GetModPathInternal();
    if (ModPath.GetLength() > 0)
    {
        Temp<char> buffer((const char*)ModPath, ModPath.GetLength() + 1);

        char* ptr;
        while (ptr = strrchr(buffer, ';'))
        {
            *ptr = 0;
            if (callback(ptr + 1, context))
            {
                return true;
            }
        }
        if (callback((const char*)buffer, context))
        {
            return true;
        }
    }
    return callback("", context);
}

static bool ParseAddonConfigCallback(QFBank* bank, BankContextBase* context)
{
    return AddonSystem::ParseAddonConfig(bank->GetPrefix());
}

static void LoadBanks(const char* path, const char* fullPath, bool emptyPrefix, bool parseConfig = false)
{
    FindArrayRStringCI bankNames;
    FindBank find;
    if (find.First(fullPath))
    {
        do
        {
            char prefix[256];
            snprintf(prefix, sizeof(prefix), "%s", (const char*)find.GetName());
            strlwr(prefix);
            char* ext = strrchr(prefix, '.');
            PoseidonAssert(ext);
            *ext = 0;
            bankNames.AddUnique(prefix);
        } while (find.Next());
        find.Close();
    }
    RString bankPrefix = RString(path) + RString("\\");
    const char* langSuffix = GetLanguagePboSuffix(GLanguage);
    for (int i = 0; i < bankNames.Size(); i++)
    {
        const RString& bName = bankNames[i];
        // Skip base PBO when language-specific variant exists (e.g., skip "1985" when "1985.cz" exists)
        if (langSuffix)
        {
            RString langVariant = bName + RString(".") + RString(langSuffix);
            if (bankNames.Find(langVariant) >= 0)
                continue;
        }
        RString prefix;
        if (emptyPrefix)
        {
            prefix = bName;
        }
        else
        {
            prefix = bankPrefix + bName;
        }
        // Runtime bank prefix remapping for language-specific PBOs
        if (stricmp(GLanguage, "Czech") == 0 && stricmp(prefix, "fonts.cz") == 0)
            prefix = "fonts";
        else if (stricmp(GLanguage, "Russian") == 0 && stricmp(prefix, "fonts.russian") == 0)
            prefix = "fonts";
        else if (stricmp(GLanguage, "Polish") == 0 && stricmp(prefix, "fonts.polish") == 0)
            prefix = "fonts";
        // Strip language PBO suffix to remap to base prefix (e.g., "campaigns\1985.cz" → "campaigns\1985")
        if (langSuffix)
        {
            char dotSuffix[16];
            snprintf(dotSuffix, sizeof(dotSuffix), ".%s", langSuffix);
            int sLen = (int)strlen(dotSuffix);
            int pLen = (int)strlen(prefix);
            if (pLen > sLen && stricmp((const char*)prefix + pLen - sLen, dotSuffix) == 0)
            {
                char buf[256];
                memcpy(buf, (const char*)prefix, pLen - sLen);
                buf[pLen - sLen] = 0;
                prefix = buf;
            }
        }
        RString prefixPath = prefix + RString("\\");
        if (QIFStreamB::AutoBank(prefixPath))
        {
            continue;
        }

        Ref<CheckAddonContext> cpc = new CheckAddonContext;
        cpc->productList = ProductList;
        cpc->encryptionRequired = parseConfig && AppConfig::Instance().RequireEncryptedAddons();
        OpenCallback addonCallback = (OpenCallback) nullptr;
        if (parseConfig)
        {
            addonCallback = ParseAddonConfigCallback;
        }

        RString bankPath = RString(fullPath) + RString("\\");
        GFileBanks.Load(bankPath, bankPrefix, bName, emptyPrefix, CheckProductCallback, addonCallback, cpc);
    }
}

static bool LoadBanksCallback(RStringB dir, void* context)
{
    LoadBanksContext* ctx = reinterpret_cast<LoadBanksContext*>(context);

    if (dir.GetLength() == 0)
    {
        dir = ctx->path;
    }
    else
    {
        dir = dir + RString("\\") + ctx->path;
    }

    LoadBanks(ctx->path, dir, ctx->emptyPrefix, ctx->parseConfig);
    return false;
}

static void LoadBanksEx(const char* path, bool emptyPrefix, bool parseConfig = false)
{
    LoadBanksContext ctx;
    ctx.path = path;
    ctx.emptyPrefix = emptyPrefix;
    ctx.parseConfig = parseConfig;
    EnumModDirectories(LoadBanksCallback, &ctx);
}

// Inject the "Ammo Crates (Arsenal)" editor object directly into the merged config.
//
// A text config.cpp cannot do this: ParamClass::Parse aborts on any base class it
// cannot resolve inside the same file (external "class ReammoBox;" forward-decls are
// not supported by this parser), so a loose/PBO addon that inherits an engine crate
// never registers. Instead we clone the real ReammoBoxWest class at runtime and
// override only the display name - it keeps ReammoBoxWest's model, cargo, side,
// scope and vehicleClass, so it places and behaves like a normal ammo crate while
// the inventory engine detects it by the class name "ArsenalCrate".
static void RegisterArsenalCrate()
{
    ParamEntry* cfgVEntry = Pars.FindEntry("CfgVehicles");
    ParamClass* cfgV = cfgVEntry ? cfgVEntry->GetClassInterface() : nullptr;
    if (!cfgV)
        return;
    // Remove any prior ArsenalCrate (re-mount idempotency). By NAME only - never
    // touch other classes.
    for (int i = cfgV->GetEntryCount() - 1; i >= 0; i--)
    {
        const ParamEntry& e = cfgV->GetEntry(i);
        if (e.IsClass() && stricmp((const char*)e.GetName(), "ArsenalCrate") == 0)
            cfgV->Delete(RStringB("ArsenalCrate"));
    }

    ParamEntry* baseEntry = cfgV->FindEntry("ReammoBoxWest");
    const ParamClass* base = baseEntry ? baseEntry->GetClassInterface() : nullptr;
    if (!base)
    {
        baseEntry = cfgV->FindEntry("ReammoBox");
        base = baseEntry ? baseEntry->GetClassInterface() : nullptr;
    }
    if (!base)
        return;

    ParamClass* ac = cfgV->AddClass("ArsenalCrate");
    if (!ac)
        return;
    // ORDER MATTERS: add displayName BEFORE SetBase. ParamClass::Add uses the
    // inheritance-aware FindEntry, so with a base already set it would find and
    // OVERWRITE the base class's (ReammoBoxWest's) displayName - which renamed the
    // stock "Ammo Crates (West)" to "Ammo Crates (Arsenal)" and produced the phantom
    // duplicate. With no base yet, Add creates ArsenalCrate's OWN displayName entry.
    ac->Add(RStringB("displayName"), RStringB("Ammo Crates (Arsenal)"));
    ac->SetBase(base);   // now inherit model/cargo/side/scope/vehicleClass
}

// Pick a valid concrete magazine class to clone for the medkits. A magazine (in
// CfgWeapons for this engine) needs magazineType/count/modes/scopeMagazine set;
// cloning a real one guarantees those are valid regardless of the loaded mod. The
// medkit is never listed in any muzzle's magazines[] list, so it can never be
// loaded or fired - it only exists to be carried and "used".
static const ParamClass* FindMagazineBase(ParamClass* cfgW)
{
    static const char* prefer[] = { "HandGrenade", "Grenade", "PipeBomb", nullptr };
    for (int p = 0; prefer[p]; p++)
    {
        ParamEntry* e = cfgW->FindEntry(prefer[p]);
        if (e && e->IsClass())
        {
            const ParamClass* c = e->GetClassInterface();
            if (c && c->FindEntry("magazineType") && c->FindEntry("count"))
                return c;
        }
    }
    for (int i = 0; i < cfgW->GetEntryCount(); i++)
    {
        const ParamEntry& e = cfgW->GetEntry(i);
        if (!e.IsClass())
            continue;
        if (stricmp((const char*)e.GetName(), "US_FirstAidKit") == 0 ||
            stricmp((const char*)e.GetName(), "AI2_FirstAidKit") == 0)
            continue;
        const ParamClass* c = e.GetClassInterface();
        if (!c)
            continue;
        const ParamEntry* mt = c->FindEntry("magazineType");
        const ParamEntry* ct = c->FindEntry("count");
        const ParamEntry* sc = c->FindEntry("scopeMagazine");
        if (mt && ct && sc && (int)(*sc) == 2)
            return c;
    }
    return nullptr;
}

// Inject the two first-aid-kit "magazines" into CfgWeapons at runtime (a text
// config.cpp cannot inherit an engine base class with this parser - see
// RegisterArsenalCrate). Each kit is a carry-only magazine with count=3 (three
// uses) and useAction=1 so the engine auto-adds a "Use first aid kit" scroll
// action (see EntityAI::PerformAction / Man::StartActionProcessing for the medkit
// heal). Icons resolve by class name to invicons\m_<class>.paa.
static void RegisterMedkits()
{
    ParamEntry* cfgWEntry = Pars.FindEntry("CfgWeapons");
    ParamClass* cfgW = cfgWEntry ? cfgWEntry->GetClassInterface() : nullptr;
    if (!cfgW)
        return;

    const char* names[2] = { "US_FirstAidKit", "AI2_FirstAidKit" };
    for (int k = 0; k < 2; k++)
    {
        for (int i = cfgW->GetEntryCount() - 1; i >= 0; i--)
        {
            const ParamEntry& e = cfgW->GetEntry(i);
            if (e.IsClass() && stricmp((const char*)e.GetName(), names[k]) == 0)
                cfgW->Delete(RStringB(names[k]));
        }
    }

    const ParamClass* base = FindMagazineBase(cfgW);
    if (!base)
        return;

    // Realistic loaded weights (kg): the US IFAK is ~1 lb (0.45 kg); the Soviet AI-2
    // kit is a tiny plastic case at ~70 g (0.07 kg). Read by ItemMass::Of via the
    // magazine's "mass" config entry, so they weigh correctly in the encumbrance system.
    struct { const char* cls; const char* disp; float mass; } kits[2] = {
        { "US_FirstAidKit",  "US Individual First Aid Kit",  0.45f },
        { "AI2_FirstAidKit", "AI-2 Individual First Aid Kit", 0.07f },
    };
    for (int k = 0; k < 2; k++)
    {
        ParamClass* m = cfgW->AddClass(kits[k].cls);
        if (!m)
            continue;
        // ORDER MATTERS: add own entries BEFORE SetBase, otherwise the
        // inheritance-aware Add would find and overwrite the base class's entries
        // (see RegisterArsenalCrate for the same hazard).
        m->Add(RStringB("scopeMagazine"), 2);
        m->Add(RStringB("displayNameMagazine"), RStringB(kits[k].disp));
        m->Add(RStringB("shortNameMagazine"), RStringB(kits[k].disp));
        m->Add(RStringB("picture"), RStringB(kits[k].cls));
        m->Add(RStringB("count"), 3);
        m->Add(RStringB("useAction"), 1);
        m->Add(RStringB("useActionTitle"), RStringB("Use first aid kit"));
        m->Add(RStringB("mass"), kits[k].mass);
        m->SetBase(base);
    }

    // Jerry can: a 20-litre fuel container. count = current litres (starts full at 20).
    // Its weight is computed specially in ItemMass (4 kg steel can + 0.74 kg per litre
    // of fuel), so the config "mass" here is just the full figure for generic consumers.
    for (int i = cfgW->GetEntryCount() - 1; i >= 0; i--)
    {
        const ParamEntry& e = cfgW->GetEntry(i);
        if (e.IsClass() && stricmp((const char*)e.GetName(), "JerryCan") == 0)
            cfgW->Delete(RStringB("JerryCan"));
    }
    if (ParamClass* j = cfgW->AddClass("JerryCan"))
    {
        j->Add(RStringB("scopeMagazine"), 2);
        j->Add(RStringB("displayNameMagazine"), RStringB("Jerry Can"));
        j->Add(RStringB("shortNameMagazine"), RStringB("Jerry Can"));
        j->Add(RStringB("picture"), RStringB("JerryCan"));
        j->Add(RStringB("count"), 20);
        j->Add(RStringB("mass"), 21.0f);   // full 20 L can (4 kg can + 17 kg fuel)
        j->SetBase(base);
    }

    // "Refueling" / "Syphoning" cut-scenes: clones of the vanilla "Refuel" cut-scene, so
    // they inherit its lower title position, flicker and pouring sound, with just the
    // title text overridden. The jerry-can transfer fires these via CutScene().
    if (ParamEntry* csE = Pars.FindEntry("CfgCutScenes"))
    {
        if (ParamClass* cfgC = csE->GetClassInterface())
        {
            ParamEntry* refE = cfgC->FindEntry("Refuel");
            const ParamClass* refBase = refE ? refE->GetClassInterface() : nullptr;
            if (refBase)
            {
                struct { const char* cls; const char* title; } cs[2] = {
                    { "JerryRefuel", "Refueling" },
                    { "JerrySyphon", "Syphoning" },
                };
                for (int k = 0; k < 2; k++)
                {
                    for (int i = cfgC->GetEntryCount() - 1; i >= 0; i--)
                    {
                        const ParamEntry& e = cfgC->GetEntry(i);
                        if (e.IsClass() && stricmp((const char*)e.GetName(), cs[k].cls) == 0)
                            cfgC->Delete(RStringB(cs[k].cls));
                    }
                    if (ParamClass* c = cfgC->AddClass(cs[k].cls))
                    {
                        c->Add(RStringB("title"), RStringB(cs[k].title));  // own title BEFORE SetBase
                        c->SetBase(refBase);   // inherit titleType + sound from Refuel
                    }
                }
            }
        }
    }
}

// Override each vehicle's fuelCapacity (litres) with its precise real-world tank size.
// The stock config uses coarse per-class defaults (every Tank 700, every APC 700, every
// helicopter/plane 1000, etc.); these are the researched per-vehicle figures. Applied
// directly in code (no external config file) so there's nothing that can go stale.
//
// fuelCapacity is normally INHERITED from a base class (Tank/Car/APC/...), so a plain
// ParamClass::Add - which uses the inheritance-aware FindEntry - would find and modify
// the BASE's entry, changing every vehicle of that type. To give a class its OWN entry
// we temporarily detach its base, Add (which now creates a fresh own entry), then
// reattach the base. Classes that already own a fuelCapacity are set directly.
static void RegisterFuelCapacities()
{
    ParamEntry* cfgVEntry = Pars.FindEntry("CfgVehicles");
    ParamClass* cfgV = cfgVEntry ? cfgVEntry->GetClassInterface() : nullptr;
    if (!cfgV)
        return;

    struct FuelDef { const char* cls; float litres; };
    static const FuelDef kFuel[] = {
        // Tanks (stock 700)
        { "M1Abrams", 1909.f }, { "M60", 1420.f }, { "T55G", 680.f },
        { "T72", 705.f }, { "T72Res", 705.f }, { "T80", 1090.f }, { "T80Res", 1090.f }, { "ZSU", 515.f },
        // APCs / IFVs (stock 700)
        { "BMP", 462.f }, { "BMPRes", 462.f }, { "BMPAmbul", 462.f }, { "BMP2", 460.f },
        { "Bradley", 662.f }, { "M113", 360.f }, { "M113Ambul", 360.f }, { "Vulcan", 360.f },
        // Jeeps / cars (stock 50-100)
        { "Jeep", 67.f }, { "JeepMG", 67.f }, { "JeepPolice", 67.f }, { "GJeep", 67.f },
        { "UAZ", 78.f }, { "UAZG", 78.f }, { "SGUAZG", 78.f },
        { "Skoda", 45.f }, { "SkodaBlue", 45.f }, { "SkodaGreen", 45.f }, { "SkodaRed", 45.f },
        { "Rapid", 80.f }, { "RapidY", 80.f }, { "Mini", 48.f }, { "Tractor", 65.f }, { "Bus", 200.f },
        // Trucks (stock 100-200)
        { "Ural", 360.f }, { "UralReammo", 360.f }, { "UralRefuel", 360.f }, { "UralRepair", 360.f },
        { "TruckV3SCivil", 120.f }, { "TruckV3SG", 120.f }, { "TruckV3SGReammo", 120.f },
        { "TruckV3SGRefuel", 120.f }, { "TruckV3SGRepair", 120.f },
        { "Truck5t", 189.f }, { "Truck5tOpen", 189.f }, { "Truck5tReammo", 189.f },
        { "Truck5tRefuel", 189.f }, { "Truck5tRepair", 189.f }, { "Scud", 700.f },
        // Motorcycles (stock 50); Kolo is a bicycle
        { "Jawa", 14.f }, { "Kolo", 0.f },
        // Helicopters (stock 1000)
        { "AH64", 1423.f }, { "Cobra", 935.f }, { "Mi17", 1870.f }, { "Mi24", 1840.f },
        { "Kamov", 1730.f }, { "OH58", 424.f }, { "UH60", 1361.f }, { "UH60MG", 1361.f },
        // Planes (stock 1000)
        { "A10", 6132.f }, { "A10LGB", 6132.f }, { "Cessna", 212.f }, { "BISCamel", 37.f }, { "BISCamel2", 37.f },
        // Boats (stock 700)
        { "BoatE", 95.f }, { "BoatW", 400.f },
    };

    for (const FuelDef& fd : kFuel)
    {
        ParamEntry* e = cfgV->FindEntry(fd.cls);
        if (!e || !e->IsClass())
            continue;
        ParamClass* v = e->GetClassInterface();
        if (!v)
            continue;

        // Already owns a fuelCapacity? Just set it - no base juggling needed.
        if (v->FindEntryNoInheritance("fuelCapacity"))
        {
            v->Add(RStringB("fuelCapacity"), fd.litres);
            continue;
        }

        // Inherited: detach the base so Add creates an OWN entry, then reattach. Only do
        // this if the base resolves - never leave a class permanently base-less.
        const char* baseName = v->GetBaseName();
        if (baseName && baseName[0])
        {
            ParamEntry* be = cfgV->FindEntry(baseName);
            const ParamClass* basePtr = (be && be->IsClass()) ? be->GetClassInterface() : nullptr;
            if (!basePtr)
                continue;
            v->SetBase(nullptr);
            v->Add(RStringB("fuelCapacity"), fd.litres);
            v->SetBase(basePtr);
        }
        else
        {
            v->Add(RStringB("fuelCapacity"), fd.litres);
        }
    }
}

// Defined in UI/Inventory/InventoryModel.cpp - reads the inventory key / edit-menu
// settings from <mod>\inventory_settings.cfg. Namespace-qualified so it matches the
// definition's linkage (this file declares Globals:: at global scope).
namespace Poseidon { void LoadInventorySettings(); void LoadIconTuning(); }

void Globals::Init()
{
    LoadInventorySettings();
    LoadIconTuning();
    if (GUseFileBanks)
    {
        // Rebuild addon state from scratch for the current mod set: a re-mount keeps the
        // engine alive, so the previous mount's addon registry would otherwise persist.
        AddonSystem::ClearRegistry();
        GFileBanks.Clear();
        if (GFileBankPrefix.GetLength() > 0)
        {
            LoadBanksEx(RString("dta\\") + GFileBankPrefix, true);
            LoadBanksEx(RString("addons\\") + GFileBankPrefix, true, true);
        }
        LoadBanksEx("dta", true);
        LoadBanksEx("addons", true, true);
        LoadBanksEx("Campaigns", false);
        AddonSystem::ParseAllAddonConfigs();
        // Add the "Ammo Crates (Arsenal)" editor crate as a runtime clone of
        // ReammoBoxWest (see RegisterArsenalCrate above for why config-file
        // inheritance can't do this with this engine's text parser).
        RegisterArsenalCrate();
        // Inject the US / AI-2 first-aid-kit magazines (see RegisterMedkits).
        RegisterMedkits();
        // Set precise real-world fuelCapacity per vehicle (see RegisterFuelCapacities).
        RegisterFuelCapacities();
        AddonSystem::ClearAddonConfigs();
        AddonSystem::MarkAllAddonsLockable();
    }

    remove("clipboard.txt");

    int fileMemory = ENGINE_CONFIG.fileHeapSize * (1024 * 1024);
    if (fileMemory < 1024 * 1024)
    {
        fileMemory = 1024 * 1024;
    }

    GFileServer = new FileServerST(fileMemory);
    GFileServer->Start();

    // Resolve and apply the coarse process-wide heap limits. Precedence: --maxmem CLI
    // (hard = N MB, soft = 75% of it) overrides everything; else per config field:
    // -1 = auto (a fixed-cap backstop clamped by physical RAM; inert on a normal
    // machine), 0 = unlimited, >0 = explicit MB. Idempotent: re-applied on each
    // in-place remount alongside the file cache.
    {
        const size_t mb = 1024u * 1024u;
        const size_t physBytes = Foundation::QueryPhysicalMemoryBytes();
        const int maxMemMB = AppConfig::Instance().GetMaxMemMB();
        size_t soft = 0;
        size_t hard = 0;
        const char* source = "auto";
        if (maxMemMB > 0)
        {
            hard = static_cast<size_t>(maxMemMB) * mb;
            soft = hard / 4 * 3; // soft watermark at 75% of the hard cap
            source = "--maxmem";
        }
        else
        {
            const Foundation::MemoryLimits autoLimits = Foundation::DeriveDefaultMemoryLimits(physBytes);
            auto resolve = [mb](int cfgMB, size_t autoBytes) -> size_t
            { return cfgMB < 0 ? autoBytes : static_cast<size_t>(cfgMB) * mb; };
            soft = resolve(ENGINE_CONFIG.memorySoftLimitMB, autoLimits.soft);
            hard = resolve(ENGINE_CONFIG.memoryHardLimitMB, autoLimits.hard);
            if (ENGINE_CONFIG.memorySoftLimitMB >= 0 || ENGINE_CONFIG.memoryHardLimitMB >= 0)
                source = "config";
        }
        Foundation::SetProcessMemoryLimits(soft, hard);
        if (hard == 0)
            LOG_INFO(Memory, "Process memory: {} MB physical, no limit ({})", physBytes / mb, source);
        else
            LOG_INFO(Memory, "Process memory: {} MB physical, soft={} MB hard={} MB ({})", physBytes / mb, soft / mb,
                     hard / mb, source);
    }

    drawTreshold = 2.0 * 2.0;
    shadowTreshold = reflectTreshold = 4 * 4;

    time = Time(0);
    uiTime = UITime(0);

    newGame = false;
    exit = false;
    demo = false;

    strcpy(header.filename, "Game001");
    RString worldInit = Pars >> "CfgWorlds" >> "initWorld";
    strcpy(header.worldname, worldInit);

    if (Glob.header.playerName.GetLength() == 0)
    {
        // Prefer an existing usable profile (the last-used one when it still
        // exists, otherwise a deterministic existing profile) over creating a
        // fresh OS-login/default profile; only create and remember a default
        // when no profile exists yet.
        ProfileService selector({std::string(Foundation::GamePaths::Instance().UserDir()),
                                 [] { return Foundation::prefsGetString(AppName, "PlayerName"); },
                                 [](const std::string& name)
                                 { Foundation::prefsSetString(AppName, "PlayerName", name.c_str()); },
                                 []() -> std::string
                                 {
#ifdef _WIN32
                                     char buf[256];
                                     DWORD bufSize = sizeof(buf);
                                     if (::GetUserName(buf, &bufSize) && bufSize > 0)
                                         return buf;
                                     return {};
#else
                                     const char* loginName = getlogin();
                                     return loginName ? loginName : std::string();
#endif
                                 }});
        header.playerName = selector.ResolveStartupProfile().c_str();
    }

    header.playerFace = "Default";
    header.playerGlasses = "None";
    header.playerSpeaker = (Pars >> "CfgVoice" >> "voices")[0];
    header.playerPitch = 1.0;
    RString filename = Poseidon::GetUserParams();
    ParamFile cfg;
    cfg.Parse(filename);
    const ParamEntry* identity = cfg.FindEntry("Identity");
    if (identity)
    {
        header.playerFace = (*identity) >> "face";
        if (identity->FindEntry("glasses"))
        {
            header.playerGlasses = (*identity) >> "glasses";
        }
        header.playerSpeaker = (*identity) >> "speaker";
        header.playerPitch = (*identity) >> "pitch";
    }

    header.playerSide = TWest;
    USER_CONFIG.easyMode = true;
#if _ENABLE_CHEATS
    config.super = false;
#endif

    LoadGameSettings();

    InputSubsystem::Instance().LoadKeys();
    UserConfig_LoadDifficulties(USER_CONFIG);

    ParamFile userCfg;
    userCfg.Parse(Poseidon::GetUserParams());
}

void Globals::Clear()
{
    QIFStreamB::ClearBanks();
    GFileServer.Free();
    ClearShapes();
    Pars.Clear();
    ExtParsCampaign.Clear();
    ExtParsMission.Clear();
    Res.Clear();
    DestroyMsgFormats();
}

namespace Poseidon
{
Globals Glob;
}
