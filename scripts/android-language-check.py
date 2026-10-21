"""Run the production pure-Java language policy over edition/locale edge cases."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/tests/android-language-check'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    test = OUT / 'LanguagePolicyCheck.java'
    test.write_text('''package org.mahoyo.port;import java.util.*;public class LanguagePolicyCheck {
static int checks;static void check(boolean b){checks++;if(!b)throw new AssertionError("check "+checks);}
static void row(String tag,String original,String korean){Locale primary=Locale.forLanguageTag(tag);check(LanguagePolicy.resolve(false,"auto",primary).equals(original));check(LanguagePolicy.resolve(true,"auto",primary).equals(korean));}
public static void main(String[]args){
row("ko-KR","en","ko");row("ja-JP","ja","ja");row("en-US","en","ko");row("en-GB","en","ko");
for(String s:new String[]{"zh","zh-CN","zh-SG","zh-Hans","zh-Hans-TW"})row(s,"zh-Hans","zh-Hans");
for(String s:new String[]{"zh-TW","zh-HK","zh-MO","zh-Hant","zh-Hant-CN"})row(s,"zh-Hant","zh-Hant");
row("fr-FR","en","ko");row("und","en","ko");
for(boolean korean:new boolean[]{false,true})for(String preference:LanguagePolicy.choices(korean)){
check(LanguagePolicy.preference(korean,preference).equals(preference));
if(!preference.equals("auto"))check(LanguagePolicy.resolve(korean,preference,Locale.FRENCH).equals(preference));
}
check(LanguagePolicy.preference(false,"ko").equals("auto"));check(LanguagePolicy.preference(true,"en").equals("auto"));check(LanguagePolicy.preference(true,null).equals("auto"));
check(LanguagePolicy.resolve(false,"auto",null).equals("en"));check(LanguagePolicy.resolve(true,"auto",null).equals("ko"));
check(Arrays.equals(LanguagePolicy.labels(false),new String[]{"Auto","日","En","简","繁"}));check(Arrays.equals(LanguagePolicy.labels(true),new String[]{"Auto","한","日","简","繁"}));
check(LanguagePolicy.koreanPackage("org.mahoyo.port.kr"));check(!LanguagePolicy.koreanPackage("org.mahoyo.port"));
try{LanguagePolicy.koreanPackage("org.other");throw new AssertionError();}catch(IllegalArgumentException expected){checks++;}
check(LanguagePolicy.slot("ko")==1&&LanguagePolicy.suffix("ko").equals("en"));check(LanguagePolicy.slot("zh-Hans")==2&&LanguagePolicy.suffix("zh-Hant").equals("zt"));
System.out.println(checks+" production language policy checks passed");}}
''', encoding='utf-8')
    jdk = Path(os.environ.get('JAVA_HOME', '')) / 'bin'
    subprocess.run([jdk / 'javac.exe', '-encoding', 'UTF-8', '-d', OUT,
                    ROOT / 'android/app/src/main/java/org/mahoyo/port/LanguagePolicy.java', test], check=True)
    subprocess.run([jdk / 'java.exe', '-cp', OUT, 'org.mahoyo.port.LanguagePolicyCheck'], check=True)


if __name__ == '__main__':
    main()
