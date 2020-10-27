# SEA: Secure Encryption Algorithm

This is a program which can be used to encrypt any given file,
by shifting bytes of the file by pseudorandom numbers.

This sequence of pseudorandom numbers acts as the key, and is
written to a USB drive. Essentially, the USB drive acts as
a physical key.

## Installation

<ol>
	<li>Change to the source directory.</li>
	<li>Run the following commands:</li>
</ol>

```
make
make install
```

## Usage Information

```
Usage: sea <-e | -d> <file name> <key device>

SEA is a program which can be used to encrypt any given file,
by shifting bytes of the file by pseudorandom numbers.

Options:
	-e, --encrypt <file name>	Encrypt given file
	-d, --decrypt <file name>	Decrypt given file
	-h. --help			Show this help message
	-V, --version			Show version information

NOTE: This program requires root privileges for writing encryption key to
the given device, or to read the encryption key from the same device.
```

See "COPYING" for license information.
