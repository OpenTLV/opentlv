# Repository governance

## Community defaults

OpenTLV/opentlv inherits the organization's contribution, conduct, security,
and support policies, all five issue forms, and the pull request template.
There are no local overrides. Keep the project license and any applicable
notices in this repository. See the [adoption guide] for how GitHub resolves
shared files.

On 2026-11-23, [Community Standards] linked the shared contribution, conduct,
security, and pull request files. The [security policy] displayed private
reporting instructions, and the chooser's [community support destination]
was reachable. On 2026-11-17, the maintainer saw all five forms and both
contact links in the chooser and accepted the shared PR template. Prefilling
the description of a new PR still needs direct observation. [Adoption record]

| Issue form | Default in this repository |
| --- | --- |
| Bug report | `bug` label |
| Feature request | `enhancement` label |
| Task | `Task` organization issue type |
| Chore | `chore` label |
| Epic | `Epic` organization issue type |

All three [repository labels] were present on 2026-11-23. The maintainer
confirmed both organization issue types and saw the expected defaults
preselected in the inherited forms on 2026-11-18. Automatic assignment on
submission and the chooser as seen by a signed-in non-maintainer remain
untested. [Form verification] · [Adoption record]

## Merging to main

Use a pull request with signed commits and resolve review conversations before
merging. Use a regular merge commit to preserve the branch history. The
[effective rules for main] and the [repository ruleset] showed the following
on 2026-11-23.

- The active ruleset targets only `refs/heads/main`. It requires signatures
  and a pull request, permits only merge commits, and blocks force pushes
  and branch deletion.
- The solo-maintainer configuration requires zero approving reviews and
  requires conversation resolution. Code Owner approval, last-push approval,
  and linear history are disabled.
- The [maintainer audit] recorded an empty bypass list. The public ruleset
  response on 2026-11-23 did not expose that list.

There were no project workflows, check runs on `main`, or required status
checks on 2026-11-23. When project CI first reports on a pull request, verify
the exact check names and source app IDs, then require the applicable checks
and an up-to-date branch under the [governance baseline]. At the first signed
PR merge, verify the merge commit and the introduced commits on GitHub.

## Actions and dependency security

The [maintainer audit] recorded the following repository settings by
2026-11-17. The Actions and Dependabot settings require authenticated access
for a fresh inspection.

| Control | Verified setting |
| --- | --- |
| Allowed actions | Organization actions plus `actions/checkout@*` and `actions/setup-python@*`. Actions must be pinned to full commit SHAs. |
| Default `GITHUB_TOKEN` | Read access to contents and packages. The option allowing GitHub Actions to create or approve pull requests is disabled. |
| Fork workflows | All external contributors require approval. |
| Dependency security | Dependency graph, Dependabot alerts, and Dependabot security updates enabled. |

On 2026-11-23, GitHub returned the repository's [dependency graph export] and
reported [private vulnerability reporting] enabled. The public [Advisories page]
displayed **Report a vulnerability**. Opening the form as a signed-in
non-maintainer remains unverified. [Reporting verification]

[adoption guide]: https://github.com/OpenTLV/.github/blob/main/docs/adoption.md
[Community Standards]: https://github.com/OpenTLV/opentlv/community
[security policy]: https://github.com/OpenTLV/opentlv/security/policy
[community support destination]: https://github.com/OpenTLV/opentlv/discussions/categories/q-a
[Adoption record]: https://github.com/OpenTLV/.github/blob/main/docs/adoption.md#follow-up-evidence-and-remaining-checks
[repository labels]: https://github.com/OpenTLV/opentlv/labels
[Form verification]: https://github.com/OpenTLV/.github/issues/42
[effective rules for main]: https://api.github.com/repos/OpenTLV/opentlv/rules/branches/main
[repository ruleset]: https://github.com/OpenTLV/opentlv/rules/27461544
[maintainer audit]: https://github.com/OpenTLV/.github/issues/10
[governance baseline]: https://github.com/OpenTLV/.github/blob/main/docs/governance.md
[dependency graph export]: https://api.github.com/repos/OpenTLV/opentlv/dependency-graph/sbom
[private vulnerability reporting]: https://api.github.com/repos/OpenTLV/opentlv/private-vulnerability-reporting
[Advisories page]: https://github.com/OpenTLV/opentlv/security/advisories
[Reporting verification]: https://github.com/OpenTLV/.github/issues/28
