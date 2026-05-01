// 06_pulley_wheel.scad — Ø50 mm cable pulley with 1.4 mm groove
include <00_common.scad>

module pulley() {
    difference() {
        union() {
            // main body
            cylinder(d=PULLEY_D, h=PULLEY_W);
            // hub
            translate([0,0,PULLEY_W])
                cylinder(d=PULLEY_HUB_D, h=PULLEY_HUB_H);
        }
        // bore
        translate([0,0,-0.1])
            cylinder(d=PULLEY_BORE, h=PULLEY_W + PULLEY_HUB_H + 0.2);
        // groove
        translate([0,0,PULLEY_W/2])
            rotate_extrude($fn=128)
                translate([PULLEY_D/2 - PULLEY_GROOVE_D/2, 0, 0])
                    circle(d=PULLEY_GROOVE_D, $fn=32);
        // hub set-screw
        translate([0, 0, PULLEY_W + PULLEY_HUB_H/2])
            rotate([90,0,0])
                cylinder(d=2.5, h=PULLEY_HUB_D);
        // weight-reduction holes (5×)
        for (a = [0:72:359])
            rotate([0,0,a])
                translate([15, 0, -0.1])
                    cylinder(d=5, h=PULLEY_W + 0.2);
    }
}

pulley();
