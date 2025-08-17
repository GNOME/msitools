#!/usr/bin/perl
#
# Validate a set of wxi files to determine if they
# list all the DLL dependencies required by any EXE
# or DLL files that contain and that all listed files
# do exist on disk
#
# Copyright 2018-2019 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see
# <http://www.gnu.org/licenses/>.
#

use strict;
use warnings;

use XML::XPath;
use XML::XPath::XMLParser;

# WXI Filename -> group name
my %filegroups;

# Group name -> dict ( array of component groups, array of component hashes )
my %groups;

# Component hash -> file path
my %components;

# DLL file name -> component hash
my %dllcomponents;
# DLL file name -> group name
my %dllgroups;

# DLL files provided by Windows runtime which
# thus don't need to be listed in wxi files
# as dependencies
my $dllbuiltin = "^(" .
    "advapi32|" .
    "api-ms-.*|" .
    "avrt|" .
    "bcrypt|" .
    "bcryptprimitives|" .
    "comctl32|" .
    "comdlg32|" .
    "crypt32|" .
    "dbghelp|" .
    "dwrite|" .
    "d3d10|" .
    "d3d11|" .
    "d3d12|" .
    "d3d9|" .
    "dnsapi|" .
    "dsound|" .
    "dwmapi|" .
    "dxgi|" .
    "gdi32|" .
    "gdiplus|" .
    "hid|" .
    "imm32|" .
    "iphlpapi|" .
    "kernel32|" .
    "ksuser|" .
    "msimg32|" .
    "msvcrt|" .
    "mswsock|" .
    "ncrypt|" .
    "ntdll|" .
    "ole32|" .
    "oleaut32|" .
    "opengl32|" .
    "psapi|" .
    "secur32|" .
    "setupapi|" .
    "shell32|" .
    "shlwapi|" .
    "user32|" .
    "userenv|" .
    "usp10|" .
    "version|" .
    "winmm|" .
    "wldap32|" .
    "ws2_32|" .
    ").dll\$";

my $drvbuiltin = "^(" .
    "winspool|" .
    ").drv\$";

my @checkgroups;
my $errors = 0;
# Load all the requested wxi files, doing
# a basic sanity check that all files in
# the manifest exist
foreach my $file (@ARGV) {
    push @checkgroups, &load($file);
}
exit 1 if $errors;

# Check all DLL/EXE files to see if they
# reference DLLs which are not mentioned as
# dependencies
foreach my $group (@checkgroups) {
    my %allowedcomps;
    &expand($group, \%allowedcomps);
    &check($group, \%allowedcomps);
}
exit $errors;

sub load {
    my $file = shift;

    # By convention the .wxi files must be named to match
    # the mingw RPM name
    my $rpmname = $file;
    $rpmname =~ s,^.*/,,;
    $rpmname =~ s/.wxi//;

    my $rpmname32 = "mingw32-$rpmname";
    my $rpmname64 = "mingw64-$rpmname";

    # This testing is only going to work if the RPMs are
    # actually installed
    my $hasrpm32 = (system "rpm -q $rpmname32 >/dev/null 2>&1") == 0;
    my $hasrpm64 = (system "rpm -q $rpmname64 >/dev/null 2>&1") == 0;

    unless ($hasrpm32) {
        $errors = 1;
        print "Cannot check $file, $rpmname32 not installed\n";
    }
    unless ($hasrpm32) {
        $errors = 1;
        print "Cannot check $file, $rpmname32 not installed\n";
    }

    # Since we're recursively expanding <require> imports
    # we might be called repeatedly for the same file, so
    # short circuit if that happens
    if (exists $filegroups{$file}) {
        return $filegroups{$file};
    }

    # XML::XPath will automatically process the
    # <?require> directives, but that is problematic
    # as we want to track the exact source file that
    # problems are present in.
    #
    # Thus we pre-emptively load any includes here
    # so we can report errors directly, and setup
    # the right file groups
    open XML, "$file" or die "cannot read $file: $!";

    foreach (<XML>) {
        if (/<\?require (\S+)\?>/) {
            my $incname = $1;
            my $path = $file;
            $path =~ s,/[^/]+$,/,;
            my $incfile = $path . $incname;
            &load($incfile);
        }
    }

    close XML;

    my $xp = XML::XPath->new(filename => $file);
    my $nodeset;

    # Extract all the listed dependencies
    $nodeset = eval { $xp->find("//ComponentGroup"); };
    if ($@) {
        print STDERR "Parsing failed on $file: $@\n";
        $errors = 1;
        return;
    }

    my $cg = $nodeset->[0];

    my $cgid = $cg->getAttribute("Id");
    $groups{$cgid} = {
        "file" => $file,
        "groups" => [],
        "components" => [],
    };
    foreach my $child ($cg->getChildNodes()) {
        next unless defined $child->getName();

        if ($child->getName() eq "ComponentRef") {
            # These are file local dependencies
            push @{$groups{$cgid}->{components}}, $child->getAttribute("Id");
        } else {
            # These are external file dependencies
            push @{$groups{$cgid}->{groups}}, $child->getAttribute("Id");
        }
    }


    # Extract all files listed in this wxi manifest
    $nodeset = eval { $xp->find("//Component"); };
    if ($@) {
        print STDERR "Parsing failed on $file: $@\n";
        return;
    }

    foreach my $node ($nodeset->get_nodelist) {
        my $id = $node->getAttribute("Id");

        foreach my $child ($node->getChildNodes()) {
            next unless defined $child->getName() && $child->getName() eq "File";

            my $fname = $child->getAttribute("Source");

            my $msifile32 = $fname;
            my $msifile64 = $fname;

            $msifile32 =~ s,\$\(var.GLIB_ARCH\),win32,;
            $msifile64 =~ s,\$\(var.GLIB_ARCH\),win64,;

            $msifile32 =~ s,\$\(var.SourceDir\),/usr/i686-w64-mingw32/sys-root/mingw,;
            $msifile64 =~ s,\$\(var.SourceDir\),/usr/x86_64-w64-mingw32/sys-root/mingw,;

            # Hacks because we don't parse conditionals

            # gcc
            $msifile32 = undef if $msifile32 =~ /libgcc_s_seh-1\.dll/;
            $msifile64 = undef if $msifile64 =~ /libgcc_s_dw2-1.dll/;

            # openssl
            $msifile32 = undef if $msifile32 && $msifile32 =~ /lib(ssl|crypto)-[^-]+-x64.dll/;
            $msifile64 = undef if $msifile64 && $msifile64 =~ /lib(ssl|crypto)-[^-]+.dll/;

            # gstreamer1-plugins-base
            $msifile32 = undef if $msifile32 && $msifile32 =~ /x86_64/;
            $msifile64 = undef if $msifile64 && $msifile64 =~ /i686/;


            # Check that the listed files really do exist
            if ($hasrpm32 && $msifile32 && ! -e $msifile32) {
                print "\n\nErrors in $file\n\n" unless $errors;
                $errors = 1;
                print "  - Missing $msifile32\n";
            }
            if ($hasrpm64 && $msifile64 && ! -e $msifile64) {
                print "\n\nErrors in $file\n\n" unless $errors;
                $errors = 1;
                print "  - Missing $msifile64\n";
            }

            # Setup mappings for the component and file
            $components{$id} = [] unless exists $components{$id};
            push @{$components{$id}}, $msifile32 if $msifile32;
            push @{$components{$id}}, $msifile64 if $msifile64;

            # Setup mappings for the DLLs
            if ($fname =~ m,.*/([^/]+.dll)$,) {
                my $dllname = lc $1;
                $dllcomponents{$dllname} = $id;
                $dllgroups{$dllname} = $cgid;
            }
        }
    }

    $filegroups{$file} = $cgid;

    return $cgid;
}


# Recursively expand a file to find the full
# transitive set of dependencies / references
sub expand {
    my $name = shift;
    my $allowed = shift;

    foreach (@{$groups{$name}->{components}}) {
        $allowed->{$_} = 1;
    }
    foreach my $ref (@{$groups{$name}->{groups}}) {
        &expand($ref, $allowed);
    }
}


# Check that all DLL/EXE files have their
# dependencies satisfied
sub check {
    my $name = shift;
    my $allowed = shift;

    foreach my $comp (@{$groups{$name}->{components}}) {
        if (!exists $components{$comp}) {
            $errors = 1;
            print "Referenced component $comp does not exist in $name\n";
            return;
        }
        my @files = @{$components{$comp}};

        foreach my $file (@files) {
            next unless $file =~ /\.(exe|dll)$/;

            my $problems = 0;

            unless (-f $file) {
                print " > $file\n" if ++$problems == 1;
                print "     - Cannot analyse missing file $file\n";
                next;
            }

            # Get the list of dependancies from objdump. The
            # output contains list of stuff we don't care
            # about so we're looking for lines like:
            #
            #    DLL Name: libcurl-4.dll
            #    DLL Name: libdl.dll
            #    DLL Name: libgcc_s_sjlj-1.dll
            #    DLL Name: libgnutls-30.dll
            #    DLL Name: libintl-8.dll

            open OBJ, "x86_64-w64-mingw32-objdump -p $file |";
            foreach my $info (<OBJ>) {
                next unless $info =~ /DLL Name: (\S+)/;
                my $dllname = lc $1;
                next if $dllname =~ $dllbuiltin;
                next if $dllname =~ $drvbuiltin;

                unless (exists $dllcomponents{$dllname}) {
                    print " > $file\n" if ++$problems == 1;
                    print "     - Unknown component for $dllname\n";
                    next;
                }

                my $dllcomp = $dllcomponents{$dllname};

                next if exists $allowed->{$dllcomp};

                my $dllgroup = $dllgroups{$dllname};

                print " > $file\n" if ++$problems == 1;
                print "     - Missing group ref $dllgroup for $dllname\n";
            }

            $errors = 1 if $problems;
        }
    }
}
