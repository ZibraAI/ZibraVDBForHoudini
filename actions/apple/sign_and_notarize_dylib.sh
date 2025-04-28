#!/bin/bash

set -e
ORIGINAL_KEYCHAINS=$(security list-keychains | tr -d '"')
cleanup() {
    rm -f /tmp/developer_id_application.p12
    rm -f /tmp/notarization.zip
    security list-keychains -s $ORIGINAL_KEYCHAINS
    set +e
    security delete-keychain github-actions.keychain
}
trap cleanup EXIT

# SIGNING
# -------

# remove the old certificate if it exists
rm -f /tmp/developer_id_application.p12

# remove the old keychain if it exists
set +e
security delete-keychain github-actions.keychain > /dev/null
set -e

# create a new keychain
security create-keychain -p rgbfxVp2jvHmVXVJMhnHpeijjVEH44xv github-actions.keychain
security default-keychain -s github-actions.keychain
security unlock-keychain -p rgbfxVp2jvHmVXVJMhnHpeijjVEH44xv github-actions.keychain
security set-keychain-settings -t 3600 -u github-actions.keychain

# add new keychain to the search list
security list-keychains -s $ORIGINAL_KEYCHAINS github-actions.keychain

# download Apple Developer ID intermediate certificates
curl -o /tmp/intermediate.cer https://www.apple.com/certificateauthority/DeveloperIDCA.cer
curl -o /tmp/intermediate2.cer https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer

# import the intermediate certificates
security import /tmp/intermediate.cer -k github-actions.keychain
security import /tmp/intermediate2.cer -k github-actions.keychain

# unpack the codesigning certificate
echo $DEVELOPER_ID_APPLICATION_CERT | base64 --decode > /tmp/developer_id_application.p12

# import the codesigning certificate
security import /tmp/developer_id_application.p12 -k github-actions.keychain -P $DEVELOPER_ID_APPLICATION_PASS -T /usr/bin/codesign -T /usr/bin/productsign

# delete the certificate from disk
rm -f /tmp/developer_id_application.p12

# set the keychain to be unlocked
security set-key-partition-list -S apple-tool:,apple: -k rgbfxVp2jvHmVXVJMhnHpeijjVEH44xv github-actions.keychain

# sign file
codesign --deep --force --verify --verbose --timestamp --options runtime --sign "$DEVELOPER_ID_APPLICATION_NAME" "$1"
codesign -v "$1" --verbose

# reset default search list
security list-keychains -s $ORIGINAL_KEYCHAINS

# delete keychain
security delete-keychain github-actions.keychain

# NOTARIZATION
# ------------

# check if the file is already notarized
set +e
xcrun stapler staple $1 
if [ $? -eq 0 ]; then
    echo "File is already notarized."
    exit 0
fi
set -e

# remove archite if it already exists
rm -f /tmp/notarization.zip
chmod 755 "$1"
ditto -c -k --sequesterRsrc --keepParent "$1" /tmp/notarization.zip
xcrun notarytool submit /tmp/notarization.zip --apple-id $NOTARIZATION_USER --password $NOTARIZATION_PASS --team-id $APPLE_DEVELOPER_TEAM --wait
# here we would staple the notarization ticket to the file
# unfortunately, you can not staple the notarization ticket to .dylib files
# meaning that you have to be online for first load of the .dylib
# unless it is bundled with an app that is notarized and with ticked stapled
#xcrun stapler staple "$1"
rm -f /tmp/notarization.zip
