#include <PixelGroupLayout.h>
#include <cassert>
#include <cstdio>
template<size_t Groups,uint8_t Default,size_t Capacity> void profile(){
  using Layout=BoardCore::PixelGroupLayout<Groups,Default,Capacity>;
  Layout value;assert(value.revision()==0&&!value.customized()&&value.used()==Groups*Default);
  for(uint16_t group=1;group<=Groups;++group){
    assert(value.count(group)==Default&&value.start(group)==(group-1)*Default);
    assert(value.diagnosticRange(value.start(group),Default));
  }
  uint8_t overrides[Groups]{};overrides[0]=Default-1;overrides[1]=Default+1;overrides[2]=Default-2;
  assert(value.configure(overrides,19));assert(value.used()==Groups*Default-2);
  assert(value.start(2)==Default-1&&value.count(2)==Default+1&&value.start(3)==2*Default);
  assert(value.groupAt(Default-2)==1&&value.groupAt(Default-1)==2);
  assert(value.diagnosticRange(Default-1,Default+1));
  assert(!value.diagnosticRange(Default,Default));
  assert(!value.diagnosticRange(value.used(),1)&&!value.diagnosticRange(0,0));
  for(uint16_t i=0;i<value.used();++i)assert(value.diagnosticRange(i,1));
  const auto retained=value;overrides[0]=255;
  assert(!value.configure(overrides,20));assert(value.sameOverrides(retained)&&value.revision()==19);
  assert(!value.configure(nullptr,20));
  for(auto &count:overrides)count=0;
  assert(value.configure(overrides,20)&&!value.customized());
}
int main(){
  profile<80,6,480>();profile<18,12,216>();
  BoardCore::PixelGroupLayout<2,6,510> maximum;
  uint8_t counts[2]={255,255};assert(maximum.configure(counts,UINT32_MAX));
  assert(maximum.used()==510&&maximum.start(2)==255&&maximum.count(2)==255);
  assert(maximum.diagnosticRange(255,255)&&!maximum.diagnosticRange(256,255));
  puts("Physical group topology: default/inherit, variable prefix offsets, capacity, 255 pixels and exact diagnostic ranges passed.");
}
