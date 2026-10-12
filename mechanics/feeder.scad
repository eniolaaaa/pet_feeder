// Pet feeder mechanics: hopper, swinging gate on an SG90, base and legs.
// All sizes in mm. Origin = servo shaft, on the top surface of the base plate.
//
// Export one part at a time, e.g.:
//   openscad -D "part=\"base\"" -o base.stl feeder.scad
// Parts: assembly (view only), servo_test, base, gate, hopper, lid, leg (print 4)

part = "assembly";
gate_angle = 0;          // assembly view: 0 = closed, 90 = open
show_hopper = true;      // assembly view

$fn = 72;
tol = 0.3;              // clearance for fits; raise if the test print is too tight

// ---------- SG90 (datasheet: body 22.2 x 11.8, 32.2 over the tabs) ----------
// Vendors differ by up to ~0.6 mm, so the cut-out uses the larger values.
sg_len            = 22.8;   // body length
sg_wid            = 12.4;   // body width
sg_tab_pitch      = 27.8;   // between the two tab screw holes (slots allow +-1.5)
sg_shaft_from_end = 5.9;    // shaft centre to the near end of the body
// Height of the horn's underside above the top of the base plate when the
// servo tabs are screwed under the plate. Measure this on the test print and
// set it here; the gate and the chamber height follow from it.
horn_bottom_z     = 4.5;

// Single-arm horn supplied with the SG90
horn_hub_d   = 7.2;
horn_tip_d   = 4.2;
horn_arm_len = 15.0;        // shaft centre to centre of the tip
horn_t       = 1.6;         // pocket depth in the gate

// ---------- Food path ----------
outlet_d   = 30;            // hopper outlet, about 3x the kibble size
R          = 36;            // servo shaft to outlet centre
plate_t    = 4;
gate_t     = 3;
gate_end_r = 17;            // paddle end radius, covers the outlet with ~2 mm overlap
gap        = 1.0;           // between gate top and hopper flange

gate_z0    = horn_bottom_z;                 // gate underside sits on the horn
chamber_h  = gate_z0 + gate_t + gap;        // hopper flange rests at this height
chamber_r  = R + gate_end_r + 4;            // swing clearance
wall       = 3;

half       = chamber_r + wall + 8;          // base is a square of 2*half
post_d     = 12;
post_pos   = half - 8;
pin_d      = 4;   pin_h = 4;               // hopper alignment pins on the posts
leg_peg_d  = 6;   leg_peg_h = plate_t + 6;  // leg peg goes up into the post
leg_d      = 14;  leg_h = 80;              // room for a bowl under the outlet

// ---------- Hopper ----------
flange_t   = 3;
hop_wall   = 2;
top_d      = 100;           // inner diameter at the top
slope      = 55;            // funnel wall angle from horizontal (> kibble angle of repose)
cone_h     = (top_d - outlet_d) / 2 * tan(slope);
cyl_h      = 70;

// ===================================================================
module rounded_square(s, r) {
    offset(r) square([s - 2*r, s - 2*r], center = true);
}

module servo_cutout_2d() {
    body_cx = sg_shaft_from_end - sg_len/2;      // body centre, shaft at x = 0
    translate([body_cx, 0])
        square([sg_len + 2*tol, sg_wid + 2*tol], center = true);
    for (s = [-1, 1])                             // pilot slots for the tab screws
        translate([body_cx + s*sg_tab_pitch/2, 0])
            hull() for (dx = [-1.5, 1.5]) translate([dx, 0]) circle(d = 1.8);
}

module corner_points() {
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx*post_pos, sy*post_pos]) children();
}

// ---------- Base: plate, gate chamber, posts ----------
module base() {
    difference() {
        union() {
            translate([0, 0, -plate_t])
                linear_extrude(plate_t) rounded_square(2*half, 6);
            // chamber wall around the gate's swing area
            linear_extrude(chamber_h) difference() {
                circle(r = chamber_r + wall);
                circle(r = chamber_r);
            }
            corner_points() {
                cylinder(d = post_d, h = chamber_h);
                translate([0, 0, chamber_h]) cylinder(d = pin_d, h = pin_h);
            }
        }
        // drop hole under the outlet
        translate([R, 0, -plate_t - 1]) cylinder(d = outlet_d + 2, h = plate_t + 2);
        // servo body and tab screw slots
        translate([0, 0, -plate_t - 1]) linear_extrude(plate_t + 2) servo_cutout_2d();
        // leg sockets, up into the posts
        corner_points() translate([0, 0, -plate_t - 0.01])
            cylinder(d = leg_peg_d + tol, h = leg_peg_h + 0.5);
    }
}

// ---------- Gate: paddle on the servo horn ----------
module gate() {
    difference() {
        linear_extrude(gate_t) hull() {
            circle(d = 14);
            translate([R + 1, 0]) circle(r = gate_end_r);
        }
        // horn pocket from below (press fit, a drop of glue if loose)
        translate([0, 0, -0.01]) linear_extrude(horn_t + 0.01) hull() {
            circle(d = horn_hub_d + tol);
            translate([horn_arm_len, 0]) circle(d = horn_tip_d + tol);
        }
        // hole for the horn screw head
        translate([0, 0, -1]) cylinder(d = 4.5, h = gate_t + 2);
    }
}

// ---------- Hopper: flange with funnel, centred on the outlet ----------
module hopper() {
    difference() {
        union() {
            linear_extrude(flange_t) rounded_square(2*half, 6);
            translate([R, 0, flange_t]) {
                cylinder(d1 = outlet_d + 2*hop_wall, d2 = top_d + 2*hop_wall, h = cone_h);
                translate([0, 0, cone_h]) cylinder(d = top_d + 2*hop_wall, h = cyl_h);
            }
        }
        translate([R, 0, -1]) cylinder(d = outlet_d, h = flange_t + 1.02);
        translate([R, 0, flange_t]) {
            cylinder(d1 = outlet_d, d2 = top_d, h = cone_h + 0.01);
            translate([0, 0, cone_h]) cylinder(d = top_d, h = cyl_h + 1);
        }
        corner_points() translate([0, 0, -1]) cylinder(d = pin_d + 2*tol, h = flange_t + 2);
    }
}

// ---------- Lid ----------
module lid() {
    cylinder(d = top_d + 2*hop_wall + 6, h = 2.5);
    translate([0, 0, 2.5]) difference() {
        cylinder(d = top_d - 2*tol, h = 8);
        translate([0, 0, -1]) cylinder(d = top_d - 2*tol - 4, h = 10);
    }
}

// ---------- Leg (print 4) ----------
module leg() {
    cylinder(d = leg_d, h = leg_h);
    translate([0, 0, leg_h]) cylinder(d = leg_peg_d, h = leg_peg_h);
}

// ---------- Servo test: plate with the servo cut-out + horn pocket coupon ----------
module servo_test() {
    difference() {
        translate([sg_shaft_from_end - sg_len/2, 0, 0])
            linear_extrude(plate_t) square([sg_tab_pitch + 14, sg_wid + 16], center = true);
        translate([0, 0, -1]) linear_extrude(plate_t + 2) servo_cutout_2d();
    }
    translate([0, sg_wid/2 + 28, 0]) difference() {
        linear_extrude(gate_t) hull() {
            circle(d = 14);
            translate([horn_arm_len + 4, 0]) circle(d = 10);
        }
        translate([0, 0, -0.01]) linear_extrude(horn_t + 0.01) hull() {
            circle(d = horn_hub_d + tol);
            translate([horn_arm_len, 0]) circle(d = horn_tip_d + tol);
        }
        translate([0, 0, -1]) cylinder(d = 4.5, h = gate_t + 2);
    }
}

// Rough SG90 shape, for the assembly view only
module sg90_dummy() {
    color("royalblue") translate([sg_shaft_from_end - sg_len/2, 0, 0]) {
        translate([0, 0, -plate_t - 16]) translate([-sg_len/2, -sg_wid/2 + 0.2, 0])
            cube([sg_len, sg_wid - 0.4, 16 + plate_t + 0.5]);
        translate([-32.2/2, -sg_wid/2 + 0.2, -plate_t - 2.5]) cube([32.2, sg_wid - 0.4, 2.5]);
    }
}

// ===================================================================
if (part == "assembly") {
    color("lightgray") base();
    sg90_dummy();
    color("orange") translate([0, 0, gate_z0]) rotate(gate_angle) gate();
    if (show_hopper) {
        color("white", 0.6) translate([0, 0, chamber_h]) hopper();
        color("white") translate([R, 0, chamber_h + flange_t + cone_h + cyl_h + 2.5 + 8]) mirror([0, 0, 1]) lid();
    }
    corner_points() color("gray") translate([0, 0, -plate_t - leg_h]) leg();
} else if (part == "base") {
    translate([0, 0, plate_t]) base();          // plate on the bed, chamber up
} else if (part == "gate") {
    mirror([0, 0, 1]) translate([0, 0, -gate_t]) gate();   // flat top on the bed
} else if (part == "hopper") {
    hopper();                                   // flange on the bed, funnel up
} else if (part == "lid") {
    lid();
} else if (part == "leg") {
    leg();
} else if (part == "servo_test") {
    servo_test();
}

echo(str("chamber_h = ", chamber_h, " mm, hopper volume ~ ",
    round((PI*pow(top_d/2,2)*cyl_h + PI*cone_h/3*(pow(top_d/2,2) + top_d/2*outlet_d/2 + pow(outlet_d/2,2))) / 1000), " ml"));
