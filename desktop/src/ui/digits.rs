//! Adapted from RayanLamp internal/i18n/digits.go (MIT, YektaYar).
//! Numeric UI inputs only: never rewrite peer names or wire protocol strings.
pub fn latin(value: &str) -> String {
    value
        .chars()
        .map(|c| match c {
            '۰'..='۹' => char::from_u32('0' as u32 + c as u32 - '۰' as u32).unwrap(),
            '٠'..='٩' => char::from_u32('0' as u32 + c as u32 - '٠' as u32).unwrap(),
            _ => c,
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn both_decimal_scripts_preserve_other_unicode() {
        assert_eq!(latin("۰۱۲۳۴۵۶۷۸۹ / ٠١٢٣٤٥٦٧٨٩"), "0123456789 / 0123456789");
        assert_eq!(latin("خانه ۱۲ — #٣"), "خانه 12 — #3");
    }
}
