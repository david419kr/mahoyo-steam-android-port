from pathlib import Path
import sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from prepare_korean_geometry import dimensions,generate

class KoreanGeometryTest(unittest.TestCase):
 def test_only_changed_pairs_are_emitted(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);a=root/'a.tsv';b=root/'b.tsv';out=root/'table.h'
   a.write_text('WOH_IMAGE_DIMENSIONS1\nface.mzp 100 200\nlogo.cbg 40 20\n');b.write_text('WOH_IMAGE_DIMENSIONS1\nface.mzp 120 200\nlogo.cbg 40 20\nnew.mzp 20 20\n')
   self.assertEqual(generate(a,b,out),1);self.assertIn('"face.mzp",{{100,200},{120,200}}',out.read_text());self.assertNotIn('"new.mzp"',out.read_text())
 def test_unsafe_duplicate_and_invalid_sizes_are_rejected(self):
  with tempfile.TemporaryDirectory() as folder:
   path=Path(folder)/'table.tsv'
   for row in ['../face.mzp 1 1','face.mzp 0 1','face.mzp 65536 1','face.mzp 20000 20000','face.mzp 1 1\nface.mzp 2 2']:
    path.write_text('WOH_IMAGE_DIMENSIONS1\n'+row+'\n')
    with self.assertRaises(ValueError):dimensions(path)
 def test_inputs_are_read_only(self):
  with tempfile.TemporaryDirectory() as folder:
   path=Path(folder)/'table.tsv';data='WOH_IMAGE_DIMENSIONS1\nface.mzp 1 1\n';path.write_text(data)
   with self.assertRaises(ValueError):generate(path,path,path)
   self.assertEqual(path.read_text(),data)

if __name__=='__main__':unittest.main()
