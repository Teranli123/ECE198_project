// ECE198: simple strap-on box for the breadboard (ESP32 build, no battery inside).
// x = along the arm, y = across the arm, z = up.
part = "both";            // "box", "lid", "assembled", "both"

bb_l = 82; bb_w = 55; bb_h = 9;   // breadboard
headroom = 25.4;                  // inside height measured from the bottom of the breadboard (~1 inch)
led_win_l = 12; led_win_w = 44;  // big LED viewing window in the lid, over the wire-channel end
hr_l = 21; hr_w = 16;             // MAX30102 breakout, faces down through the floor
hr_win_l = 13; hr_win_w = 11;     // skin window under the sensor chip
rail_h = 4;                       // breadboard sits on rails so the sensor and its wires fit underneath
wire_gap = 8.5;                   // channel along one short end wall for the sensor wires (wide enough to centre the sensor on the breadboard edge)
switch_hole = false; switch_d = 6.5;
strap_w = 30; tab = 8;

wall = 2; floor_t = 2; lid_t = 2; clr = 0.4; r = 3;
in_l = bb_l + 2*clr + wire_gap; in_w = bb_w + 2*clr;
out_l = in_l + 2*wall; out_w = in_w + 2*wall; out_h = floor_t + rail_h + headroom;
bb_x = wire_gap + clr;            // breadboard pushed to the far end, channel at the x = 0 end
strap_xs = [(strap_w + 10)/2, out_l - (strap_w + 10)/2];
$fn = 40;
echo(str("Body L x W x H: ", out_l, " x ", out_w, " x ", out_h + lid_t, "   width with strap tabs: ", out_w + 2*tab));

module rbox(l, w, h, rad) { hull() for (x = [rad, l-rad], y = [rad, w-rad]) translate([x, y, 0]) cylinder(r=rad, h=h); }

clip_w = 10; clip_drop = 8; clip_t = 1.4; hook = 0.9; win_h = 2.0; lc = 0.3;
clip_xs = [out_l*0.3, out_l*0.7];

module box() {
  difference() {
    union() {
      rbox(out_l, out_w, out_h, r);
      for (x = strap_xs, side = [0, 1]) translate([x - (strap_w + 10)/2, side ? out_w - 2 : -tab, 0]) rbox(strap_w + 10, tab + 2, 3.5, 2);
    }
    translate([wall, wall, floor_t]) rbox(in_l, in_w, out_h, 1);
    // heart rate sensor: recess + skin window centred on the breadboard's end edge (channel end) and across the width
    // (sensor turned so its 16 mm side runs along the box, straddling the breadboard's end edge)
    translate([wall + bb_x - (hr_w + 0.5)/2, wall + (in_w - hr_l - 0.5)/2, floor_t - 0.8]) cube([hr_w + 0.5, hr_l + 0.5, 2]);
    translate([wall + bb_x - hr_win_w/2, wall + (in_w - hr_win_l)/2, -1]) cube([hr_win_w, hr_win_l, 5]);
    for (x = strap_xs, side = [0, 1]) translate([x - (strap_w + 2)/2, side ? out_w + 1 : -tab + 2, -1]) cube([strap_w + 2, tab - 3, 6]);
    for (x = clip_xs, side = [0, 1])
      translate([x - (clip_w + 1)/2, side ? out_w - wall - 1 : -1, out_h - clip_drop + 0.4]) cube([clip_w + 1, wall + 2, win_h]);
  }
  // breadboard rails along both long walls (sensor wires run under the breadboard to the channel)
  for (y = [wall - 0.5, wall + in_w - 4]) translate([wall + bb_x, y, floor_t - 0.01]) cube([bb_l + clr + 0.5, 4.5, rail_h]);
  // stops that keep the breadboard out of the wire channel
  for (y = [wall - 0.5, wall + in_w - 4]) translate([wall + bb_x - 1.2, y, floor_t - 0.01]) cube([1.3, 4.5, rail_h + 4]);
}

module lid() {
  lip_h = 3; lip_t = 1.6;
  difference() {
    rbox(out_l, out_w, lid_t, r);
    // LED window centred over the breadboard's end edge at the channel end
    translate([wall + bb_x - led_win_l/2, (out_w - led_win_w)/2, -1]) rbox(led_win_l, led_win_w, lid_t + 2, 2);
    if (switch_hole) translate([out_l/2 + 10, out_w/2, -1]) cylinder(d=switch_d, h=lid_t + 2);
  }
  difference() {
    translate([wall + lc, wall + lc, lid_t]) difference() {
      rbox(in_l - 2*lc, in_w - 2*lc, lip_h, 1);
      translate([lip_t, lip_t, -1]) cube([in_l - 2*lc - 2*lip_t, in_w - 2*lc - 2*lip_t, lip_h + 2]);
    }
    for (x = clip_xs) translate([x - clip_w/2 - 1.5, -1, lid_t - 0.1]) cube([clip_w + 3, out_w + 2, lip_h + 1]);
  }
  for (x = clip_xs, side = [0, 1])
    translate([x - clip_w/2, side ? out_w - wall - lc - clip_t : wall + lc, lid_t]) clip(side);
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
