#!/usr/bin/env bats

load common

@test "wixl - invalid command line" {
  run "$wixl"
  [ "$status" -eq 1 ]
  run "$wixl" out.msi foo.wxs
  [ "$status" -eq 1 ]
  run "$wixl" -o out.msi
  [ "$status" -eq 1 ]
  run "$wixl" -E
  [ "$status" -eq 1 ]
  run "$wixl" -D
  [ "$status" -eq 1 ]
  run "$wixl" -E -o out.msi
  [ "$status" -eq 1 ]
  run test -f out.msi
  [ "$status" -eq 1 ]
}

@test "wixl - WiX tutorial SampleFirst" {
  cd wixl
  run "$wixl" -o out.msi SampleFirst.wxs
  [ "$status" -eq 0 ]
  # FIXME: add tons of tests on out.msi
  test -f out.msi
}

@test "wixl - SampleUser" {
  cd wixl
  run "$wixl" -o out.msi SampleUser.wxs
  [ "$status" -eq 0 ]
  # FIXME: add tons of tests on out.msi
  test -f out.msi
}

@test "wixl - SampleMachine" {
  cd wixl
  run "$wixl" -o out.msi SampleMachine.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi Property
  echo "$output" | grep -q 'ALLUSERS	1'
}

@test "wixl - stable component GUIDs" {
  cd wixl
  run "$wixl" -o out.msi ComponentGUID.wxs
  run "$msiinfo" export -s out.msi Component
  echo "$output" > /tmp/out
  out=$(echo "$output" | sed 's/reg\w*'// | sort | grep INSERT)
  exp="INSERT INTO \`Component\` (\`Component\`, \`ComponentId\`, \`Directory_\`, \`Attributes\`, \`KeyPath\`) VALUES ('MainExecutable', '{0E066A5A-AE0E-5F43-B984-F6C685AF13F0}', 'INSTALLDIR', 0, 'FoobarEXE')
INSERT INTO \`Component\` (\`Component\`, \`ComponentId\`, \`Directory_\`, \`Attributes\`, \`KeyPath\`) VALUES ('Manual', '{BE256176-257D-5ACD-902D-801F8E305172}', 'INSTALLDIR', 0, 'Manual')
INSERT INTO \`Component\` (\`Component\`, \`ComponentId\`, \`Directory_\`, \`Attributes\`, \`KeyPath\`) VALUES ('ProgramMenuDir', '{F47E26A5-C6A8-57BA-B6B5-CB2AE74E5256}', 'ProgramMenuDir', 4, '')"
  [ "$out" = "$exp" ]
}

@test "wixl - WiX tutorial SampleFragment" {
  cd wixl
  run "$wixl" -o out.msi SampleFragment.wxs Manual.wxs
  test -f out.msi
}

@test "wixl - preprocessor variables" {
  cd wixl
  export MY_VAR="Hello!"
  cat >variables.wxs <<EOF
<?xml version="1.0"?>
<?define Version = "0.2.0"?>
<?define UpgradeCode = "ABCDDCBA-8392-0202-1993-199374829923"?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
   <Property Id="Id0" Value="\$(var.UpgradeCode)"/>
   <Property Id="Id0.1" Value="$\$(var.UpgradeCode)"/>
   <Property Id="Id0.2" Value="$$\$(var.UpgradeCode)"/>
   <?define UpgradeCode = "ABCDDCBA-8392-0202-1993-199374829924"?>
   <Property Id="Id2" Value="\$(var.UpgradeCode)"/>
   <Property Id="Id3" Value="\$(var.Version)"/>
   <?define A = "A"?><?define B = "B"?>
   <Property Id="IdAB" Value="\$(var.A)\$(var.B)"/>
   <Property Id="IdHello" Value="\$(env.MY_VAR)"/>
   <Property Id="IdSys" Value="(\$(sys.SOURCEFILEDIR))foo"/>
</Wix>
EOF
  run "$wixl" -E variables.wxs

  read -d '' exp <<EOF || true
<?xml version="1.0"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Property Id="Id0" Value="ABCDDCBA-8392-0202-1993-199374829923"/>
  <Property Id="Id0.1" Value="\$ABCDDCBA-8392-0202-1993-199374829923"/>
  <Property Id="Id0.2" Value="$$ABCDDCBA-8392-0202-1993-199374829923"/>
  <Property Id="Id2" Value="ABCDDCBA-8392-0202-1993-199374829924"/>
  <Property Id="Id3" Value="0.2.0"/>
  <Property Id="IdAB" Value="AB"/>
  <Property Id="IdHello" Value="Hello!"/>
  <Property Id="IdSys" Value="(variables.wxs)foo"/>
</Wix>
EOF
  [ "$output" = "$exp" ]

  cat >variables.wxs <<EOF
<?xml version="1.0"?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
  <Property Id="Id\$(var.Foo)" Value="\$(var.Foo)"/>
  <Property Id="Id\$(var.Zig)" Value="\$(var.Zig)"/>
</Wix>
EOF
  run "$wixl" -E variables.wxs -D Foo -D Zig=Zag
  read -d '' exp <<EOF || true
<?xml version="1.0"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Property Id="Id1" Value="1"/>
  <Property Id="IdZag" Value="Zag"/>
</Wix>
EOF
  [ "$output" = "$exp" ]
}

@test "wixl - preprocessor include & condition" {
  cd wixl
  run "$wixl" -o out.msi IncludeTest.wxs
  [ "$output" = "IncludeWarn.wxi:3: warning: IncludeWarn is included" ]
  run "$wixl" -D Bar -o out.msi IncludeTest.wxs
  [ "$output" = "IncludeTest.wxs:11: warning: Bar" ]
  run "$wixl" -D Foo -o out.msi IncludeTest.wxs
  [ "$output" = "IncludeTest.wxs:4: warning: Foo" ]
  run "$wixl" -o out.msi arch-condition.wxs
  [ "$output" = "arch-condition.wxs:12: warning: no" ]
  test -f out.msi
}

@test "wixl - system include directory" {
  cd wixl
  cat >include.wxs <<EOF
<?xml version='1.0' encoding='windows-1252'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
  <?include zlib.wxi ?>
</Wix>
EOF
  run "$wixl" --wxidir "$SRCDIR/data/wixl" -E include.wxs -D SourceDir=foo -D Win64=no
  echo "$output" | grep -F zlib1.dll
}

@test "wixl - ARP example" {
  cd wixl
  run "$wixl" -o out.msi test-arp.wxs
  test -f out.msi
}

@test "wixl - Binary/CustomAction" {
  cd wixl
  run "$wixl" -o out.msi binary.wxs
  test -f out.msi
}

@test "wixl-heat - simple" {
  mkdir -p test/a/b test/c
  touch test/a/file test/c/file
  find test > list
  out=$(cat list | "$wixlheat" -p test/ | grep File | sort)
  [ "$out" = '          <File Id="fil18D0F9984B0565992BE4B64E573B4237" KeyPath="yes" Source="SourceDir/a/file"/>
          <File Id="filD6217F3B9CF0F6E4697D603E4E611F1C" KeyPath="yes" Source="SourceDir/c/file"/>' ]
  out=$(cat list | "$wixlheat" -p test/ -x c | grep File | sort)
  [ "$out" = '          <File Id="fil18D0F9984B0565992BE4B64E573B4237" KeyPath="yes" Source="SourceDir/a/file"/>' ]
}

@test "wixl - XML error" {
  cd wixl

  echo " " > test.wxs
  run "$wixl" test.wxs
  [ "$status" -eq 1 ]
}

@test "wixl - UI ext" {
  cd wixl
  run "$wixl" -o out.msi TestUI.wxs --ext ui --extdir "$SRCDIR/data/ext"
  [ "$status" -eq 0 ]
  # FIXME: add tons of tests on out.msi
  test -f out.msi
}
