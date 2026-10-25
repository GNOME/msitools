#!/usr/bin/env bats

load common

fw_wxs() {
  cat <<EOF
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
            <File Id="app.exe" Source="app.exe" KeyPath="yes" />
            <fw:FirewallException Id="Rule" Name="My App" $1>
              <fw:RemoteAddress>127.0.0.1</fw:RemoteAddress>
            </fw:FirewallException>
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
}

# the netsh command line of the custom action that adds the rule
fw_add_rule() {
  fw_wxs "$1" > fw.wxs
  run "$wixl" -o out.msi fw.wxs
  [ "$status" -eq 0 ]
  run "$msiinfo" export out.msi CustomAction
  [ "$status" -eq 0 ]
  rule=$(echo "$output" | tr -d '\r' | awk -F '\t' '$1 == "Fw_Rule.Add" { print $4 }')
}

@test "wixl - FirewallException with Program, Port and Protocol" {
  echo "hello" > app.exe
  # like WiX, the rule is restricted to the program and the port
  fw_add_rule 'Program="[#app.exe]" Port="[LISTEN_PORT]" Protocol="tcp"'
  [ "$rule" = 'advfirewall firewall add rule name="My App" dir=in program="[#app.exe]" protocol=tcp localport="[LISTEN_PORT]" action=allow enable=yes profile=any remoteip="127.0.0.1" edge=yes' ]

  # TCP unless set otherwise, also with a File
  fw_add_rule 'File="app.exe" Port="9182"'
  [ "$rule" = 'advfirewall firewall add rule name="My App" dir=in program="[#app.exe]" protocol=tcp localport="9182" action=allow enable=yes profile=any remoteip="127.0.0.1" edge=yes' ]

  # a Protocol without a Port
  fw_add_rule 'Program="[#app.exe]" Protocol="udp"'
  [ "$rule" = 'advfirewall firewall add rule name="My App" dir=in program="[#app.exe]" protocol=udp action=allow enable=yes profile=any remoteip="127.0.0.1" edge=yes' ]
}

@test "wixl - FirewallException with Program only" {
  echo "hello" > app.exe
  fw_add_rule 'Program="[#app.exe]"'
  [ "$rule" = 'advfirewall firewall add rule name="My App" dir=in program="[#app.exe]" action=allow enable=yes profile=any remoteip="127.0.0.1" edge=yes' ]
}

@test "wixl - FirewallException with Port only" {
  echo "hello" > app.exe
  fw_add_rule 'Port="9182" Protocol="udp"'
  [ "$rule" = 'advfirewall firewall add rule name="My App" dir=in protocol=udp localport="9182" action=allow enable=yes profile=any remoteip="127.0.0.1"' ]
}
