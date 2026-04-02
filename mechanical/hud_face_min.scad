// Minimal front face for Waveshare ESP32-S3 Touch LCD 1.28
// Goal: smallest possible outer size that still seats the board.
// Units: mm
//
// Export examples:
//   openscad -o hud_face_min.stl hud_face_min.scad

$fn = 96;

// ----- Board dimensions (from provided drawing) -----
board_d = 39.49;        // main circular board diameter
board_total_h = 40.36;  // overall height including lower tail
board_tail_w = 25.28;   // lower width
usb_w = 13.0;           // USB-C slot width (connector + shell tolerance)
usb_h = 5.5;            // USB-C slot height

// ----- Fit / print parameters -----
clearance = 0.55;       // board pocket — extra room near USB tail / corners
wall = 1.6;             // thin wall for compact form
front_t = 2.0;          // front plate thickness
seat_depth = 3.0;       // back pocket depth
bezel = 1.1;            // visible ring around LCD opening

// Active area from drawing: 33.40
lcd_open_d = 33.40 + 0.3; // tiny margin to avoid clipping

// Derived
board_r = board_d / 2;
tail_drop = board_total_h - board_d; // extra height below circle

module board_profile_2d(extra = 0) {
    // Approximate Waveshare "round + short tail" board shape.
    // Keep shape simple and robust for OpenSCAD.
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

module face_shell() {
    // Bottom of tail (USB end): center of cut sits slightly outward so plastic lip clears connector
    usb_y = -(board_r + tail_drop) + 1.4;
    usb_cut_depth_y = 9.0;

    difference() {
        // Main front face
        linear_extrude(height = front_t + seat_depth)
            outer_profile_2d();

        // Back pocket where board sits
        translate([0, 0, front_t])
            linear_extrude(height = seat_depth + 0.2)
                board_profile_2d(extra = clearance);

        // Front LCD window
        translate([0, 0, -0.1])
            cylinder(h = front_t + 0.3, d = lcd_open_d);

        // USB-C: cut through full shell depth so board can seat flat (no lip riding on port)
        translate([0, usb_y, (front_t + seat_depth) / 2 - 0.15])
            cube([usb_w + 1.2, usb_cut_depth_y, front_t + seat_depth + 0.6], center = true);

        // Inner corner relief where round body meets tail (both sides of USB)
        for (sx = [-1, 1]) {
            translate([sx * (board_tail_w / 2 + clearance + 0.9), -board_r + 0.8, front_t + seat_depth / 2 - 0.1])
                cube([3.2, 5.0, seat_depth + 0.5], center = true);
        }
    }
}

face_shell();

