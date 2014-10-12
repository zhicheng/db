#!/usr/bin/env python

import sys 
import struct

def main():
	with open(sys.argv[1], 'rb') as file:
		data = file.read(40)
		magic, version, head, tail, toff, tlen = struct.unpack('IIQQQQ', data)
		if toff != 0 and tlen != 0:
			file.seek(toff + tlen * 24, 0) # ignore table if has
		else:
			file.seek(head, 0)

		db  = {}
		off = 0
		while True:
			data = file.read(8)
			if len(data) != 8:
				break
			klen, vlen = struct.unpack('II', data)
			if klen == 0 or vlen == 0:	# not data or deleted data
				file.seek(klen + vlen, 1)
				continue
			key = file.read(klen)
			val = file.read(vlen)
			if len(key) != klen or len(val) != vlen:
				break;
			db[key] = val
			print '%s: %s' % (key, val)
		print len(db)
		print tail - head
	
if __name__ == '__main__':
	main()
