//! Bounded, semantic cell-color interchange; file selection never actuates hardware.
use serde::{Deserialize, Serialize};
use std::{collections::BTreeSet, io::Cursor, path::Path};
pub const LIMIT: usize = 1_048_576;
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Assignment { pub cell: u8, pub state: String }
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Document { pub role: String, pub assignments: Vec<Assignment> }
pub fn state(value: &str) -> Result<String, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "white" | "#ffffff" => Ok("white".into()),
        "red" | "#ff0000" => Ok("red".into()),
        "off" | "#000000" => Ok("off".into()),
        _ => Err("State must be white, red or off (or its exact RGB hex value)".into()),
    }
}
pub fn rejection(code:&str)->String {
    match code {
        "red_cells_locked"=>"Red lock prevents changing red cells to white; unlock explicitly or keep those assignments unchanged".into(),
        "invalid_cell_assignments"=>"Assignment import rejected: use unique cells 1–80 and white, red or off".into(),
        "unsupported_board_role"=>"This operation is not supported by the connected board's baked-in role".into(),
        _=>format!("Board rejected command: {code}"),
    }
}
pub fn usb_rejection(code:u8)->String {match code{7=>rejection("red_cells_locked"),8=>rejection("invalid_cell_assignments"),9=>rejection("unsupported_board_role"),_=>format!("USB command rejected (error {code}); no replay")}}
impl Document {
    pub fn new(mut assignments: Vec<Assignment>) -> Result<Self, String> {
        if assignments.is_empty() || assignments.len() > 80 { return Err("Import requires 1–80 assignments".into()); }
        let mut seen = BTreeSet::new();
        for row in &mut assignments {
            if !(1..=80).contains(&row.cell) || !seen.insert(row.cell) { return Err("Cell IDs must be unique integers from 1 to 80".into()); }
            row.state = state(&row.state)?;
        }
        assignments.sort_by_key(|row| row.cell);
        Ok(Self { role: "table-selection".into(), assignments })
    }
    pub fn from_status(status: &crate::DeviceStatus) -> Result<Self, String> {
        if !supported(status) || status.cells.len() != 80 { return Err("Export requires all 80 known assignments from this board".into()); }
        Self::new(status.cells.iter().enumerate().map(|(i,s)| Assignment {cell:i as u8+1,state:s.clone()}).collect())
    }
    pub fn verify(&self, status: &crate::DeviceStatus) -> bool {
        self.assignments.iter().all(|a| status.cells.get(a.cell as usize-1) == Some(&a.state))
    }
    pub fn form(&self) -> Vec<(String,String)> { vec![("assignments".into(),serde_json::to_string(&self.assignments).expect("bounded assignments"))] }
    pub fn payload(&self) -> Vec<u8> {
        let mut payload=vec![0;30];
        for a in &self.assignments { let i=usize::from(a.cell-1);payload[i/8]|=1<<(i%8);payload[10+i/4]|=match a.state.as_str(){"red"=>1,"off"=>2,_=>0}<<((i%4)*2); }
        payload
    }
    pub fn from_payload(payload:&[u8])->Result<Self,String>{
        if payload.len()!=30{return Err("Invalid assignment payload".into());}
        Self::new((0..80).filter(|i|payload[i/8]&(1<<(i%8))!=0).map(|i|{
            let state=match (payload[10+i/4]>>((i%4)*2))&3{0=>"white",1=>"red",2=>"off",_=>"invalid"};
            Assignment{cell:i as u8+1,state:state.into()}
        }).collect())
    }
}
pub fn supported(status:&crate::DeviceStatus)->bool { !status.programming_latch && status.capabilities.as_ref().is_some_and(|m|m.table_selection() && m.has("cell-assignments")) }
fn rows(values:Vec<Vec<String>>)->Result<Document,String>{
    if values.len()<2 || values.len()>81 || values[0].len()!=2 || values[0][0].trim()!="cell" || values[0][1].trim()!="state" {return Err("Expected header cell,state and 1–80 rows".into());}
    Document::new(values.into_iter().skip(1).map(|row|{
        if row.len()!=2{return Err("Every row needs exactly cell and state".into());}
        let cell=row[0].trim().parse::<u8>().map_err(|_|"Cell IDs must be integers, not formulas")?;
        Ok(Assignment{cell,state:row[1].clone()})
    }).collect::<Result<Vec<_>,String>>()?)
}
pub fn decode(extension:&str,bytes:&[u8])->Result<Document,String>{
    if bytes.is_empty() || bytes.len()>LIMIT{return Err("Assignment file must be at most 1 MiB".into());}
    match extension.to_ascii_lowercase().as_str(){
        "json"=>{let doc:Document=serde_json::from_slice(bytes).map_err(|e|format!("Invalid assignment JSON: {e}"))?;
            if doc.role!="table-selection"{return Err("This assignment file belongs to a different board role".into());}Document::new(doc.assignments)},
        "csv"=>{
            let text=std::str::from_utf8(bytes).map_err(|_|"CSV must be UTF-8")?.trim_start_matches('\u{feff}');
            // Only two fixed scalar columns are accepted. No spreadsheet formulas,
            // multiline cells, arbitrary quoting or silently ignored extra columns.
            let values=text.lines().map(|line|line.split(',').map(|v|{
                let v=v.trim();
                let v=if v.starts_with('"')&&v.ends_with('"')&&v.len()>=2 {&v[1..v.len()-1]}else{v};
                if v.contains('"'){return Err("Invalid CSV quoting".to_owned());}Ok(v.to_owned())
            }).collect::<Result<Vec<_>,String>>()).collect::<Result<Vec<_>,String>>()?;
            rows(values)
        },
        "xlsx"=>{
            // Inspect the compressed container before a parser can allocate a huge
            // shared-string table. No macro, external relationship or formula input.
            let mut archive=zip::ZipArchive::new(Cursor::new(bytes)).map_err(|_|"Invalid XLSX container")?;
            if archive.len()>64{return Err("Workbook has too many parts".into());}
            let mut total=0u64;let mut inflated_total=0usize;let mut parts=BTreeSet::new();
            for i in 0..archive.len(){let mut file=archive.by_index(i).map_err(|_|"Invalid workbook part")?;
                total=total.checked_add(file.size()).ok_or("Workbook size overflow")?;
                let name=file.name().map_err(|_|"Invalid workbook part name")?.into_owned();
                if !parts.insert(name.clone()) || total>4*LIMIT as u64 || name.contains("externalLinks") || name.contains("vbaProject"){return Err("Workbook is oversized or contains duplicate/external/macro content".into());}
                use std::io::Read;
                let declared=file.size();let mut inflated=Vec::new();
                (&mut file).take((4*LIMIT-inflated_total+1) as u64).read_to_end(&mut inflated).map_err(|_|"Corrupt workbook part")?;
                inflated_total+=inflated.len();
                if inflated_total>4*LIMIT || inflated.len() as u64!=declared{return Err("Workbook inflation limit or declared-size mismatch".into());}
            }
            use calamine::Reader;
            let mut book=calamine::Xlsx::new(Cursor::new(bytes)).map_err(|e|format!("Invalid workbook: {e}"))?;
            let names=book.sheet_names();if names.len()!=1{return Err("Use exactly one worksheet with cell and state columns".into());}
            let name=&names[0];
            // Streaming coordinates are bounded before allocating a rectangular
            // range: a tiny sparse workbook must not allocate Excel's full grid.
            let mut reader=book.worksheet_cells_reader(name).map_err(|e|e.to_string())?;
            let mut values=vec![vec![String::new();2];81];let mut seen=BTreeSet::new();let mut last=0;
            while let Some(cell)=reader.next_cell_with_formula_metadata().map_err(|e|e.to_string())?{
                if cell.formula.is_some(){return Err("Formula cells are not allowed".into());}
                let (row,column)=cell.pos;
                if row>80||column>1||!seen.insert(cell.pos){return Err("Workbook contains out-of-range or duplicate cells".into());}
                let text=match cell.value {
                    calamine::DataRef::String(s)=>s,calamine::DataRef::SharedString(s)=>s.into(),
                    calamine::DataRef::Int(v) if column==0=>v.to_string(),
                    calamine::DataRef::Float(v) if column==0 && v.is_finite() && v.fract()==0.0=>v.to_string(),
                    calamine::DataRef::Empty=>String::new(),
                    _=>return Err("Only integer cell IDs and text states are allowed".into()),
                };
                values[row as usize][column as usize]=text;last=last.max(row as usize);
            }
            values.truncate(last+1);rows(values)
        },
        _=>Err("Choose JSON, CSV or Excel .xlsx".into()),
    }
}
pub fn encode(extension:&str,doc:&Document)->Result<Vec<u8>,String>{
    let checked=Document::new(doc.assignments.clone())?;
    match extension.to_ascii_lowercase().as_str(){
        "json"=>serde_json::to_vec_pretty(&checked).map_err(|e|e.to_string()),
        "csv"=>Ok(format!("cell,state\r\n{}",checked.assignments.iter().map(|a|format!("{},{}\r\n",a.cell,a.state)).collect::<String>()).into_bytes()),
        "xlsx"=>{
            let mut book=rust_xlsxwriter::Workbook::new();let sheet=book.add_worksheet();
            let head=rust_xlsxwriter::Format::new().set_bold().set_background_color("#183541").set_font_color("#FFFFFF");
            sheet.set_name("Cells").map_err(|e|e.to_string())?;
            sheet.set_column_width(0,12).map_err(|e|e.to_string())?;sheet.set_column_width(1,18).map_err(|e|e.to_string())?;
            sheet.write_string_with_format(0,0,"cell",&head).map_err(|e|e.to_string())?;sheet.write_string_with_format(0,1,"state",&head).map_err(|e|e.to_string())?;
            for (i,a) in checked.assignments.iter().enumerate(){sheet.write_number(i as u32+1,0,a.cell).map_err(|e|e.to_string())?;sheet.write_string(i as u32+1,1,&a.state).map_err(|e|e.to_string())?;}
            book.save_to_buffer().map_err(|e|e.to_string())
        },_=>Err("Choose JSON, CSV or Excel .xlsx".into())
    }
}
pub fn load(path:&Path)->Result<Document,String>{
    if std::fs::metadata(path).map_err(|e|e.to_string())?.len()>LIMIT as u64{return Err("Assignment file exceeds 1 MiB".into());}
    use std::io::Read;
    let mut bytes=Vec::new();std::fs::File::open(path).map_err(|e|e.to_string())?.take(LIMIT as u64+1).read_to_end(&mut bytes).map_err(|e|e.to_string())?;
    decode(path.extension().and_then(|s|s.to_str()).unwrap_or_default(),&bytes)
}
#[cfg(test)]mod tests{
    use super::*;
    #[test]fn semantic_import_is_atomic_and_bounded(){
        assert!(decode("csv",b"cell,state\n1,red\n1,white").is_err());assert!(decode("csv",b"cell,state\n81,red").is_err());
        assert!(decode("csv",b"cell,state\n1,=RED()").is_err());assert!(decode("csv",b"cell,state\n=1,red").is_err());
        let doc=decode("csv",b"cell,state\n80,#FF0000\n1,#ffffff").unwrap();assert_eq!(doc.assignments[0].cell,1);
        assert_eq!(Document::from_payload(&doc.payload()).unwrap(),doc);
    }
    #[test]fn json_csv_and_real_xlsx_roundtrip(){
        let doc=Document::new((1..=80).map(|cell|Assignment{cell,state:if cell%2==0{"red"}else{"off"}.into()}).collect()).unwrap();
        for extension in ["json","csv","xlsx"]{let bytes=encode(extension,&doc).unwrap();assert_eq!(decode(extension,&bytes).unwrap(),doc);}
    }
    #[test]fn xlsx_formulas_sparse_grid_and_invalid_typed_values_reject(){
        for case in 0..3 {
            let mut book=rust_xlsxwriter::Workbook::new();let sheet=book.add_worksheet();
            sheet.write_string(0,0,"cell").unwrap();sheet.write_string(0,1,"state").unwrap();
            match case {
                0=>{sheet.write_formula(1,0,"=1").unwrap();sheet.write_string(1,1,"red").unwrap();},
                1=>{sheet.write_number(1_048_575,1,80).unwrap();},
                _=>{sheet.write_boolean(1,0,true).unwrap();sheet.write_string(1,1,"red").unwrap();},
            }
            assert!(decode("xlsx",&book.save_to_buffer().unwrap()).is_err());
        }
        assert!(decode("json",br##"{"role":"table-selection","assignments":[{"cell":1,"state":"#FF0000","future_optional":3}]}"##).is_ok());
        assert!(decode("csv",b"cell,state\n\"1,red").is_err());
    }
    fn status()->crate::DeviceStatus{
        serde_json::from_value(serde_json::json!({"version":crate::peer_identity::VERSION,"hostname":"ledboard","board_id":"esp32s3-fixture",
            "boot_id":"a".repeat(32),"http_peer_fencing":true,"command_clock_ms":1000,"mode":"cell","red_lock":false,"cells":vec!["white";80],
            "capabilities":{"role":"table-selection","board_kind":"table","acknowledged_features":["table","cell-assignments","cell-lock","command-ack"],
            "modes":[{"id":"cell","code":1,"kind":"presentation"}],"table":{"columns":10,"rows":8,"pixels_per_cell":6}}})).unwrap()
    }
    #[test]fn role_and_unknown_assignments_never_grant_an_export(){
        let mut current=status();assert_eq!(Document::from_status(&current).unwrap().assignments.len(),80);
        current.cells[79]="unknown".into();assert!(Document::from_status(&current).is_err());
        current=status();current.capabilities.as_mut().unwrap().role="first-wins".into();
        assert!(!supported(&current));assert!(!current.capabilities.as_ref().unwrap().allows("/api/cells/lock",&[]));
    }
    #[test]fn websocket_assignment_and_lock_require_exact_correlated_readback(){
        let current=status();let gate=crate::connection::Gate::default();let epoch=gate.select(Some(crate::Transport::WiFi));
        let (tx,_)=std::sync::mpsc::channel();let mut queue=crate::peer_http::Queue::new(tx);queue.observe(&current);
        let doc=Document::new(vec![Assignment{cell:1,state:"red".into()},Assignment{cell:80,state:"off".into()}]).unwrap();
        let endpoint=crate::Endpoint{base:"http://127.0.0.1:1".into(),user:String::new(),password:String::new()};
        let intent=crate::realtime::Intent::new(endpoint.clone(),&current,"/api/cells/assignments",doc.form(),queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).unwrap();
        let packet:serde_json::Value=serde_json::from_str(&intent.packet).unwrap();assert!(packet["assignments"].is_array());assert_eq!(packet["command"],"cell-assignments");
        let mut after=current.clone();after.cells[0]="red".into();after.cells[79]="off".into();
        let good=serde_json::json!({"type":"command-result","request_id":intent.id,"board_id":current.board_id,"boot_id":current.boot_id,"accepted":true,"count":2,"status":after});
        assert!(intent.verify(good.clone()).is_ok());let mut bad=good;bad["status"]["cells"][79]="white".into();assert!(intent.verify(bad).is_err());
        let lock=crate::realtime::Intent::new(endpoint,&current,"/api/cells/lock",vec![("enabled".into(),"true".into())],queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).unwrap();
        let packet:serde_json::Value=serde_json::from_str(&lock.packet).unwrap();assert_eq!(packet["enabled"],true);assert_eq!(packet["command"],"cell-lock");
        after.red_lock=Some(true);let good=serde_json::json!({"type":"command-result","request_id":lock.id,"board_id":current.board_id,"boot_id":current.boot_id,"accepted":true,"status":after});
        assert!(lock.verify(good.clone()).is_ok());let mut bad=good;bad["status"]["red_lock"]=serde_json::Value::Null;assert!(lock.verify(bad).is_err());
    }
    #[test]fn usb_assignments_and_lock_stay_boot_fenced_and_verified(){
        let current=status();let doc=Document::new(vec![Assignment{cell:1,state:"red".into()},Assignment{cell:80,state:"off".into()}]).unwrap();
        let (opcode,payload,_)=crate::bridge::usb_request("/api/cells/assignments",&doc.form(),&current).unwrap();assert_eq!(opcode,0x62);
        let fenced=crate::usb_table::fenced(&current,opcode,&payload).unwrap();assert_eq!(fenced.len(),47);assert_eq!(fenced[16],0x62);
        let mut reply=vec![0x62];reply.extend([0xaa;16]);reply.extend([1,0,1,255,0,1,0]);reply.extend([0;20]);reply.push(1);reply[24]=1;reply[43]=2<<6;
        let after=crate::usb_table::command_state(&current,0x62,&reply).unwrap();assert_eq!(after.red_lock,Some(true));assert!(doc.verify(&after));
        let (opcode,payload,_)=crate::bridge::usb_request("/api/cells/lock",&[("enabled".into(),"true".into())],&current).unwrap();assert_eq!((opcode,payload),(0x63,vec![1]));
        assert!(usb_rejection(7).contains("Red lock"));assert!(usb_rejection(8).contains("unique"));assert!(usb_rejection(9).contains("baked-in role"));
    }
}
