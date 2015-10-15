#!/opt/perlbrew/perls/perl-5.22.0/bin/perl

use strict;
use warnings;

use Getopt::Long;
use YAML qw/LoadFile/;
use Cwd qw/abs_path/;
use File::Basename;

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

my $scp = '/opt/sv-ssh/bin/scp';
my $key = '/var/jump/s/s';
my @opt = (
    '-r',
    '-o', 'IdentitiesOnly=yes',
    '-o', 'PubkeyAuthentication=yes',
    '-o', 'PasswordAuthentication=no',
    '-o', "SendEnv='$user'",
);
my @com = ( $scp, @opt, '-i', $key );

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

my $_src = $ARGV[0];
my $_dst = $ARGV[1];
defined $_src or die "Failed to provide source\n";
defined $_dst or die "Failed to provide destination.\n";

my ( $host, $src, $dst, $direction ) = get_params( $_src, $_dst );
defined $host or die "Failed to provide host\n";

for my $uh (@user_hosts) {
    if ( $config->{'hosts'}{$uh}{'hostname'} eq $host ) {
        my $rhost = $config->{'hosts'}{$uh};
        scp( $rhost, $src, $dst, $direction );
    }
}

for my $group (@posix_groups) {
    my @members = get_members($group);
    next if scalar @members == 0;
    for my $member (@members) {
        if ( $config->{'hosts'}{$member}{'hostname'} eq $host ) {
            my $rhost = $config->{'hosts'}{$member};
            scp( $rhost, $src, $dst, $direction );
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

sub scp {
    my ( $rhost, $src, $dst, $direction ) = @_;

    my $remote_host = $rhost->{'ipaddr'};
    my $remote_user = get_remote_user($rhost);

    if ( $direction eq 'local' ) {
        check_local($dst);
        push @com, "$remote_user\@$remote_host:$src", "$dst";
    }
    elsif ( $direction eq 'remote' ) {
        check_local($src);
        push @com, "$src", "$remote_user\@$remote_host:$dst";
    }
    else {
        die "Invalid direction\n";
    }

    printf "\n  [+] Connecting to %s\@%s (%s)\n\n",
      $remote_user, $rhost->{'hostname'}, $remote_host;

    system(@com);

    if ( $direction eq 'local' ) {
        my @chown;

        if ( -f "$dst/$src" ) {
            print "DEBUG -- its a dir\n";
            @chown = ( 'chown', "$user:$user", "$dst/$src" );
        }
        else {
            $src = basename($src);
            @chown = ( 'chown', '-R', "$user:$user", "$dst/$src" );
        }
        system(@chown);
    }

    exit;
}

sub get_params {
    my ( $_src, $_dst ) = @_;

    my ( $host, $src, $dst, $direction );
    if ( $_src =~ /^(\S+):(\S+)$/ ) {
        ( $host, $src ) = ( $1, $2 );
        $direction = 'local';
    }
    else {
        $src = $_src;
    }
    
    if ( $_dst =~ /^(\S+):(\S+)$/ ) {
        ( $host, $dst ) = ( $1, $2 );
        $direction = 'remote';
    }
    else {
        $dst = $_dst;
    }

    if ( !$host and !$src and !$dst and !$direction ) {
        die "Failed to provide usable targets.\n";
    }

    return ( $host, $src, $dst, $direction );
}

sub check_local {
    my ($file) = @_;

    my $uid;
    my $user_uid = getpwnam $user;

    if ( -x $file ) {
        $uid = (stat $file)[4];
    }
    else {
        my $dir_name = dirname($file);
        my $dir = abs_path($dir_name);
        $uid = (stat $dir)[4];
        $file = $dir;
    }

    if ( $uid != $user_uid ) {
        die "You do not own '$file'\n";
    }

    return;
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

