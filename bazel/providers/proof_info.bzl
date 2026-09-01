"""Providers that bind proof reports to exact artifact subjects."""

OrlixProofSubjectInfo = provider(
    doc = "Identifies the exact artifact or signed buildset under test.",
    fields = {
        "buildset_digest": "Signed buildset digest, when applicable.",
        "component": "Owning Orlix component.",
        "subject": "Artifact or tree under test.",
        "subject_digest": "Content digest of the subject.",
    },
)

OrlixProofReportInfo = provider(
    doc = "Provides one ordered proof report bound to an exact subject.",
    fields = {
        "destination": "Host, simulator, or device destination.",
        "forbidden_behavior_fields": "Required negative proof fields.",
        "prerequisite_reports": "Reports that must authorize this tier.",
        "profile": "Orlix profile under test.",
        "proof_tier": "Accepted proof tier name.",
        "report": "Structured proof report.",
        "result": "Native owning-suite result.",
        "subject_digest": "Exact tested artifact digest.",
        "toolchain_digest": "Exact toolchain manifest digest.",
    },
)
