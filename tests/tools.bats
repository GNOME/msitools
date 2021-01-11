#!/usr/bin/env bats

load common

@test "testdatabase shouldn't print fail" {
  run "$testdatabase"
  [ "$status" -eq 0 ]
  ! echo "$output" | grep ^FAIL
}

@test "testrecord shouldn't print fail" {
  run "$testrecord"
  [ "$status" -eq 0 ]
  ! echo "$output" | grep ^FAIL
}

@test "testsuminfo shouldn't print fail" {
  if [ ! -x "$testsuminfo" ]; then
     # meson 0.55 (at least), fails to parse skipped lines properly meson#7538
     # skip "testsuminfo isn't built"
     return 0
  fi
  run "$testsuminfo"
  [ "$status" -eq 0 ]
  ! echo "$output" | grep ^FAIL
}

@test "msibuild - invalid command line" {
  run "$msibuild"
  [ "$status" -eq 1 ]
  run "$msibuild" out.msi
  [ "$status" -eq 1 ]
  [ ! -f out.msi ]
}

@test "msibuild - empty MSI" {
  run "$msibuild" out.msi -s 'Project name'
  [ "$status" -eq 0 ]
  run "$msiinfo" suminfo out.msi
  [ "$status" -eq 0 ]
  echo "$output" | grep "Subject: Project name"
  echo "$output" | grep "Template: ;1033"
  echo "$output" | grep "Version: 200"
}

@test "msibuild - UUID" {
  run "$msibuild" out.msi -s 'Project name'
  [ "$status" -eq 0 ]
  run "$msiinfo" suminfo out.msi
  [ "$status" -eq 0 ]
  UUID='\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}'
  echo "$output" | grep -E "^Revision number \\(UUID\\): $UUID\$"
}

@test "msibuild - UUID does not change" {
  run "$msibuild" out.msi -s 'Project name'
  run "$msiinfo" suminfo out.msi
  exp=$(echo "$output" | grep "^Revision")
  run "$msibuild" out.msi -s 'Project name2'
  run "$msiinfo" suminfo out.msi
  echo "$output" | grep "Subject: Project name2"
  [ "$(echo "$output" | grep "^Revision")" = "$exp" ]
}

@test "msibuild - Set summary" {
  run "$msibuild" out.msi -s 'Project name'
  run "$msibuild" out.msi -s 'Project name3' 'Somebody' ';1042' '24265297-EC57-4E72-B5E9-000000000000'
  run "$msiinfo" suminfo out.msi
  echo "$output" | grep "Subject: Project name3"
  echo "$output" | grep "Author: Somebody"
  echo "$output" | grep "Template: ;1042"
  echo "$output" | grep "Revision number (UUID): 24265297-EC57-4E72-B5E9-000000000000"
}

@test "msibuild - add stream" {
  echo "This is test.txt" > test.txt
  run "$msibuild" out.msi -a Binary.testtxt test.txt
  run "$msiinfo" streams out.msi
  output=$(echo "$output" | grep -v SummaryInformation)
  [ "$output" = "Binary.testtxt" ]
  run "$msiinfo" extract out.msi Binary.testtxt
  [ "$output" = "This is test.txt" ]
}

@test "msibuild - add tables" {
  cat >tables.txt <<EOF
Name
s64
_Tables
Binary
EOF
  run "$msibuild" out.msi -i tables.txt
  run "$msiinfo" tables out.msi
  [ "$output" = "_SummaryInformation
_ForceCodepage
Binary" ]
}

@test "msibuild - add tables and definitions" {
  run "$msibuild" out.msi -i tables.txt columns.txt
  run "$msiinfo" export out.msi Icon
  exp=$(printf "Name	Data\r\ns72	v0\r\nIcon	Name\r\n")
  [ "$output" = "$exp" ]
}

@test "msibuild - seperate invocations" {
  run "$msibuild" out.msi -i tables.txt
  run "$msiinfo" tables out.msi
  exp="$output"
  run "$msibuild" out.msi -i columns.txt
  run "$msiinfo" tables out.msi
  [ "$output" = "$exp" ]
}

@test "msibuild - add table with data" {
  run "$msibuild" out.msi -i tables.txt columns.txt button.txt
  run "$msiinfo" export out.msi RadioButton
  exp=$(cat button.txt)
  [ "$output" = "$exp" ]
}

@test "msibuild - add table with stream" {
  run "$msibuild" out.msi -i tables.txt columns.txt icon.txt
  run "$msiinfo" streams out.msi
  out=$(echo "$output" | grep -v SummaryInformation)
  [ "$out" = "Icon.firefox.16.0.2.0.ico.exe" ]
  output=$("$msiinfo" extract out.msi Icon.firefox.16.0.2.0.ico.exe | sha1sum)
  exp=$(cat Icon/firefox.16.0.2.0.ico.exe | sha1sum)
  [ "$output" = "$exp" ]
}

@test "msibuild - update _SummaryInformation table" {
  run "$msibuild" -i out.msi _SummaryInformation.idt
  run "$msiinfo" suminfo out.msi
  [ "$output" = "Title: Installation Database
Subject: Acme's Foobar 1.0 Installer
Author: Acme Ltd.
Keywords: Installer
Comments: Foobar is a registered trademark of Acme Ltd.
Template: Intel;1033
Revision number (UUID): {D045A303-F114-4A3B-A01D-24FC2D2A67D7}
Created: Tue Dec 18 15:12:50 2012
Last saved: Tue Dec 18 15:12:50 2012
Version: 100 (64)
Source: 2 (2)
Restrict: 0 (0)
Application: Windows Installer XML (3.7.1119.0)
Security: 2 (2)" ]
}
