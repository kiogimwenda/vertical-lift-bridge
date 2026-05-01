// 09_road_barrier.scad — servo-driven barrier arm + post
include <00_common.scad>

module barrier_arm() {
    // arm proper
    difference() {
        cube([BARRIER_L, BARRIER_W, BARRIER_T]);
        // servo horn pocket at one end
        translate([6, BARRIER_W/2, -0.1])
            cylinder(d=BARRIER_SERVO_HORN_D + 0.3, h=BARRIER_T + 0.2);
        // 4 horn screw holes
        for (a = [0:90:359])
            rotate([0,0,a])
                translate([6,0,0])
                    translate([4, 0, -0.1])
                        cylinder(d=1.6, h=BARRIER_T + 0.2);
        // tip warning slot (high-vis stripes painted later)
        for (i = [0:1:5])
            translate([BARRIER_L - 30 + i*5, 1, BARRIER_T - 0.6])
                cube([3, BARRIER_W - 2, 0.7]);
    }
}

module barrier_post() {
    // 25 mm tall post that bolts to base; servo body friction-fits
    SERVO_X = 23;
    SERVO_Y = 12;
    SERVO_H = 22;
    BASE = 35;
    difference() {
        union() {
            cube([BASE, BASE, BARRIER_T]);
            translate([(BASE - SERVO_X)/2, (BASE - SERVO_Y)/2, BARRIER_T])
                cube([SERVO_X + 0.4, SERVO_Y + 0.4, SERVO_H]);
        }
        // hollow servo cavity
        translate([(BASE - SERVO_X)/2 + 1, (BASE - SERVO_Y)/2 + 1, BARRIER_T - 0.1])
            cube([SERVO_X - 1.6, SERVO_Y - 1.6, SERVO_H + 0.2]);
        // base mounting holes
        for (x = [4, BASE - 4], y = [4, BASE - 4])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=BARRIER_T + 0.2);
    }
}

barrier_arm();
translate([0, BARRIER_W + 10, 0]) barrier_post();
