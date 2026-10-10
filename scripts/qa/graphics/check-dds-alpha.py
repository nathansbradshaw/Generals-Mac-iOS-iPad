#!/usr/bin/env python3
"""Compile the actual retail DDS decoder functions with synthetic BC alpha blocks.
No retail data or graphics device is needed. GeneralsX @build Codex 09/10/2026.
"""
from pathlib import Path
import subprocess
import tempfile
import os

ROOT = Path(__file__).resolve().parents[3]

def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

PRELUDE = r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#define WWINLINE inline
#define WWASSERT assert
enum WW3DFormat { WW3D_FORMAT_A8R8G8B8, WW3D_FORMAT_DXT1,
 WW3D_FORMAT_DXT2, WW3D_FORMAT_DXT3, WW3D_FORMAT_DXT4, WW3D_FORMAT_DXT5 };
struct Vector3 { float v[3]{}; float operator[](int i) const {return v[i];} };
void Recolor(unsigned&, const Vector3&) {}
unsigned Get_Bytes_Per_Pixel(WW3DFormat) { return 4; }
struct BitmapHandlerClass {
 static void Write_B8G8R8A8(unsigned char *dest, WW3DFormat, unsigned pixel) {
  std::memcpy(dest, &pixel, 4);
 }
};
struct DDSFileClass {
 WW3DFormat Format; unsigned MipLevels=1;
 alignas(8) unsigned char block[16]{};
 const unsigned char *Get_Memory_Pointer(unsigned) const { return block; }
 unsigned Get_Width(unsigned) const { return 4; }
 unsigned Get_Height(unsigned) const { return 4; }
 unsigned Get_Pixel(unsigned,unsigned,unsigned) const;
 bool Get_4x4_Block(unsigned char*,unsigned,WW3DFormat,unsigned,unsigned,unsigned,const Vector3&) const;
};
"""
TEST = r"""
int main() {
 for (auto format : {WW3D_FORMAT_DXT2, WW3D_FORMAT_DXT3}) {
  DDSFileClass d; d.Format=format;
  for (unsigned i=0;i<8;++i) d.block[i]=(2*i)|((2*i+1)<<4);
  d.block[8]=0; d.block[9]=0xf8; d.block[10]=0x1f;
  for (unsigned i=12;i<16;++i) d.block[i]=0xe4;
  unsigned char out[4*24]; std::memset(out,0xcd,sizeof(out));
  assert(d.Get_4x4_Block(out,24,WW3D_FORMAT_A8R8G8B8,0,0,0,Vector3{}));
  for (unsigned y=0;y<4;++y) {
   for (unsigned x=0;x<4;++x) {
    unsigned p; std::memcpy(&p,out+y*24+x*4,4);
    assert((p>>24)==(y*4+x)*17);
    assert(p==d.Get_Pixel(0,x,y));
   }
   for (unsigned x=16;x<24;++x) assert(out[y*24+x]==0xcd);
  }
 }
 for (auto format : {WW3D_FORMAT_DXT4, WW3D_FORMAT_DXT5}) {
  for (bool eight : {false,true}) {
   DDSFileClass d; d.Format=format;
   d.block[0]=eight?255:0; d.block[1]=eight?0:255;
   unsigned long long packed=0;
   for (unsigned i=0;i<16;++i) packed|=(unsigned long long)(i%8)<<(3*i);
   for (unsigned i=0;i<6;++i) d.block[2+i]=(packed>>(8*i))&255;
   d.block[8]=0; d.block[9]=0xf8; d.block[10]=0x1f;
   unsigned char out[64]{};
   assert(d.Get_4x4_Block(out,16,WW3D_FORMAT_A8R8G8B8,0,0,0,Vector3{}));
   unsigned expected6[8]={0,255,51,102,153,204,0,255};
   unsigned expected8[8]={255,0,219,182,146,109,73,36};
   for (unsigned i=0;i<16;++i) {
    assert(out[i*4+3]==(eight?expected8:expected6)[i%8]);
    assert(out[i*4+3]==(d.Get_Pixel(0,i%4,i/4)>>24));
   }
  }
 }
 DDSFileClass bc1; bc1.Format=WW3D_FORMAT_DXT1;
 for(unsigned i=4;i<8;++i) bc1.block[i]=255;
 unsigned char out[64]; std::memset(out,0xcd,sizeof(out));
 assert(bc1.Get_4x4_Block(out,16,WW3D_FORMAT_A8R8G8B8,0,0,0,Vector3{}));
 for(unsigned i=0;i<16;++i) assert(out[i*4+3]==0);
 puts("PASS: BC1 transparency, BC2 explicit alpha, BC3 both alpha palettes, padded rows");
}
"""
for game in ("GeneralsMD", "Generals"):
    source = (ROOT / game / "Code/Libraries/Source/WWVegas/WW3D2/ddsfile.cpp").read_text()
    code = PRELUDE + "\n#include <initializer_list>\n" + "\n".join(function(source, sig) for sig in (
        "WWINLINE static unsigned RGB565_To_ARGB8888(",
        "WWINLINE static unsigned Combine_Colors(",
        "unsigned DDSFileClass::Get_Pixel(",
        "bool DDSFileClass::Get_4x4_Block(")) + TEST
    with tempfile.TemporaryDirectory(prefix="generalsx-dds-") as directory:
        cpp = Path(directory)/"test.cpp"; exe = Path(directory)/"test"
        cpp.write_text(code)
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-fsanitize=address,undefined",
                        "-g", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        print(game, "actual decoder functions passed")
