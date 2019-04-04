# SEA: Secure Encryption Algorithm

This is a program which can be used to encrypt any given file,
by shifting bytes of the file by (pseudo)random numbers.

This sequence of (pseudo)random numbers acts as the key, and this
can be written to a USB drive. Without this USB drive, decryption
of the file is almost impossible. Essentially, the USB drive becomes
the key.

## Installation

<ol>
	<li>Change to the source directory.</li>
	<li>Run the following commands:</li>
</ol>

```
sudo make
sudo make install
```

## Usage Information

```
Usage: sea <-e | -d> <file name> <key device>

SEA is a program which can be used to encrypt any given file,
by shifting bytes of the file by (pseudo)random numbers.

Options:
	-e, --encrypt <file name>	Encrypt given file
	-d, --decrypt <file name>	Decrypt given file
	-h. --help			Show this help message
	-V, --version			Show version information

NOTE: This program requires root privileges. For writing encryption key to
the given device, or to read the encryption key from the same device.
```

See "COPYING" for license information.

Tested on Elementary OS 5.0 (Juno)
