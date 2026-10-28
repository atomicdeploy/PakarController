// Compile the ACTUAL shell branch and shared AVR buzzer branch together.
// Doubles capture protocol bytes/receipts; they do not prove audible output.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
const source=readFileSync('firmware/esp/src/ShellService.h','utf8');
const app=readFileSync('firmware/esp/application.ino','utf8');
function section(text,start,end){const a=text.indexOf(start),b=text.indexOf(end,a+start.length);if(a<0||b<a)throw Error('Actual tone dispatch markers changed');return text.slice(a,b);}
const shell=section(source,'    else if(verb=="beep"){','    else if(verb=="cell"');
const avr=section(app,'  if (command == F("buzzer")) {','  if (command == F("audio")) {');
const fixture=`
#include <cassert>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include "ShellToneCommand.h"
#define F(value) value
using String=std::string;
using Value=std::variant<uint32_t,String>;
using Document=std::map<String,Value>;
namespace PakarCommand {long integerValue(const Document& d,const char* key,long fallback){auto at=d.find(key);return at==d.end()?fallback:std::get<uint32_t>(at->second);}}
unsigned attempts=0,rejections=0;bool ack=true;std::vector<uint8_t> bytes;
constexpr uint8_t kBuzzer=19;
bool submitAvrCommand(uint8_t opcode,const uint8_t* p,size_t n,String& result){assert(opcode==kBuzzer);++attempts;bytes.assign(p,p+n);result=ack?"accepted":"AVR busy";return ack;}
bool handleCloudCommand(const Document& commandRequest,String& result){const auto command=std::get<String>(commandRequest.at("command"));${avr}return false;}
struct Session{} session;
void shellResult(Session&,bool accepted,const String&){if(!accepted)++rejections;}
void shellPrompt(Session&){}
void execute(const String& args){auto& s=session;const String verb="beep";bool valid=true;String result;Document d;
if(false){}${shell}
if(valid)shellResult(s,handleCloudCommand(d,result),result);
}
int main(){
execute("");assert(attempts==1&&rejections==0&&(bytes==std::vector<uint8_t>{232,3,100,0}));
execute("440 250");assert(attempts==2&&(bytes==std::vector<uint8_t>{184,1,250,0}));
ack=false;execute("440 250");assert(attempts==3&&rejections==1);
for(const char* bad:{"0","12001 250","440 0","440 5001","nope","440 250 extra"})execute(bad);
assert(attempts==3&&rejections==7);
}
`;
const directory=resolve('build/native');mkdirSync(directory,{recursive:true});
const generated=resolve(directory,'shell_tone_dispatch_fixture.cpp'),output=resolve(directory,'shell_tone_dispatch_test.exe');
let prior;try{prior=readFileSync(generated,'utf8')}catch(error){if(error.code!=='ENOENT')throw error;}
if(prior!==fixture)writeFileSync(generated,fixture);
for(const [command,args] of [[process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror','-I',resolve('firmware/esp/src'),generated,'-o',output]],[output,[]]]){
 const result=spawnSync(command,args,{stdio:'inherit',timeout:60000});if(result.error||result.status!==0)throw Error('Actual shell/AVR tone dispatch fixture failed');
}
console.log('Actual beep-to-buzzer-to-AVR branch: finite exact tone bytes, rejection, no replay and argument bounds passed; not audible acceptance.');
