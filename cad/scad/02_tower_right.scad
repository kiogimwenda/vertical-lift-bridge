// 02_tower_right.scad — RIGHT tower, UPPER section (175 to 350 mm)
// Includes pulley shaft mounting bosses at the top and the dovetail
// SOCKET that mates with the lower section's tongue.
include <00_common.scad>

module tower_dovetail_socket(w, d) {
    // 10.4 mm deep socket (0.4 over tongue for fit)
    translate([0, 0, 0])
        linear_extrude(10.4)
            polygon([[-w/2-0.2, -d/2-0.2], [w/2+0.2, -d/2-0.2],
                     [w/2-3+0.2, d/2+0.2], [-w/2+3-0.2, d/2+0.2]]);
}

module pulley_shaft_boss() {
    difference() {
        cylinder(d=14, h=12);
        translate([0,0,-0.1]) cylinder(d=PULLEY_BORE, h=12.2);
    }
}

module tower_upper_right() {
    UPPER_H = TOWER_H - TOWER_SPLIT;  // 175 mm
    difference() {
        union() {
            // body
            translate([0, 0, 10.4])
                cube([TOWER_W, TOWER_D, UPPER_H - 10.4]);
            // top cap (closed)
            translate([0, 0, TOWER_H - TOWER_SPLIT - 6])
                cube([TOWER_W, TOWER_D, 6]);
        }
        // hollow
        translate([TOWER_WALL, TOWER_WALL, 10.4])
            cube([TOWER_INNER_W, TOWER_INNER_D, UPPER_H - 10.4 - 6]);

        // pulley shaft hole through both side walls (Y-axis, near top)
        translate([TOWER_W/2, -0.1, UPPER_H - 25])
            rotate([-90,0,0]) cylinder(d=PULLEY_BORE, h=TOWER_D + 0.2);

        // cable pass-through slot to outside (mirror of lower slot)
        translate([TOWER_W/2 - 4, -0.1, UPPER_H - 60])
            cube([8, TOWER_WALL + 0.2, 25]);

        // limit switch screw holes (KW12-3 has 19.8 mm hole spacing, M2)
        for (z = [25, UPPER_H - 50])
            for (x = [TOWER_W/2 - 9.9, TOWER_W/2 + 9.9])
                translate([x, TOWER_WALL + 0.5, z])
                    rotate([90,0,0])
                        cylinder(d=2.2, h=TOWER_WALL + 1);

        // dovetail SOCKET cut at bottom
        translate([TOWER_W/2, TOWER_D/2, 0])
            tower_dovetail_socket(TOWER_W - 8, TOWER_D - 8);
    }
    // pulley shaft bosses inside the box, both side walls
    translate([TOWER_W/2 - 6, TOWER_WALL + 0.1, UPPER_H - 25])
        rotate([-90,0,0]) pulley_shaft_boss();
    translate([TOWER_W/2 - 6, TOWER_D - TOWER_WALL - 12.1, UPPER_H - 25])
        rotate([-90,0,0]) pulley_shaft_boss();
}

tower_upper_right();
