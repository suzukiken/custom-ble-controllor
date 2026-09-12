/*
difference() {
    union() {
        cube([12, 12, 10], center=true);
        translate([0, 0, 4])
            cube([20, 20, 2], center=true);
    }
    translate([0, 0, 1])
        cube([10, 10, 8], center=true);
    translate([0, 0, -4])
        cylinder(h=2, r=2, center=true, $fn=16);
    translate([7.5, 7.5, 4])
        con();
    translate([7.5, -7.5, 4])
        con();
    translate([-7.5, 7.5, 4])
        con();
    translate([-7.5, -7.5, 4])
        con();
}

translate([0, 0, -2.5]) {
    difference() {
        cylinder(h=5, r=3, center=true, $fn=16);
        cylinder(h=5, r=2, center=true, $fn=16);
    }
}

module con() {
    cylinder(h=2, r=1.1, center=true, $fn=16);
}
*/

translate([0, 0, -1.375])
    #cylinder(h=6.75, r=1.75, center=true, $fn=16);
translate([0, 0, 1.5])
    cylinder(h=1, r=3, center=true, $fn=20);
