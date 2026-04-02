// Tall back cover V2 for hud_face_min.scad
// Like hud_back_min_tall, plus a flat rectangular slot at the TOP (+Y) for the FPC / dupont wire bundle.
// Board coords: USB / tail toward -Y, header at top toward +Y (12 o'clock).
// Units: mm

$fn = 96;

// Shared geometry (must match hud_face_min.scad)
board_d = 39.49;
board_total_h = 40.36;
board_tail_w = 25.28;
clearance = 0.35;
wall = 1.6;

back_t = 1.8;
rim_h = 4.0;
// Loosen insert into hud_face_min (printed bezel may vary slightly)
fit_gap = 0.55;

tall_h = 16.0;
// Less shrink on tall section = wider cavity for battery + room around front shell
tall_shrink = 0.45;

// Inner hollow: was wall+0.9 which made cavity *smaller than the board* — too tight.
// Single-wall thickness only so inside clears ~outer profile of hud_face_min.
inner_shell = wall + 0.25;

// Wire bundle slot (header side, +Y)
wire_slot_w = 22.0;   // along X (wide for dupont row)
wire_slot_h = 9.0;    // along Z (tall enough for angled bundle)
wire_slot_y = 14.0;   // how far forward in +Y to punch through outer wall

board_r = board_d / 2;
tail_drop = board_total_h - board_d;

// Approx outer radius at top of round part (for placing slot)
r_outer_top = board_r + clearance + wall;

module board_profile_2d(extra = 0) {
    union() {
        circle(d = board_d + extra * 2);
        translate([0, -(board_r + tail_drop / 2)])
            square([board_tail_w + extra * 2, tail_drop + extra], center = true);
    }
}

module outer_profile_2d() {
    offset(r = wall)
        board_profile_2d(extra = clearance);
}

module tall_shell_v2() {
    difference() {
        union() {
            linear_extrude(height = back_t)
                outer_profile_2d();

            translate([0, 0, back_t])
                linear_extrude(height = rim_h)
                    offset(delta = -fit_gap)
                        outer_profile_2d();

            translate([0, 0, back_t + rim_h])
                linear_extrude(height = tall_h)
                    offset(delta = -tall_shrink)
                        outer_profile_2d();
        }

        translate([0, 0, back_t - 0.1])
            linear_extrude(height = rim_h + tall_h + 0.3)
                offset(delta = -inner_shell)
                    outer_profile_2d();

        // Bottom: USB area relief
        translate([0, -(board_r + tail_drop) + 0.2, back_t + 1.0])
            cube([10.0, wall + 2.0, 5.0], center = true);

        // Top (+Y): rectangular wire exit for header bundle
        translate([0, r_outer_top + wire_slot_y / 2 - 1.2, back_t + rim_h + tall_h * 0.52])
            cube([wire_slot_w, wire_slot_y, wire_slot_h], center = true);

        // Rear small pass + vent (optional, keep from V1)
        translate([0, -6, back_t + rim_h + tall_h - 0.1])
            cylinder(h = 2.0, d = 6.5);

        for (i = [-2:2]) {
            translate([i * 5.2, 8.5, back_t + rim_h + tall_h - 0.1])
                cube([3.0, 8.0, 2.0], center = true);
        }
    }
}

tall_shell_v2();
