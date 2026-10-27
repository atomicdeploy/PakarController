#include <cassert>
#include <SnakeGame.h>
#include <GameRole.h>
int main(){
  using S=BoardCore::SnakeGame;
  bool seen[81]={};for(unsigned i=0;i<80;++i){const auto id=S::logical(i);assert(id>=1&&id<=80&&!seen[id]);seen[id]=true;}
  assert(S::logical(0)==1&&S::logical(1)==11&&S::logical(20)==21);
  assert(S::next(19,1)==0&&S::next(0,3)==19&&S::next(7,0)==67&&S::next(67,2)==7);
  S s;assert(!s.service(10000));s.start(0);assert(!s.turn(3));assert(s.turn(0));assert(!s.turn(3));
  assert(!s.service(249));assert(s.service(250));assert(s.body[0]==4);
  s.pause();assert(!s.service(5000));s.resume(6000);assert(!s.service(6001));assert(s.service(6250));assert(s.body[0]==64);
  s.stop();assert(!s.service(10000));s.start(0);s.food=25;assert(s.service(250));assert(s.length==4&&s.score==1);
  assert(BoardCore::GameRole::mode==1);assert(!BoardCore::GameRole::accepts(3));
}
