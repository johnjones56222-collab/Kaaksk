#!/bin/bash
# Usage: vault_upload.sh <file> "<note>"  -> uploads in 20MB chunks to the Cloudflare KV vault, then verifies by downloading back.
set -e
F="$1"; NOTE="$2"; NAME=$(basename "$F")
V="https://llama-model-vault.johnjones56222.workers.dev"; K="${VAULT_KEY:?set VAULT_KEY}"  # redacted from the backup
SIZE=$(stat -c %s "$F"); SHA=$(sha256sum "$F" | cut -d' ' -f1)
T=$(mktemp -d); split -b 20000000 -d -a 3 "$F" "$T/part"
N=0
for P in $(ls "$T"/part* | sort); do
  curl -sS --fail --retry 3 -X PUT -H "x-key: $K" -H 'content-type: application/octet-stream' --data-binary @"$P" "$V/chunk/$NAME/$N" > /dev/null
  N=$((N+1))
done
curl -sS --fail -X PUT -H "x-key: $K" -H 'content-type: application/json' \
  -d "{\"name\":\"$NAME\",\"size\":$SIZE,\"sha256\":\"$SHA\",\"chunks\":$N,\"note\":\"$NOTE\"}" "$V/manifest/$NAME" > /dev/null
GOT=$(curl -sS --fail -H "x-key: $K" "$V/file/$NAME" | sha256sum | cut -d' ' -f1)
rm -rf "$T"
[ "$GOT" = "$SHA" ] && echo "VAULT_OK $NAME size=$SIZE chunks=$N sha256=$SHA" || { echo "VAULT_MISMATCH $NAME expected=$SHA got=$GOT"; exit 1; }
