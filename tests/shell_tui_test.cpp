#include "ShellTui.h"
#include <assert.h>
#include <iostream>
#include <new>
#include <string>

int main(){
  using T=PakarShell::ShellTui;
  T model;T::Action action;T::Snapshot snapshot;
  T::Row rows[]={{"Hostname","hostname","ledboard",true,false},{"Firmware","version","0.2.7",false,false},{"Password","station_password","never-render-this",true,true}};
  snapshot.rows=rows;snapshot.rowCount=3;
  std::string output;
  auto write=[&](const uint8_t* bytes,size_t count){output.append(reinterpret_cast<const char*>(bytes),count);};
  auto feed=[&](const std::string& bytes){bool emitted=false;for(unsigned char byte:bytes)emitted=model.feed(byte,snapshot,action)||emitted;return emitted;};
  model.begin();assert(model.active());assert(model.render(snapshot,write));assert(output.size()<=4096);
  assert(output.find("never-render-this")==std::string::npos);
  assert(output.find("[read]")!=std::string::npos&&output.find("[edit]")!=std::string::npos);
  output.clear();model.feedback("Saved\x1b]52;c;private\a");
  assert(model.render(snapshot,write)&&output.find("Saved")!=std::string::npos&&output.find("private")==std::string::npos);
  assert(action.type==T::Action::None); // Feedback is display-only, never a command.
  output.clear();assert(!model.render(snapshot,write)&&output.empty());
  assert(!feed("\r\r"));assert(!model.editing()&&action.type==T::Action::None); // Unchanged Enter is not a write.
  assert(!feed("\r"));assert(model.editing());model.render(snapshot,write);output.clear();
  assert(!feed("\x15" "lamp"));assert(model.render(snapshot,write));
  assert(output.find("\x1b[2J")==std::string::npos); // Edit only, never full repaint.
  assert(output.find("Edit Hostname: lamp")!=std::string::npos);
  rows[1].value="0.2.8";output.clear();assert(!model.render(snapshot,write)); // Async observation cannot repaint draft.
  assert(feed("\r"));assert(action.type==T::Action::Set&&std::string(action.key)=="hostname"&&std::string(action.value)=="lamp");
  assert(!model.editing());assert(!feed("\n")); // CR LF submits once.

  assert(!feed("\x1b[B\r"));assert(!model.editing()); // Readonly never emits Set.
  assert(!feed("\x1b[B\r"));assert(model.editing()&&std::string(model.draft()).empty());
  assert(!feed("private\trefresh"));output.clear();model.render(snapshot,write);
  assert(output.find("private")==std::string::npos); // Secret is masked, not history or event text.
  assert(!feed("\x1b"));assert(!model.flushEscape(action));assert(!model.editing());
  assert(!feed("\t"));assert(model.page()==T::Network);
  assert(!feed("\t"));assert(model.page()==T::Device); // LED/pixel pages hidden without ACK.
  snapshot.canLeds=true;snapshot.canPixels=true;snapshot.pixelCount=480;
  assert(!feed("\t\t"));assert(model.page()==T::Led);
  assert(action.type==T::Action::None); // Page entry never actuates.
  output.clear();assert(model.render(snapshot,write));assert(output.find("not physical geometry")!=std::string::npos);
  assert(!feed("\x1b[C\x1b[B"));assert(model.selectedCell()==12);
  assert(feed("r"));assert(action.type==T::Action::Cell&&action.index==12&&std::string(action.value)=="red");
  snapshot.canLeds=false;assert(!feed("w"));
  snapshot.canLeds=true;assert(!feed("\t"));assert(model.page()==T::Pixel&&model.selectedPixel()==0);
  assert(!feed("g"));assert(model.group());
  assert(feed("\x1b[C"));assert(action.type==T::Action::Pixel&&action.index==6&&action.group);
  for(unsigned i=0;i<78;++i)assert(feed("+"));
  assert(model.selectedPixel()==474);assert(!feed("+"));assert(model.selectedPixel()==474);
  assert(!feed("g"));for(unsigned i=0;i<5;++i)assert(feed("+"));
  assert(model.selectedPixel()==479);assert(!feed("+"));
  snapshot.pixelCount=481;assert(!feed("-")); // No unbounded or mismatched strip geometry.
  assert(!feed("\x1b"));assert(model.flushEscape(action));assert(action.type==T::Action::Exit&&!model.active());

  // Full and partial CSI frames, bad metadata, unsafe keys, stale edit authority.
  model.begin();snapshot.pixelCount=480;snapshot.title="ledboard\x1b]52;c;private\a";
  rows[0].label="Name\x1b[2J";rows[0].value="ledboard\r\nhello";
  output.clear();assert(model.render(snapshot,write));assert(output.find("private")==std::string::npos);
  assert(!feed("\r"));assert(!model.editing()); // Never normalize unsafe source into an applied value.
  rows[0].value="ledboard";rows[0].key="bad;key";assert(!feed("\r"));assert(!model.editing());
  rows[0].key="hostname";assert(!feed("\r"));assert(model.editing());
  rows[0].editable=false;assert(!feed("\r"));assert(!model.editing()&&action.type==T::Action::None);
  model.begin(false);output.clear();assert(model.render(snapshot,write));assert(output.find('\x1b')==std::string::npos);
  assert(sizeof(model)<2048); // No heap, IO object, unbounded transcript or SDK dependency.

  alignas(T) unsigned char modelStorage[sizeof(T)]{};
  auto *dying=new(modelStorage) T;dying->begin();
  T::Row secretRow={"Password","network.password","",true,true};
  snapshot.rows=&secretRow;snapshot.rowCount=1;
  dying->feed('\r',snapshot,action);
  for(unsigned char ch:std::string("unsubmitted-secret-marker"))dying->feed(ch,snapshot,action);
  assert(dying->editing());const size_t beforeDestruction=output.size();dying->~ShellTui();
  assert(output.size()==beforeDestruction);
  const std::string erased(reinterpret_cast<const char*>(modelStorage),sizeof(modelStorage));
  assert(erased.find("unsubmitted-secret-marker")==std::string::npos);
  alignas(T::Action) unsigned char actionStorage[sizeof(T::Action)]{};
  auto *handoff=new(actionStorage) T::Action;
  strcpy(handoff->value,"submitted-secret-marker");handoff->~Action();
  assert(std::string(reinterpret_cast<const char*>(actionStorage),sizeof(actionStorage)).find("submitted-secret-marker")==std::string::npos);
  std::cout<<"Device shell TUI navigation, drafts, capability gates, bounds and terminal safety passed.\n";
}
