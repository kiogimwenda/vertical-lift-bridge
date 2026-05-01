// 13_tft_display_mount.scad — operator dashboard panel mount.
// Tilt = 35°  (ratified Q5).  Justification: with eye height ≈ 1.6 m
// and panel centre at ≈ 0.6 m, the line-of-sight makes ≈ 32° below
// horizontal; a 35° panel tilt gives near-perpendicular viewing,
// reducing IPS off-axis colour shift and parallax on touch.
include <00_common.scad>

module tft_mount() {
    BASE_X = TFT_BOARD_X + 30;
    BASE_Y = 70;
    BASE_T = 6;

    // base flange with bolt-down holes
    difference() {
        cube([BASE_X, BASE_Y, BASE_T]);
        for (x = [8, BASE_X - 8], y = [8, BASE_Y - 8])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=BASE_T + 0.2);
    }

    // tilted cradle plate
    translate([(BASE_X - (TFT_BOARD_X + 2*TFT_BEZEL))/2, BASE_Y/2 - 5, BASE_T])
        rotate([90 - TFT_TILT_DEG, 0, 0]) {
            difference() {
                // backing plate
                cube([TFT_BOARD_X + 2*TFT_BEZEL,
                      TFT_BOARD_Y + 2*TFT_BEZEL,
                      4]);
                // active-area window (cuts through the back)
                translate([(TFT_BOARD_X + 2*TFT_BEZEL - TFT_VISIBLE_X)/2,
                           (TFT_BOARD_Y + 2*TFT_BEZEL - TFT_VISIBLE_Y)/2,
                           -0.1])
                    cube([TFT_VISIBLE_X, TFT_VISIBLE_Y, 4.2]);
                // header recess
                translate([0, TFT_BOARD_Y + 2*TFT_BEZEL - 8, -0.1])
                    cube([TFT_BOARD_X + 2*TFT_BEZEL, 8, 1]);
                // PCB-corner mounting holes (4× M3 to clip the board)
                for (x = [3, TFT_BOARD_X + 2*TFT_BEZEL - 3],
                     y = [3, TFT_BOARD_Y + 2*TFT_BEZEL - 3])
                    translate([x, y, -0.1])
                        cylinder(d=2.7, h=4.2);  // self-tap M3
            }
            // rear standoff legs (raise board off backing plate by 4 mm)
            for (x = [3, TFT_BOARD_X + 2*TFT_BEZEL - 3],
                 y = [3, TFT_BOARD_Y + 2*TFT_BEZEL - 3])
                translate([x, y, 4])
                    cylinder(d=5, h=4);
        }

    // diagonal support struts (left + right)
    for (mx = [0, BASE_X])
        translate([mx, BASE_Y/2 - 2, BASE_T])
            rotate([0, mx == 0 ? 0 : 180, 0])
                linear_extrude(4)
                    polygon([[0,0],[40,0],[0, 40 * tan(TFT_TILT_DEG)]]);
}

tft_mount();
