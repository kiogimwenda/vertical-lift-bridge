// 12_camera_mount.scad — ESP32-CAM holder on a 220 mm gantry post.
// Two pieces: GANTRY post (flat-printed) and CAMERA cradle (printed
// already at the 25° downtilt so the road is centred in the FOV).
include <00_common.scad>

module gantry_post() {
    POST_X = 30;
    POST_Y = 30;
    POST_H = CAM_GANTRY_H;
    BASE_X = 80;
    BASE_Y = 80;
    BASE_T = 6;

    difference() {
        union() {
            // base flange
            cube([BASE_X, BASE_Y, BASE_T]);
            // post (centred on flange)
            translate([(BASE_X - POST_X)/2, (BASE_Y - POST_Y)/2, BASE_T])
                cube([POST_X, POST_Y, POST_H - BASE_T]);
        }
        // base bolt-down holes
        for (x = [10, BASE_X - 10], y = [10, BASE_Y - 10])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=BASE_T + 0.2);
        // hollow inside post (cable pass-through)
        translate([(BASE_X - POST_X)/2 + 4, (BASE_Y - POST_Y)/2 + 4, 1])
            cube([POST_X - 8, POST_Y - 8, POST_H + 0.1]);
        // cradle attachment holes at top
        for (a = [0:90:359])
            rotate([0,0,a])
                translate([0, POST_Y/2 - 1, POST_H - 8])
                    rotate([90,0,0])
                        cylinder(d=M3_CLEAR, h=4);
    }
}

module cam_cradle() {
    // pre-tilted housing
    rotate([CAM_TILT_DEG, 0, 0]) {
        difference() {
            cube([CAM_BOARD_X + 6, CAM_BOARD_Y + 6, 12]);
            // PCB pocket
            translate([3, 3, 4])
                cube([CAM_BOARD_X + 0.4, CAM_BOARD_Y + 0.4, 8.1]);
            // lens cylinder
            translate([CAM_BOARD_X/2 + 3, CAM_BOARD_Y/2 + 3, -0.1])
                cylinder(d=CAM_LENS_D + 0.6, h=4.2);
            // wire exit (back)
            translate([CAM_BOARD_X/2 - 4, -0.1, 6])
                cube([8, 4, 4]);
            // mounting screws
            for (x = [3.5, CAM_BOARD_X + 2.5], y = [3.5, CAM_BOARD_Y + 2.5])
                translate([x, y, 4 - 0.1])
                    cylinder(d=M3_CLEAR, h=2.4);
        }
    }
}

gantry_post();
translate([100, 0, 0]) cam_cradle();
