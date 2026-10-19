//! Bounded observation journal, never a command replay queue. Adapts the actual
//! PCController internal/localdevice/events.go bounded typed-event admission;
//! its fixed product contract selector is deliberately not copied into this
//! living API. Board/boot fencing and cursor gaps are explicit here.
use serde_json::{Value,json};
use std::collections::VecDeque;

pub(super) struct Journal {
    session:String,
    sequence:u64,
    entries:VecDeque<(u64,Value)>,
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn journal_is_bounded_reports_gaps_and_never_replays_other_peer(){
        let mut journal=Journal::default();let cursor=journal.cursor();
        for n in 0..70 {journal.push(json!({"type":"panel-event","board_id":"board","boot_id":"boot","event_id":n}));}
        let result=journal.read(Some(&cursor),"board","boot").unwrap();
        assert_eq!(result["events"].as_array().unwrap().len(),64);assert_eq!(result["gap"],true);
        assert!(journal.read(Some(&journal.cursor()),"board","boot").unwrap()["events"].as_array().unwrap().is_empty());
        assert!(journal.read(None,"board","other-boot").unwrap()["events"].as_array().unwrap().is_empty());
        assert_eq!(journal.read(Some(&format!("{}:1","a".repeat(32))),"board","boot").unwrap()["gap"],true);
        assert!(journal.read(Some("bad:NaN"),"board","boot").is_err());
        let before=journal.cursor();journal.push(json!({"board_id":"board","boot_id":"boot","text":"x".repeat(9000)}));assert_eq!(journal.cursor(),before);
    }
}
impl Default for Journal {
    fn default()->Self {Self{session:uuid::Uuid::new_v4().simple().to_string(),sequence:0,entries:VecDeque::new()}}
}
impl Journal {
    pub fn cursor(&self)->String {format!("{}:{}",self.session,self.sequence)}
    pub fn push(&mut self,event:Value) {
        if event.to_string().len()>8192{return;}
        self.sequence+=1;
        if self.entries.len()==64 {self.entries.pop_front();}
        self.entries.push_back((self.sequence,event));
    }
    pub fn read(&self,after:Option<&str>,board:&str,boot:&str)->Result<Value,String> {
        let (sequence,mut gap)=match after {
            None=>(0,false),
            Some(value)=>{
                let (session,sequence)=value.split_once(':').ok_or("Invalid event cursor")?;
                if session.len()!=32||!session.bytes().all(|b|b.is_ascii_hexdigit()) {return Err("Invalid event cursor".into());}
                let sequence=sequence.parse::<u64>().map_err(|_|"Invalid event cursor")?;
                if session!=self.session {(0,true)}else if sequence>self.sequence {(0,true)}else{(sequence,false)}
            }
        };
        if self.entries.front().is_some_and(|(first,_)|sequence.saturating_add(1)<*first){gap=true;}
        let events:Vec<_>=self.entries.iter().filter(|(id,event)|*id>sequence&&event["board_id"]==board&&event["boot_id"]==boot)
            .map(|(id,event)|json!({"cursor":format!("{}:{id}",self.session),"event":event})).collect();
        Ok(json!({"cursor":self.cursor(),"events":events,"gap":gap}))
    }
}
