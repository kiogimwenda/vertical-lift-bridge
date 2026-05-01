// 07_motor_mount.scad — JGA25-370 mount with cable-drum bearing block
include <00_common.scad>

module motor_mount() {
    PLATE_X = 80;
    PLATE_Y = 60;
    PLATE_T = 5;

    difference() {
        union() {
            // base plate
            cube([PLATE_X, PLATE_Y, PLATE_T]);
            // motor cradle (full height of motor body)
            translate([20, PLATE_Y/2, PLATE_T])
                rotate([0,90,0])
                    cylinder(d=MOTOR_BODY_D + 2*MOTOR_MOUNT_T,
                             h=MOTOR_BODY_L);
            // drum bearing pillar
            translate([20 + MOTOR_BODY_L + 5, PLATE_Y/2, PLATE_T])
                rotate([0,90,0])
                    cylinder(d=22, h=15);
        }
        // motor body bore (subtract)
        translate([20 - 0.1, PLATE_Y/2, PLATE_T + (MOTOR_BODY_D + 2*MOTOR_MOUNT_T)/2])
            rotate([0,90,0])
                cylinder(d=MOTOR_BODY_D + 0.4, h=MOTOR_BODY_L + 0.2);
        // shaft pass-through
        translate([20 + MOTOR_BODY_L - 0.1, PLATE_Y/2,
                   PLATE_T + (MOTOR_BODY_D + 2*MOTOR_MOUNT_T)/2])
            rotate([0,90,0])
                cylinder(d=MOTOR_SHAFT_D + 4, h=25);
        // drum bearing bore
        translate([20 + MOTOR_BODY_L + 5 - 0.1, PLATE_Y/2,
                   PLATE_T + (MOTOR_BODY_D + 2*MOTOR_MOUNT_T)/2])
            rotate([0,90,0])
                cylinder(d=MOTOR_SHAFT_D + 0.6, h=15.2);
        // baseplate fastener holes (4× M3)
        for (x = [10, PLATE_X-10], y = [10, PLATE_Y-10])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=PLATE_T + 0.2);
    }
}

motor_mount();
