import sys
import re
import os.path

# this class implements a filter that produces
# a C array of unsigned integers

class CDataOutput:
	def __init__(self, sprite_name):
		self.col = 0
		self.wrapcol = 74
		self.sprite_name = sprite_name
		self.first = True
		self._output = []

	def _out(self, s):
		if self.col > self.wrapcol:
			self._output.append(",\n")
			self.first = True
			self.col = 0
		if not self.first:
			self._output.append(", ")
		else:
			self._output.append("  ")
			self.first = False
		self._output.append(s)
		self.col += len(s)
	
	def out(self, r, g, b, a):
		self._out("0x{:02x}{:02x}{:02x}{:02x}".format(r, g, b, a))	

	def finish(self):
		return "unsigned int {}[] = {{\n{}\n}};".format(self.sprite_name, "".join(self._output))


class SpriteCutter:
	swidth = 24
	sheight = 21
	sbytes = swidth * sheight / 4
	bsize = 65536

	def __init__(self, snap_file, offset=132):
		self.out_type = None
		with open(snap_file, "rb") as f:
			buf = ""
			while True:
				_bytes = f.read(self.bsize)
				if _bytes:
					buf = "".join((buf, _bytes))
				else:
					break
			self.memview = memoryview(bytes(buf))[offset:]
			name=re.split("[^a-zA-Z]", os.path.basename(snap_file))
			lname = [ n.lower() for n in name if len(n) ]
			self.sprites_name = "".join(lname)
	
	def _addr(self, addr):
		self.sview = self.memview[addr:addr+self.sbytes]
		self.addr = addr

	def goto(self, number):
		self._addr(64 * number)
	
	def cut(self, mode="multi", scale=2, outline=None, remap=None, mirror_h=False, mirror_v=False, rotate=0):
		self.scale = scale
		self.width = self.swidth * scale
		self.height = self.sheight * scale
		self.bitmap = bytearray([0] * self.width * self.height)

		if mode == "multi":
			self.cut_mc()
		else:
			self.cut_sc()
		
		if outline:
			self._outline(outline)

		# at this point we have an img which is 24s x 21s
		# add 1s empty lines to the top and 2s empty lines
		# at the bottom to make a perfect square 

		prepend = [[0]*self.width]
		append = [[0]*2*self.width]
		new_bitmap = []
		new_bitmap.extend(prepend)
		new_bitmap.extend(self.bitmap)
		new_bitmap.extend(append)
		self.height += 3 * scale
		self.bitmap = bytearray(new_bitmap)

		if mirror_h:
			self._mirror_h()

		if mirror_v:
			self._mirror_v()

		if rotate:
			self._rotate(rotate)

		if remap:
			self._remap(remap)
			
	def convert(self):
		out=[self.width]
		out.extend(self.bitmap)
		return bytes(out)

	def _mirror_h(self):
		bitmap = bytearray([0] * self.width * self.height)
		for y in range(self.height):
			new_line = bitmap[y*self.width:]
			org_line = self.bitmap[y*s.width + s.width - 1:]
			for x in range(self.width):
				new_line[x] = org_line[-x]
		self.bitmap = bitmap

	def _mirror_v(self):
		bitmap = []
		for y in range(self.height):
			baddr = self.height - 1 - y
			bitmap.extend(self.bitmap[baddr:baddr+self.width])
		self.bitmap = bytearray(bitmap)

	def _rot_plus_90(self):
		bitmap = bytearray([0] * self.width * self.height)
		for y in range(self.height):
			for x in range(self.width):
				bitmap[y*self.width+x] = self.bitmap[x*self.width+y]
		self.bitmap = bitmap

	def _rot_minus_90(self):
		bitmap = bytearray([0] * self.width * self.height)
		for y in range(self.height):
			for x in range(self.width):
				bitmap[x*self.width+y] = self.bitmap[y*self.width+x]
		self.bitmap = bitmap

	def _rot_180(self):
		self.mirror_h()
		self.mirror_v()

	def _rotate(self, rotate):
		if rotate == 90:
			self._rot_plus_90()
		elif rotate == -90:
			self._rot_minus_90()
		elif rotate == 180:
			self._rot_180()

	def _pixeltest(self, x, y):
		if y < 0 or y >= self.height:
			return True
		if x < 0 or x >= self.width:
			return True
		return self.bitmap[self.width * y + x] == 0			
	
	def _outline(self, outline):
		by = 0
		for y in range(self.height):
			for x in range(self.width):
				baddr = x + by
				border = False
				if self.bitmap[baddr] != 0:
					for dy in range(-1,2):
						for dx in range(-1,2):
							if self._pixeltest(x+dx, y+dy):
								border = True
								break
						if border:
							break
				if border:
					self.bitmap[baddr] = outline
			by += self.width

	def _cut_mc(self):
		by = 0
		for y in range(self.sheight):
			sline = self.sview[3*y:3*y+3]
			bx = 0
			for x in range(0, self.swidth, 2):
				if (x & 6) == 0:
					shift = 6
				bits = ord(sline[x // 8])
				cindex = (bits >> shift) & 3
				if pindex:
					baddr = bx + by
					for yy in range(0, self.scale):
						for xx in range(0, 2*self.scale):
							self.bitmap[baddr+xx] = cindex
						baddr += xx
				shift -= 2
				bx += self.scale
			by += bx
	
	def _cut_sc(self):
		by = 0
		for y in range(self.sheight):
			sline = self.sview[3*y:3*y+3]
			bx = 0
			for x in range(self.swidth):
				if (x & 7) == 0:
					shift = 7
				bits = ord(sline[x // 8])
				cindex = (bits >> shift) & 1
				if pindex:
					baddr = bx + by
					for yy in range(0, self.scale):
						for xx in range(0, self.scale):
							self.bitmap[baddr+xx] = cindex
						baddr += xx
				shift -= 1
				bx += self.scale
			by += bx

if __name__ == "__main__":
	snap_file = sys.argv[1]
	start = int(sys.argv[2])
	end = start + 1
	if len(sys.argv) > 3:
		end = int(sys.argv[3])
	sc = SpriteCutter(snap_file)

	for sprite_no in range(start, end):
		sc.goto(sprite_no)
		sc.cut(4, (4, 2))
		print(sc.convert())

