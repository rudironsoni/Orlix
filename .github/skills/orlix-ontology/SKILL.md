---
name: orlix-ontology
description: >-
  Apply the Orlix Markdown ontology when choosing object types, properties,
  typed links, normalization, source modeling, or schema changes.
---
# Orlix Ontology Modeling

Read `docs/ontology.md` first.

- Use a property when a value has no independent identity.
- Use a typed link when the target has its own page.
- Create an object when the thing has identity, facts, lifecycle, or future references.
- Use concrete Orlix domain nouns and create folders only when a real instance exists.
- Keep one canonical home per fact.
- Promote a relationship to an object when it has its own date, owner, status, or evidence.
- Add a link type only when it recurs, reads naturally, and `relates_to` loses important meaning.

Schema changes update `docs/ontology.md` and the ontology linter in the same change.
