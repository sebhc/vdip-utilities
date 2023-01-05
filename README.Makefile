For building VDIP utilities using vcpm, SWTW C/80, and DRI RMAC/LINK.

NOTE: For vcpm, all filenames must be lowercase.

SETUP

'vcpm' (virtual CP/M) uses a JAVA JAR file obtained from
https://github.com/durgadas311/virtual-cpm/blob/master/bin/VirtualCpm.jar.
See https://github.com/durgadas311/virtual-cpm/blob/master/doc/VirtualCpm.pdf
for documentation.

The Makefile expects there to be a script named 'vcpm' on the user's PATH.
This script is described in the VirtualCpm documentation.  Don't forget
to set the script executable, e.g. "chmod +x ~/bin/vcpm".

If the H8D images are to also be built, then "format.jar" is needed.
Download http://sebhc.durgadas.com/mms89/format.jar and create a script
on your PATH to invoke it, e.g.

	java -jar /path/to/format.jar "${@}"

Then add "FMT=<script>" to your 'make' command lines.

CP/M FILES NEEDED

At a minimum, install SWTW C/80 and RMAC/LINK in your vcpm A: directory.
Remember, all files there must have lowercase names. The minimal required
files are:

	c.com		clibrary.rel	fprintf.h	flibrary.rel
	printf.h	stdlib.rel	rmac.com	link.com

Other SWTW C/80 files are required, from both CP/M and HDOS versions.
Rather than try to define a minimum set of files, it is easier to just
copy all files from the C/80 disks 1 and 2 and the "mathpack" disk.
Remember, these files must have lowercase names.

The default directory for CP/M C/80 files is ~/swtw-c80-cpm.
The default directory for HDOS C/80 files is ~/swtw-c80-hdos.
These directory names can be overridden using the MAKE variables
CSRC and HSRC, although it is probably simpler to just use those names.

The default location for the build results (.COM and .ABS files) will be
"./build", with subdirectories "cpm" and "hdos". This can be changed using
the BLD variable, if necessary.

Note that 'make' allows setting variables on the commandline,
which overrides default settings in the Makefile.

ERROR HANDLING

Note that CP/M commands do not have an "exit status" and thus cannot
report success/failure. This means that 'make' does not know if the
command failed. There are some make rules used to try and verify that
a command succeeded, however it is possible that a command may fail
in such a way that it cannot be reasonably detected. It is a good idea
to run make commands, at least those that are building all utilities,
with the output redirected to a log file, which can be examined later
to check for errors.  Note that this is not too different than SUBMIT
on CP/M, which also could not detect errors and abort.

EXAMPLES

These examples assume 'vcpm' is already setup.

To make both cpm and hdos utilities, and copy all output to "make.log":

	make cpm hdos 2>&1 | tee make.log

To make a quick check for errors, the following command may be used:

	grep -Ei 'error|undef|multi' make.log

The only output from the above command should be "0 errors in compilation"
lines.

Individual C files may be compiled/assembled using the following (in
this case, VDIR):

	make vdir.rel

Note that 'make' will only recompile files that have changed since
the last compile.  If you want to force a recompile (without change
the source file), you can use the command "touch file.c" or the "-B"
option to 'make'.

Only CP/M utilties will be built using "make cpm". Likewise, only HDOS
utilities will be built using "make hdos".

The ZIP and (optionally) H8D images are built with a separate step.
Use the target "cpm-img" to build the CP/M ZIP (and H8D).  Use the target
"hdos-img" to build the HDOS ZIP (and H8D).  Use the target "img" to
build all. For example, to build CP/M and HDOS images, with format.jar
installed using a script named 'format', use the command:

	make img FMT=format

NOTES ON L80 VS LINK

Microsoft's L80 produces executable files slightly differently than
Digital Research's LINK.

L80:        CP/M                         HDOS
       -------------                -------------
                                    FF 00 (hdos ident)
                                    80 22 (load address)
                                    LL LL (load length)
                                    EE EE (ENTRY/start address)
       [JMP ENTRY] (*org 0100h*)
       [data - dseg]                [data - dseg] (*org 2280h*)
       [code - cseg] <-ENTRY        [code - cseg] <-ENTRY


LINK:       CP/M                    HDOS on CP/M (*)
       -------------                -------------
                                    FF 00 (hdos ident)
                                    80 22 (load address)
                                    SS SS (file size)
                                    80 22 (start address)
       [JMP ENTRY] (*org 0100h*)    [JMP ENTRY] (*org 2280h*)
       [code - cseg] <-ENTRY        [code - cseg] <-ENTRY
       [data - dseg]                [data - dseg]

(*) HDOS executables are completed by adding the header after LINK.
This results in two steps, REL... -> BIN, then BIN -> ABS.

Executables produced using LINK on CP/M will not exactly match those
produced on their native OS with L80.  The main reason is that the order
of cseg and dseg is reversed.

NOTES ON HDOS

For cross-building HDOS, we seem to need RMAC/LINK to get the desired
behavior for org 2280 binaries. LINK may not tolerate REL files produced
by M80, so need to use RMAC also.

The ABS file is slightly different compared to what is produced by the
HDOS version of L80.  Because of the difficulty of obtaining the ENTRY
address after LINK/L80 is finished (clibrary does not export the symbol),
we let the linker add a JMP ENTRY to the start of the program, and then
use 2280H as both the load address and the ENTRY address.

Thus, the ABS header is equivalent to:

	db	0ffh,0
	dw	2280h
	dw	length
	dw	2280h

Where 'length' is computed from the size of the linker output *file*,
as opposed to the total size of the data and code sections.  If the
programs are changed to have ".bss" (unitialized) data, this computation
of 'length' may need to change. This 'length'may be slightly larger than
what is produced by the HDOS version of L80, because we don't have easy
access to the symbols that define the end of program and data space. This
ABS header is generated by the script "c-hdos/hdosabs".

Also, because of differences between LINK and L80 which affect allocation
of memory, a modified CLIBRARY.REL for HDOS is used.  This library has
been modified to use the same code as the CP/M version for handling
differences in LINK and L80 w.r.t.  auto-detecting the program "heap"
address.  This modified library is in "c-hdos/clibrary.rel".
