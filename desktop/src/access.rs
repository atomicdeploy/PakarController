pub fn validate(user: &str, password: &str, confirmation: &str) -> Result<(), String> {
    if user.is_empty()
        || user.len() > 32
        || !user
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b"._-".contains(&b))
    {
        return Err("Username must be 1–32 letters, digits, dots, underscores or hyphens".into());
    }
    if !(8..=64).contains(&password.len()) || !password.bytes().all(|b| (0x20..=0x7e).contains(&b))
    {
        return Err("Password must be 8–64 printable ASCII characters".into());
    }
    if password != confirmation {
        return Err("Passwords do not match".into());
    }
    Ok(())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn credential_boundaries_and_confirmation() {
        assert!(validate("admin", "new password", "new password").is_ok());
        for user in ["", "a:b", "a b", "a/b", "é", &"x".repeat(33)] {
            assert!(validate(user, "abcdefgh", "abcdefgh").is_err());
        }
        for password in ["short", "abcdefgh\n", "éabcdefg", &"x".repeat(65)] {
            assert!(validate("admin", password, password).is_err());
        }
        assert!(validate(&"x".repeat(32), &"x".repeat(64), &"x".repeat(64)).is_ok());
        assert!(validate("admin", "abcdefgh", "abcdEfgh").is_err());
    }
}
