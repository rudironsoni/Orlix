# Orlix TCTI MCP

This project-local MCP server exposes Orlix TCTI status and no-phone verification tools without exposing arbitrary shell execution.

Tools:

- `tcti_status`
- `tcti_next`
- `tcti_report_read`
- `tcti_reproducer_read`
- `tcti_golden_list`
- `tcti_golden_validate`
- `tcti_safety_audit`
- `tcti_plan_consistency`

The server is intentionally domain-specific. It may run only allowlisted Make targets and may read only allowlisted TCTI report, reducer, plan, and fixture paths.
