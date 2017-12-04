#! /usr/bin/perl

open(S, "basic-dyngen-ops-x86_64.hpp") || die;
open(O, "> basic-dyngen-ops-x86_64_macos.hpp") || die;
select O;
&patch;
close S;
close O;
open(S, "ppc-dyngen-ops-x86_64.hpp") || die;
open(O, "> ppc-dyngen-ops-x86_64_macos.hpp") || die;
select O;
&patch;
close S;
close O;
exit 0;

sub patch {
print << "EOM";
#define ADD_RAX_RCX 0x01,0xc8
#define ADD_RDX_RCX 0x01,0xca
#define ADD_RAX_RDX 0x01,0xd0
#define TRANS_RAX \\
	0x48,0x3D,0x00,0x30,0x00,0x00,\\
	0x72,0x16,\\
	0x48,0x3D,0x00,0xE0,0xFF,0x5F,\\
	0x72,0x14,\\
	0x48,0x25,0xFF,0x1F,0x00,0x00,\\
	0x48,0x05,0x00,0x00,0x00,0x00,\\
	0xEB,0x06,\\
	0x48,0x05,0x00,0x00,0x00,0x00

#define TRANS_RDX \\
	0x48,0x81,0xFA,0x00,0x30,0x00,0x00,\\
	0x72,0x19,\\
	0x48,0x81,0xFA,0x00,0xE0,0xFF,0x5F,\\
	0x72,0x17,\\
	0x48,0x81,0xE2,0xFF,0x1F,0x00,0x00,\\
	0x48,0x81,0xC2,0x00,0x00,0x00,0x00,\\
	0xEB,0x07,\\
	0x48,0x81,0xC2,0x00,0x00,0x00,0x00

#ifdef DYNGEN_IMPL
extern uint8 gZeroPage[0x3000], gKernelData[0x2000];
#endif

EOM
@keys = ("8b0402", "890c10", "891401", "890a", "8910", "882402", "8b00",
	"8902", "0fb620", "8820", "0fb700", "0fb62402", "0fb70402", "890411");
@keys_add_rax_rdx = ("8b0402", "890c10", "882402", "0fb62402", "0fb70402");
@keys_trans_rdx = ("890a", "8902", "890411");
$keys_add{"891401"} = "ADD_RAX_RCX";
$keys_add{"890411"} = "ADD_RDX_RCX";

$keys{$_} = 1 while $_ = shift @keys;
$keys_add{$_} = "ADD_RAX_RDX" while $_ = shift @keys_add_rax_rdx;
$keys_trans_rdx{$_} = 1 while $_ = shift @keys_trans_rdx;
while (<S>) {
	if (/static const uint8 (.+)\[/) {
		print;
		$name = $1;
		$valid = $name =~ /load|store/;
	}
	elsif ($valid) {
		if (/0x/) {
			s/\s//g;
			s/0x//g;
			@code = (@code, split(",", $_));
		}
		elsif (/};/) {
			$n = 0;
			$once = 0;
			@ofs_k = ();
			@ofs_z = ();
			while (1) {
				$found = -1;
				$prefix = 0;
				for ($i = 0; $i < @code - 1; $i++) {
					$key = $code[$i] . $code[$i + 1];
					if ($keys{$key}) {
						$found = $i;
						$once = 1;
						last;
					}
					$key .= $code[$i + 2];
					if ($keys{$key}) {
						$found = $i;
						$once = 1;
						last;
					}
					$key .= $code[$i + 3];
					if ($keys{$key}) {
						$found = $i;
						$once = 1;
						last;
					}
				}
				if ($i > 0 && $code[$i - 1] =~ /44|48|66/) {
					$prefix = 1;
					$found--;
				}
				last if $found < 0;
				$n += $found;
				print "       ";
				for ($i = 0; $i < $found; $i++) {
					printf "0x%s", shift @code;
					print @code ? $i % 12 == 11 ? ",\n       " : ", " : "\n    };\n";
				}
				if ($keys_add{$key}) {
					$n += 2;
					printf "\n       %s,", $keys_add{$key};
				}
				$trans_rdx = $keys_trans_rdx{$key};
				push @ofs_k, $n + ($trans_rdx ? 0x1c : 0x18);
				push @ofs_z, $n + ($trans_rdx ? 0x25 : 0x20);
				$n += $trans_rdx ? 0x29 : 0x24;
				printf "\n       TRANS_%s,\n       ", $trans_rdx ? "RDX" : "RAX";
				if ($prefix) {
					$n++;
					printf "0x%02s, ", shift @code;
				}
				if ($keys_add{$key}) {
					$n += 2;
					if (length($key) == 8) {
						$n++;
						printf "0x%02s, ", shift @code if length($key) == 8;
					}
					printf "0x%02s, ", shift @code;
					printf "0x%02x,\n", hex(shift @code) - ($keys_add{$key} =~ /RAX/ ? 4 : 2);
					shift @code;
				}
				else {
					for ($i = 0; $i < length($key); $i += 2) {
						$n++;
						printf "0x%s, ", shift @code;
					}
					print "\n";
				}
			}
			$valid = $once;
			if (@code) {
				$n += @code;
				print "       ";
				for ($i = 0; @code; $i++) {
					printf "0x%s", shift @code;
					printf "%s", @code ==0 ? "\n" : $i % 12 == 11 ? ",\n       " : ", ";
				}
			}
			print "    };\n";
		}
		elsif (/copy_block/) {
			printf "    copy_block(%s, %d);\n", $name, $n;
			printf "    *(uint32_t *)(code_ptr() + %d) = (uint32_t)(uintptr)gKernelData;\n", shift @ofs_k while @ofs_k;
			printf "    *(uint32_t *)(code_ptr() + %d) = (uint32_t)(uintptr)gZeroPage;\n", shift @ofs_z while @ofs_z;
		}
		elsif (/inc_code_ptr/) {
			printf "    inc_code_ptr(%d);\n", $n;
		}
		else {
			print;
		}
	}
	else {
		print;
	}
}
}
