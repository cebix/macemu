#!/usr/bin/perl
use strict;

my (@handlers, @extra_handlers, %templates);

sub split_arglist($) {
	(map { $_ =~ s/\s//g; $_ } split ",", $_[0]);
}

my @lines = map { split ";", $_ } (<STDIN>);

my $is_template = 0;
my $e;
foreach (@lines) {
	$_ =~ s/;/&\n/g;
	if (/^DEFINE_TEMPLATE\((\w+),.+,.+\((.+)\)\)/) {
		$is_template = 1;
		$e = { name => $1 };
		push @{$e->{args}}, split_arglist $2;
	}
	elsif ($is_template && /^\}/) {
		$is_template = 0;
		$templates{$e->{name}} = $e;
	}
	elsif (/(powerpc_cpu::execute_\w+)<(.+)>/) {
		my $h = { name => $1, args => $2 };
		if ($is_template) {
			push @{$e->{handlers}}, $h;
		}
		else {
			push @handlers, $h;
		}
	}
	elsif (/template.+decode_(\w+)<(.+)>/) {
		my $template = $templates{$1};
		my @template_args = @{$template->{args}};
		my @args = split_arglist $2;
		my %vars;
		$vars{$template_args[$_]} = $args[$_] foreach (0 .. $#template_args);
		foreach my $h (@{$template->{handlers}}) {
			my @new_args = map { $vars{$_} || $_ } split_arglist $h->{args};
			push @extra_handlers, { name => $h->{name}, args => join(", ", @new_args) };
		}
	}
}

my %output_handlers;
foreach (@handlers, @extra_handlers) {
	my $line = "template void $_->{name}<".join(", ", $_->{args}).">(uint32);";
	print "$line\n" if (!$output_handlers{$line});
	$output_handlers{$line} = 1;
}
