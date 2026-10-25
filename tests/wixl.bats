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

@test "wixl - Error CustomAction" {
  cd wixl
  run "$wixl" -o out.msi custom-action-error.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  # Type 19 is msidbCustomActionTypeTextData: display Target as an error message
  [ "$(echo "$output" | tr -d '\r' | grep '^AbortInstall' | cut -f1-4)" = "$(printf 'AbortInstall\t19\t\tInstallation aborted by custom action.')" ]

  # Execute/Return/Impersonate are ignored for error actions, like WiX
  sed 's|Error="Installation aborted by custom action."|Error="Installation aborted by custom action." Execute="deferred" Return="ignore" Impersonate="no"|' custom-action-error.wxs > error-flags.wxs
  run "$wixl" -o flags.msi error-flags.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export flags.msi CustomAction
  [ "$(echo "$output" | tr -d '\r' | grep '^AbortInstall' | cut -f1-4)" = "$(printf 'AbortInstall\t19\t\tInstallation aborted by custom action.')" ]

  # WiX rejects Error combined with another action source
  sed 's|Error="Installation aborted by custom action."|Error="Installation aborted by custom action." Property="SomeProp"|' custom-action-error.wxs > error-conflict.wxs
  run "$wixl" -o conflict.msi error-conflict.wxs
  [ "$status" -ne 0 ]
}

@test "wixl - script CustomAction sourced from a Property" {
  cd wixl
  run "$wixl" -o out.msi custom-action-script-property.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  # Type 53/54: JScript/VBScript stored in the Property named by Source,
  # entry point named by Target.
  [ "$(echo "$output" | tr -d '\r' | grep '^RunVBScript' | cut -f1-4)" = "$(printf 'RunVBScript\t54\tVBScriptText\tEntry')" ]
  [ "$(echo "$output" | tr -d '\r' | grep '^RunJScript' | cut -f1-4)" = "$(printf 'RunJScript\t53\tJScriptText\tEntry')" ]
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

@test "wixl - codepage" {
  mkdir -p SourceDir
  echo "hello" > "SourceDir/hello_日本語.txt"
  cat >codepage.wxs <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test"
           Language="1033" Codepage="65001"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="F1" Source="SourceDir/hello_日本語.txt" KeyPath="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
    </Feature>
  </Product>
</Wix>
EOF
  run "$wixl" -o out.msi codepage.wxs
  [ "$status" -eq 0 ]
  test -f out.msi

  # verify the non-ASCII filename is preserved in the MSI
  run "$msiinfo" export out.msi File
  echo "$output" | grep -q "hello_日本語.txt"
}

@test "wixl - UI ext" {
  cd wixl
  run "$wixl" -o out.msi TestUI.wxs --ext ui --extdir "$SRCDIR/data/ext"
  [ "$status" -eq 0 ]
  # FIXME: add tons of tests on out.msi
  test -f out.msi
}

@test "wixl - Publish default Order" {
  cd wixl
  mkdir -p SourceDir
  echo "hello" > SourceDir/hello.txt
  cat >publish.wxs <<EOF
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
            <File Id="F1" Source="SourceDir/hello.txt" KeyPath="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
    </Feature>
    <UIRef Id="TestUI" />
  </Product>
  <Fragment>
    <UI Id="TestUI">
      <UIRef Id="WixUI_Minimal" />
      <Dialog Id="TestDlg" Width="370" Height="270" Title="Test">
        <Control Id="Next" Type="PushButton" X="236" Y="243" Width="56" Height="17" Text="Next">
          <Publish Property="A" Value="1" />
          <Publish Property="B" Value="1" />
          <Publish Property="C" Value="1" Order="5" />
          <Publish Property="D" Value="1" />
        </Control>
        <Control Id="Cancel" Type="PushButton" X="304" Y="243" Width="56" Height="17" Text="Cancel">
          <Publish Event="EndDialog" Value="Exit" />
        </Control>
      </Dialog>
      <Publish Dialog="WelcomeDlg" Control="Next" Event="NewDialog" Value="TestDlg" Order="10" />
      <Publish Dialog="TestDlg" Control="Next" Event="EndDialog" Value="Return" />
    </UI>
  </Fragment>
</Wix>
EOF
  run "$wixl" -o out.msi publish.wxs --ext ui --extdir "$SRCDIR/data/ext"
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi ControlEvent
  [ "$status" -eq 0 ]
  table=$(echo "$output" | tr -d '\r' | tail -n +4)
  echo "$table"
  orderof() { echo "$table" | awk -F '\t' -v d="$1" -v c="$2" -v e="$3" -v a="$4" \
    '$1 == d && $2 == c && $3 == e && $4 == a { print $6 }'; }

  # nested under a Control: one greater than the previous Publish of
  # that control, the next default counts on from an explicit Order
  [ "$(orderof TestDlg Next '[A]' 1)" -eq 1 ]
  [ "$(orderof TestDlg Next '[B]' 1)" -eq 2 ]
  [ "$(orderof TestDlg Next '[C]' 1)" -eq 5 ]
  [ "$(orderof TestDlg Next '[D]' 1)" -eq 6 ]
  [ "$(orderof TestDlg Cancel EndDialog Exit)" -eq 1 ]
  [ "$(orderof WelcomeDlg Next '[WixUI_InstallMode]' Update)" -eq 1 ]
  # under a UI element: always 1, an earlier Order="999" does not leak
  [ "$(orderof ExitDialog Finish EndDialog Return)" -eq 999 ]
  [ "$(orderof VerifyReadyDlg Back NewDialog MaintenanceTypeDlg)" -eq 1 ]
  [ "$(orderof WelcomeDlg Next NewDialog VerifyReadyDlg)" -eq 1 ]
  [ "$(orderof TestDlg Next EndDialog Return)" -eq 1 ]
  # an explicit Order is used as given and runs after the defaults
  [ "$(orderof WelcomeDlg Next NewDialog TestDlg)" -eq 10 ]
}

@test "wixl - Before/After next to standard actions" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/hello.txt
  cat >sequence.wxs <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <MajorUpgrade DowngradeErrorMessage="Newer version installed." />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="F1" Source="SourceDir/hello.txt" KeyPath="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
    </Feature>
    <CustomAction Id="SetA" Property="A" Value="a" />
    <CustomAction Id="SetB" Property="B" Value="b" />
    <CustomAction Id="SetC" Property="C" Value="c" />
    <CustomAction Id="SetD" Property="D" Value="d" />
    <CustomAction Id="SetE" Property="E" Value="e" />
    <CustomAction Id="SetF" Property="F" Value="f" />
    <CustomAction Id="SetG" Property="G" Value="g" />
    <CustomAction Id="SetH" Property="H" Value="h" />
    <CustomAction Id="SetI" Property="I" Value="i" />
    <CustomAction Id="SetX" Property="X" Value="x" />
    <InstallExecuteSequence>
      <Custom Action="SetA" Before="RemoveFiles" />
      <Custom Action="SetB" After="InstallFiles" />
      <Custom Action="SetC" After="InstallFiles" />
      <Custom Action="SetD" Before="SetE" />
      <Custom Action="SetE" After="WriteRegistryValues" />
      <Custom Action="SetF" Before="InstallFinalize" />
      <Custom Action="SetG" After="SetF" />
      <Custom Action="SetH" Before="LaunchConditions" />
      <Custom Action="SetI" Before="SetG" />
      <Custom Action="SetX" OnExit="success" />
    </InstallExecuteSequence>
  </Product>
</Wix>
EOF
  run "$wixl" -o out.msi sequence.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi InstallExecuteSequence
  [ "$status" -eq 0 ]
  table=$(echo "$output" | tr -d '\r' | tail -n +4 | sort -t "$(printf '\t')" -k3,3n)
  order=" $(echo "$table" | cut -f1 | tr '\n' ' ')"
  echo "$order"
  seqof() { echo "$table" | awk -F '\t' -v a="$1" '$1 == a { print $3 }'; }

  # each action sits next to its anchor, not at the start or end of the table
  [[ "$order" == *" InstallValidate RemoveExistingProducts InstallInitialize "* ]]
  [[ "$order" == *" SetA RemoveFiles "* ]]
  [[ "$order" == *" InstallFiles SetB SetC "* ]]
  [[ "$order" == *" WriteRegistryValues SetD SetE "* ]]
  [[ "$order" == *" SetF SetI SetG InstallFinalize "* ]]
  [[ "$order" == *" SetH LaunchConditions "* ]]
  [ "$(seqof RemoveExistingProducts)" -eq 1401 ]
  [ "$(seqof SetB)" -eq 4001 ]
  [ "$(seqof SetC)" -eq 4002 ]
  [ "$(seqof SetA)" -lt "$(seqof RemoveFiles)" ]
  [ "$(seqof SetF)" -lt "$(seqof InstallFinalize)" ]
  [ "$(seqof SetH)" -gt 0 ]
  [ "$(seqof SetX)" -eq -1 ]
}

@test "wixl - reproducible build with SOURCE_DATE_EPOCH" {
  cd wixl
  sed "s/<Package Id='\*'/<Package Id='11111111-2222-3333-4444-555555555555'/" SampleFirst.wxs > repro.wxs
  export SOURCE_DATE_EPOCH=1700000000
  run "$wixl" -o out1.msi repro.wxs
  [ "$status" -eq 0 ]
  # input files newer than SOURCE_DATE_EPOCH are clamped in the cabinet
  touch -t 203001010000 FoobarAppl10.exe Manual.pdf Helper.dll
  sleep 1
  run "$wixl" -o out2.msi repro.wxs
  [ "$status" -eq 0 ]
  cmp out1.msi out2.msi

  run "$msiinfo" suminfo out1.msi
  echo "$output" | grep -F "{11111111-2222-3333-4444-555555555555}"
  unset SOURCE_DATE_EPOCH
}

@test "wixl - invalid Package Id" {
  cd wixl
  for id in '{11111111-2222-3333-4444-555555555555' '11111111-2222-3333-4444-555555555555}'; do
    sed "s/<Package Id='\*'/<Package Id='$id'/" SampleFirst.wxs > invalid.wxs
    run "$wixl" -o out.msi invalid.wxs
    [ "$status" -ne 0 ]
    echo "$output" | grep -F "invalid Package Id: $id"
  done
}

@test "wixl - environment ids" {
  cd wixl
  cat >environment.wxs <<EOF
<?xml version='1.0' encoding='windows-1252'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
  <Product Name='Env test' Id='ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE' UpgradeCode='ABCDDCBA-7349-453F-94F6-BCB5110BA4FD'
    Language='1033' Codepage='1252' Version='1.0.0' Manufacturer='Test'>
    <Package Id='*' Keywords='Installer' Description='Env test' Manufacturer='Test'
      InstallerVersion='100' Languages='1033' Compressed='yes' SummaryCodepage='1252' />
    <Directory Id='TARGETDIR' Name='SourceDir'>
      <Directory Id='INSTALLDIR' Name='EnvTest'>
        <Component Id='EnvComponent' Guid='ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471'>
          <Environment Name='FOO' Value='a' Action='set' Part='all' Permanent='no' System='no' />
          <Environment Name='FOO' Value='b' Action='set' Part='all' Permanent='no' System='yes' />
          <Environment Id='ExplicitEnv' Name='BAR' Value='c' Action='set' Part='all' Permanent='no' System='no' />
          <RegistryValue Root='HKCU' Key='Software\EnvTest' Type='string' Value='' KeyPath='yes' />
        </Component>
      </Directory>
    </Directory>
    <Feature Id='Complete' Level='1'>
      <ComponentRef Id='EnvComponent' />
    </Feature>
  </Product>
</Wix>
EOF
  run "$wixl" -o out.msi environment.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export -s out.msi Environment
  [ "$status" -eq 0 ]
  [ "$(echo "$output" | grep -c '^INSERT INTO')" -eq 3 ]
  echo "$output" | grep -F "'ExplicitEnv'"
  ids=$(echo "$output" | sed -n "s/^INSERT INTO .* VALUES ('\([^']*\)'.*/\1/p" | sort -u)
  [ "$(echo "$ids" | wc -l)" -eq 3 ]
}

@test "wixl-heat - reproducible directory ids" {
  mkdir -p test/a/b test/c
  touch test/a/file test/a/b/file test/c/file
  # test/a is listed again after test/c
  printf '%s\n' test/a/file test/c/file test/a/b/file > list
  out1=$("$wixlheat" -p test/ < list)
  out2=$("$wixlheat" -p test/ < list)
  [ "$out1" = "$out2" ]
  ids=$(echo "$out1" | grep -o 'Directory Id="[^"]*"' | sort)
  [ "$(echo "$ids" | wc -l)" -eq 4 ]
  [ "$(echo "$ids" | uniq | wc -l)" -eq 4 ]
  # fragments for other component groups don't share directory ids
  out3=$("$wixlheat" -p test/ --component-group CG.other < list)
  ids3=$(echo "$out3" | grep -o 'Directory Id="[^"]*"' | sort)
  [ -z "$(comm -12 <(echo "$ids") <(echo "$ids3"))" ]
  # fragments harvested from different source roots don't share directory ids
  mkdir -p other/a
  touch other/a/file
  out4=$(printf '%s\n' other/a/file | "$wixlheat" -p other/)
  ids4=$(echo "$out4" | grep -o 'Directory Id="[^"]*"' | sort)
  [ -z "$(comm -12 <(echo "$ids") <(echo "$ids4"))" ]
}

@test "wixl - File DefaultVersion and DefaultLanguage" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/app.exe
  echo "hello" > SourceDir/readme.txt
  cat >version.wxs <<EOF2
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
            <File Id="F1" Source="SourceDir/app.exe" KeyPath="yes"
                  DefaultVersion="1.2.3.4" DefaultLanguage="1033" />
          </Component>
          <Component Id="C2" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD472">
            <File Id="F2" Source="SourceDir/readme.txt" KeyPath="yes"
                  DefaultLanguage="0" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Complete" Level="1">
      <ComponentRef Id="C1" />
      <ComponentRef Id="C2" />
    </Feature>
  </Product>
</Wix>
EOF2
  run "$wixl" -o out.msi version.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi File
  [ "$status" -eq 0 ]
  table=$(echo "$output" | tr -d '\r')
  echo "$table"
  col() { echo "$table" | awk -F '\t' -v f="$1" -v c="$2" '$1 == f { print $c }'; }
  [ "$(col F1 5)" = "1.2.3.4" ]
  [ "$(col F1 6)" = "1033" ]
  [ "$(col F2 5)" = "" ]
  [ "$(col F2 6)" = "0" ]
  [ "$(col F1 7)" -eq 512 ]
  [ "$(col F1 8)" -ge 1 ]
  [ "$(col F2 8)" -ge 1 ]

  # versioned files must not get a hash row; unversioned files still need one
  run "$msiinfo" export out.msi MsiFileHash
  [ "$status" -eq 0 ]
  hashes=$(echo "$output" | tr -d '\r')
  echo "$hashes"
  hashof() { echo "$hashes" | awk -F '\t' -v f="$1" '$1 == f { print $1 }'; }
  [ -z "$(hashof F1)" ]
  [ "$(hashof F2)" = "F2" ]

  for bad in en-US 1033abc 65536 -1; do
    sed "s/DefaultLanguage=\"1033\"/DefaultLanguage=\"$bad\"/" version.wxs > invalid.wxs
    run "$wixl" -o invalid.msi invalid.wxs
    [ "$status" -eq 1 ]
    echo "$output" | grep -q "invalid DefaultLanguage"
  done
}

@test "wixl - file version from PE version resource" {
  mkdir -p SourceDir
  cp wixl/Versioned.dll SourceDir/
  echo "hello" > SourceDir/conf.ini
  echo "hello" > SourceDir/readme.txt
  cat >version.wxs <<EOF2
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
            <File Id="Dll" Source="SourceDir/Versioned.dll" KeyPath="yes"
                  DefaultVersion="9.9.9.9" DefaultLanguage="1031" />
          </Component>
          <Component Id="C2" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD472">
            <File Id="Conf" Source="SourceDir/conf.ini" KeyPath="yes"
                  DefaultVersion="2.0.0.0" DefaultLanguage="1033" />
          </Component>
          <Component Id="C3" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD473">
            <File Id="Readme" Source="SourceDir/readme.txt" KeyPath="yes" />
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
EOF2
  run "$wixl" -o out.msi version.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi File
  [ "$status" -eq 0 ]
  echo "$output"
  field() { echo "$output" | awk -F '\t' -v f="$1" -v c="$2" '$1 == f { print $c }'; }
  # the version resource wins over DefaultVersion/DefaultLanguage
  [ "$(field Dll 5)" = "1.2.3.4" ]
  [ "$(field Dll 6)" = "1033" ]
  [ "$(field Conf 5)" = "2.0.0.0" ]
  [ "$(field Conf 6)" = "1033" ]
  [ "$(field Readme 5)" = "" ]
  [ "$(field Readme 6)" = "" ]

  # only unversioned files get a hash
  run "$msiinfo" export out.msi MsiFileHash
  [ "$status" -eq 0 ]
  hashes=$(echo "$output" | tail -n +4 | cut -f1)
  [ "$hashes" = "Readme" ]
}

@test "wixl - unsupported attributes and elements" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/hello.txt
  cat >unsupported.wxs <<EOF2
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:util="http://schemas.microsoft.com/wix/UtilExtension">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012" xml:lang="en">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471" util:Guid="nope" util:Bogus="1">
            <File Id="F1" Source="SourceDir/hello.txt" KeyPath="yes" Bogus="yes" />
            @CHILD@
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
    </Feature>
  </Product>
</Wix>
EOF2
  # an unsupported attribute is ignored with a warning
  sed 's/@CHILD@//' unsupported.wxs > attr.wxs
  run "$wixl" -o out.msi attr.wxs
  [ "$status" -eq 0 ]
  echo "$output" | grep -F "File F1: unsupported attribute Bogus, ignored"
  # an extension attribute doesn't override wixl's attribute of the same name
  echo "$output" | grep -F "Component C1: unsupported attribute util:Guid, ignored"
  echo "$output" | grep -F "Component C1: unsupported attribute util:Bogus, ignored"
  ! echo "$output" | grep -F "xml:lang"
  run "$msiinfo" export out.msi Component
  [ "$status" -eq 0 ]
  echo "$output" | grep -F "{ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471}"

  # an unsupported element is an error, not a crash
  sed 's/@CHILD@/<Bogus \/>/' unsupported.wxs > elem.wxs
  run "$wixl" -o out.msi elem.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "Component C1: unsupported element Bogus"

  # a foreign element with a known name is not a WiX element
  sed 's|@CHILD@|<util:File Id="F2" Source="SourceDir/hello.txt" KeyPath="yes" />|' \
      unsupported.wxs > ns-elem.wxs
  run "$wixl" -o out.msi ns-elem.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "Component C1: unsupported element util:File"
}
