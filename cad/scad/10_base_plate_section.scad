// 10_base_plate_section.scad — one section of the 6-panel base plate.
// Six identical panels join along Y-edges with M3 brass-insert + tongue.
include <00_common.scad>

PANEL_X = FOOTPRINT_X / 6;   // 200 mm
PANEL_Y = FOOTPRINT_Y;       // 600 mm
PANEL_T = BASE_T;            // 6 mm

module base_panel() {
    difference() {
        cube([PANEL_X, PANEL_Y, PANEL_T]);

        // weight-reduction hex grid (decorative + functional)
        for (x = [25:25:PANEL_X-25])
            for (y = [25:25:PANEL_Y-25])
                translate([x, y, -0.1])
                    cylinder(d=12, h=PANEL_T + 0.2, $fn=6);

        // edge fastener holes (M3 inserts on +X edge, M3 clearance on -X)
        for (y = [50:100:PANEL_Y-50]) {
            // -X edge (clearance)
            translate([-0.1, y, PANEL_T/2])
                rotate([0,90,0]) cylinder(d=M3_CLEAR, h=15);
            // +X edge (insert pocket)
            translate([PANEL_X - 15, y, PANEL_T/2])
                rotate([0,90,0]) cylinder(d=M3_INSERT_D, h=15.1);
        }

        // tower mounting pattern (only on panels 1-2 and 5-6)
        // (left as-is; base supports all panels' bolt patterns)
        for (x = [40, PANEL_X-40], y = [80, PANEL_Y-80])
            translate([x, y, -0.1])
                cylinder(d=M3_CLEAR, h=PANEL_T + 0.2);
    }

    // tongue on +X edge (mates with -X groove of next panel)
    translate([PANEL_X, 5, PANEL_T/4])
        cube([4, PANEL_Y - 10, PANEL_T/2]);
}

base_panel();
