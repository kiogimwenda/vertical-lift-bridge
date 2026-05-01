// 08_cable_guide.scad — fairlead between drum and tower-top pulley
include <00_common.scad>

module cable_guide() {
    difference() {
        // body
        cube([CABLE_GUIDE_W, CABLE_GUIDE_D, CABLE_GUIDE_H]);
        // cable channel through long axis
        translate([-0.1, CABLE_GUIDE_D/2, CABLE_GUIDE_H/2])
            rotate([0,90,0])
                cylinder(r=CABLE_GUIDE_GROOVE_R + 0.1,
                         h=CABLE_GUIDE_W + 0.2, $fn=32);
        // open the channel from above (slot to thread cable)
        translate([-0.1, CABLE_GUIDE_D/2 - 0.6, CABLE_GUIDE_H/2])
            cube([CABLE_GUIDE_W + 0.2, 1.2, CABLE_GUIDE_H/2 + 0.1]);
        // fastener holes (2× M3 to base)
        for (x = [8, CABLE_GUIDE_W - 8])
            translate([x, CABLE_GUIDE_D/2, -0.1])
                cylinder(d=M3_CLEAR, h=CABLE_GUIDE_H/2);
    }
}

cable_guide();
