#include <PeerCompatibility.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace BoardCore::PeerCompatibility;
int main() {
  for(const char *invalid:{"","0.2","v0.2.7","00.2.7","0.02.7","0.2.07","0.2.7-","0.2.7+","0.2.7-01","0.2.7-x..y","0.2.7\r\n","4294967296.0.0"}) {
    Version value;assert(!parse(invalid,value));
  }
  for(const char *valid:{"0.2.7","0.2.7+build.001","0.2.8-alpha.1","4294967295.0.0"}) {
    Version value;assert(parse(valid,value));
  }
  assert(supported("0.2.7","0.2.7"));
  assert(supported("0.2.8","0.2.7"));
  assert(supported("1.0.0","0.2.7")); // No invented ceiling or exact-build gate.
  assert(!supported("0.2.6","0.2.7"));
  assert(!supported("0.2.7-alpha","0.2.7"));
  assert(supported("0.2.8-alpha","0.2.7"));
  assert(supported("0.2.7+abc","0.2.7+different"));
  assert(!supported("1.0.0","0.2.7","1.0.0"));
  assert(!supported("0.2.7","malformed"));
  assert(!supported("0.2.7","0.2.7","malformed"));
  Version a,b;
  assert(parse("1.0.0-alpha.9",a)&&parse("1.0.0-alpha.10",b)&&compare(a,b)<0);
  assert(parse("1.0.0-alpha",a)&&parse("1.0.0-alpha.1",b)&&compare(a,b)<0);
  assert(parse("1.0.0-alpha.1",a)&&parse("1.0.0-alpha",b)&&compare(a,b)>0);
  assert(parse("1.0.0-9",a)&&parse("1.0.0-a",b)&&compare(a,b)<0);
  assert(appAllowed("0.2.8"));
  assert(numericAppAllowed(0,2,8));
  assert(numericAppAllowed(0,2,7)==!enforced);
  assert(numericAppAllowed(0,3,0));
  assert(appAllowed("0.2.6")==!enforced);
  assert(appAllowed(nullptr)==!enforced);
  assert(boardAllowed("0.2.6")==!enforced);
  puts(enforced?"Peer release policy: supported ranges, exact SemVer prerelease/build rules, malformed/future boundaries and default enforcement passed.":"Peer release policy: explicit disabled build permits unknown/old releases without changing transport identity rules.");
}
