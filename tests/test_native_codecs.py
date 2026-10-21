"""Compare native decoder bytes with the independent Python format implementation."""
from pathlib import Path
import os,shutil,struct,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'tests'))
from asset_codecs import decode_cbg,decode_mzp,decode_mzx,decode_lenzu
from test_asset_codecs import cbg,mzp,mzx_literal,image_header,palette,lenzu_two_literal
class NativeCodecs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.root=Path(cls.temp.name);cls.binary=cls.root/'codec.exe'
  compiler=shutil.which('clang++') or shutil.which('g++')
  if not compiler:raise unittest.SkipTest('C++ compiler required')
  subprocess.run([compiler,'-O2','-std=c++17','-I',str(ROOT/'runtime'),str(ROOT/'tests/asset_codecs_tests.cpp'),str(ROOT/'runtime/asset_codecs.cpp'),'-o',str(cls.binary)],check=True)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def invoke(self,mode,raw,crop=(),valid=True):
  source=self.root/'input';output=self.root/'output';source.write_bytes(raw)
  r=subprocess.run([str(self.binary),mode,str(source),str(output),*map(str,crop)],capture_output=True)
  if valid:self.assertEqual(r.returncode,0,r.stderr.decode());return output.read_bytes()
  self.assertNotEqual(r.returncode,0)
 def test_mzx_commands_and_length(self):
  for raw in [mzx_literal(b'12345'),b'MZX0'+struct.pack('<I',12)+b'\x07AABB\x00\x05\x01\x02',b'MZX0\x02\0\0\0\xfc']:
   for invert in [False,True]:self.assertEqual(self.invoke('mzx-invert' if invert else 'mzx',raw),decode_mzx(raw,invert=invert))
 def test_lenzu_checksum_and_truncation(self):
  raw=lenzu_two_literal();self.assertEqual(self.invoke('lenzu',raw),decode_lenzu(raw))
  self.invoke('lenzu',raw[:-2],valid=False);bad=bytearray(raw);bad[36]^=1;self.invoke('lenzu',bad,valid=False)
 def test_image_modes_and_region(self):
  pal=bytearray(palette());pal[4:8]=bytes((20,30,40,64))
  hep=struct.pack('<8I',0x00504548,32+16+1024,0,0,16,4,4,1)+bytes([1])*16+pal
  cases=[cbg(2,2,bytes((3,5,6,9))),cbg(2,2,bytes(range(12)),channels=3),cbg(2,2,bytes(range(16)),channels=4),mzp([image_header(2,2,2,2,1,1,1,0x11)+palette(),mzx_literal(bytes((0,8,16,255)))]),mzp([image_header(8,4,4,4,2,1,0xc,0x11,1),mzx_literal(hep),mzx_literal(hep)]),mzp([image_header(1,1,1,1,1,1,0xb,0x14),mzx_literal(struct.pack('<H',0xf81f)+b'\xe7\x63')])]
  for raw in cases:
   image,_=(decode_mzp(raw) if raw.startswith(b'mrgd00') else decode_cbg(raw));expected=struct.pack('<2I',*image.size)+image.tobytes();self.assertEqual(self.invoke('image',raw),expected)
   w,h=image.size;crop=(w//2,h//2,w,h);view=image.crop(crop);self.assertEqual(self.invoke('image',raw,crop),struct.pack('<2I',*view.size)+view.tobytes())
 def test_invalid_ranges_and_allocations(self):
  for raw in [b'',b'mrgd00'+bytes(5),cbg(1,1,b'\0')[:-3],b'CompressedBG_MT\0'+struct.pack('<4I',0xffffffff,0xffffffff,1,32)+bytes(24)]:self.invoke('image',raw,valid=False)
if __name__=='__main__':unittest.main()
