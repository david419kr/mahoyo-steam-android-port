"""Produce geometry with the Python tool and consume it with the real C++ parser."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,tempfile,unittest
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import bitmap_composition as composition
import cli_assets
from PIL import Image
class GeometryContract(unittest.TestCase):
 def test_profile_checksum_is_not_the_serialized_contract(self):
  compiler=shutil.which('clang++') or shutil.which('g++')
  if not compiler:self.skipTest('C++ compiler is required')
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);source=root/'raw';(source/'images').mkdir(parents=True)
   target='1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b'
   records=[{'rowIndex':i,'key':f'img{i:05}.mzp','primary':'base.mzp' if i<3818 else f'img{i:05}.mzp','alias':f'img{i:05}.mzp' if i<3818 else None} for i in range(10821)]
   table={'schema':'woh.native.bitmap-records.v1','targetSHA256':target,'records':records}
   recipes=root/'recipes.json';recipes.write_text(json.dumps(table),encoding='utf8',newline='\n')
   profile=root/'profile.json';profile.write_text(json.dumps({'targetSHA256':target,'generatedTable':{'sha256':hashlib.sha256(recipes.read_bytes()).hexdigest()}}),encoding='utf8',newline='\n')
   profile_sha=hashlib.sha256(profile.read_bytes()).hexdigest()
   manifest={'schema_version':1,'assets':{}}
   for name,size,color in [('base.mzp',(3,2),(1,2,3,255)),('img00000.mzp',(2,1),(8,9,10,128))]:
    path=source/'images'/(name+'.png');Image.new('RGBA',size,color).save(path)
    manifest['assets'][name]={'kind':'image','path':'images/'+name+'.png','width':size[0],'height':size[1],'bytes':path.stat().st_size,'sha256':cli_assets.sha(path)}
   (source/'assets.json').write_text(json.dumps(manifest))
   with mock.patch.object(composition,'AUTHORITY',profile),mock.patch.object(composition,'AUTHORITY_SHA',profile_sha):
    loaded,identity=composition.load_recipes(recipes)
    self.assertEqual(identity,composition.GEOMETRY_AUTHORITY)
    self.assertNotEqual(profile_sha,composition.GEOMETRY_AUTHORITY)
    with mock.patch.object(composition,'load_recipes',return_value=({'targetSHA256':target,'records':[loaded['records'][0]]},identity)):
     cli_assets.compose_all(source,root/'game',root/'state',recipes,workers=1)
   cpp=root/'parse.cpp';cpp.write_text('''#include "bitmap_geometry.h"
#include <fstream>
int main(int argc,char**argv){using namespace mahoyo::bitmap_geometry;try{std::ifstream in(argv[1]);auto rows=readRecipes(in,Steam11PE,Steam11Authority);if(rows.size()!=1)return 2;auto r=rows.at("img00000.mzp");return r.patch.width==2&&r.patch.height==1&&r.primary.width==3&&r.primary.height==2?0:3;}catch(...){return 1;}}
''')
   binary=root/'parse.exe';subprocess.run([compiler,'-std=c++17','-I',str(ROOT/'runtime'),str(cpp),'-o',str(binary)],check=True)
   geometry=root/'game/meta/native-composite-geometry.tsv';subprocess.run([str(binary),str(geometry)],check=True)
   broken=root/'bad.tsv';broken.write_text(geometry.read_text().replace(composition.GEOMETRY_AUTHORITY,profile_sha))
   self.assertNotEqual(subprocess.run([str(binary),str(broken)]).returncode,0)
   profile.write_text('{}')
   with mock.patch.object(composition,'AUTHORITY',profile),mock.patch.object(composition,'AUTHORITY_SHA',profile_sha):
    with self.assertRaisesRegex(ValueError,'checksum'):composition.load_recipes(recipes)
if __name__=='__main__':unittest.main()
