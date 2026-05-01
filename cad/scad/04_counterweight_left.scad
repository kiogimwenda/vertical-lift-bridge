// 04_counterweight_left.scad — counterweight shell (LEFT)
// Hollow rectangular box that accepts M8 nut/washer fill to tune
// final mass to 120 g ± 2 g. Includes T-slot guide rib for tower rail.
include <00_common.scad>

module counterweight() {
    difference() {
        cube([CW_L, CW_W, CW_H]);
        // hollow cavity for fill mass
        translate([CW_WALL, CW_WALL, CW_WALL])
            cube([CW_L - 2*CW_WALL, CW_W - 2*CW_WALL, CW_H - 2*CW_WALL]);

        // top cable attachment hole (M3 cross-pin)
        translate([CW_L/2, -0.1, CW_H - 8])
            rotate([-90,0,0]) cylinder(d=3.2, h=CW_W + 0.2);
        translate([CW_L/2 - 4, -0.1, CW_H - 6])
            cube([8, CW_W + 0.2, 4]);

        // fill access hole (top, plug with PLA glue after tuning)
        translate([CW_L/2, CW_W/2, CW_H - CW_WALL - 0.1])
            cylinder(d=10, h=CW_WALL + 0.2);
    }

    // T-slot guide rib (mates with tower inner T-slot)
    translate([CW_L, CW_W/2 - 5, 0])
        cube([4, 10, CW_H]);
}

counterweight();
