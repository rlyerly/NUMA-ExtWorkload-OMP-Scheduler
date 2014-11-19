#!/usr/bin/perl

# <copyright>
#    Copyright (c) 2013-2014 Intel Corporation.  All Rights Reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions
#    are met:
#
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#      * Neither the name of Intel Corporation nor the names of its
#        contributors may be used to endorse or promote products derived
#        from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# </copyright>

use strict;
use warnings;

use File::Glob ":glob";
use Encode qw{ encode };

use FindBin;
use lib "$FindBin::Bin/lib";

use tools;
use Platform ":vars";

our $VERSION = "0.04";
my $escape      = qr{%};
my $placeholder = qr{(\d)\$(s|l?[du])};

my $sections =
    {
        meta     => { short => "prp" }, # "prp" stands for "property".
        strings  => { short => "str" },
        formats  => { short => "fmt" },
        messages => { short => "msg" },
        hints    => { short => "hnt" },
    };
my @sections = qw{ meta strings formats messages hints };
# Assign section properties: long name, set number, base number.
map( $sections->{ $sections[ $_ ] }->{ long } = $sections[ $_ ],      ( 0 .. @sections - 1 ) );
map( $sections->{ $sections[ $_ ] }->{ set  } = ( $_ + 1 ),           ( 0 .. @sections - 1 ) );
map( $sections->{ $sections[ $_ ] }->{ base } = ( ( $_ + 1 ) << 16 ), ( 0 .. @sections - 1 ) );

# Properties of Meta section.
my @properties = qw{ Language Country LangId Version Revision };


sub _generate_comment($$$) {

    my ( $data, $open, $close ) = @_;
    my $bulk =
        $open . " Do not edit this file! " . $close . "\n" .
        $open . " The file was generated from " . get_file( $data->{ "%meta" }->{ source } ) .
            " by " . $tool . " on " . localtime() . ". " . $close . "\n";
    return $bulk;

}; # sub _generate_comment


sub msg2sgn($) {

    # Convert message string to signature. Signature is a list of placeholders in sorted order.
    # For example, signature of "%1$s value \"%2$s\" is invalid." is "%1$s %2$s".

    my ( $msg ) = @_;
    my @placeholders;
    pos( $msg ) = 0;
    while ( $msg =~ m{\G.*?$escape$placeholder}g ) {
        $placeholders[ $1 - 1 ] = "%$1\$$2";
    }; # while
    for ( my $i = 1; $i <= @placeholders; ++ $i ) {
        if ( not defined( $placeholders[ $i - 1 ] ) ) {
            $placeholders[ $i - 1 ] = "%$i\$-";
        }; # if
    }; # for $i
    return join( " ", @placeholders );

}; # sub msg2sgn


sub msg2src($) {

    # Convert message string to a C string constant.

    my ( $msg ) = @_;
    if ( $target_os eq "win" ) {
        $msg =~ s{$escape$placeholder}{\%$1!$2!}g;
    }; # if
    return $msg;

}; # sub msg2src


my $special =
    {
        "n" => "\n",
        "t" => "\t",
    };

sub msg2mc($) {
    my ( $msg ) = @_;
    $msg = msg2src( $msg ); # Get windows style placeholders.
    $msg =~ s{\\(.)}{ exists( $special->{ $1 } ) ? $special->{ $1 } : $1 }ge;
    return $msg;
}; # sub msg2mc



sub parse_message($) {

    my ( $msg ) = @_;
    pos( $msg ) = 0;
    for ( ; ; ) {
        if ( $msg !~ m{\G.*?$escape}gc ) {
            last;
        }
        if ( $msg !~ m{\G$placeholder}gc ) {
            return "Bad %-sequence near \"%" . substr( $msg, pos( $msg ), 7 ) . "\"";
        }; # if
    }; # forever
    return undef;

}; # sub parse_message


sub parse_source($) {

    my ( $name ) = @_;

    my @bulk = read_file( $name, -layer => ":utf8" );
    my $data = {};

    my $line;
    my $n = 0;         # Line number.
    my $obsolete = 0;  # Counter of obsolete entries.
    my $last_idx;
    my %idents;
    my $section;

    my $error =
        sub {
            my ( $n, $line, $msg ) = @_;
            runtime_error( "Error parsing $name line $n: " . "$msg:\n" . "    $line" );
        }; # sub

    foreach $line ( @bulk ) {
        ++ $n;
        # Skip empty lines and comments.
        if ( $line =~ m{\A\s*(\n|#)} ) {
            $last_idx = undef;
            next;
        }; # if
        # Parse section header.
        if ( $line =~ m{\A-\*-\s*([A-Z_]*)\s*-\*-\s*\n\z}i ) {
            $section = ( lc( $1 ) );
            if ( not grep( $section eq $_, @sections ) ) {
                $error->( $n, $line, "Unknown section \"$section\" specified" );
            }; # if
            if ( exists( $data->{ $section } ) ) {
                $error->( $n, $line, "Multiple sections of the same type specified" );
            }; # if
            %idents = ();     # Clean list of known message identifiers.
            next;
        }; # if
        if ( not defined( $section ) ) {
            $error->( $n, $line, "Section heading expected" );
        }; # if
        # Parse section body.
        if ( $section eq "meta" ) {
            if ( $line =~ m{\A([A-Z_][A-Z_0-9]*)\s+"(.*)"\s*?\n?\z}i ) {
                # Parse meta properties (such as Language, Country, and LangId).
                my ( $property, $value ) = ( $1, $2 );
                if ( not grep( $_ eq $property , @properties ) ) {
                    $error->( $n, $line, "Unknown property \"$property\" specified" );
                }; # if
                if ( exists( $data->{ "%meta" }->{ $property } ) ) {
                    $error->( $n, $line, "Property \"$property\" has already been specified" );
                }; # if
                $data->{ "%meta" }->{ $property } = $value;
                $last_idx = undef;
                next;
            }; # if
            $error->( $n, $line, "Property line expected" );
        }; # if
        # Parse message.
        if ( $line =~ m{\A([A-Z_][A-Z_0-9]*)\s+"(.*)"\s*?\n?\z}i ) {
            my ( $ident, $message ) = ( $1, $2 );
            if ( $ident eq "OBSOLETE" ) {
                # If id is "OBSOLETE", add a unique suffix. It provides convenient way to mark
                # obsolete messages.
                ++ $obsolete;
                $ident .= $obsolete;
            }; # if
            if ( exists( $idents{ $ident } ) ) {
                $error->( $n, $line, "Identifier \"$ident\" is redefined" );
            }; # if
            # Check %-sequences.
            my $err = parse_message( $message );
            if ( $err ) {
                $error->( $n, $line, $err );
            }; # if
            # Save message.
            push( @{ $data->{ $section } }, [ $ident, $message ] );
            $idents{ $ident } = 1;
            $last_idx = @{ $data->{ $section } } - 1;
            next;
        }; # if
        # Parse continuation line.
        if ( $line =~ m{\A\s*"(.*)"\s*\z} ) {
            my $message = $1;
            if ( not defined( $last_idx )  ) {
                $error->( $n, $line, "Unexpected continuation line" );
            }; # if
            # Check %-sequences.
            my $err = parse_message( $message );
            if ( $err ) {
                $error->( $n, $line, $err );
            }; # if
            # Save continuation.
            $data->{ $section }->[ $last_idx ]->[ 1 ] .= $message;
            next;
        }; # if
        $error->( $n, $line, "Message definition expected" );
    }; # foreach
    $data->{ "%meta" }->{ source } = $name;
    foreach my $section ( @sections ) {
        if ( not exists( $data->{ $section } ) ) {
            $data->{ $section } = [];
        }; # if
    }; # foreach $section

    foreach my $property ( @properties ) {
        if ( not defined( $data->{ "%meta" }->{ $property } ) ) {
            runtime_error(
                "Error parsing $name: " .
                    "Required \"$property\" property is not specified"
            );
        }; # if
        push( @{ $data->{ meta } }, [ $property, $data->{ "%meta" }->{ $property } ] );
    }; # foreach

    return $data;

}; # sub parse_source


sub generate_enum($$$) {

    my ( $data, $file, $prefix ) = @_;
    my $bulk = "";

    $bulk =
        _generate_comment( $data, "//", "//" ) .
        "\n" .
        "enum ${prefix}_id {\n\n" .
        "    // A special id for absence of message.\n" .
        "    ${prefix}_null = 0,\n\n";

    foreach my $section ( @sections ) {
        my $props = $sections->{ $section };    # Section properties.
        my $short = $props->{ short };          # Short section name, frequently used.
        $bulk .=
            "    // Set #$props->{ set }, $props->{ long }.\n" .
            "    ${prefix}_${short}_first = $props->{ base },\n";
        foreach my $item ( @{ $data->{ $section } } ) {
            my ( $ident, undef ) = @$item;
            $bulk .= "    ${prefix}_${short}_${ident},\n";
        }; # foreach
        $bulk .= "    ${prefix}_${short}_last,\n\n";
    }; # foreach $type
    $bulk .= "    ${prefix}_xxx_lastest\n\n";

    $bulk .=
        "}; // enum ${prefix}_id\n" .
        "\n" .
        "typedef enum ${prefix}_id  ${prefix}_id_t;\n" .
        "\n";

    $bulk .=
        "\n" .
        "// end of file //\n";

    write_file( $file, \$bulk );

}; # sub generate_enum


sub generate_signature($$) {

    my ( $data, $file ) = @_;
    my $bulk = "";

    $bulk .= "// message catalog signature file //\n\n";

    foreach my $section ( @sections ) {
        my $props = $sections->{ $section };    # Section properties.
        my $short = $props->{ short };          # Short section name, frequently used.
        $bulk .= "-*- " . uc( $props->{ long } ) . "-*-\n\n";
        foreach my $item ( @{ $data->{ $section } } ) {
            my ( $ident, $msg ) = @$item;
            $bulk .= sprintf( "%-40s %s\n", $ident, msg2sgn( $msg ) );
        }; # foreach
        $bulk .= "\n";
    }; # foreach $type

    $bulk .= "// end of file //\n";

    write_file( $file, \$bulk );

}; # sub generate_signature


sub generate_default($$$) {

    my ( $data, $file, $prefix ) = @_;
    my $bulk = "";

    $bulk .=
        _generate_comment( $data, "//", "//" ) .
        "\n";

    foreach my $section ( @sections ) {
        $bulk .=
            "static char const *\n" .
            "__${prefix}_default_${section}" . "[] =\n" .
            "    {\n" .
            "        NULL,\n";
        foreach my $item ( @{ $data->{ $section } } ) {
            my ( undef, $msg ) = @$item;
            $bulk .= "        \"" . msg2src( $msg ) . "\",\n";
        }; # while
        $bulk .=
            "        NULL\n" .
            "    };\n" .
            "\n";
    }; # foreach $type

    $bulk .=
        "struct kmp_i18n_section {\n" .
        "    int           size;\n" .
        "    char const ** str;\n" .
        "}; // struct kmp_i18n_section\n" .
        "typedef struct kmp_i18n_section  kmp_i18n_section_t;\n" .
        "\n" .
        "static kmp_i18n_section_t\n" .
        "__${prefix}_sections[] =\n" .
        "    {\n" .
        "        { 0, NULL },\n";
    foreach my $section ( @sections ) {
        $bulk .=
            "        { " . @{ $data->{ $section } } . ", __${prefix}_default_${section} },\n";
    }; # foreach $type
    $bulk .=
        "        { 0, NULL }\n" .
        "    };\n" .
        "\n";

    $bulk .=
        "struct kmp_i18n_table {\n" .
        "    int                   size;\n" .
        "    kmp_i18n_section_t *  sect;\n" .
        "}; // struct kmp_i18n_table\n" .
        "typedef struct kmp_i18n_table  kmp_i18n_table_t;\n" .
        "\n" .
        "static kmp_i18n_table_t __kmp_i18n_default_table =\n" .
        "    {\n" .
        "        " . @sections . ",\n" .
        "        __kmp_i18n_sections\n" .
        "    };\n" .
        "\n" .
        "// end of file //\n";

    write_file( $file, \$bulk );

}; # sub generate_default


sub generate_message_unix($$) {

    my ( $data, $file ) = @_;
    my $bulk     = "";

    $bulk .=
        _generate_comment( $data, "\$", "\$" ) .
        "\n" .
        "\$quote \"\n\n";

    foreach my $section ( @sections ) {
        $bulk .=
            "\$ " . ( "-" x 78 ) . "\n\$ $section\n\$ " . ( "-" x 78 ) . "\n\n" .
            "\$set $sections->{ $section }->{ set }\n" .
            "\n";
        my $n = 0;
        foreach my $item ( @{ $data->{ $section } } ) {
            my ( undef, $msg ) = @$item;
            ++ $n;
            $bulk .= "$n \"" . msg2src( $msg ) . "\"\n";
        }; # foreach
        $bulk .= "\n";
    }; # foreach $type

    $bulk .=
        "\n" .
        "\$ end of file \$\n";

    write_file( $file, \$bulk, -layer => ":utf8" );

}; # sub generate_message_linux


sub generate_message_windows($$) {

    my ( $data, $file ) = @_;
    my $bulk = "";
    my $language = $data->{ "%meta" }->{ Language };
    my $langid   = $data->{ "%meta" }->{ LangId };

    $bulk .=
        _generate_comment( $data, ";", ";" ) .
        "\n" .
        "LanguageNames = ($language=$langid:msg_$langid)\n" .
        "\n";

    $bulk .=
        "FacilityNames=(\n";
    foreach my $section ( @sections ) {
        my $props = $sections->{ $section };    # Section properties.
        $bulk .=
            " $props->{ short }=" . $props->{ set } ."\n";
    }; # foreach $section
    $bulk .=
        ")\n\n";

    foreach my $section ( @sections ) {
        my $short = $sections->{ $section }->{ short };
        my $n = 0;
        foreach my $item ( @{ $data->{ $section } } ) {
            my ( undef, $msg ) = @$item;
            ++ $n;
            $bulk .=
                "MessageId=$n\n" .
                "Facility=$short\n" .
                "Language=$language\n" .
                msg2mc( $msg ) . "\n.\n\n";
        }; # foreach $item
    }; # foreach $section

    $bulk .=
        "\n" .
        "; end of file ;\n";

    $bulk = encode( "UTF-16LE", $bulk ); # Convert text to UTF-16LE used in Windows* OS.
    write_file( $file, \$bulk, -binary => 1 );

}; # sub generate_message_windows


#
# Parse command line.
#

my $input_file;
my $enum_file;
my $signature_file;
my $default_file;
my $message_file;
my $id;
my $prefix = "";
get_options(
    Platform::target_options(),
    "enum-file=s"      => \$enum_file,
    "signature-file=s" => \$signature_file,
    "default-file=s"   => \$default_file,
    "message-file=s"   => \$message_file,
    "id|lang-id"       => \$id,
    "prefix=s"	       => \$prefix,
);
if ( @ARGV == 0 ) {
    cmdline_error( "No source file specified -- nothing to do" );
}; # if
if ( @ARGV > 1 ) {
    cmdline_error( "Too many source files specified" );
}; # if
$input_file = $ARGV[ 0 ];


my $generate_message;
if ( $target_os =~ m{\A(?:lin|lrb|mac)\z} ) {
    $generate_message = \&generate_message_unix;
} elsif ( $target_os eq "win" ) {
    $generate_message = \&generate_message_windows;
} else {
    runtime_error( "OS \"$target_os\" is not supported" );
}; # if


#
# Do the work.
#

my $data = parse_source( $input_file );
if ( defined( $id ) ) {
    print( $data->{ "%meta" }->{ LangId }, "\n" );
}; # if
if ( defined( $enum_file ) ) {
    generate_enum( $data, $enum_file, $prefix );
}; # if
if ( defined( $signature_file ) ) {
    generate_signature( $data, $signature_file );
}; # if
if ( defined( $default_file ) ) {
    generate_default( $data, $default_file, $prefix );
}; # if
if ( defined( $message_file ) ) {
    $generate_message->( $data, $message_file );
}; # if

exit( 0 );

__END__

=pod

=head1 NAME

B<message-converter.pl> -- Convert message catalog source file into another text forms.

=head1 SYNOPSIS

B<message-converter.pl> I<option>... <file>

=head1 OPTIONS

=over

=item B<--enum-file=>I<file>

Generate enum file named I<file>.

=item B<--default-file=>I<file>

Generate default messages file named I<file>.

=item B<--lang-id>

Print language identifier of the message catalog source file.

=item B<--message-file=>I<file>

Generate message file.

=item B<--signature-file=>I<file>

Generate signature file.

Signatures are used for checking compatibility. For example, to check a primary
catalog and its translation to another language, signatures of both catalogs should be generated
and compared. If signatures are identical, catalogs are compatible.

=item B<--prefix=>I<prefix>

Prefix to be used for all C identifiers (type and variable names) in enum and default messages
files.

=item B<--os=>I<str>

Specify OS name the message formats to be converted for. If not specified expolicitly, value of
LIBOMP_OS environment variable is used. If LIBOMP_OS is not defined, host OS is detected.

Depending on OS, B<message-converter.pl> converts message formats to GNU style or MS style.

=item Standard Options

=over

=item B<--doc>

=item B<--manual>

Print full documentation and exit.

=item B<--help>

Print short help message and exit.

=item B<--version>

Print version string and exit.

=back

=back

=head1 ARGUMENTS

=over

=item I<file>

A name of input file.

=back

=head1 DESCRIPTION

=head2 Message Catalog File Format

It is plain text file in UTF-8 encoding. Empty lines and lines beginning with sharp sign (C<#>) are
ignored. EBNF syntax of content:

    catalog    = { section };
    section    = header body;
    header     = "-*- " section-id " -*-" "\n";
    body       = { message };
    message    = message-id string "\n" { string "\n" };
    section-id = identifier;
    message-id = "OBSOLETE" | identifier;
    identifier = letter { letter | digit | "_" };
    string     = """ { character } """;

Identifier starts with letter, with following letters, digits, and underscores. Identifiers are
case-sensitive. Setion identifiers are fixed: C<META>, C<STRINGS>, C<FORMATS>, C<MESSAGES> and
C<HINTS>. Message identifiers must be unique within section. Special C<OBSOLETE> pseudo-identifier
may be used many times.

String is a C string literal which must not cross line boundaries.
Long messages may occupy multiple lines, a string per line.

Message may include printf-like GNU-style placeholders for arguments: C<%I<n>$I<t>>,
where I<n> is argument number (C<1>, C<2>, ...),
I<t> -- argument type, C<s> (string) or C<d> (32-bit integer).

See also comments in F<i18n/en_US.txt>.

=head2 Output Files

This script can generate 3 different text files from single source:

=over

=item Enum file.

Enum file is a C include file, containing definitions of message identifiers, e. g.:

    enum kmp_i18n_id {

        // Set #1, meta.
        kmp_i18n_prp_first = 65536,
        kmp_i18n_prp_Language,
        kmp_i18n_prp_Country,
        kmp_i18n_prp_LangId,
        kmp_i18n_prp_Version,
        kmp_i18n_prp_Revision,
        kmp_i18n_prp_last,

        // Set #2, strings.
        kmp_i18n_str_first = 131072,
        kmp_i18n_str_Error,
        kmp_i18n_str_UnknownFile,
        kmp_i18n_str_NotANumber,
        ...

        // Set #3, fotrmats.
        ...

        kmp_i18n_xxx_lastest

    }; // enum kmp_i18n_id

    typedef enum kmp_i18n_id  kmp_i18n_id_t;

=item Default messages file.

Default messages file is a C include file containing default messages to be embedded into
application (and used if external message catalog does not exist or could not be open):

    static char const *
    __kmp_i18n_default_meta[] =
        {
            NULL,
            "English",
            "USA",
            "1033",
            "2",
            "20090806",
            NULL
        };

    static char const *
    __kmp_i18n_default_strings[] =
        {
            "Error",
            "(unknown file)",
            "not a number",
            ...
            NULL
        };

    ...

=item Message file.

Message file is an input for message compiler, F<gencat> on Linux* OS and OS X*, or F<mc.exe> on
Windows* OS.

Here is the example of Linux* OS message file:

    $quote "
    1 "Japanese"
    2 "Japan"
    3 "1041"
    4 "2"
    5 "Based on Enlish message catalog revision 20090806"
    ...

Example of Windows* OS message file:

    LanguageNames = (Japanese=10041:msg_1041)

    FacilityNames = (
     prp=1
     str=2
     fmt=3
     ...
    )

    MessageId=1
    Facility=prp
    Language=Japanese
    Japanese
    .

    ...

=item Signature.

Signature is a processed source file: comments stripped, strings deleted, but placeholders kept and
sorted.

    -*- FORMATS-*-

    Info                                     %1$d %2$s
    Warning                                  %1$d %2$s
    Fatal                                    %1$d %2$s
    SysErr                                   %1$d %2$s
    Hint                                     %1$- %2$s
    Pragma                                   %1$s %2$s %3$s %4$s

The purpose of signatures -- compare two message source files for compatibility. If signatures of
two message sources are the same, binary message catalogs will be compatible.

=back

=head1 EXAMPLES

Generate include file containing message identifiers:

    $ message-converter.pl --enum-file=kmp_i18n_id.inc en_US.txt

Generate include file contating default messages:

    $ message-converter.pl --default-file=kmp_i18n_default.inc en_US.txt

Generate input file for message compiler, Linux* OS example:

    $ message-converter.pl --message-file=ru_RU.UTF-8.msg ru_RU.txt

Generate input file for message compiler, Windows* OS example:

    > message-converter.pl --message-file=ru_RU.UTF-8.mc ru_RU.txt

=cut

# end of file #

