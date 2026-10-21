"""Catalogue integrity and language handoff guards; APK rendering is tested separately."""
import json
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]

class UiCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.rows=json.loads((ROOT/'data/ui-text.json').read_text(encoding='utf-8'))['messages']

    def test_unique_keys_aliases_and_complete_translations(self):
        keys=set();aliases=set()
        for row in self.rows:
            self.assertEqual(len(row),7)
            self.assertNotIn(row[0],keys);keys.add(row[0])
            for source in row[1]:self.assertNotIn(source,aliases);aliases.add(source)
            for translation in row[2:]:self.assertTrue(translation.strip(),row[0])

    def test_format_arguments_match_in_all_languages(self):
        pattern=r'%(?:\d+\$)?[-+0 #]*(?:\d+)?(?:\.\d+)?[dfs]'
        for row in self.rows:
            expected=re.findall(pattern,row[2])
            for translated in row[3:]:self.assertEqual(re.findall(pattern,translated),expected,row[0])

    def test_runtime_format_keys_exist(self):
        keys={r[0] for r in self.rows}
        for path in (ROOT/'android/app/src/main/java/org/mahoyo/port').glob('*.java'):
            text=path.read_text(encoding='utf-8')
            for key in re.findall(r'UiText\.(?:key|format)\([^,]+,"([a-z_]+)"',text):self.assertIn(key,keys,path.name)

if __name__=='__main__':unittest.main()
