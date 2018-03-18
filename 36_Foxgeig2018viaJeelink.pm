
# $Id: 36_Foxgeig2018viaJeelink.pm $
# This is mostly a modified version of 36_LaCrosse.pm from FHEM.
# You need to put this into /opt/fhem/FHEM/ and to make it work, you'll
# also need to modify 36_JeeLink.pm: to the string "clientsJeeLink" append
# ":Foxgeig2018viaJeelink".

package main;

use strict;
use warnings;
use SetExtensions;

sub Foxgeig2018viaJeelink_Parse($$);


sub Foxgeig2018viaJeelink_Initialize($) {
  my ($hash) = @_;
                       # OK CC 21 249 0 0 26 255 255 255 161
  $hash->{'Match'}     = '^\S+\s+CC\s+\d+\s+249\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s*$';  # FIXME
  $hash->{'SetFn'}     = "Foxgeig2018viaJeelink_Set";
  ###$hash->{'GetFn'}     = "Foxgeig2018viaJeelink_Get";
  $hash->{'DefFn'}     = "Foxgeig2018viaJeelink_Define";
  $hash->{'UndefFn'}   = "Foxgeig2018viaJeelink_Undef";
  $hash->{'FingerprintFn'}   = "Foxgeig2018viaJeelink_Fingerprint";
  $hash->{'ParseFn'}   = "Foxgeig2018viaJeelink_Parse";
  ###$hash->{'AttrFn'}    = "Foxgeig2018viaJeelink_Attr";
  $hash->{'AttrList'}  = "IODev"
    ." ignore:1,0"
    ." $readingFnAttributes";

  $hash->{'AutoCreate'} = { "Foxgeig2018viaJeelink.*" => { autocreateThreshold => "2:120", FILTER => "%NAME" }};

}

sub Foxgeig2018viaJeelink_Define($$) {
  my ($hash, $def) = @_;
  my @a = split("[ \t][ \t]*", $def);

  if ((int(@a) < 3) || (int(@a) > 3)) {
    my $msg = "wrong syntax: define <name> Foxgeig2018viaJeelink <addr>";
    Log3 undef, 2, $msg;
    return $msg;
  }

  $a[2] =~ m/^(\d{1,3}|0x[\da-f]{2})$/i;
  unless (defined($1)) {
    return "$a[2] is not a valid Foxgeig2018 address";
  }

  my $name = $a[0];
  my $addr;
  if ($a[2] =~ m/0x(.+)/) {
    $addr = $1;
  } else {
    $addr = sprintf("%02x", $a[2]);
  }

  return "Foxgeig2018viaJeelink device $addr already used for $modules{Foxgeig2018viaJeelink}{defptr}{$addr}->{NAME}." if( $modules{Foxgeig2018viaJeelink}{defptr}{$addr} && $modules{Foxgeig2018viaJeelink}{defptr}{$addr}->{NAME} ne $name );

  $hash->{addr} = $addr;

  $modules{Foxgeig2018viaJeelink}{defptr}{$addr} = $hash;

  AssignIoPort($hash);
  if (defined($hash->{IODev}->{NAME})) {
    Log3 $name, 3, "$name: I/O device is " . $hash->{IODev}->{NAME};
  } else {
    Log3 $name, 1, "$name: no I/O device";
  }

  return undef;
}


#-----------------------------------#
sub Foxgeig2018viaJeelink_Undef($$) {
  my ($hash, $arg) = @_;
  my $name = $hash->{NAME};
  my $addr = $hash->{addr};

  delete( $modules{Foxgeig2018viaJeelink}{defptr}{$addr} );

  return undef;
}


#-----------------------------------#
sub Foxgeig2018viaJeelink_Get($@) {
  my ($hash, $name, $cmd, @args) = @_;

  return "\"get $name\" needs at least one parameter" if(@_ < 3);

  my $list = "";

  return "Unknown argument $cmd, choose one of $list";
}

#-----------------------------------#
sub Foxgeig2018viaJeelink_Attr(@) {
  my ($cmd, $name, $attrName, $attrVal) = @_;

  return undef;
}


#-----------------------------------#
sub Foxgeig2018viaJeelink_Fingerprint($$) {
  my ($name, $msg) = @_;

  return ( "", $msg );
}


#-----------------------------------#
sub Foxgeig2018viaJeelink_Set($@) {
  my ($hash, $name, $cmd, $arg, $arg2) = @_;

  my $list = "";

  if( $cmd eq "nothingToSetHereYet" ) {
    #
  } else {
    return "Unknown argument $cmd, choose one of ".$list;
  }

  return undef;
}

#-----------------------------------#
sub Foxgeig2018viaJeelink_Parse($$) {
  my ($hash, $msg) = @_;
  my $name = $hash->{NAME};

  my ( @bytes, $addr, $cpm1min, $cpm60min );
  my $batvolt = -1.0;

  if ($msg =~ m/^OK CC /) {
    # OK CC 21 249 0 0 26 255 255 255 161
    # c+p from main.c. Warning: All Perl offsets are off by 2!
    # Byte  2: Sensor-ID (0 - 255/0xff)
    # Byte  3: Sensortype (=0xf9 for FoxGeig)
    # Byte  4: CountsPerMinute for last minute, MSB
    # Byte  5: CountsPerMinute for last minute,
    # Byte  6: CountsPerMinute for last minute, LSB
    # Byte  7: CountsPerMinute for last 60 minutes, MSB
    # Byte  8: CountsPerMinute for last 60 minutes,
    # Byte  9: CountsPerMinute for last 60 minutes, LSB
    # Byte 10: Battery voltage (0-255, 255 = 6.6V)
    @bytes = split( ' ', substr($msg, 6) );

    if (int(@bytes) != 9) {
      DoTrigger($name, "UNKNOWNCODE $msg");
      return "";
    }
    if ($bytes[1] != 0xF9) {
      DoTrigger($name, "UNKNOWNCODE $msg");
      return "";
    }

    #Log3 $name, 3, "$name: $msg cnt ".int(@bytes)." addr ".$bytes[0];

    $addr = sprintf( "%02x", $bytes[0] );
    $cpm1min = ($bytes[2] << 16) | ($bytes[3] << 8) | ($bytes[4] << 0);
    $cpm60min = ($bytes[5] << 16) | ($bytes[6] << 8) | ($bytes[7] << 0);
    $batvolt = sprintf("%.2f", (6.6 * $bytes[8] / 255.0));
  } else {
    DoTrigger($name, "UNKNOWNCODE $msg");
    return "";
  }

  my $raddr = $addr;
  my $rhash = $modules{Foxgeig2018viaJeelink}{defptr}{$raddr};
  my $rname = $rhash ? $rhash->{NAME} : $raddr;

  return "" if( IsIgnored($rname) );

  if ( !$modules{Foxgeig2018viaJeelink}{defptr}{$raddr} ) {
    # get info about autocreate
    my $autoCreateState = 0;
    foreach my $d (keys %defs) {
      next if ($defs{$d}{TYPE} ne "autocreate");
      $autoCreateState = 1;
      $autoCreateState = 2 if(!AttrVal($defs{$d}{NAME}, "disable", undef));
    }

    # decide how to log
    my $loglevel = 4;
    if ($autoCreateState < 2) {
      $loglevel = 3;
    }

    Log3 $name, $loglevel, "Foxgeig2018viaJeelink: Unknown device $rname, please define it";

    return "";
  }

  my @list;
  push(@list, $rname);

  $rhash->{"Foxgeig2018viaJeelink_lastRcv"} = TimeNow();
  $rhash->{"sensorType"} = "Foxgeig2018viaJeelink";

  readingsBeginUpdate($rhash);

  # What is it good for? I haven't got the slightest clue, and the FHEM docu
  # about it could just as well be in Russian, it's absolutely not understandable
  # (at least for non-seasoned FHEM developers) what this is actually used for.
  readingsBulkUpdate($rhash, "state", "Initialized");
  # Round and write temperature and humidity
  if ($cpm1min != 0xFFFFFF) { # 0xFFFFFF means the reading is invalid.
    readingsBulkUpdate($rhash, "cpm1min", $cpm1min);
    # The magic 0.0057 value comes from the original mightyohm firmware.
    # It's pretty much guesswork anyways, because you cannot know how much
    # of which type of radiation you counted.
    my $deriveddosage = 0.0057 * $cpm1min;
    readingsBulkUpdate($rhash, "raddose1min", sprintf("%.3f", $deriveddosage));
  }
  if ($cpm60min != 0xFFFFFF) { # 0xFFFFFF means the reading is invalid.
    readingsBulkUpdate($rhash, "cpm60min", $cpm60min);
    my $deriveddosage = 0.0057 * $cpm60min;
    readingsBulkUpdate($rhash, "raddose60min", sprintf("%.3f", $deriveddosage));
  }

  if ($batvolt > 0.0) {
    readingsBulkUpdate($rhash, "batteryLevel", $batvolt);
  }

  readingsEndUpdate($rhash,1);

  return @list;
}


1;

=pod
=begin html

<a name="Foxgeig2018viaJeelink"></a>
<h3>Foxgeig2018viaJeelink</h3>
<ul>

  <tr><td>
  FHEM module for the Foxgeig2018-device.<br><br>

  It can be integrated into FHEM via a <a href="#JeeLink">JeeLink</a> as the IODevice.<br><br>

  On the JeeLink, you'll need to run a slightly modified version of the firmware
  for reading LaCrosse (found in the FHEM repository as
  /contrib/36_LaCrosse-LaCrosseITPlusReader.zip):
  It has support for a sensor type called "CustomSensor", but that is usually
  not compiled in. There is a line<br>
   <code>CustomSensor::AnalyzeFrame(payload);</code><br>
  commented out in <code>LaCrosseITPlusReader10.ino</code> -
  you need to remove the `////` to enable it, then recompile the firmware
  and flash the JeeLink.<br><br>

  You need to put this into /opt/fhem/FHEM/ and to make it work, you'll
  also need to modify 36_JeeLink.pm: to the string "clientsJeeLink" append
  ":Foxgeig2018viaJeelink".<br><br>

  <a name="Foxgeig2018viaJeelink_Define"></a>
  <b>Define</b>
  <ul>
    <code>define &lt;name&gt; Foxgeig2018viaJeelink &lt;addr&gt;</code> <br>
    <br>
    addr is the ID of the sensor, either in decimal or (prefixed with 0x) in hexadecimal notation.<br>
  </ul>
  <br>

  <a name="Foxgeig2018viaJeelink_Set"></a>
  <b>Set</b>
  <ul>
    <li>there is nothing to set here.</li>
  </ul><br>

  <a name="Foxgeig2018viaJeelink_Get"></a>
  <b>Get</b>
  <ul>
  </ul><br>

  <a name="Foxgeig2018viaJeelink_Readings"></a>
  <b>Readings</b>
  <ul>
    <li>batvolt (V)<br>
      the battery voltage of the LiPo battery in volts.</li>
    <li>cpm1min<br>
      the radiation measurement for the last minute, in Counts Per Minute.</li>
    <li>cpm60min<br>
      the radiation measurement averaged over the last 60 minutes, in Counts Per Minute.</li>
  </ul><br>

  <a name="Foxgeig2018viaJeelink_Attr"></a>
  <b>Attributes</b>
  <ul>
    <li>ignore<br>
    1 -> ignore this device.</li>
  </ul><br>

  <b>Logging and autocreate</b><br>
  <ul>
  <li>If autocreate is not active (not defined or disabled) and LaCrosse is not contained in the ignoreTypes attribute of autocreate then
  the <i>Unknown device xx, please define it</i> messages will be logged with loglevel 3. In all other cases they will be logged with loglevel 4. </li>
  <li>The autocreateThreshold attribute of the autocreate module (see <a href="#autocreate">autocreate</a>) is respected. The default is 2:120, means, that
  autocreate will create a device for a sensor only, if the sensor was received at least two times within two minutes.</li>
  </ul>

</ul>

=end html
=cut
