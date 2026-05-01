// 03_bridge_deck_section.scad — bridge deck, monolithic
// Fits 220×220 print bed at 200×120; printed flat with 30 % gyroid infill.
include <00_common.scad>

module deck() {
    difference() {
        // base plate
        cube([DECK_L, DECK_W, DECK_T]);

        // road surface texture: 20 grooves running across the road
        for (i = [1:1:19])
            translate([i * (DECK_L/20) - 0.5, 5, DECK_T - 0.6])
                cube([1, DECK_W - 10, 0.7]);
    }

    // linear-guide blocks at each end (run inside tower side rails)
    for (x = [-2, DECK_L - 18]) {
        translate([x, DECK_W/2 - 10, DECK_T])
            cube([20, 20, 12]);
    }

    // cable attachment ears (centre of each end)
    for (x = [-4, DECK_L - 4])
        translate([x, DECK_W/2 - 10, DECK_T])
            difference() {
                cube([8, 20, 16]);
                translate([4, 10, 12])
                    rotate([90,0,0])
                        cylinder(d=2.5, h=22, center=true);
            }
}

deck();
