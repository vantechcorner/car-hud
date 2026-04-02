// Tall back cover V3 for hud_face_min
// Fixes:
// - Increase cavity size so ESP32-S3 round board fits
// - Add a much larger USB-C cutout at bottom (so no manual trimming)
// - Keep TOP (+Y) wire slot for header bundle
// Units: mm

$fn = 96;

// Shared geometry (must match hud_face_min.scad)
board_d = 39.49;
board_total_h = 40.36;
board_tail_w = 25.28;
wall = 1.6;

// Front face fit (into hud_face_min)
back_t = 1.8;
rim_h = 4.0;

// Roominess controls
clearance = 0.65;     // outer shell around the board
fit_gap = 0.70;       // loosen rim insert into printed face
tall_h = 16.0;
tall_shrink = 0.30;  // less shrink = more cavity space

// Internal void thickness (reduce to increase cavity)
inner_shell = wall - 0.20;

// Board coords
board_r = board_d / 2;
tail_drop = board_total_h - board_d;

// USB-C cutout parameters (matches hud_face_min values + extra margin)
usb_w = 13.0;         // width in X direction
usb_h = 5.5;          // height in Z direction (for the slot)
usb_cut_y = -(board_r + tail_drop) + 2.0;
usb_cut_x = usb_w + 4.0;
usb_cut_y_len = 12.0; // open more towards Y
usb_cut_z_len = 10.0; // open deeper into thickness

// Top wire slot (+Y)
wire_slot_w = 24.0;
wire_slot_h = 10.0;
wire_slot_y = 14.0;
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

module tall_shell_v3() {
    difference() {
        union() {
            // Main cover plate
            linear_extrude(height = back_t)
                outer_profile_2d();

            // Insert rim into hud_face_min
            translate([0, 0, back_t])
                linear_extrude(height = rim_h)
                    offset(delta = -(fit_gap + 0.15))
                        outer_profile_2d();

            // Rear extension (battery cavity cap)
            translate([0, 0, back_t + rim_h])
                linear_extrude(height = tall_h)
                    offset(delta = -tall_shrink)
                        outer_profile_2d();
        }

        // Hollow inner volume (bigger cavity)
        translate([0, 0, back_t - 0.1])
            linear_extrude(height = rim_h + tall_h + 0.3)
                offset(delta = -inner_shell)
                    outer_profile_2d();

        // --- Large USB-C cutout at bottom ---
        // Carve a rectangular trench through the shell depth around the USB connector.
        translate([0, usb_cut_y, back_t + rim_h * 0.25])
            cube([usb_cut_x, usb_cut_y_len, usb_cut_z_len], center = true);

        // Small chamfer relief near USB corners (helps the angled connector)
        for (sx = [-1, 1]) {
            translate([sx * (usb_cut_x / 2 - 3.0), usb_cut_y + sx * 0.5, back_t + rim_h * 0.25])
                rotate([0, 0, sx * 18])
                    cube([6.0, 6.0, usb_cut_z_len], center = true);
        }

        // --- TOP (+Y) wire slot for header bundle ---
        translate([0, r_outer_top + wire_slot_y / 2 - 1.2, back_t + rim_h + tall_h * 0.55])
            cube([wire_slot_w, wire_slot_y, wire_slot_h], center = true);

        // Rear small pass + vent (optional)
        translate([0, -6, back_t + rim_h + tall_h - 0.1])
            cylinder(h = 2.0, d = 6.5);

        // Vents (simple)
        for (i = [-2:2]) {
            translate([i * 5.2, 8.5, back_t + rim_h + tall_h - 0.1])
                cube([3.0, 8.0, 2.0], center = true);
        }
    }
}

tall_shell_v3();

