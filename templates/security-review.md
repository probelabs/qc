---
id: security-review
applies_when: The change touches auth, secrets, or untrusted input
scope: [src/**]
expires: 14d
---

- [ ] authz-paths :: Which authorization checks did you re-read?
- [ ] secrets     :: Which new secret or credential surfaces exist?
- [ ] input       :: Which untrusted inputs reach this change?
