#!/usr/bin/env bash
# GeneralsX @build Codex 09/10/2026 Resolve the installed Git for Windows CA bundle.
set -euo pipefail
output_dir="${1:?Usage: package-windows-ca.sh output-directory}"
configured_ca="$(git config --path --get http.sslCAInfo || true)"
for candidate in "$configured_ca" /mingw64/ssl/certs/ca-bundle.crt /mingw64/etc/ssl/certs/ca-bundle.crt /usr/ssl/certs/ca-bundle.crt; do
  if [ -n "$candidate" ] && [ -s "$candidate" ]; then
    openssl x509 -in "$candidate" -noout
    cp "$candidate" "$output_dir/cacert.pem"
    printf 'Packaged installed Git for Windows CA bundle from %s\n' "$candidate"
    exit 0
  fi
done
printf 'Installed Git for Windows CA bundle was not found.\n' >&2
exit 1
