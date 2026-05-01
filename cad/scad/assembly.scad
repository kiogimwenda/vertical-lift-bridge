// assembly.scad — non-printable visual reference of the assembled bridge.
// Renders an exploded preview to verify clearances. NOT for printing.
include <00_common.scad>
use <01_tower_left.scad>
use <02_tower_right.scad>
use <03_bridge_deck_section.scad>
use <06_pulley_wheel.scad>

// base footprint outline
%cube([FOOTPRINT_X, FOOTPRINT_Y, BASE_T]);

// left tower (lower + upper sections)
translate([300, FOOTPRINT_Y/2 - TOWER_D/2, BASE_T])
    color("LightSteelBlue") {
        tower_lower_left();
        translate([0, 0, TOWER_SPLIT]) tower_upper_right();
    }

// right tower (mirror)
translate([300 + TOWER_SPACING, FOOTPRINT_Y/2 - TOWER_D/2, BASE_T])
    color("LightSteelBlue") mirror([1,0,0]) {
        tower_lower_left();
        translate([0, 0, TOWER_SPLIT]) tower_upper_right();
    }

// deck (raised at 50% travel)
translate([(FOOTPRINT_X - DECK_L)/2,
           (FOOTPRINT_Y - DECK_W)/2,
           BASE_T + DECK_LIFT/2])
    color("Goldenrod") deck();

// tower-top pulleys (visual only)
for (xt = [300 + TOWER_W/2, 300 + TOWER_SPACING + TOWER_W/2])
    translate([xt, FOOTPRINT_Y/2, BASE_T + TOWER_H - 25])
        rotate([90,0,0]) color("DimGray") pulley();
