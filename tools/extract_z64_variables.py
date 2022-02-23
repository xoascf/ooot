import struct
import json
from oot import *

conf = config()

def reverse(s):
	return s[::-1]
	return int.from_bytes(s, byteorder='big').to_bytes(len(s), 'little')

def readConvert(f, size, swap = True):
	pos = f.tell()
	r = f.read(size)
	
	if len(r) != size:
		raise IOError('failed to read %d bytes, got %d' % (size, len(r)))

	return r

def readS8(f, swap = True):
	return int.from_bytes(readConvert(f, 1, swap), byteorder='big', signed=True)
	
def readU8(f, swap = True):
	return int.from_bytes(readConvert(f, 1, swap), byteorder='big', signed=False)
	
def readS16(f, swap = True):
	return int.from_bytes(readConvert(f, 2, swap), byteorder='big', signed=True)
	
def readU32(f, swap = True):
	return int.from_bytes(readConvert(f, 4, swap), byteorder='big', signed=False)
	
def readS32(f, swap = True):
	return int.from_bytes(readConvert(f, 4, swap), byteorder='big', signed=True)
	
def readFloat(f, swap = True):
	b = readConvert(f, 4, swap)
	return float(struct.unpack('>f', b)[0])
	
	
class ConvFile:
	def __init__(self, path, mode):
		self.path = path
		self.i = open(path, 'rb')
		self.o = open(path + '_le', 'wb')
		if True:
			self.seek(0)
			buffer = self.i.read()
			self.o.write(buffer)
		else:
			self.seek(0)
			buffer = self.i.read()
			self.o.write(b'\xFF' * len(buffer))
		self.seek(0)
		
		self.writes = {}
		
	def __enter__(self):
		return self
		
	def __exit__(self, type, value, tb):
		self.close()
		
	def seek(self, offset):
		self.i.seek(offset)
		self.o.seek(offset)
		self.pos = offset
		
	def tell(self):
		return self.i.tell()
		
	def close(self):
		f = self.o
		self.i.close()
		self.o.close()
		
	def read(self, n, swap = True):
		if n == 0:
			raise IOError('null read')
		pos = self.i.tell()
		self.seek(pos)

		r = self.i.read(n)
		
		if len(r) != n:
			raise IOError('incomplete read: %d, expected %d' % (len(r), n))
		
		if swap:
			self.write2(reverse(r), n)
		else:
			self.write2(r, n)
		self.o.flush()
		return r
		
	def write2(self, data, n):		
		f = self.o
		f.seek(self.pos)
		f.write(data)
		#self.writes[self.pos] = data
		self.pos += len(data)
		
	#def write(self, data):
	#	self.o.write(data)
	#	self.i.seek(self.o.tell())
	#	return len(data)


class Section:
	def __init__(self, name, offset, sz, elementSize = 8):
		self.name = name
		self.offset = offset
		self.sz = sz
		self.elementSize = elementSize
		
	def serialize(self, f, z64):
		z64.seek(self.offset)

		if self.getSize() == self.getElementSize():
			f.write('u%d %s = ' % (self.getElementSize() * 8, self.name))
			buffer = z64.read(self.getElementSize())
			n = int.from_bytes(buffer, "big")
			f.write(('0x%%%d.%dX' % (self.getElementSize() * 2, self.getElementSize() * 2)) % (n))
			f.write(';\n\n')
			return

		f.write('u%d %s[0x%X] = {' % (self.getElementSize() * 8, self.name, self.getSize()))
		
		lst = []
		while z64.tell() < self.offset + self.sz:
			buffer = z64.read(self.getElementSize())
			n = int.from_bytes(buffer, "big")
			lst.append(('0x%%%d.%dX' % (self.getElementSize() * 2, self.getElementSize() * 2)) % (n))
			
		f.write(', '.join(lst))
			
		f.write('};\n\n')
		
	def getSize(self):
		return int(self.sz / self.getElementSize())
		
	def getElementSize(self):
		return self.elementSize
		
	def includes(self):
		return []
		
class Reloc:
	def __init__(self, address, size, medium, cachePolicy, shortData1, shortData2, shortData3, dataFile, f):
		self.address = address
		self.size = size
		self.medium = medium
		self.cachePolicy = cachePolicy
		self.shortData1 = shortData1
		self.shortData2 = shortData2
		self.shortData3 = shortData3
		
		self.dataFile = dataFile
		self.f = f
		
	def getDef(self):
		return '';
		
def OFFSET(o):
	if o > 0x2BDC0:
		raise IOError('bad offset: %8.8X' % o)
	return o

class AdpcmLoop:
	def __init__(self, f, parent):
		self.start = readU32(f)
		self.end = readU32(f)
		self.count = readU32(f)
		self.unk_0C = f.read(4, swap = False)
		
		self.states = []
		
		if self.count > 0:
			for i in range(16):
				self.states.append(readS16(f, swap = True))

class AdpcmBook:
	def __init__(self, f, parent):
		self.order = readS32(f)
		self.npredictors = readS32(f)
		print('order = %d, npred = %d, offset = %8.8X' % (self.order, self.npredictors, f.tell()))

		self.books = [] # TODO LOOP THROUGH: size 8 * order * npredictors. 8-byte aligned
		for i in range(self.order * self.npredictors * 8):
			self.books.append(readS16(f, swap = True))

def RSHIFT(n, offset, length):
	return (n >> offset) & ((0x01 << length) - 1)

class SoundFontSample:
	def __init__(self, f, parent):
		self.flags = readU32(f)
		self.sampleOffset = readU32(f)
		self.loopOffset = readU32(f)
		self.bookOffset = readU32(f)
		
		pos = f.tell()
		

		
		self.codec = RSHIFT(self.flags, 0, 4)
		self.medium = RSHIFT(self.flags, 4, 2)
		self.unk_bit26 = RSHIFT(self.flags, 6, 1)
		self.unk_bit25 = RSHIFT(self.flags, 7, 1)
		self.size = RSHIFT(self.flags, 8, 24)
		print('sample flags: %8.8X, size = %d, codec = %d, medium = %d, unk_bit26 = %d, unk_bit25 = %d, sampleOffset = %8.8X' % (self.flags, self.size, self.codec, self.medium, self.unk_bit26, self.unk_bit25, self.sampleOffset))
		
		#print('sample flags: %8.8X, size = %d' % (self.flags, RSHIFT(self.flags, 8, 24)))
		#exit(0)
		#if OFFSET(self.sampleOffset) > 0:
		#	pass
			#print('sample flags: %8.8X' % self.flags)
			#exit(0)
			#f.seek(self.sampleOffset + parent.address)

		
		if OFFSET(self.loopOffset) > 0:
			f.seek(self.loopOffset + parent.address)
			self.loop = AdpcmLoop(f, parent)
		
		if OFFSET(self.bookOffset) > 0:
			f.seek(self.bookOffset + parent.address)
			self.book = AdpcmBook(f, parent)
			
		f.seek(pos)
		
class SoundFontSound:
	def __init__(self, f, parent):
		
		self.sampleOffset = readU32(f)
		self.tuning = readFloat(f)
		
		pos = f.tell()
		
		if OFFSET(self.sampleOffset) > 0:
			f.seek(self.sampleOffset + parent.address)
			self.sample = SoundFontSample(f, parent)
			
		f.seek(pos)

		print('SoundFontSound: %8.8X, tuning: %f' % (self.sampleOffset, self.tuning))

class Drum:
	def __init__(self, f, parent):
		self.releaseRate = readU8(f)
		self.pan = readU8(f)
		self.loaded = readU8(f)
		
		readU8(f) # padding
		
		self.soundFontSound = SoundFontSound(f, parent)
		
		self.envelopeOffset = readU32(f)
		
		pos = f.tell()

		if OFFSET(self.envelopeOffset) > 0:
			f.seek(self.envelopeOffset + parent.address)
			self.adsrEnvelope = AdsrEnvelope(f)
			
		f.seek(pos)
		
		print('Drum: releaseRate: %d, pan: %d, envelopeOffset: %8.8X' % (self.releaseRate, self.pan, self.envelopeOffset))

class AdsrEnvelope:
	def __init__(self, f):
		self.delay = readS16(f)
		self.arg = readS16(f)

class Instrument:
	def __init__(self, f, parent):
		self.loaded = readU8(f)
		self.normalRangeLo = readU8(f)
		self.normalRangeHi = readU8(f)
		self.releaseRate = readU8(f)
		
		self.envelopeOffset = readU32(f)
		self.lowNotesSound = SoundFontSound(f, parent)
		self.normalNotesSound = SoundFontSound(f, parent)
		self.highNotesSound = SoundFontSound(f, parent)
		
		pos = f.tell()

		if OFFSET(self.envelopeOffset) > 0:
			f.seek(self.envelopeOffset + parent.address)
			self.adsrEnvelope = AdsrEnvelope(f)
			
		f.seek(pos)
		
		print('Instrument: normalRangeLo: %d, normalRangeHi: %d, releaseRate: %d, envelopeOffset = %8.8X (%8.8X)' % (self.normalRangeLo, self.normalRangeHi, self.releaseRate, self.envelopeOffset, parent.address))
		

		
class FontReloc(Reloc):
	def __init__(self, address, size, medium, cachePolicy, shortData1, shortData2, shortData3, dataFile, f):
		super(FontReloc, self).__init__(address, size, medium, cachePolicy, shortData1, shortData2, shortData3, dataFile, f)
		
		self.sampleBankId1 = (shortData1 >> 8) & 0xFF
		self.sampleBankId2 = (shortData1) & 0xFF
		self.numInstruments = (shortData2 >> 8) & 0xFF
		self.numDrums = shortData2 & 0xFF
		self.numSfx = shortData3
		
		f.seek(address)
		self.offsets = []
		self.offsets.append(readU32(f)) # drums
		self.offsets.append(readU32(f)) # SoundFontSound
		
		for i in range(2, self.numInstruments + 2): # instruments
			self.offsets.append(readU32(f))
		
		print('bankId1 = %d, bankId2 = %d, numInstruments = %d, numDrums = %d, , numSfx = %d' % (self.sampleBankId1, self.sampleBankId2, self.numInstruments, self.numDrums, self.numSfx))
		
	def getName(self):
		return 'font_%X' % self.address

	def getDef(self):
		buf = 'FontReloc %s = {' % self.getName()
		r = []
		for offset in self.offsets:
			r.append('0x%8.8X' % offset)
			
		buf += ', '.join(r) + ' }; // data starts at 0x%8.8X\n' % (len(self.offsets) * 4)
		
		drums = []
		sfxs = []
		instruments = []
		
		f = self.f

		if self.offsets[0] > 0:
			for i in range(self.numDrums):
				f.seek(self.offsets[0] + self.address + (i * 4))
				p = readS32(f)
				
				if p > 0:
					f.seek(p + self.address)
					drums.append(Drum(f, self))
			
		if self.offsets[1] > 0:
			f.seek(self.offsets[1] + self.address)
			for i in range(self.numSfx):
				sfxs.append(SoundFontSound(f, self))
				
		for i in range(2, self.numInstruments + 2):
			if self.offsets[i] > 0:
				f.seek(self.offsets[i] + self.address)
				instruments.append(Instrument(f, self))

		return buf
		

		
class Table(Section):
	def __init__(self, name, offset, sz, dataFile):
		super(Table, self).__init__(name, offset, sz, 1)
		
		self.dataFile = dataFile

		
	def serialize(self, f, z64):
		z64.seek(self.offset)
		
		numEntries = readS16(z64, swap = False)
		unkMediumParam = readS16(z64, swap = False)
		address = readU32(z64, swap = False)
		
		z64.read(8) # padding


		lst = []
		relocs = []
		
		with ConvFile(assetPath(self.dataFile), 'rb') as x:
			while z64.tell() < self.offset + self.sz:
				address = readU32(z64, swap = False)
				size = readU32(z64, swap = False)
				medium = readS8(z64, swap = False)
				cachePolicy = readS8(z64, swap = False)
				shortData1 = readS16(z64, swap = False)
				shortData2 = readS16(z64, swap = False)
				shortData3 = readS16(z64, swap = False)

				reloc = self.getReloc(address, size, medium, cachePolicy, shortData1, shortData2, shortData3, f = x)
				reloc.getDef() # reverses endian
				relocs.append(reloc)
				lst.append('{ .romAddr = 0x%8.8X, .size = 0x%8.8X, .medium = 0x%2.2X, .cachePolicy = %d, .shortData1 = 0x%4.4X, .shortData2 = 0x%4.4X, .shortData3 = 0x%4.4X }' % (address, size, medium, cachePolicy, shortData1, shortData2, shortData3))
			
		f.write('AudioTableDef %s = {\n.numEntries = 0x%4.4X, .unkMediumParam = 0x%4.4X, .romAddr = 0x%8.8X, .pad = {0}, .entries = {\n' % (self.name, numEntries, unkMediumParam, address))

		f.write(',\n'.join(lst))
			
		f.write('\n}};\n\n')
		
	def getReloc(self, address, size, medium, cachePolicy, shortData1, shortData2, shortData3, f):
		return Reloc(address, size, medium, cachePolicy, shortData1, shortData2, shortData3, self.dataFile, f = f)
		
		
	def includes(self):
		return ['z64audio.h']
		
class FontTable(Table):
	def __init__(self, name, offset, sz, dataFile):
		super(FontTable, self).__init__(name, offset, sz, dataFile)
		
	def getReloc(self, address, size, medium, cachePolicy, shortData1, shortData2, shortData3, f):
		return FontReloc(address, size, medium, cachePolicy, shortData1, shortData2, shortData3, self.dataFile, f = f)

sections = {'misc/rsp.h': [
		FontTable('gSoundFontTable', conf.sections.gSoundFontTable.offset, conf.sections.gSoundFontTable.size, 'baserom/Audiobank'),
		Section('gSequenceFontTable', conf.sections.gSequenceFontTable.offset, conf.sections.gSequenceFontTable.size, 2),
		Table('gSequenceTable', conf.sections.gSequenceTable.offset, conf.sections.gSequenceTable.size, 'baserom/Audioseq'),
		Table('gSampleBankTable', conf.sections.gSampleBankTable.offset, conf.sections.gSampleBankTable.size, 'baserom/Audiotable'),
		Section('rspAspMainDataStart', conf.sections.rspAspMainData.offset, conf.sections.rspAspMainData.size, 4),
		Section('D_80155F50', conf.sections.rspF3DZEXText.offset, conf.sections.rspF3DZEXText.size, 1),
		Section('D_80157580', conf.sections.rspF3DZEXData.offset, conf.sections.rspF3DZEXData.size, 1),
		Section('D_801579A0', conf.sections.rspS2DEXData.offset, conf.sections.rspS2DEXData.size, 1),
		Section('gJpegUCodeData', conf.sections.rspJpegData.offset, conf.sections.rspJpegData.size),
		Section('D_801120C0', conf.sections.rspAspMainText.offset, conf.sections.rspAspMainText.size, 8),
		Section('D_80113070', conf.sections.rspS2DEXText.offset, conf.sections.rspS2DEXText.size, 1),
		Section('gJpegUCode', conf.sections.rspJpegTextStart.offset, conf.sections.rspJpegTextStart.size, 8)
	],
	'misc/rsp_boot.h': [
		Section('D_80009320', conf.sections.rspBootText.offset, conf.sections.rspBootText.size, 1),
		Section('D_800093F0', conf.sections.D_800093F0.offset, conf.sections.D_800093F0.size, 1)
	],
	'misc/code_800F9280.h': [
		Section('sSeqCmdWrPos', conf.sections.sSeqCmdWrPos.offset, conf.sections.sSeqCmdWrPos.size, 1),
		Section('sSeqCmdRdPos', conf.sections.sSeqCmdRdPos.offset, conf.sections.sSeqCmdRdPos.size, 1),
		Section('D_80133408', conf.sections.D_80133408.offset, conf.sections.D_80133408.size, 1),
		Section('D_8013340C', conf.sections.D_8013340C.offset, conf.sections.D_8013340C.size, 1),
		Section('D_80133410', conf.sections.D_80133410.offset, conf.sections.D_80133410.size, 1),
		Section('gAudioSpecId', conf.sections.gAudioSpecId.offset, conf.sections.gAudioSpecId.size, 1),
		Section('D_80133418', conf.sections.D_80133418.offset, conf.sections.D_80133418.size, 1),
	]
}

createDir(assetPath('misc'))

with open(romPath('baserom.z64'), 'rb') as z64:
	for filename, s in sections.items():
		with open(assetPath(filename), 'w') as f:
			f.write('#include "global.h"\n')
			includes = {}
			for section in s:
				for i in section.includes():
					includes[i] = i
					
			for i in includes.keys():
				f.write('#include "%s"\n' % i)
				
			f.write('\n')

			for section in s:
				section.serialize(f, z64)