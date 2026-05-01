// 11_pcb_enclosure.scad — 2-piece enclosure for the 100×80 mm PCB
// Generates BASE and LID side-by-side.
include <00_common.scad>

module pcb_enclosure_base() {
    difference() {
        cube([ENC_OUTER_X, ENC_OUTER_Y, ENC_BASE_H]);
        // hollow
        translate([ENC_WALL, ENC_WALL, ENC_WALL])
            cube([PCB_X + 2*ENC_GAP, PCB_Y + 2*ENC_GAP, ENC_BASE_H]);
        // USB-C cutout (long edge, near ESP32)
        translate([10, -0.1, ENC_PCB_STANDOFF_H + ENC_WALL + 1])
            cube([10, ENC_WALL + 0.2, 4]);
        // 12 V power jack cutout
        translate([ENC_OUTER_X - 18, -0.1, ENC_PCB_STANDOFF_H + ENC_WALL + 1])
            cube([12, ENC_WALL + 0.2, 8]);
        // fan vents (pattern of slots on long sides)
        for (x = [25:8:ENC_OUTER_X - 25])
            translate([x, ENC_OUTER_Y - 0.1, ENC_BASE_H/2])
                cube([4, ENC_WALL + 0.2, ENC_BASE_H/2 - 4]);
        // lid screw holes (4 corners, M3 inserts pressed in)
        for (x = [3, ENC_OUTER_X - 3], y = [3, ENC_OUTER_Y - 3])
            translate([x, y, ENC_BASE_H - M3_INSERT_H])
                cylinder(d=M3_INSERT_D, h=M3_INSERT_H + 0.1);
    }
    // PCB standoffs (4× M3-tapped)
    for (x = [3.5, PCB_X - 3.5], y = [3.5, PCB_Y - 3.5])
        translate([ENC_WALL + ENC_GAP + x, ENC_WALL + ENC_GAP + y, ENC_WALL])
            difference() {
                cylinder(d=6, h=ENC_PCB_STANDOFF_H);
                translate([0,0,1])
                    cylinder(d=M3_TAP, h=ENC_PCB_STANDOFF_H);
            }
}

module pcb_enclosure_lid() {
    difference() {
        cube([ENC_OUTER_X, ENC_OUTER_Y, ENC_LID_H]);
        // shallow cavity that nests into the base lip
        translate([ENC_WALL, ENC_WALL, -0.1])
            cube([ENC_OUTER_X - 2*ENC_WALL, ENC_OUTER_Y - 2*ENC_WALL, 3]);
        // screw clearance holes
        for (x = [3, ENC_OUTER_X - 3], y = [3, ENC_OUTER_Y - 3])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=ENC_LID_H + 0.2);
        // ESP32 antenna keep-out window (top of board)
        translate([ENC_WALL + ENC_GAP + 35, ENC_WALL + ENC_GAP + PCB_Y - 18, 2])
            cube([20, 12, ENC_LID_H]);
    }
    // Project label inset (silkscreen substitute)
    translate([ENC_OUTER_X/2 - 25, ENC_OUTER_Y/2 - 4, ENC_LID_H - 0.6])
        linear_extrude(0.7)
            text("VLB-G5", size=6, font="Liberation Sans:style=Bold");
}

pcb_enclosure_base();
translate([ENC_OUTER_X + 15, 0, 0]) pcb_enclosure_lid();
