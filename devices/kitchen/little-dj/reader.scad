r = 3;
$fn = 64;

tab_w = 12;    // tab width (Y direction)
tab_l = 8;     // tab length sticking out (X direction)
tab_h = 4;     // tab thickness
screw_r = 1.6; // M3 clearance hole

card_width = 60; // nfc card width + clearance
reader_height = 66; // sl018 nfc reader + clearance
wall_thickness = 3;

module rounded_box(x, y, z, r) {
    hull() {
        // Bottom corners (fully rounded)
        translate([r, r, r])       sphere(r=r);
        translate([x-r, r, r])     sphere(r=r);
        translate([r, y-r, r])     sphere(r=r);
        translate([x-r, y-r, r])   sphere(r=r);

        // Top corners (no rounding — flat on Z=z)
        translate([r, r, z])       cylinder(r=r, h=0.001);
        translate([x-r, r, z])     cylinder(r=r, h=0.001);
        translate([r, y-r, z])     cylinder(r=r, h=0.001);
        translate([x-r, y-r, z])   cylinder(r=r, h=0.001);
    }
}

module tab() {
    difference() {
        // Tab body flush with flat top (Z=25), sticking out in -X direction
        translate([-tab_l, (72 - tab_w) / 2, 25 - tab_h])
            cube([tab_l, tab_w, tab_h]);
        // Screw hole through tab (vertical, Z axis)
        translate([-tab_l / 2, 72 / 2, 25 - tab_h - 1])
            cylinder(r=screw_r, h=tab_h + 2);
    }
}

union() {
    difference() {
        rounded_box(card_width + 2 * wall_thickness, reader_height + 2 * wall_thickness, 25, r);

        // Top pocket (from 66x72 face, flat top)
        translate([wall_thickness, wall_thickness, 25 - 15])
            cube([card_width, reader_height, 15 + 1]);

        // Side pocket (from 66x25 face at Y=0)
        translate([wall_thickness, 0, wall_thickness])
            cube([card_width, reader_height + wall_thickness, 4]);
    }

    // Left tab sticking out from X=0
    tab();

    // Right tab sticking out from X=66
    translate([66, 0, 0]) mirror([1, 0, 0]) tab();
}
