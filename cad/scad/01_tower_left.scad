// 01_tower_left.scad — LEFT tower, LOWER section (0 to 175 mm)
// Hollow box with linear-guide T-slot on inner face for counterweight,
// pulley shaft mount at top, dovetail tongue for upper section.
include <00_common.scad>

module tower_dovetail_tongue(w, d) {
    // 10 mm tall trapezoidal tongue centred on top face
    translate([0, 0, 0])
        linear_extrude(10)
            polygon([[-w/2, -d/2], [w/2, -d/2],
                     [w/2-3, d/2], [-w/2+3, d/2]]);
}

module tower_lower_left() {
    difference() {
        // outer shell
        cube([TOWER_W, TOWER_D, TOWER_SPLIT]);
        // hollow inside
        translate([TOWER_WALL, TOWER_WALL, BASE_T])
            cube([TOWER_INNER_W, TOWER_INNER_D, TOWER_SPLIT]);

        // counterweight T-slot (left tower has CW on inner face = +X face)
        translate([TOWER_W - TOWER_WALL - 0.1, TOWER_D/2 - 6, BASE_T])
            cube([TOWER_WALL + 0.2, 12, TOWER_SPLIT - BASE_T]);

        // base mounting holes (4× M3, 60 mm spacing)
        for (x = [15, TOWER_W-15], y = [10, TOWER_D-10])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=BASE_T+0.2);

        // cable pass-through slot (top of section, on +X side)
        translate([TOWER_W/2 - 4, TOWER_D - TOWER_WALL - 0.1,
                   TOWER_SPLIT - 30])
            cube([8, TOWER_WALL + 0.2, 25]);

        // inspection window
        translate([TOWER_W/2 - 12, -0.1, 60])
            cube([24, TOWER_WALL + 0.2, 60]);
    }
    // dovetail tongue on top
    translate([TOWER_W/2, TOWER_D/2, TOWER_SPLIT])
        tower_dovetail_tongue(TOWER_W - 8, TOWER_D - 8);
}

tower_lower_left();
