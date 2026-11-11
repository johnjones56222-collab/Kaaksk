#!/usr/bin/env bash
# Builds VettaStudio.apk on-device in Termux (no Gradle / Android Studio).
# Needs: pkg install aapt aapt2 apksigner d8 openjdk-21 zip
# ANDROID_JAR must point to an API 34 android.jar.
set -euo pipefail
cd "$(dirname "$0")"
: "${ANDROID_JAR:?set ANDROID_JAR to an API 34 android.jar}"
KEYSTORE="${KEYSTORE:-$HOME/.vetta-studio.keystore}"

rm -rf build && mkdir -p build/res build/gen build/classes build/dex

aapt2 compile --dir res -o build/res.zip
aapt2 link -I "$ANDROID_JAR" --manifest AndroidManifest.xml \
  --java build/gen -o build/unsigned.apk build/res.zip

javac --release 11 -classpath "$ANDROID_JAR" -d build/classes \
  $(find src build/gen -name '*.java')
d8 --release --min-api 24 --lib "$ANDROID_JAR" --output build/dex \
  $(find build/classes -name '*.class')

cp build/dex/classes.dex build/
(cd build && zip -q unsigned.apk classes.dex)

if [ ! -f "$KEYSTORE" ]; then
  keytool -genkeypair -keystore "$KEYSTORE" -alias vetta -keyalg RSA -keysize 2048 \
    -validity 10000 -storepass android -keypass android -dname "CN=Vetta Studio"
fi
if command -v zipalign >/dev/null; then
  zipalign -f 4 build/unsigned.apk build/aligned.apk
else
  cp build/unsigned.apk build/aligned.apk
fi
apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
  --out VettaStudio.apk build/aligned.apk
apksigner verify VettaStudio.apk
echo "Built $(pwd)/VettaStudio.apk"
