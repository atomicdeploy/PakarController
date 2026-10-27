#include <PixelCorrections.h>
#include <cassert>
#include <initializer_list>
using Map=BoardCore::PixelCorrections<6>;
static void expect(const Map &map,std::initializer_list<int> values){size_t i=0;for(int v:values)assert(map.source(i++)==v);}
int main(){
  Map map;expect(map,{0,1,2,3,4,5});
  Map::Rule black{2,0,Map::Black};assert(map.configure(true,&black,1));expect(map,{0,1,-1,3,4,5});
  Map::Rule remove{2,0,Map::Remove};assert(map.configure(true,&remove,1));expect(map,{0,1,3,4,5,-1});
  Map::Rule shift{2,2,Map::Shift};assert(map.configure(true,&shift,1));expect(map,{0,1,-1,-1,2,3});
  shift.offset=-2;assert(map.configure(true,&shift,1));expect(map,{2,3,4,5,-1,-1});
  shift.offset=0;assert(map.configure(true,&shift,1));expect(map,{0,1,2,3,4,5});
  shift.offset=-6;assert(map.configure(true,&shift,1));expect(map,{0,1,-1,-1,-1,-1});
  Map::Rule rules[]={remove,{2,0,Map::Black}};assert(map.configure(true,rules,2));expect(map,{0,1,-1,4,5,-1});
  rules[0].enabled=false;assert(map.configure(true,rules,2));expect(map,{0,1,-1,3,4,5});
  rules[0].enabled=true;assert(map.configure(true,rules,2));expect(map,{0,1,-1,4,5,-1});
  int raw[]={10,20,30,40,50,60},wire[6]{};map.render(raw,wire,0);assert(wire[2]==0&&wire[3]==50&&raw[2]==30);
  assert(map.configure(false,rules,2));expect(map,{0,1,2,3,4,5});
  Map::Rule invalid{6,0,Map::Black};assert(!map.configure(true,&invalid,1));expect(map,{0,1,2,3,4,5});
  for(int start=0;start<6;++start)for(int delta=-6;delta<=6;++delta){
    Map::Rule rule{uint16_t(start),int16_t(delta),Map::Shift};assert(map.configure(true,&rule,1));
    int expected[]={0,1,2,3,4,5};for(int i=start;i<6;++i)expected[i]=-1;
    for(int i=start;i<6;++i)if(i+delta>=0&&i+delta<6)expected[i+delta]=i;
    for(int i=0;i<6;++i)assert(map.source(i)==expected[i]);
  }
  BoardCore::PixelCorrections<480> table;assert(table.source(479)==479);
  BoardCore::PixelCorrections<216> keys;assert(keys.source(215)==215);
}
