use serde::{Deserialize, Serialize};
pub const PATH: &str = "/api/pixels/corrections";
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Rule {
    pub kind: String,
    pub pixel: u16,
    pub offset: i16,
    #[serde(default = "enabled_by_default")]
    pub enabled: bool,
}
const fn enabled_by_default() -> bool { true }
impl Rule {
    /// Integer adaptation of Pealayer's bounded numeric adjustment: never
    /// alter another rule, its enabled flag or the last board-owned mapping.
    pub fn step_pixel(&mut self, delta: i16, pixels: u16) -> bool {
        if pixels == 0 { return false; }
        let next=(i32::from(self.pixel)+i32::from(delta)).clamp(0,i32::from(pixels)-1) as u16;
        let changed=next!=self.pixel;self.pixel=next;changed
    }
    pub fn step_offset(&mut self, delta: i16, pixels: u16) -> bool {
        if pixels == 0 || self.kind!="shift" { return false; }
        let bound=i32::from(pixels.min(i16::MAX as u16));
        let next=(i32::from(self.offset)+i32::from(delta)).clamp(-bound,bound) as i16;
        let changed=next!=self.offset;self.offset=next;changed
    }
}
#[derive(Clone, Serialize, Deserialize)]
pub struct Configuration {
    pub revision: u32,
    pub enabled: bool,
    pub rules: Vec<Rule>,
    #[serde(default)]
    pub pixels: u16,
    #[serde(default)]
    pub sources: Vec<i16>,
}
#[derive(Default)]
pub struct Editor {
    pub tab: bool,
    pub owner: Option<crate::peer_http::Pin>,
    pub value: Option<Configuration>,
    pub dirty: bool,
    pub message: String,
    pub pending: Option<std::time::Instant>,
    pub expected: Option<Configuration>,
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn pixel_corrections_disabled_rules_survive_read_edit_reorder_and_write() {
        let mut value:Configuration=serde_json::from_value(serde_json::json!({"revision":7,"enabled":true,"pixels":480,
            "sources":[0,-1,2],"rules":[{"kind":"shift","pixel":1,"offset":-1,"enabled":false},
                {"kind":"black","pixel":9,"offset":0}]})).unwrap();
        assert!(!value.rules[0].enabled);assert!(value.rules[1].enabled,"Omitted old field inherits true only");
        assert!(value.rules[0].step_pixel(1,value.pixels));
        assert!(value.rules[0].step_offset(1,value.pixels));
        value.rules.swap(0,1);
        let saved=serde_json::to_value(&value).unwrap();
        assert_eq!(saved["rules"][1]["enabled"],false);
        assert_eq!(saved["rules"][1]["pixel"],2);
        assert_eq!(saved["sources"],serde_json::json!([0,-1,2]));
        assert_eq!(saved["revision"],7);
        assert!(serde_json::from_value::<Rule>(serde_json::json!({"kind":"black","pixel":0,"offset":0,"enabled":"false"})).is_err());
    }
    #[test]
    fn pixel_corrections_integer_adjustments_obey_boundaries_without_reenabling() {
        let mut rule=Rule{kind:"shift".into(),pixel:0,offset:-480,enabled:false};
        assert!(!rule.step_pixel(-1,480));assert!(!rule.step_offset(-1,480));
        assert!(rule.step_pixel(1,480));assert!(rule.step_offset(1,480));
        rule.pixel=479;rule.offset=480;
        assert!(!rule.step_pixel(1,480));assert!(!rule.step_offset(1,480));
        assert!(rule.step_pixel(-1,480));assert!(rule.step_offset(-1,480));
        assert!(!rule.enabled);
        assert!(!rule.step_pixel(1,0));assert!(!rule.step_offset(1,0));
        rule.kind="black".into();assert!(!rule.step_offset(-1,480));
    }
}
