package Cupt::Download::Downloader

=head1 SYNOPSIS

  my $config = new Cupt::Config;
  my $downloader = new Cupt::Download::Downloader($config, $uri, $filename);
  $downloader->perform();

=cut

use strict;
use warnings;
use 5.10.0;

use fields qw(_config _uri _filename);

use Cupt::Core;
use WWW::Curl::Easy 4.05;

=head1 METHODS

=head2 new

return the reference to Cupt::Download::Downloader.

Parameters:

I<config> - reference to Cupt::Config

I<uri> - string that determines which URL to download

I<filename> - target file name to download

=cut

sub new {
	my ($class, $config, $uri, $filename) = shift;
	my $self = fields::new($class);
	$self->{_config} = $config;
	$self->{_uri} = $uri;
	$self->{_filename} = $filename;
	return $self;
}

my $curl = new WWW::Curl::Easy;
open(my $fd, '>>', 'downloaded.gz');

my $total_bytes = tell($fd);
sub writefunction {
	print { $_[1] } $_[0];
	my $written_bytes = length($_[0]);
	print "Written bytes: $written_bytes, total bytes: $total_bytes\n";
	$total_bytes += $written_bytes;
	return $written_bytes;
}

$curl->setopt(CURLOPT_URL, "http://ftp.cz.debian.org/debian/dists/experimental/main/binary-amd64/Packages.gz");
$curl->setopt(CURLOPT_MAX_RECV_SPEED_LARGE, 20000);
$curl->setopt(CURLOPT_WRITEFUNCTION, \&writefunction);
$curl->setopt(CURLOPT_WRITEDATA, $fd);
$curl->setopt(CURLOPT_RESUME_FROM, tell($fd));
$curl->perform();
