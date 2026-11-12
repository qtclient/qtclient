# Security Policy

## Supported versions

Security fixes target the latest `main` of the public [qtclient/qtclient](https://github.com/qtclient/qtclient) tree and the most recent tagged release when tags exist.

## Reporting a vulnerability

Please report security issues privately:

- GitHub Security Advisories on the qtclient repository, or
- Email the maintainers listed in AUTHORS

Include steps to reproduce, affected platforms (desktop / WASM), and impact.
Do not open a public issue for unreleased vulnerabilities.

## Scope

In scope: memory safety in the stock client/launcher, auth token handling in
our code, and packaging scripts that could leak secrets.

Out of scope: CipSoft servers/assets, product-only packs not shipped in this
repository, and issues that require a private Tibia install to exploit without
a clear stock-binary vector.
