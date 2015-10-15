#!/opt/perlbrew/perls/perl-5.22.0/bin/perl

use strict;
use warnings;

use Getopt::Long;
use YAML qw/LoadFile/;

my $user = $ENV{'SUDO_USER'};
defined $user or die "Failed to provide user\n";
my @posix_groups = get_groups($user);

unless ( grep { $_ eq 'jumpers' } @posix_groups ) { 
    die "You are not a jumper.\n";
}

my %args;
GetOptions(
    'list-hosts' => \$args{'list-hosts'}
);

my $ssh = '/opt/sv-ssh/bin/ssh';
my $key = '/var/jump/s/s';
my @opt = (
#    '-vvv',
    '-o', 'IdentitiesOnly=yes',
    '-o', 'PubkeyAuthentication=yes',
#    '-o', 'GSSAPIAuthentication=no',
    '-o', 'PasswordAuthentication=no',
    '-o', "SendEnv='$user'",
#    '-o', 'LogLevel=QUIET',
);
my @com = ( $ssh, @opt, '-i', $key );

my $config_file = '/var/jump/s/s.yaml';
my $config = LoadFile($config_file);

my @user_hosts;
my $user_ref;
if ( $config->{'users'} ) {
    if ( $config->{'users'}{$user} ) {
        if ( $config->{'users'}{$user}{'hosts'} ) {
            $user_ref = $config->{'users'}{$user}{'hosts'};
        }
    }
}
if ( $user_ref ) {
    @user_hosts = @{ $user_ref };
}

if ( $args{'list-hosts'} ) {
    list_hostnames();
}

my $host = $ARGV[0];
defined $host or die "Failed to provide host\n";

for my $uh (@user_hosts) {
    if ( $config->{'hosts'}{$uh}{'hostname'} eq $host ) {
        my $rhost = $config->{'hosts'}{$uh};
        ssh($rhost);
    }
}

for my $group (@posix_groups) {
    my @members = get_members($group);
    next if scalar @members == 0;
    for my $member (@members) {
        if ( $config->{'hosts'}{$member}{'hostname'} eq $host ) {
            my $rhost = $config->{'hosts'}{$member};
            ssh($rhost);
        }
    }
}

die "You are not allowed to connect to $host.\n";

sub get_groups {
    my ($target_user) = @_;

    my $raw_groups = `/usr/bin/id -Gn "$target_user"`;
    chomp $raw_groups;

    my @groups = split / /, $raw_groups;

    return @groups;
}

sub get_members {
    my ($group) = @_;

    my @members = ();

    my $members_ref = $config->{'groups'}{$group};
    return @members if !$members_ref;

    @members = @{ $members_ref };

    return @members;
}

sub get_remote_user {
    my ($host) = @_;

    my $remote_user;
    if ( defined $host->{'username'} ) {
        $remote_user = $host->{'username'};
    }
    else {
        $remote_user = $user;
    }

    return $remote_user;
}

sub ssh {
    my ($rhost) = @_;

    my $remote_host = $rhost->{'ipaddr'};
    my $remote_user = get_remote_user($rhost);

    printf "\n  [+] Connecting to %s\@%s (%s)\n\n",
      $remote_user, $rhost->{'hostname'}, $remote_host;

    push @com, "$remote_user\@$remote_host";
    system(@com);

    exit;
}

sub list_hostnames {
    my %hostnames;

    for my $uh (@user_hosts) {
        my $hostname = $config->{'hosts'}{$uh}{'hostname'};
        $hostnames{$hostname}++;
    }

    for my $group (@posix_groups) {
        my @members = get_members($group);
        next if scalar @members == 0;

        for my $member (@members) {
            my $hostname = $config->{'hosts'}{$member}{'hostname'};
            $hostnames{$hostname}++;
        }
    }

    for my $key ( keys %hostnames ) {
        if ( $hostnames{$key} > 1 ) {
            warn "$key has multiple entries\n";
        }
        printf "%s\n", $key;
    }

    exit;
}

