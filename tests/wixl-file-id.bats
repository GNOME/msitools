#!/usr/bin/env bats

load common

file_id_wxs() {
  cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File $1 KeyPath="yes" />
          </Component>
          <Component Id="C2" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD472">
            <File $2 KeyPath="yes" />
          </Component>
          <Component Id="C3" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD473">
            <File $3 KeyPath="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
      <ComponentRef Id="C2" />
      <ComponentRef Id="C3" />
    </Feature>
  </Product>
</Wix>
EOF
}

@test "wixl - File Id defaults to the file name" {
  mkdir -p a b
  echo "license" > a/LICENSE.txt
  echo "run" > b/run
  echo "run" > b/1-run.cmd
  file_id_wxs 'Name="License.txt" Source="a/LICENSE.txt"' \
              'Source="b/run"' 'Source="b/1-run.cmd"' > default.wxs
  run "$wixl" -o out.msi default.wxs
  [ "$status" -eq 0 ]
  [[ "$output" != *CRITICAL* ]]
  # the Name, else the Source file name, made a valid identifier
  run "$msiinfo" export out.msi File
  [ "$status" -eq 0 ]
  files=$(echo "$output" | tr -d '\r' | tail -n +4 | cut -f1-3 | LC_ALL=C sort)
  [ "$files" = "$(printf '%s\t%s\t%s\n' \
      License.txt C1 License.txt \
      _1_run.cmd C3 1-run.cmd \
      run C2 run)" ]
  run "$msiinfo" export out.msi Component
  [ "$status" -eq 0 ]
  keypaths=$(echo "$output" | tr -d '\r' | tail -n +4 | cut -f1,6 | LC_ALL=C sort)
  [ "$keypaths" = "$(printf '%s\t%s\n' C1 License.txt C2 run C3 _1_run.cmd)" ]

  # like WiX v3, the prefix depends on the name's first character, and a
  # run of dots is one '_'
  echo "run" > b/-run
  echo "run" > b/a..b.txt
  echo "run" > b/ü.txt
  file_id_wxs 'Source="b/-run"' 'Source="b/a..b.txt"' 'Source="b/ü.txt"' > sanitize.wxs
  run "$wixl" -o out.msi sanitize.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi File
  [ "$status" -eq 0 ]
  ids=$(echo "$output" | tr -d '\r' | tail -n +4 | cut -f1,2 | LC_ALL=C sort)
  [ "$ids" = "$(printf '%s\t%s\n' __.txt C3 __run C1 a_b.txt C2)" ]
}

@test "wixl - File without Id, Name or Source" {
  echo "hello" > hello.txt
  file_id_wxs '' 'Source="hello.txt"' 'Id="other" Source="hello.txt"' > none.wxs
  run "$wixl" -o out.msi none.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "File: needs an Id, a Name or a Source"
  [ ! -f out.msi ]
}

@test "wixl - duplicate File Id" {
  mkdir -p a b
  echo "a" > a/hello.txt
  echo "b" > b/hello.txt
  # two Files without an Id and the same file name
  file_id_wxs 'Source="a/hello.txt"' 'Source="b/hello.txt"' \
              'Id="other" Source="a/hello.txt"' > dup.wxs
  run "$wixl" -o out.msi dup.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "duplicate File Id: hello.txt (Source b/hello.txt)"
  [ ! -f out.msi ]

  # an explicit Id taken by a File without one
  file_id_wxs 'Source="a/hello.txt"' 'Id="hello.txt" Source="b/hello.txt"' \
              'Id="other" Source="a/hello.txt"' > dup.wxs
  run "$wixl" -o out.msi dup.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "duplicate File Id: hello.txt"
  [ ! -f out.msi ]
}

@test "wixl - empty File Id and Name" {
  mkdir -p a
  echo x > a/x.txt
  echo y > a/y.txt
  echo z > a/z.txt
  # an empty Id or Name is treated like an absent one
  file_id_wxs 'Id="" Source="a/x.txt"' 'Name="" Source="a/y.txt"' \
              'Id="" Name="named.txt" Source="a/z.txt"' > empty.wxs
  run "$wixl" -o out.msi empty.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi File
  [ "$status" -eq 0 ]
  files=$(echo "$output" | tr -d '\r' | tail -n +4 | cut -f1-3 | LC_ALL=C sort)
  [ "$files" = "$(printf '%s\t%s\t%s\n' \
      named.txt C3 named.txt \
      x.txt C1 x.txt \
      y.txt C2 y.txt)" ]

  # an explicit Id still needs a Name or a Source
  file_id_wxs 'Id="x"' 'Source="a/x.txt"' 'Source="a/y.txt"' > noname.wxs
  run "$wixl" -o none.msi noname.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "File: needs a Name or a Source"
  [ ! -f none.msi ]
}

@test "wixl - File Id longer than 72 characters" {
  mkdir -p a
  long=$(printf 'a%.0s' {1..73})
  echo x > "a/$long.txt"
  echo y > a/x.txt
  echo z > a/y.txt
  file_id_wxs "Source=\"a/$long.txt\"" 'Source="a/x.txt"' 'Source="a/y.txt"' > long.wxs
  run "$wixl" -o out.msi long.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "Id is longer than 72 characters"
  [ ! -f out.msi ]
}
