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

@test "wixl - ServiceConfig" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/svc.exe
  cat >serviceconfig.wxs <<EOF2
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="500" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="F1" Source="SourceDir/svc.exe" KeyPath="yes" />
            <ServiceInstall Id="S1" Name="my-svc" Type="ownProcess" Start="auto" ErrorControl="normal">
              <ServiceConfig DelayedAutoStart="yes" FailureActionsWhen="failedToStopOrReturnedError"
                             OnInstall="yes" OnReinstall="yes" />
            </ServiceInstall>
            <ServiceConfig Id="Other" ServiceName="other-svc" PreShutdownDelay="60000"
                           ServiceSid="restricted" OnUninstall="yes" />
            <ServiceConfig Id="Zeros" ServiceName="zeros-svc" DelayedAutoStart="no"
                           FailureActionsWhen="failedToStop" ServiceSid="none" OnInstall="yes" />
            <ServiceConfig Id="Prop" ServiceName="prop-svc" PreShutdownDelay="[DELAY]"
                           OnReinstall="yes" />
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
  run "$wixl" -o out.msi serviceconfig.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi MsiServiceConfig
  [ "$status" -eq 0 ]
  rows=$(echo "$output" | tr -d '\r' | tail -n +4 | LC_ALL=C sort)
  [ "$rows" = "$(printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
      Other.PD other-svc 2 7 60000 C1 \
      Other.SS other-svc 2 5 3 C1 \
      Prop.PD prop-svc 4 7 [DELAY] C1 \
      Zeros.DS zeros-svc 1 3 0 C1 \
      Zeros.FA zeros-svc 1 4 0 C1 \
      Zeros.SS zeros-svc 1 5 0 C1 \
      my_svc.DS my-svc 5 3 1 C1 \
      my_svc.FA my-svc 5 4 1 C1)" ]

  run "$msiinfo" export out.msi InstallExecuteSequence
  [ "$status" -eq 0 ]
  echo "$output" | tr -d '\r' | grep -q "$(printf 'MsiConfigureServices\tVersionNT>=600\t5850')"

  # WiX requires an event and a setting
  sed 's/ OnUninstall="yes"//' serviceconfig.wxs > noevent.wxs
  run "$wixl" -o out.msi noevent.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "OnInstall, OnReinstall or OnUninstall is required"

  # an invalid yes/no value is an error, not silently treated as no
  sed 's/OnUninstall="yes"/OnUninstall="maybe"/' serviceconfig.wxs > badyesno.wxs
  run "$wixl" -o out.msi badyesno.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "OnUninstall must be yes or no, not 'maybe'"

  # the same setting twice for one service needs distinct ids
  sed 's/Id="Other" ServiceName="other-svc" PreShutdownDelay="60000"/ServiceName="my-svc" DelayedAutoStart="no"/' serviceconfig.wxs > dup.wxs
  run "$wixl" -o out.msi dup.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "duplicate MsiServiceConfig row my_svc.DS"

  # ids are made from the name like WiX does
  sed 's/Id="Other" ServiceName="other-svc"/ServiceName="1a..b"/' serviceconfig.wxs > name.wxs
  run "$wixl" -o out.msi name.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi MsiServiceConfig
  echo "$output" | tr -d '\r' | grep -q "^_1a_b\.PD"
}

@test "wixl - util:ServiceConfig" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/svc.exe
  cat >utilsc.wxs <<'EOF2'
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:util="http://schemas.microsoft.com/wix/UtilExtension">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="F1" Source="SourceDir/svc.exe" KeyPath="yes" />
            <ServiceInstall Id="S1" Name="my svc" Type="ownProcess" Start="auto" ErrorControl="normal">
              <util:ServiceConfig FirstFailureActionType="restart" SecondFailureActionType="restart"
                                  ThirdFailureActionType="none" ResetPeriodInDays="1"
                                  RestartServiceDelayInSeconds="60" />
            </ServiceInstall>
            <util:ServiceConfig ServiceName="other" FirstFailureActionType="runCommand"
                                SecondFailureActionType="reboot" ThirdFailureActionType="restart"
                                ProgramCommandLine='"[INSTALLDIR]svc.exe" --recover' RebootMessage="bye [x]" />
            <util:ServiceConfig ServiceName="other" FirstFailureActionType="restart"
                                ProgramCommandLine="[INSTALLDIR]" />
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
  run "$wixl" -o out.msi utilsc.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi CustomAction
  [ "$status" -eq 0 ]
  ca=$(echo "$output" | tr -d '\r')
  # 3122 = exe from property (50), deferred, no impersonation
  echo "$ca" | grep -qxF "$(printf 'UtilServiceConfig_my_svc\t3122\tWixlScExe\tfailure "my svc" reset= 86400 actions= restart/60000/restart/60000\t')"
  echo "$ca" | grep -qxF "$(printf 'UtilServiceConfig_other\t3122\tWixlScExe\tfailure "other" reset= 0 actions= run/0/reboot/0/restart/0 command= "\\"[INSTALLDIR]svc.exe\\" --recover" reboot= "bye [\\[]x[\\]]"\t')"
  # a value ending with a property gets a space, so a trailing backslash can't escape the quote
  echo "$ca" | grep -qxF "$(printf 'UtilServiceConfig_other_2\t3122\tWixlScExe\tfailure "other" reset= 0 actions= restart/0 command= "[INSTALLDIR] "\t')"
  echo "$ca" | grep -qxF "$(printf 'WixlSetScExe\t51\tWixlScExe\t[SystemFolder]sc.exe\t')"

  run "$msiinfo" export out.msi InstallExecuteSequence
  [ "$status" -eq 0 ]
  seq=$(echo "$output" | tr -d '\r')
  seqof() { echo "$seq" | awk -F '\t' -v a="$1" '$1 == a { print $3 }'; }
  condof() { echo "$seq" | awk -F '\t' -v a="$1" '$1 == a { print $2 }'; }
  [ "$(condof UtilServiceConfig_my_svc)" = '$C1>2' ]
  # the default condition of InstallServices is kept
  [ "$(condof InstallServices)" = 'VersionNT' ]
  [ "$(seqof UtilServiceConfig_my_svc)" -gt "$(seqof InstallServices)" ]
  [ "$(seqof UtilServiceConfig_my_svc)" -lt "$(seqof InstallFinalize)" ]
  [ "$(seqof WixlSetScExe)" -gt "$(seqof CostFinalize)" ]
  [ "$(seqof WixlSetScExe)" -lt "$(seqof InstallInitialize)" ]

  # one action per util:ServiceConfig
  [ "$(echo "$ca" | grep -c '^UtilServiceConfig_')" -eq 3 ]

  # a none action between two others can't be expressed for sc.exe
  sed 's/ThirdFailureActionType="none"/ThirdFailureActionType="restart"/; s/SecondFailureActionType="restart"/SecondFailureActionType="none"/' utilsc.wxs > gap.wxs
  run "$wixl" -o out.msi gap.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "a none failure action before another action is not supported"

  # numbers must fit sc.exe's milliseconds
  sed 's/RestartServiceDelayInSeconds="60"/RestartServiceDelayInSeconds="3000000"/' utilsc.wxs > big.wxs
  run "$wixl" -o out.msi big.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "invalid RestartServiceDelayInSeconds 3000000"

  # WiX's custom action compares the failure action types case-insensitively
  sed 's/FirstFailureActionType="restart"/FirstFailureActionType="Restart"/' utilsc.wxs > case.wxs
  run "$wixl" -o out.msi case.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  echo "$output" | tr -d '\r' | grep -qF 'actions= restart/60000/restart/60000'

  # an unknown failure action type is still an error
  sed 's/FirstFailureActionType="restart"/FirstFailureActionType="bogus"/' utilsc.wxs > badtype.wxs
  run "$wixl" -o out.msi badtype.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "invalid failure action type bogus"
}

@test "wixl - util:ServiceConfig without ServiceInstall" {
  cat >utilsc2.wxs <<'EOF2'
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:util="http://schemas.microsoft.com/wix/UtilExtension">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Property Id="SVCNAME" Value="Spooler" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
        <RegistryValue Root="HKLM" Key="Software\Test" Name="v" Value="1" Type="string" KeyPath="yes" />
        <util:ServiceConfig ServiceName="[SVCNAME]" FirstFailureActionType="runCommand"
                            ProgramCommandLine='C:\a b\run.exe "C:\dir\\"' />
      </Component>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
    </Feature>
  </Product>
</Wix>
EOF2
  run "$wixl" -o out.msi utilsc2.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi CustomAction
  [ "$status" -eq 0 ]
  ca=$(echo "$output" | tr -d '\r')
  # no space after the formatted service name, and the backslashes before
  # a quote and at the end are doubled
  echo "$ca" | grep -F 'failure "[SVCNAME]" reset= 0 actions= run/0 command= "C:\a b\run.exe \"C:\dir\\\\\""'

  # inside the install script, without adding InstallFiles or InstallServices
  run "$msiinfo" export out.msi InstallExecuteSequence
  [ "$status" -eq 0 ]
  seq=$(echo "$output" | tr -d '\r')
  seqof() { echo "$seq" | awk -F '\t' -v a="$1" '$1 == a { print $3 }'; }
  [ "$(seqof UtilServiceConfig__SVCNAME_)" -gt "$(seqof InstallInitialize)" ]
  [ "$(seqof UtilServiceConfig__SVCNAME_)" -lt "$(seqof InstallFinalize)" ]
  [ -z "$(seqof InstallFiles)" ]
  [ -z "$(seqof InstallServices)" ]
}
@test "wixl - fw:FirewallException" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/app.exe
  cat >fw.wxs <<'EOF2'
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:fw="http://schemas.microsoft.com/wix/FirewallExtension">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="F1" Source="SourceDir/app.exe" KeyPath="yes">
              <fw:FirewallException Id="App" Name="My App" Scope="localSubnet" Profile="private"
                                    fw:Description="a \ b [x]" xml:lang="en" />
            </File>
          </Component>
          <Component Id="C2" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD472">
            <CreateFolder />
            <fw:FirewallException Id="Port" Name="[ProductName] port" Port="[LISTEN_PORT]" IgnoreFailure="yes">
              <fw:RemoteAddress>10.0.0.0/8</fw:RemoteAddress>
              <fw:RemoteAddress>LocalSubnet</fw:RemoteAddress>
              <fw:RemoteAddress>*</fw:RemoteAddress>
            </fw:FirewallException>
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
      <ComponentRef Id="C2" />
    </Feature>
  </Product>
</Wix>
EOF2
  run "$wixl" -o out.msi fw.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi CustomAction
  [ "$status" -eq 0 ]
  ca=$(echo "$output" | tr -d '\r')
  row() { printf '%s\t%s\tWixlNetshExe\t%s\t' "$1" "$2" "$3"; }
  # 3122 = exe from property (50), deferred, no impersonation; +64 ignore
  # the exit code; +256 rollback
  del='advfirewall firewall delete rule name="My App" dir=in'
  add='advfirewall firewall add rule name="My App" dir=in program="[#F1]" action=allow enable=yes profile=private remoteip="localsubnet" edge=yes description="a \ b [\[]x[\]]"'
  echo "$ca" | grep -qxF "$(row Fw_App.RestoreRollback 3442 "$add")"
  echo "$ca" | grep -qxF "$(row Fw_App.RemoveBeforeAdd 3186 "$del")"
  echo "$ca" | grep -qxF "$(row Fw_App.AddRollback 3442 "$del")"
  echo "$ca" | grep -qxF "$(row Fw_App.Add 3122 "$add")"
  echo "$ca" | grep -qxF "$(row Fw_App.RemoveRollback 3442 "$add")"
  echo "$ca" | grep -qxF "$(row Fw_App.Remove 3186 "$del")"
  # a port rule, TCP by default, with IgnoreFailure
  echo "$ca" | grep -qxF "$(row Fw_Port.Add 3186 'advfirewall firewall add rule name="[ProductName] port" dir=in protocol=tcp localport="[LISTEN_PORT]" action=allow enable=yes profile=any remoteip="any"')"
  # without *, the remote addresses are passed as a list
  sed '/<fw:RemoteAddress>\*<\/fw:RemoteAddress>/d' fw.wxs > noany.wxs
  run "$wixl" -o out.msi noany.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  echo "$output" | tr -d '\r' | grep -qF 'remoteip="10.0.0.0/8,LocalSubnet"'
  # a port from a property may differ when the rule is removed, so
  # deleting doesn't use it
  echo "$ca" | grep -qxF "$(row Fw_Port.Remove 3186 'advfirewall firewall delete rule name="[ProductName] port" dir=in')"
  echo "$ca" | grep -qxF "$(printf 'WixlSetNetshExe\t51\tWixlNetshExe\t[SystemFolder]netsh.exe\t')"
  [ "$(echo "$ca" | grep -c '^Fw_')" -eq 12 ]

  run "$msiinfo" export out.msi InstallExecuteSequence
  [ "$status" -eq 0 ]
  seq=$(echo "$output" | tr -d '\r')
  seqof() { echo "$seq" | awk -F '\t' -v a="$1" '$1 == a { print $3 }'; }
  condof() { echo "$seq" | awk -F '\t' -v a="$1" '$1 == a { print $2 }'; }
  [ "$(condof Fw_App.Add)" = '$C1>2' ]
  [ "$(condof Fw_App.Remove)" = '$C1=2' ]
  [ "$(condof Fw_App.RestoreRollback)" = '$C1>2 AND ?C1>2' ]
  [ "$(condof Fw_Port.Add)" = '$C2>2' ]
  [ "$(seqof Fw_App.RestoreRollback)" -gt "$(seqof InstallFiles)" ]
  [ "$(seqof Fw_App.RemoveBeforeAdd)" -gt "$(seqof Fw_App.RestoreRollback)" ]
  [ "$(seqof Fw_App.AddRollback)" -gt "$(seqof Fw_App.RemoveBeforeAdd)" ]
  [ "$(seqof Fw_App.Add)" -gt "$(seqof Fw_App.AddRollback)" ]
  # all rules are deleted before any is added, as they may share a name
  [ "$(seqof Fw_App.AddRollback)" -gt "$(seqof Fw_Port.RemoveBeforeAdd)" ]
  [ "$(seqof Fw_Port.AddRollback)" -gt "$(seqof Fw_App.RemoveBeforeAdd)" ]
  [ "$(seqof Fw_App.Add)" -lt "$(seqof InstallFinalize)" ]
  [ "$(seqof Fw_App.RemoveRollback)" -gt "$(seqof InstallInitialize)" ]
  [ "$(seqof Fw_App.Remove)" -gt "$(seqof Fw_App.RemoveRollback)" ]
  [ "$(seqof Fw_App.Remove)" -lt "$(seqof RemoveFiles)" ]
  [ "$(seqof WixlSetNetshExe)" -gt "$(seqof CostFinalize)" ]
  [ "$(seqof WixlSetNetshExe)" -lt "$(seqof InstallInitialize)" ]

  # the native netsh.exe for 64-bit packages
  run "$wixl" -a x64 -o out64.msi fw.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out64.msi CustomAction
  [ "$status" -eq 0 ]
  echo "$output" | tr -d '\r' | grep -qxF "$(printf 'WixlSetNetshExe\t51\tWixlNetshExe\t[System64Folder]netsh.exe\t')"

  # errors like WiX's
  sed 's/ Scope="localSubnet"//' fw.wxs > noscope.wxs
  run "$wixl" -o out.msi noscope.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException App: Scope or RemoteAddress is required"
  sed 's/ IgnoreFailure="yes"/ IgnoreFailure="yes" Scope="any"/' fw.wxs > both.wxs
  run "$wixl" -o out.msi both.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException Port: Scope and RemoteAddress can't be used together"
  sed 's#<fw:RemoteAddress>\*</fw:RemoteAddress>#<fw:RemoteAddress> </fw:RemoteAddress>#' fw.wxs > empty.wxs
  run "$wixl" -o out.msi empty.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException Port: empty RemoteAddress"
  sed 's/ Profile="private"/ Profile="private" Direction="out"/' fw.wxs > v4.wxs
  run "$wixl" -o out.msi v4.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException App: unsupported attribute Direction"

  # WiX treats a rule with a program as an application exception, so a
  # Port or Protocol on it is ignored
  sed 's/Id="App" Name="My App"/Id="App" Name="My App" Port="8080" Protocol="udp"/' fw.wxs > pport.wxs
  run "$wixl" -o out.msi pport.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  ca=$(echo "$output" | tr -d '\r')
  addrow=$(echo "$ca" | awk -F '\t' '$1 == "Fw_App.Add" { print $4 }')
  echo "$addrow" | grep -q 'program='
  echo "$addrow" | grep -q 'edge=yes'
  ! echo "$addrow" | grep -q 'localport='
  ! echo "$addrow" | grep -q 'protocol='

  # a longer name than the CustomAction.Target column can hold gives a warning
  long=$(printf 'x%.0s' {1..240})
  sed "s/Name=\"My App\"/Name=\"$long\"/" fw.wxs > long.wxs
  run "$wixl" -o out.msi long.wxs
  [ "$status" -eq 0 ]
  echo "$output" | grep -F "FirewallException App: the netsh command line is longer than 255 characters"

  # Ids must be valid identifiers, as in WiX
  sed 's/Id="App"/Id="A B"/' fw.wxs > badid.wxs
  run "$wixl" -o out.msi badid.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException A B: invalid Id"

  # duplicate Ids would produce duplicate action names
  sed 's/Id="Port"/Id="App"/' fw.wxs > dupid.wxs
  run "$wixl" -o out.msi dupid.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "FirewallException App: duplicate Id"
}
@test "wixl - util:EventSource" {
  mkdir -p SourceDir
  echo "hello" > SourceDir/app.exe
  cat >eventsource.wxs <<'EOF2'
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"
     xmlns:util="http://schemas.microsoft.com/wix/UtilExtension">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE"
           Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
    <Media Id="1" Cabinet="test.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFilesFolder">
        <Directory Id="INSTALLDIR" Name="Test">
          <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471">
            <File Id="App" Source="SourceDir/app.exe" KeyPath="yes" />
            <util:EventSource Log="Application" Name="app"
                              EventMessageFile="%SystemRoot%\System32\EventCreate.exe"
                              SupportsErrors="yes" SupportsInformationals="yes"
                              SupportsWarnings="yes" />
          </Component>
          <Component Id="C2" Guid="*">
            <util:EventSource Log="Custom" Name="other" KeyPath="yes"
                              EventMessageFile="[INSTALLDIR]msg.dll"
                              CategoryMessageFile="[INSTALLDIR]cat.dll" CategoryCount="3"
                              ParameterMessageFile="[INSTALLDIR]param.dll"
                              SupportsSuccessAudits="yes" SupportsFailureAudits="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
    <Feature Id="Main" Level="1">
      <ComponentRef Id="C1" />
      <ComponentRef Id="C2" />
    </Feature>
  </Product>
</Wix>
EOF2
  run "$wixl" -o out.msi eventsource.wxs
  [ "$status" -eq 0 ]

  run "$msiinfo" export out.msi Registry
  [ "$status" -eq 0 ]
  regs=$(echo "$output" | tail -n +4 | cut -f2- | tr -d '\r' | sort)
  exp=$(sort <<'EOF2'
2	SYSTEM\CurrentControlSet\Services\EventLog\Application\app	EventMessageFile	#%%SystemRoot%\System32\EventCreate.exe	C1
2	SYSTEM\CurrentControlSet\Services\EventLog\Application\app	TypesSupported	#7	C1
2	SYSTEM\CurrentControlSet\Services\EventLog\Custom\other	EventMessageFile	#%[INSTALLDIR]msg.dll	C2
2	SYSTEM\CurrentControlSet\Services\EventLog\Custom\other	CategoryMessageFile	#%[INSTALLDIR]cat.dll	C2
2	SYSTEM\CurrentControlSet\Services\EventLog\Custom\other	CategoryCount	#3	C2
2	SYSTEM\CurrentControlSet\Services\EventLog\Custom\other	ParameterMessageFile	#%[INSTALLDIR]param.dll	C2
2	SYSTEM\CurrentControlSet\Services\EventLog\Custom\other	TypesSupported	#24	C2
EOF2
)
  [ "$regs" = "$exp" ]

  # KeyPath="yes" makes the EventMessageFile value the key path
  run "$msiinfo" export out.msi Component
  [ "$status" -eq 0 ]
  c2=$(echo "$output" | awk -F '\t' '$1 == "C2" { print $6 }' | tr -d '\r')
  # msidbComponentAttributesRegistryKeyPath
  [ "$(echo "$output" | awk -F '\t' '$1 == "C2" { print $4 }')" = "4" ]
  # without KeyPath the EventSource doesn't take the File's place
  [ "$(echo "$output" | awk -F '\t' '$1 == "C1" { print $6 }' | tr -d '\r')" = "App" ]
  run "$msiinfo" export out.msi Registry
  [ "$status" -eq 0 ]
  name=$(echo "$output" | awk -F '\t' -v id="$c2" '$1 == id { print $4 }' | tr -d '\r')
  [ "$name" = "EventMessageFile" ]

  # CategoryMessageFile needs CategoryCount
  sed 's/ CategoryCount="3"//' eventsource.wxs > nocount.wxs
  run "$wixl" -o out.msi nocount.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "CategoryMessageFile requires CategoryCount"

  # an EventSource outside the util namespace is not the extension's
  sed 's/<util:EventSource Log="Application"/<EventSource Log="Application"/' eventsource.wxs > core.wxs
  run "$wixl" -o out.msi core.wxs
  [ "$status" -ne 0 ]
  echo "$output" | grep -F "EventSource"

  # WiX's EventSource has no Id attribute
  sed 's/<util:EventSource Log="Application"/<util:EventSource Id="app" Log="Application"/' eventsource.wxs > id.wxs
  run "$wixl" -o out.msi id.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "EventSource app: Id is not supported"

  # the remaining validations
  sed 's/Log="Application"/Log="Security"/' eventsource.wxs > security.wxs
  run "$wixl" -o out.msi security.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "Log can't be Security"

  sed 's/EventMessageFile="[^"]*"//' eventsource.wxs > nomessage.wxs
  run "$wixl" -o out.msi nomessage.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "Log, Name and EventMessageFile are required"

  sed 's/CategoryMessageFile="[^"]*"//' eventsource.wxs > nocatcount.wxs
  run "$wixl" -o out.msi nocatcount.wxs
  [ "$status" -eq 1 ]
  echo "$output" | grep -F "CategoryCount requires CategoryMessageFile"

  # all supports bits
  sed 's/SupportsWarnings="yes"/SupportsWarnings="yes" SupportsSuccessAudits="yes" SupportsFailureAudits="yes"/' eventsource.wxs > alltypes.wxs
  run "$wixl" -o out.msi alltypes.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi Registry
  [ "$status" -eq 0 ]
  echo "$output" | tr -d '\r' | grep -qF "$(printf 'TypesSupported\t#31')"

  # a non-KeyPath EventSource doesn't make the component key path
  sed 's/Guid="\*"/Guid="A07E8D44-5F60-7182-BDCE-4567890123DE"/; s/ Name="other" KeyPath="yes"/ Name="other"/' eventsource.wxs > nokey.wxs
  run "$wixl" -o out.msi nokey.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi Component
  [ "$status" -eq 0 ]
  [ "$(echo "$output" | awk -F '\t' '$1 == "C2" { print $4 }')" = "0" ]
  [ -z "$(echo "$output" | awk -F '\t' '$1 == "C2" { print $6 }' | tr -d '\r')" ]
}
@test "wixl - Package Platform" {
  cd wixl
  cat >platform.wxs <<'EOF2'
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="ABCDDCBA-86C7-4D14-AEC0-86416A69ABDE" Name="Test" Version="1.0.0" Manufacturer="Test" Language="1033"
           UpgradeCode="12345678-1234-1234-1234-123456789012">
    <Package InstallerVersion="200" Compressed="yes" Platform="PLATFORM" />
    <Media Id="1" Cabinet="t.cab" EmbedCab="yes" />
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder"><Directory Id="INSTALLDIR" Name="Test">
        <Component Id="C1" Guid="ABCDDCBA-83F1-4F22-985B-FDB3C8ABD471"><File Id="F1" Source="FoobarAppl10.exe" KeyPath="yes" /></Component>
      </Directory></Directory>
    </Directory>
    <Feature Id="Main" Level="1"><ComponentRef Id="C1" /></Feature>
  </Product>
</Wix>
EOF2
  build() { sed "s/PLATFORM/$1/" platform.wxs >p.wxs; shift; run "$wixl" -o out.msi "$@" p.wxs; }
  template() { "$msiinfo" suminfo out.msi | grep '^Template:' | sed 's/^Template: *//'; }
  win64() { "$msiinfo" export out.msi Component | tr -d '\r' | awk -F '\t' '$1 == "C1" { print $4 }'; }

  # like WiX, Platform sets the architecture without -a
  build x64
  [ "$status" -eq 0 ]
  [ "$(template)" = "x64;1033" ]
  [ $(( $(win64) & 256 )) -eq 256 ]
  build intel
  [ "$status" -eq 0 ]
  [ "$(template)" = "Intel;1033" ]
  [ $(( $(win64) & 256 )) -eq 0 ]

  # -a may repeat it, but not contradict it
  build x64 -a x64
  [ "$status" -eq 0 ]
  [ "$(template)" = "x64;1033" ]
  build x64 -a x86
  [ "$status" -ne 0 ]
  [[ "$output" == *"conflicts with arch"* ]]

  build arm
  [ "$status" -ne 0 ]
  [[ "$output" == *"Platform 'arm' is not supported"* ]]

  # nor the sys.BUILDARCH the sources already used
  sed 's|<Package |<?if $(sys.BUILDARCH) = x64 ?><?endif?>&|' platform.wxs >buildarch.wxs
  sed "s/PLATFORM/x64/" buildarch.wxs >p.wxs
  run "$wixl" -o out.msi p.wxs
  [ "$status" -ne 0 ]
  [[ "$output" == *"conflicts with sys.BUILDARCH 'x86'"* ]]
  run "$wixl" -o out.msi -a x64 p.wxs
  [ "$status" -eq 0 ]
}
