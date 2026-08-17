// Selecting a multiplayer wizard template must refresh the overview (name, title,
// description, image) to the chosen template. The templates for one world all
// extract through the shared missions\__cur_sp.<world> bank slot; if the previous
// slot bank is not evicted every template resolves to the first one and the
// overview is stuck on the alphabetically-first template (1-10_T_TeamFlagFight).
//
// Broken state: after selecting 1-6_C_Cooperative / 2-12_T_SectorControl the
// overview text stays "Team Flag Fight ..."; the includes/excludes below fail.

triSetLanguage "English"
triAssertEq [(triDisplay), 0]

// Main menu -> Multiplayer -> New server.
triClick 105
triAssertEq [(triDisplay), 8]
triClick 104
triAssertEq [(triDisplay), 17]

// Eden ships several MP templates — enough to expose the shared-slot bug.
triAssertEq [triSelectListByData [101, "Eden"], true]
triSelectList [102, 1]
triClick 1
triAssertEq [(triDisplay), 67]

// The template list itself must expose the distinct template ids as row data.
triAssertEq [triSelectListByData [101, "1-10_T_TeamFlagFight"], true]
triWaitFrames 8
triAssertEq [(triAssertIncludes [(triControlText 102), "Team Flag Fight"]), "OK"]

// Switching templates must switch the overview, not keep the first one.
triAssertEq [triSelectListByData [101, "1-6_C_Cooperative"], true]
triWaitFrames 8
triAssertEq [(triAssertIncludes [(triControlText 102), "Clean Sweep"]), "OK"]
triAssertEq [(triAssertExcludes [(triControlText 102), "Team Flag Fight"]), "OK"]

triAssertEq [triSelectListByData [101, "2-12_T_SectorControl"], true]
triWaitFrames 8
triAssertEq [(triAssertIncludes [(triControlText 102), "Sector Control"]), "OK"]
triAssertEq [(triAssertExcludes [(triControlText 102), "Clean Sweep"]), "OK"]

// Returning to the first template must reload it too (not a one-way latch).
triAssertEq [triSelectListByData [101, "1-10_T_TeamFlagFight"], true]
triWaitFrames 8
triAssertEq [(triAssertIncludes [(triControlText 102), "Team Flag Fight"]), "OK"]
triAssertEq [(triAssertExcludes [(triControlText 102), "Sector Control"]), "OK"]

// Switching language must re-resolve the overview to the localized briefingName
// (the wizard registers a language-changed callback that reloads it). The Czech
// team-flag-fight title proves the templates are actually translated, not just
// shown by raw id.
triSetLanguage "Czech"
triWaitFrames 8
triAssertEq [(triAssertIncludes [(triControlText 102), "Týmový boj o vlajku"]), "OK"]

// And the per-template switch still holds under the localized text.
triAssertEq [triSelectListByData [101, "1-6_C_Cooperative"], true]
triWaitFrames 8
triAssertEq [(triAssertExcludes [(triControlText 102), "Týmový boj o vlajku"]), "OK"]

triEndTest
