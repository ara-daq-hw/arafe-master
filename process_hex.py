#!/usr/bin/python
import hexfile
import sys
import struct

#
# Dumb program for processing an Energia-outputted hex file
# into a flat binary file for arafebsl to program.
#
# Just do ./process_hex.py input.hex output.out.
#

[fin,fout] = sys.argv[1:]
f = hexfile.load(fin)
o = open(fout, "wb")
start_address = 0xC200
end_address = 0xFB80
vector_address = 0xFF80
data = None
vecs = None
for seg in f.segments:
    if seg.start_address == start_address:
        data = seg
    elif seg.start_address == vector_address:
        vecs = seg
address = start_address
raw = data.data
if data.size > end_address - start_address:
    print "Sketch too large!"
    exit
for val in raw:
    o.write(struct.pack('B', val))
    address = address + 1
while address < end_address:
    o.write(struct.pack('B', 0xFF))
    address = address + 1
raw = vecs.data
for val in raw:
    o.write(struct.pack('B', val))
    address = address + 1
