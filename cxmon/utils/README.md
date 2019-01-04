# What
suspend.bin is a MacBinary file which should be unpacked and run in M68k Macintosh only. It runs emul_op `0x7138` and trigger BasiliskII into cxmon so that you can add break points there.

# How
1. You must build Basilisk II `--with-mon=YES` options.
1. Copy suspend.bin into Macintosh guest OS.
1. Unpack it with MacBinary.
1. Run the program when you want to add break points.
1. Once you are in cxmon, type `h` and you can see the new break point commands.
1. Once you are done, type `x` to return back to emulation.

# Break point commands

```bash
ba [address]             Add a break point
br [breakpoints#]        Remove a break point. If # is 0, remove all break points.
bd [breakpoints#]        Disable a break point. If # is 0, disable all break points.
be [breakpoints#]        Enable a break point. If # is 0, enable all break points.
bi                       List all break points
bs "file"                Save all break points to a file
bl "file"                Load break points from a file
```
