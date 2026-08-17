triSetLanguage "English"

triAssertMissionPlayable
triSetView [9814.8, 29.5, 3972.6, 1.0, -0.25, 0.0]
triSimFrames 60
triScreenshot "01_ground_clearance"

// packages@3eeaa7e started this player in the wrong low/crouched visual state:
// this lower-leg sample was terrain-bright (~200 max channel) instead of the
// dark standing soldier silhouette (~14 max channel).
triAssertLt [(triGetPixelMaxChannel [0.550, 0.821]), 80]

triAssertGt [triPlayerGroundClearance, -0.05]
triAssertLt [triPlayerGroundClearance, 0.50]
triAssertGt [triPlayerTerrainClearance, -0.05]
triAssertLt [triPlayerTerrainClearance, 0.50]
triAssertGt [triPlayerContactGroundClearance, -0.05]
triAssertLt [triPlayerContactGroundClearance, 0.50]

triEndTest
