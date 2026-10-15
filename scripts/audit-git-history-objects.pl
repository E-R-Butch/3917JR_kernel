#!/usr/bin/env perl
use strict;
use warnings;
use IPC::Open2;

die "usage: $0 <revision> [revision ...]\n" unless @ARGV;

open my $object_format_fh, '-|', 'git', 'rev-parse', '--show-object-format'
  or die "cannot query Git object format: $!\n";
my $object_format = <$object_format_fh> // '';
close $object_format_fh or die "cannot query Git object format\n";
chomp $object_format;
my $hash_bytes = $object_format eq 'sha256' ? 32 : 20;

open my $rev_list, '-|', 'git', 'rev-list', '--objects', @ARGV
  or die "cannot enumerate reachable Git objects: $!\n";
my @objects;
while (my $line = <$rev_list>) {
  chomp $line;
  $line =~ /^([0-9a-f]+)(?: (.*))?$/
    or die "unexpected rev-list output: $line\n";
  push @objects, [$1, defined $2 ? $2 : ''];
}
close $rev_list or die "git rev-list failed\n";

my ($batch_out, $batch_in);
my $pid = open2(
  $batch_out,
  $batch_in,
  'git',
  'cat-file',
  '--batch=%(objectname) %(objecttype) %(objectsize)',
);
$batch_in->autoflush(1);
binmode $batch_out;
binmode $batch_in;

my $failed = 0;

sub read_exact {
  my ($fh, $length) = @_;
  my $data = '';
  while (length($data) < $length) {
    my $read = read($fh, my $chunk, $length - length($data));
    defined $read or die "cannot read git cat-file stream: $!\n";
    $read > 0 or die "unexpected end of git cat-file stream\n";
    $data .= $chunk;
  }
  return $data;
}

for my $entry (@objects) {
  my ($requested_oid, $path) = @$entry;
  print {$batch_in} "$requested_oid\n";

  my $header = <$batch_out> // die "missing git cat-file header\n";
  chomp $header;
  $header =~ /^([0-9a-f]+) (\S+) (\d+)$/
    or die "unexpected git cat-file header: $header\n";
  my ($oid, $type, $size) = ($1, $2, $3);
  my $content = read_exact($batch_out, $size);
  read_exact($batch_out, 1) eq "\n"
    or die "missing git cat-file record terminator\n";

  my $display_path = length($path) ? $path : '<root>';

  if ($type eq 'blob') {
    if ($size > 99_614_720) {
      print "LARGE\t$size\t$oid\t$display_path\n";
      $failed = 1;
    }
    if (substr($content, 0, 4) eq "\x7f" . 'ELF') {
      print "ELF\t$oid\t$display_path\n";
      $failed = 1;
    }
    if ($content =~ /-----BEGIN (?:[A-Z0-9]+ )?PRIVATE KEY-----/) {
      print "PRIVATE_KEY\t$oid\t$display_path\n";
      $failed = 1;
    }
    next;
  }

  next unless $type eq 'tree';

  my $offset = 0;
  while ($offset < length($content)) {
    my $space = index($content, ' ', $offset);
    $space >= 0 or die "malformed tree object $oid\n";
    my $mode = substr($content, $offset, $space - $offset);
    my $nul = index($content, "\0", $space + 1);
    $nul >= 0 or die "malformed tree object $oid\n";
    my $name = substr($content, $space + 1, $nul - $space - 1);
    $offset = $nul + 1 + $hash_bytes;
    $offset <= length($content) or die "malformed tree object $oid\n";

    if ($mode eq '160000') {
      my $gitlink_path = $display_path eq '<root>'
        ? $name
        : "$display_path/$name";
      print "GITLINK\t$oid\t$gitlink_path\n";
      $failed = 1;
    }
  }
}

close $batch_in or die "cannot close git cat-file input\n";
close $batch_out or die "git cat-file failed\n";
waitpid($pid, 0);
die "git cat-file failed\n" if $? != 0;

exit($failed ? 1 : 0);
