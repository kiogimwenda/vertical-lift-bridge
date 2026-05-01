// vertical-lift-bridge :: COMMON PARAMETERS
// All dimensions in millimetres. Imported by every part file.
// Ratified parameters from audit Q1–Q7. Do not edit without group approval.

// ==== BUILD VOLUME ====
PRINTER_X = 220;
PRINTER_Y = 220;
PRINTER_Z = 250;
$fn = 64;  // smoothness for production prints

// ==== OVERALL FOOTPRINT ====
FOOTPRINT_X = 1200;        // along the road / cable axis
FOOTPRINT_Y = 600;         // across the river
BASE_T      = 6;           // base panel thickness
WATERWAY_W  = 200;         // navigable channel width

// ==== TOWERS ====
TOWER_H        = 350;       // total height (split-printed in 2 sections)
TOWER_SPLIT    = 175;       // join height
TOWER_W        = 90;        // width
TOWER_D        = 60;        // depth
TOWER_WALL     = 4;         // wall thickness
TOWER_INNER_W  = TOWER_W - 2*TOWER_WALL;
TOWER_INNER_D  = TOWER_D - 2*TOWER_WALL;
TOWER_SPACING  = 800;       // tower-to-tower centre distance

// ==== DECK ====
DECK_L  = 200;              // along the road
DECK_W  = 120;              // across the road
DECK_T  = 6;                // thickness
DECK_LIFT = 180;            // vertical travel

// ==== COUNTERWEIGHT ====
CW_L  = 60;
CW_W  = 40;
CW_H  = 60;
CW_WALL = 3;
CW_NUT_POCKET_D = 14;       // M8 nut across-flats + clearance
CW_NUT_POCKET_H = 8;

// ==== PULLEY ====
PULLEY_D       = 50;
PULLEY_BORE    = 6.2;       // 6 mm shaft + 0.2 clearance
PULLEY_W       = 20;
PULLEY_GROOVE_D = 1.4;      // 1 mm cable + 0.4 clearance
PULLEY_HUB_D   = 12;
PULLEY_HUB_H   = 8;

// ==== MOTOR ====
MOTOR_BODY_D   = 25;        // JGA25-370 body
MOTOR_BODY_L   = 32;
MOTOR_GEARBOX_L = 19;
MOTOR_SHAFT_D  = 6.2;       // 6 mm shaft + 0.2 clearance
MOTOR_FLAT_W   = 5.5;
MOTOR_MOUNT_W  = 35;
MOTOR_MOUNT_T  = 5;
MOTOR_MOUNT_HOLE_D = 3.2;   // M3 clearance

// ==== CABLE GUIDE ====
CABLE_GUIDE_W = 40;
CABLE_GUIDE_D = 30;
CABLE_GUIDE_H = 25;
CABLE_GUIDE_GROOVE_R = 0.7; // half of 1.4 cable channel

// ==== ROAD BARRIER ====
BARRIER_L = 80;
BARRIER_W = 15;
BARRIER_T = 8;
BARRIER_SERVO_HORN_D = 6;

// ==== PCB ENCLOSURE ====
PCB_X = 100;
PCB_Y = 80;
PCB_T = 1.6;
ENC_WALL = 2.5;
ENC_GAP  = 0.3;
ENC_PCB_STANDOFF_H = 5;
ENC_LID_H = 8;
ENC_BASE_H = PCB_T + ENC_PCB_STANDOFF_H + 8;
ENC_OUTER_X = PCB_X + 2*(ENC_WALL + ENC_GAP);
ENC_OUTER_Y = PCB_Y + 2*(ENC_WALL + ENC_GAP);

// ==== CAMERA MOUNT (ESP32-CAM AI-Thinker) ====
CAM_BOARD_X = 40.5;
CAM_BOARD_Y = 27;
CAM_BOARD_T = 1.6;
CAM_LENS_D  = 9.5;
CAM_GANTRY_H = 220;          // height above road for FOV
CAM_TILT_DEG = 25;

// ==== TFT DISPLAY MOUNT ====
// 2.8" ILI9341 with XPT2046 — common module 86×56×4 mm with 8 mm header on one side
TFT_BOARD_X = 86;
TFT_BOARD_Y = 56;
TFT_BOARD_T = 4;
TFT_VISIBLE_X = 58;          // active area
TFT_VISIBLE_Y = 44;
TFT_TILT_DEG = 35;           // Q5 ratified — 35° tilt
TFT_BEZEL = 5;
TFT_BACK_DEPTH = 18;         // header height behind board

// ==== HARDWARE ====
M3_CLEAR  = 3.2;
M3_TAP    = 2.8;
M3_HEAD_D = 6.0;
M3_HEAD_H = 3.0;
M3_INSERT_D = 4.2;           // brass heat-set insert OD
M3_INSERT_H = 5.0;
M8_BOLT_D = 8.4;
