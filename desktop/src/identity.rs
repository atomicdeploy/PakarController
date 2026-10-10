pub fn normalize_name(input: &str) -> Result<String, String> {
    let lower = input.to_ascii_lowercase();
    let name = lower.strip_suffix(".local").unwrap_or(&lower);
    if name.is_empty()
        || name.len() > 63
        || name.starts_with('-')
        || name.ends_with('-')
        || matches!(name, "localhost" | "local")
        || !name
            .bytes()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'-')
    {
        return Err(
            "Use 1..63 letters/digits/hyphens, no leading/trailing hyphen; optional .local suffix"
                .into(),
        );
    }
    Ok(name.into())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn normalizes_dns_label_not_url() {
        assert_eq!(normalize_name("LEDBOARD.Local").unwrap(), "ledboard");
        assert_eq!(normalize_name("panel-2").unwrap(), "panel-2");
        assert!(normalize_name(&"a".repeat(63)).is_ok());
        assert!(normalize_name(&format!("{}.local", "a".repeat(63))).is_ok());
        for name in [
            "",
            "-x",
            "x-",
            "a.b",
            "http://ledboard.local",
            "x.local.local",
            "x_y",
            "x local",
            "local",
            "localhost",
            "é",
            "x\0y",
        ] {
            assert!(normalize_name(name).is_err(), "{name:?}");
        }
        assert!(normalize_name(&"a".repeat(64)).is_err());
    }
}
