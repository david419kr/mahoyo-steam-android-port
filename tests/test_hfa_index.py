"""Run the actual Java archive index against synthetic and malformed archives."""
from pathlib import Path
import os,shutil,struct,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class HfaIndexTest(unittest.TestCase):
 def test_index_ranges_duplicates_and_subranges(self):
  javac=shutil.which('javac') or str(Path(os.environ.get('JAVA_HOME',''))/'bin/javac.exe');java=shutil.which('java') or str(Path(os.environ.get('JAVA_HOME',''))/'bin/java.exe')
  if not Path(javac).is_file():self.skipTest('JDK required')
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);archive=root/'data.hfa';rows=b'';body=b''
   for name,data in [('Voice.HW',b'old'),('voice.hw',b'new!')]:rows+=name.encode().ljust(96,b'\0')+struct.pack('<II',len(body),len(data))+bytes(24);body+=data
   archive.write_bytes(b'HUNEXGGEFA10'+struct.pack('<I',2)+rows+body)
   source=root/'HfaTest.java';source.write_text('''package org.mahoyo.port;import java.io.*;import java.util.*;class HfaTest {public static void main(String[]args)throws Exception{try{var rows=HfaIndex.read(new File(args[0]));if(rows.size()!=1)throw new AssertionError();var e=rows.get("voice.hw");if(!Arrays.equals(e.read(16),new byte[]{110,101,119,33})||e.offset!=275)throw new AssertionError();if(e.slice(1,2).read(2)[0]!=101)throw new AssertionError();try{e.slice(3,2);throw new AssertionError();}catch(IOException ok){}if(args.length>1)System.exit(2);}catch(IOException e){if(args.length==1)throw e;}}}''')
   sources=ROOT/'android/app/src/main/java/org/mahoyo/port';subprocess.run([javac,'-d',str(root),str(sources/'HfaIndex.java'),str(sources/'AssetBounds.java'),str(source)],check=True)
   subprocess.run([java,'-cp',str(root),'org.mahoyo.port.HfaTest',str(archive)],check=True)
   raw=archive.read_bytes()
   for bad in [raw[:-1],raw[:12]+struct.pack('<I',0xffffffff)+raw[16:],raw[:16]+b'../bad'.ljust(96,b'\0')+raw[112:],raw[:240]+struct.pack('<II',0,4)+raw[248:]]:
    archive.write_bytes(bad);subprocess.run([java,'-cp',str(root),'org.mahoyo.port.HfaTest',str(archive),'invalid'],check=True)
if __name__=='__main__':unittest.main()
