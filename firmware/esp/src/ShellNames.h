#pragma once
#include <stddef.h>
namespace PakarShell {
// PU850 Shell.cpp CompStr semantics, bounded and locale-independent. Only
// identifiers are folded: SSIDs, passwords and user-provided values stay intact.
inline char foldName(char c){return c>='A'&&c<='Z'?char(c+('a'-'A')):c;}
inline bool namePrefix(const char* text,const char* prefix,size_t length){
  if(!text||!prefix)return false;
  for(size_t i=0;i<length;++i)if(!text[i]||!prefix[i]||foldName(text[i])!=foldName(prefix[i]))return false;
  return true;
}
inline bool sameName(const char* a,const char* b){
  if(!a||!b)return false;
  while(*a&&*b){if(foldName(*a++)!=foldName(*b++))return false;}
  return *a==*b;
}
}
