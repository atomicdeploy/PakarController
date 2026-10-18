#pragma once
#include "TelnetShell.h"

// Allocation-free device-owned presentation. Adapted from RayanLamp's keys,
// formIsFiltering, deterministic runtime model, bounded terminal renderer and
// sanitization, not its lamp business logic, Go runtime or host networking.
// Snapshots are observations; only the caller's common router may authorize
// and execute an explicit Action. Entering/rendering never produces an action.
namespace PakarShell {
class ShellTui {
 public:
  enum Page : uint8_t { Device, Network, Led, Pixel };
  struct Row {
    const char* label=nullptr;
    const char* key=nullptr;
    const char* value=nullptr;
    bool editable=false;
    bool secret=false;
  };
  struct Snapshot {
    const char* title="PakarController";
    const Row* rows=nullptr; // Current page only, retained through feed/render.
    uint8_t rowCount=0;     // At most 16 rows; the selected window scrolls.
    const uint8_t* cellStates=nullptr; // Exactly 80: 0 white, 1 red, 2 off.
    uint16_t pixelCount=0;  // Pixel operations require exactly 480 + canPixels.
    bool canLeds=false,canPixels=false; // Independently ACKed, default denied.
  };
  struct Action {
    enum Type : uint8_t { None, Set, Cell, Pixel, Refresh, Exit } type=None;
    char key[32]{},value[385]{};
    // Cell: logical1..80; Pixel: ZERO-based physical0..479 (group starts0..474).
    uint16_t index=0;
    bool group=false;
    Action()=default;
    Action(const Action&)=default;
    Action& operator=(const Action&)=default;
    ~Action(){volatile char* p=value;for(size_t i=0;i<sizeof(value);++i)p[i]=0;}
  };
  ~ShellTui(){clearDraft();} // No terminal output, SDK call, or allocation on teardown.
  void begin(bool ansi=true) {
    clearDraft();active_=true;ansi_=ansi;page_=Device;selected_=0;grid_=false;
    dirty_=full_=true;lastHash_=0;escape_=escapeBytes_=0;crPending_=false;
    cell_=1;pixel_=0;group_=false;pixelColor_=1;notice_[0]=0;
  }
  bool active()const{return active_;}
  bool editing()const{return editing_;}
  bool escapePending()const{return escape_==1;}
  Page page()const{return page_;}
  uint8_t selectedCell()const{return cell_;}
  uint16_t selectedPixel()const{return pixel_;}
  bool group()const{return group_;}
  const char* draft()const{return draft_;} // Secret draft is NEVER rendered.
  void invalidate(){if(!editing_)dirty_=true;}
  void feedback(const char* text){setNotice(text);}
  // A standalone ESC needs an idle delimiter (~35ms); CSI may span callbacks.
  // Only the transport owner decides that delimiter. Never flush each chunk.
  bool flushEscape(Action& action) {
    action=Action{};if(escape_!=1)return false;escape_=escapeBytes_=0;
    if(editing_){clearDraft();dirty_=full_=true;return false;}
    if(!active_){return false;}active_=false;action.type=Action::Exit;return true;
  }
  bool feed(uint8_t ch,const Snapshot& snapshot,Action& action) {
    action=Action{};if(!active_)return false;
    if(crPending_&&(ch=='\n'||ch==0)){crPending_=false;return false;}
    if(ch!='\n'&&ch!='\r')crPending_=false;
    if(escape_) {
      if(escape_==1){
        if(ch=='['||ch=='O'){escape_=2;return false;}
        escape_=escapeBytes_=0;return false;
      }
      if(++escapeBytes_>16){escape_=escapeBytes_=0;return false;}
      if(ch>=0x40 && ch<=0x7e){escape_=escapeBytes_=0;return arrow(ch,snapshot,action);}
      return false;
    }
    if(ch==27){escape_=1;escapeBytes_=0;return false;}
    if(ch==3){
      if(editing_){clearDraft();dirty_=full_=true;return false;}
      active_=false;action.type=Action::Exit;return true;
    }
    if(ch==12){dirty_=full_=true;return false;}
    if(editing_)return edit(ch,snapshot,action);
    if(ch=='\t') {nextPage(snapshot);return false;}
    if(ch==18){action.type=Action::Refresh;return true;}
    if((page_==Led&&!snapshot.canLeds)||(page_==Pixel&&!pixels(snapshot)))return false;
    if(ch=='f' && (page_==Led||page_==Pixel)) {grid_=!grid_;dirty_=true;return false;}
    if(page_==Led && snapshot.canLeds && (ch=='w'||ch=='r'||ch=='o')) {
      action.type=Action::Cell;action.index=cell_;copy(action.key,sizeof(action.key),"cell");
      copy(action.value,sizeof(action.value),ch=='w'?"white":ch=='r'?"red":"off");return true;
    }
    if(page_==Pixel && pixels(snapshot)) {
      if(ch=='g'){group_=!group_;if(group_)pixel_=(pixel_/6)*6;dirty_=true;return false;}
      if(ch=='w'||ch=='r'||ch=='o'){pixelColor_=ch=='w'?0:ch=='r'?1:2;dirty_=true;return false;}
      if(grid_ && (ch=='+'||ch=='-'))return stepPixel(ch=='+'?1:-1,action);
    }
    if(ch=='\r'||ch=='\n') {
      crPending_=ch=='\r';
      if(grid_ && page_==Pixel && pixels(snapshot))return pixelAction(action);
      if(grid_ && page_==Led){grid_=false;dirty_=true;return false;}
      const Row* row=currentRow(snapshot);
      if(row && row->editable && keySafe(row->key)) {
        if(!row->secret && !plain(row->value,384)){setNotice("Value is not editable ASCII");return false;}
        copy(editKey_,sizeof(editKey_),row->key);
        copy(editLabel_,sizeof(editLabel_),row->label);
        secret_=row->secret;copy(draft_,sizeof(draft_),secret_?"":row->value);
        cursor_=strlen(draft_);editing_=true;draftChanged_=false;dirty_=full_=true;
      }else setNotice("Read only");
    }
    return false;
  }
  template<typename Write> bool render(const Snapshot& snapshot,Write write,bool force=false) {
    if(!active_)return false;
    if(!editing_){const uint32_t next=hash(snapshot);if(next!=lastHash_)dirty_=true;lastHash_=next;}
    if(!dirty_&&!force)return false;
    dirty_=false;
    if(editing_&&!full_&&!force){editLine(write);return true;}
    full_=false;
    Writer<Write> out(write);if(ansi_)out.raw("\x1b[H\x1b[2J\x1b[1;36m");
    out.safe(snapshot.title,64);if(ansi_)out.raw("\x1b[0m");out.raw("\r\n");
    for(uint8_t i=0;i<4;++i){
      if((i==Led&&!snapshot.canLeds)||(i==Pixel&&!pixels(snapshot)))continue;
      if(ansi_&&i==page_)out.raw("\x1b[7m");
      out.raw(i==Device?" Device ":i==Network?" Network ":i==Led?" LEDs ":" Pixels ");
      if(ansi_)out.raw("\x1b[0m");
    }
    out.raw("\r\n");
    const bool allowed=page_!=Led&&page_!=Pixel?true:page_==Led?snapshot.canLeds:pixels(snapshot);
    if(!allowed){out.raw("Unavailable\r\n");}
    else {
      const uint8_t count=rowCount(snapshot);if(selected_>=count)selected_=count?count-1:0;
      const uint8_t visible=page_==Led?4:8;
      const uint8_t start=selected_>=visible?selected_-visible+1:0;
      for(uint8_t i=0;i<visible;++i){
        const uint8_t at=start+i;
        if(at<count){
          const Row& row=snapshot.rows[at];const bool selected=at==selected_&&!grid_;
          if(ansi_&&selected){out.raw("\x1b[7m");}out.raw(selected?"> ":"  ");
          out.field(row.label,18);out.raw(row.editable?" [edit] ":" [read] ");
          out.safe(row.secret?"[hidden]":row.value,44);
          if(ansi_)out.raw("\x1b[0m");
        }
        out.raw("\r\n");
      }
      if(page_==Led){
        out.raw("Logical cell list (not physical geometry)\r\n");
        for(uint8_t line=0;line<8;++line){
          for(uint8_t col=0;col<10;++col){
            const uint8_t id=line*10+col+1;const bool picked=id==cell_;
            if(ansi_){
              const uint8_t state=snapshot.cellStates?snapshot.cellStates[id-1]:255;
              out.raw(state==0?"\x1b[37m":state==1?"\x1b[31m":"\x1b[90m");
              if(picked&&grid_)out.raw("\x1b[7m");
            }
            char number[6];snprintf(number,sizeof(number),picked?"[%02u]":" %02u ",id);out.raw(number);
            if(ansi_)out.raw("\x1b[0m");
          }
          out.raw("\r\n");
        }
        out.raw("Arrows: list | w white | r red | o off\r\n");
      } else if(page_==Pixel){
        char line[96];snprintf(line,sizeof(line),"Physical pixel %u..%u / 480 | %s | %s\r\n",pixel_+1,pixel_+(group_?6:1),group_?"six-pixel group":"single pixel",color());out.raw(line);
        out.raw("Arrows +/-: step | g group | w/r/o color | Enter show\r\n");
      }
    }
    out.raw("Tab pages | Enter edit | f fields | Esc back | Ctrl-R refresh\r\n");
    out.safe(notice_,64);out.raw("\r\n");
    if(ansi_)out.raw("\x1b[23;1H\x1b[2K");
    if(editing_)editText(out);else out.raw(" ");
    return true;
  }
 private:
  template<typename Write> class Writer {
   public:
    explicit Writer(Write write):write_(write){}
    void raw(const char* text){const size_t n=strlen(text);if(n<=remaining_){write_(reinterpret_cast<const uint8_t*>(text),n);remaining_-=n;}}
    void safe(const char* text,size_t width){char clean[129];terminalText(clean,width+1<sizeof(clean)?width+1:sizeof(clean),text);raw(clean);}
    void field(const char* text,size_t width){char clean[129];const size_t n=terminalText(clean,width+1<sizeof(clean)?width+1:sizeof(clean),text);raw(clean);for(size_t i=n;i<width;++i)raw(" ");}
   private:Write write_;size_t remaining_=4096;
  };
  static void scrub(char* p,size_t n){volatile char* out=p;while(n--)*out++=0;}
  static void copy(char* out,size_t size,const char* in){size_t i=0;if(!size)return;while(in&&in[i]&&i+1<size){out[i]=in[i];++i;}out[i]=0;}
  static bool plain(const char* text,size_t maximum){if(!text)return true;size_t n=0;while(text[n]&&n<=maximum){if(text[n]<32||text[n]>126)return false;++n;}return n<=maximum;}
  static bool keySafe(const char* key){if(!key||!key[0])return false;size_t i=0;while(key[i]&&i<32){const char c=key[i++];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))return false;}return i<32;}
  static uint8_t rowCount(const Snapshot& s){return s.rows?(s.rowCount>16?16:s.rowCount):0;}
  const Row* currentRow(const Snapshot& s)const{return selected_<rowCount(s)?s.rows+selected_:nullptr;}
  static bool pixels(const Snapshot& s){return s.canPixels&&s.pixelCount==480;}
  const char* color()const{return pixelColor_==0?"white":pixelColor_==1?"red":"off";}
  void clearDraft(){scrub(draft_,sizeof(draft_));scrub(editKey_,sizeof(editKey_));scrub(editLabel_,sizeof(editLabel_));cursor_=0;editing_=secret_=draftChanged_=false;}
  void setNotice(const char* text){terminalText(notice_,sizeof(notice_),text);dirty_=true;}
  void nextPage(const Snapshot& s){
    for(uint8_t i=0;i<4;++i){page_=static_cast<Page>((page_+1)%4);if((page_!=Led||s.canLeds)&&(page_!=Pixel||pixels(s)))break;}
    selected_=0;grid_=page_==Led||page_==Pixel;dirty_=full_=true;notice_[0]=0;
  }
  bool pixelAction(Action& a){a.type=Action::Pixel;a.index=pixel_;a.group=group_;copy(a.key,sizeof(a.key),"pixel");copy(a.value,sizeof(a.value),color());return true;}
  bool stepPixel(int direction,Action& a){
    const int next=static_cast<int>(pixel_)+direction*(group_?6:1);
    if(next<0||next>(group_?474:479)){setNotice("Pixel boundary");return false;}
    pixel_=static_cast<uint16_t>(next);dirty_=true;return pixelAction(a);
  }
  bool arrow(uint8_t key,const Snapshot& s,Action& a){
    if(editing_){
      if(key=='D'&&cursor_)--cursor_;else if(key=='C'&&cursor_<strlen(draft_))++cursor_;
      else if(key=='H')cursor_=0;else if(key=='F')cursor_=strlen(draft_);
      dirty_=true;return false;
    }
    if(grid_&&page_==Led&&s.canLeds){
      int next=cell_+(key=='D'?-1:key=='C'?1:key=='A'?-10:key=='B'?10:0);
      if(next>=1&&next<=80){cell_=static_cast<uint8_t>(next);}dirty_=true;return false;
    }
    if(grid_&&page_==Pixel&&pixels(s) && (key=='D'||key=='C'||key=='A'||key=='B'))return stepPixel(key=='D'||key=='A'?-1:1,a);
    const uint8_t count=rowCount(s);
    if(key=='A'&&selected_)--selected_;else if(key=='B'&&selected_+1<count)++selected_;
    dirty_=true;return false;
  }
  bool edit(uint8_t ch,const Snapshot& s,Action& action){
    if(ch=='\r'||ch=='\n'){
      crPending_=ch=='\r';const Row* row=currentRow(s);
      if((page_==Led&&!s.canLeds)||(page_==Pixel&&!pixels(s))||!row||!row->editable||!keySafe(row->key)||strcmp(row->key,editKey_)||row->secret!=secret_){clearDraft();setNotice("Field changed; edit cancelled");full_=true;return false;}
      if(!draftChanged_){clearDraft();dirty_=full_=true;return false;}
      action.type=Action::Set;copy(action.key,sizeof(action.key),editKey_);copy(action.value,sizeof(action.value),draft_);
      clearDraft();dirty_=full_=true;return true;
    }
    if(ch==1)cursor_=0;else if(ch==5)cursor_=strlen(draft_);
    else if(ch==21){if(cursor_){memmove(draft_,draft_+cursor_,strlen(draft_+cursor_)+1);cursor_=0;draftChanged_=true;}}
    else if(ch==23){size_t target=cursor_;while(target&&draft_[target-1]==' ')--target;while(target&&draft_[target-1]!=' ')--target;if(target<cursor_){memmove(draft_+target,draft_+cursor_,strlen(draft_+cursor_)+1);cursor_=target;draftChanged_=true;}}
    else if(ch==8||ch==127){if(cursor_){--cursor_;memmove(draft_+cursor_,draft_+cursor_+1,strlen(draft_+cursor_));draftChanged_=true;}}
    else if(ch==4){if(cursor_<strlen(draft_)){memmove(draft_+cursor_,draft_+cursor_+1,strlen(draft_+cursor_));draftChanged_=true;}}
    else if(ch>=32&&ch<127){const size_t size=strlen(draft_);if(size<384){memmove(draft_+cursor_+1,draft_+cursor_,size-cursor_+1);draft_[cursor_++]=static_cast<char>(ch);draftChanged_=true;}}
    else return false; // Form owns keys: 'r'/Tab cannot invoke global shortcuts.
    dirty_=true;return false;
  }
  template<typename Write> void editText(Writer<Write>& out)const {
    out.raw("Edit ");out.safe(editLabel_,18);out.raw(": ");
    const size_t length=strlen(draft_),start=cursor_>38?cursor_-38:0;
    char visible[41]{};for(size_t i=0;i<40&&start+i<length;++i)visible[i]=secret_?'*':draft_[start+i];out.raw(visible);
    if(ansi_){char cursor[24];char label[19];terminalText(label,sizeof(label),editLabel_);snprintf(cursor,sizeof(cursor),"\x1b[23;%uH",static_cast<unsigned>(8+strlen(label)+cursor_-start));out.raw(cursor);}
  }
  template<typename Write> void editLine(Write write)const {Writer<Write> out(write);if(ansi_)out.raw("\x1b[23;1H\x1b[2K");else out.raw("\r");editText(out);}
  static uint32_t hash(const Snapshot& s){
    uint32_t value=2166136261U;
    auto add=[&](const char* text){for(size_t i=0;text&&text[i]&&i<384;++i){value^=static_cast<uint8_t>(text[i]);value*=16777619U;}};
    add(s.title);value^=s.canLeds?1:0;value^=s.canPixels?2:0;value^=s.pixelCount;
    for(uint8_t i=0;i<rowCount(s);++i){add(s.rows[i].label);add(s.rows[i].key);if(!s.rows[i].secret)add(s.rows[i].value);value^=(s.rows[i].editable?4:0)|(s.rows[i].secret?8:0);}
    if(s.canLeds&&s.cellStates)for(uint8_t i=0;i<80;++i){value^=s.cellStates[i];value*=16777619U;}
    return value;
  }
  Page page_=Device;uint8_t selected_=0,cell_=1,pixelColor_=1;
  uint16_t pixel_=0;size_t cursor_=0;
  bool active_=false,ansi_=true,dirty_=true,full_=true,editing_=false,secret_=false,draftChanged_=false,grid_=false,group_=false,crPending_=false;
  uint8_t escape_=0,escapeBytes_=0;uint32_t lastHash_=0;
  char draft_[385]{},editKey_[32]{},editLabel_[49]{},notice_[65]{};
};
} // namespace PakarShell
