import sys

import mouse


if len(sys.argv) < 2:
	print("Usage: accmouse.py <tty device file>")

with open(sys.argv[1], 'r') as ttydevice:
	for line in ttydevice:
		if len(line) == 1: # Omit 'empty' lines
			continue

		acc = int(line.strip())
		acc = (acc + 127) % 256 - 128
		
		mouse.move(1080 * 0, 720 * 0 + acc, absolute=False)
		
