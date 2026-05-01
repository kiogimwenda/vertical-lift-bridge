// 05_counterweight_right.scad — RIGHT counterweight (mirror of left)
include <00_common.scad>

module counterweight_right() {
    difference() {
        cube([CW_L, CW_W, CW_H]);
        translate([CW_WALL, CW_WALL, CW_WALL])
            cube([CW_L - 2*CW_WALL, CW_W - 2*CW_WALL, CW_H - 2*CW_WALL]);
        translate([CW_L/2, -0.1, CW_H - 8])
            rotate([-90,0,0]) cylinder(d=3.2, h=CW_W + 0.2);
        translate([CW_L/2 - 4, -0.1, CW_H - 6])
            cube([8, CW_W + 0.2, 4]);
        translate([CW_L/2, CW_W/2, CW_H - CW_WALL - 0.1])
            cylinder(d=10, h=CW_WALL + 0.2);
    }
    // T-slot rib on opposite face
    translate([-4, CW_W/2 - 5, 0])
        cube([4, 10, CW_H]);
}

counterweight_right();
