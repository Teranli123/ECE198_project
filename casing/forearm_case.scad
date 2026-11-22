// ECE198 physical therapy aid: strap-on casing
// Arduino UNO R4 Minima + breadboard + 9V battery (lying on the breadboard)
// + MAX3010x heart rate sensor (faces skin) + GY-61 accelerometer (both under the Uno)
// + RGB LED + toggle switch. Two 30 mm lanyard strap slots, snap-fit lid, no screws.
//
// x = along the arm, y = across the arm, z = up (floor sits on the skin).
// Export:  openscad -D 'breadboard=true' -D 'part="box"' -o box.stl forearm_case.scad

/* ---------- choices ---------- */
breadboard = true;      // false = no breadboard, battery lies on the floor
part       = "both";    // "box", "lid", "assembled" or "both" (print layout)

/* ---------- part sizes (mm) ---------- */
// Uno R4 Minima: 68.85 x 53.34 (Core Electronics spec). Holes = standard Uno pattern.
uno_l = 68.85; uno_w = 53.34; pcb_t = 1.6;
uno_holes = [[13.97, 2.54], [15.24, 50.8], [66.04, 7.62], [66.04, 35.56]];
usb_y = 38.0; usb_open = 16;            // USB-C centre from the Uno's lower edge (opening is wide to allow for tolerance)
hdr_h = 8.5;                            // Uno female headers
bb_l = 84;  bb_w = 56;  bb_h = 10;      // Eddie's breadboard
bat_l = 44.8; bat_w = 26.2; bat_h = 17; // Eddie's 9V, without the connector
bat_clip = 10;                          // room for the snap connector + wires
hr_l = 21;  hr_w = 16;                  // MAX30102 breakout (21 x 16 per module spec)
hr_win_l = 13; hr_win_w = 11;           // skin window under the 5.6 x 3.3 sensor chip
acc_l = 21; acc_w = 15;                 // GY-61 ADXL335 (21 x 15 x 3 without header)
led_d = 5.2;  switch_d = 6.5;
strap_w = 30;

/* ---------- case settings ---------- */
wall = 2.0;  floor_t = 2.0;  lid_t = 2.0;  clr = 0.3;  r = 3;
max_h = 35;
inner_h = max_h - floor_t - lid_t;      // 31
standoff_h = 6.5;                       // sensors fit under the Uno (solder wires flat, no tall headers)
gap = 3;
tab = 8;
$fn = 40;

/* ---------- layout ---------- */
in_w = max(uno_w, breadboard ? bb_w : 0, bat_l + bat_clip) + 2*clr;
uno_x = clr;  uno_y = (in_w - uno_w)/2;
bb_x = uno_x + uno_l + gap;
in_l = breadboard ? bb_x + bb_l + clr : uno_x + uno_l + gap + bat_w + clr + 1.2;
bat_x = in_l - clr - bat_w;                              // battery at the far end, lying across the arm
bat_y = in_w - clr - bat_l;                              // body against the far wall, connector toward the near wall
bat_z = floor_t + (breadboard ? bb_h : 0);
out_l = in_l + 2*wall; out_w = in_w + 2*wall; out_h = floor_t + inner_h;
uno_top = floor_t + standoff_h + pcb_t;

// sensors under the Uno, between its mounting holes
hr_pos  = [uno_x + 22, uno_y + 20];
acc_pos = [uno_x + 44, uno_y + 12];

strap_xs = [(strap_w + 10)/2 + 2, out_l - (strap_w + 10)/2 - 2];   // centres of the two strap pairs
echo(str("Body L x W x H: ", out_l, " x ", out_w, " x ", out_h + lid_t, "   width with strap tabs: ", out_w + 2*tab));
echo(str("Room above Uno headers: ", out_h - uno_top - hdr_h, "   above battery: ", out_h - bat_z - bat_h,
         "   under Uno for sensors: ", standoff_h + 0.8));

module rbox(l, w, h, rad) { hull() for (x = [rad, l-rad], y = [rad, w-rad]) translate([x, y, 0]) cylinder(r=rad, h=h); }

/* ---------- snap clips ---------- */
clip_w = 10; clip_drop = 8; clip_t = 1.4; hook = 0.9; win_h = 2.0; lc = 0.3;
side_clip_xs = breadboard ? [wall + bb_x + 12, wall + bb_x + 46] : [wall + uno_x + 34, wall + bat_x + bat_w/2];
end_clip_y = wall + uno_y + 22;      // USB end wall, between barrel jack and USB-C

/* ================= BOX ================= */
module box() {
  difference() {
    union() {
      rbox(out_l, out_w, out_h, r);
      // strap tabs on both long sides, one pair near each end; the strap wraps around the arm
      for (x = strap_xs, side = [0, 1]) translate([x - (strap_w + 10)/2, side ? out_w - 2 : -tab, 0]) rbox(strap_w + 10, tab + 2, 3.5, 2);
    }
    translate([wall, wall, floor_t]) rbox(in_l, in_w, inner_h + 1, max(r - wall, 0.5));
    for (x = strap_xs, side = [0, 1]) translate([x - (strap_w + 2)/2, side ? out_w + 1 : -tab + 2, -1]) cube([strap_w + 2, tab - 3, 6]);
    // USB-C
    translate([-1, wall + uno_y + usb_y - usb_open/2, uno_top - 2.5]) cube([wall + 2, usb_open, 10]);
    // clip windows
    for (x = side_clip_xs, side = [0, 1])
      translate([x - (clip_w + 1)/2, side ? out_w - wall - 1 : -1, out_h - clip_drop + 0.4]) cube([clip_w + 1, wall + 2, win_h]);
    translate([-1, end_clip_y - (clip_w + 1)/2, out_h - clip_drop + 0.4]) cube([wall + 2, clip_w + 1, win_h]);
    // heart rate sensor recess + skin window, accelerometer recess
    translate([wall + hr_pos[0], wall + hr_pos[1], floor_t - 0.8]) cube([hr_l + 0.5, hr_w + 0.5, 2]);
    translate([wall + hr_pos[0] + (hr_l + 0.5 - hr_win_l)/2, wall + hr_pos[1] + (hr_w + 0.5 - hr_win_w)/2, -1]) cube([hr_win_l, hr_win_w, 5]);
    translate([wall + acc_pos[0], wall + acc_pos[1], floor_t - 0.8]) cube([acc_l + 0.5, acc_w + 0.5, 2]);
  }
  translate([wall + uno_x, wall + uno_y, floor_t]) for (h = uno_holes) translate([h[0], h[1], 0]) snap_peg();
  if (breadboard) // low end stops so the breadboard can't slide into the Uno
    for (y = [4, in_w - 8]) translate([wall + bb_x - 1.5, wall + y, floor_t]) cube([1.6, 4, 4]);
}

module snap_peg() {
  difference() {
    union() {
      cylinder(d=6, h=standoff_h);
      cylinder(d=2.8, h=standoff_h + pcb_t + 0.2);
      translate([0, 0, standoff_h + pcb_t + 0.2]) cylinder(d1=3.5, d2=2.4, h=1.4);
    }
    translate([-0.45, -3, standoff_h + 0.3]) cube([0.9, 6, 5]);
  }
}

/* ================= LID (plate down, as printed) ================= */
module lid() {
  lip_h = 3; lip_t = 1.6;
  drop = out_h - bat_z - bat_h;           // lid underside to battery top
  difference() {
    rbox(out_l, out_w, lid_t, r);
    translate([wall + uno_x + 24, out_w/2, -1]) cylinder(d=led_d, h=lid_t + 2);
    translate([wall + uno_x + 46, out_w/2, -1]) cylinder(d=switch_d, h=lid_t + 2);
  }
  difference() {
    translate([wall + lc, wall + lc, lid_t]) difference() {
      rbox(in_l - 2*lc, in_w - 2*lc, lip_h, max(r - wall, 0.5));
      translate([lip_t, lip_t, -1]) cube([in_l - 2*lc - 2*lip_t, in_w - 2*lc - 2*lip_t, lip_h + 2]);
    }
    for (x = side_clip_xs) translate([x - clip_w/2 - 1.5, -1, lid_t - 0.1]) cube([clip_w + 3, out_w + 2, lip_h + 1]);
    translate([-1, out_w - end_clip_y - clip_w/2 - 1.5, lid_t - 0.1]) cube([wall + 4, clip_w + 3, lip_h + 1]);
  }
  for (x = side_clip_xs, side = [0, 1])
    translate([x - clip_w/2, side ? out_w - wall - lc - clip_t : wall + lc, lid_t]) clip(side);
  // USB end clip (lid is mirrored in y when flipped onto the box)
  translate([wall + lc, out_w - end_clip_y + clip_w/2, lid_t]) rotate([0, 0, -90]) clip(0);
  // battery keeper: frame around the battery top + press pads (mirrored in y)
  bx = wall + bat_x; by = out_w - (wall + bat_y + bat_l);
  intersection() {
    translate([bx - 1.6, by - 1.6, lid_t]) difference() {
      cube([bat_w + 3.2, bat_l + 3.2, drop + 2.5]);
      translate([1.3, 1.3, -1]) cube([bat_w + 0.6, bat_l + 5, drop + 5]);  // open on the connector side
    }
    translate([wall + lc + 0.5, wall + lc + 0.5, 0]) cube([in_l - 2*lc - 1, in_w - 2*lc - 1, 50]);
  }
  for (px = [0.25, 0.75], py = [0.2, 0.8])
    translate([bx + bat_w*px, by + bat_l*py, lid_t]) cylinder(d=5, h=drop - 0.2);
}

module clip(side) {
  cube([clip_w, clip_t, clip_drop]);
  translate([0, side ? clip_t : 0, clip_drop - 2.2]) hull() {
    translate([0, side ? 0 : -hook, 0]) cube([clip_w, hook, 1.2]);
    translate([0, 0, 2.2 - 0.01]) cube([clip_w, 0.01, 0.01]);
  }
}

if (part == "box") box();
else if (part == "lid") lid();
else if (part == "assembled") { box(); translate([0, out_w, out_h + lid_t]) rotate([180, 0, 0]) lid(); }
else { box(); translate([0, out_w + 15, 0]) lid(); }
