#!/bin/bash
# trust-gateway.sh — Add gateway TLS cert to Bambu Studio's trusted CA list
#
# Usage: ./trust-gateway.sh <gateway-ip>
#
# Fetches the gateway's self-signed CA cert from http://<gateway-ip>/ca.pem
# and appends it to the printer.cer trust store that Bambu Studio uses.

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <gateway-ip>"
  echo "Example: $0 192.168.88.134"
  exit 1
fi

GATEWAY_IP="$1"
STUDIO_DIR="$HOME/Library/Application Support/BambuStudio"
PRINTER_CER="$STUDIO_DIR/printer.cer"
BACKUP="$PRINTER_CER.bak.$(date +%Y%m%d%H%M%S)"
TMP_CERT=$(mktemp /tmp/gateway-cert-XXXXXX.pem)
trap "rm -f '$TMP_CERT'" EXIT

# 1. Download gateway cert
echo "Fetching cert from http://${GATEWAY_IP}/ca.pem ..."
HTTP_CODE=$(curl -s -o "$TMP_CERT" -w "%{http_code}" "http://${GATEWAY_IP}/ca.pem")

if [ "$HTTP_CODE" != "200" ]; then
  echo "ERROR: Gateway returned HTTP $HTTP_CODE (expected 200)"
  echo "Make sure the gateway is powered on and reachable at ${GATEWAY_IP}"
  exit 1
fi

if [ ! -s "$TMP_CERT" ]; then
  echo "ERROR: Downloaded cert is empty"
  exit 1
fi

# Verify it's a valid PEM cert
if ! openssl x509 -in "$TMP_CERT" -noout -subject -issuer 2>/dev/null; then
  echo "ERROR: Downloaded file is not a valid PEM certificate"
  exit 1
fi

echo ""

# 2. Show the gateway cert details
echo "Gateway certificate:"
openssl x509 -in "$TMP_CERT" -noout -subject -issuer -dates -fingerprint -sha256 2>&1 | sed 's/^/  /'

echo ""

# 3. Get the gateway cert's subject for dedup
GATEWAY_SUBJECT=$(openssl x509 -in "$TMP_CERT" -noout -subject 2>/dev/null)
echo "Gateway cert subject: $GATEWAY_SUBJECT"

# 4. Back up the original
echo "Backing up $PRINTER_CER → $BACKUP"
cp "$PRINTER_CER" "$BACKUP"

# 5. Remove old certs with same subject, then append new gateway cert
echo "Updating $PRINTER_CER ..."

# Write filter script to temp
cat > /tmp/_filter_certs.py << 'PYEOF'
import subprocess, sys

gateway_subj = sys.argv[1]
printer_cer = sys.argv[2]

try:
    with open(printer_cer) as f:
        data = f.read()
except FileNotFoundError:
    data = ''

# Split into individual PEM certs
entries = data.strip().split('\n\n')
filtered = []
removed = 0
for entry in entries:
    entry = entry.strip()
    if not entry:
        continue
    if 'BEGIN CERTIFICATE' not in entry:
        filtered.append(entry)
        continue
    p = subprocess.run(['openssl', 'x509', '-noout', '-subject'],
                       input=entry, capture_output=True, text=True)
    subj = p.stdout.strip()
    if subj != gateway_subj:
        filtered.append(entry)
    else:
        removed += 1

with open(printer_cer, 'w') as f:
    if filtered:
        f.write('\n\n'.join(filtered) + '\n')
    else:
        f.write('')
print(f'Removed {removed} old cert(s) matching subject')
PYEOF

python3 /tmp/_filter_certs.py "$GATEWAY_SUBJECT" "$PRINTER_CER"
rm /tmp/_filter_certs.py

# 6. Append the gateway cert
echo "" >> "$PRINTER_CER"
cat "$TMP_CERT" >> "$PRINTER_CER"

echo ""
echo "Done! Gateway cert added to Bambu Studio's trust store."
echo ""
echo "Next steps:"
echo "  1. Restart Bambu Studio (File → Quit, then reopen)"
echo "  2. Try adding the printer again — Studio should now trust the gateway's TLS cert"
echo ""
echo "To restore the original: cp '$BACKUP' '$PRINTER_CER'"
