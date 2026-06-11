#!/usr/bin/env bash
# macOS 发布打包：签名、公证，并用 ditto 生成可分发的 zip（保留签名与扩展属性）
set -euo pipefail

APP="${1:-RemotePro.app}"
ZIP_OUT="${2:-RemotePro.zip}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENTITLEMENTS="${SCRIPT_DIR}/entitlements.plist"

if [[ ! -d "$APP" ]]; then
  echo "❌ 未找到 app bundle: $APP"
  exit 1
fi

echo "📦 打包目标: $APP -> $ZIP_OUT"

xattr -cr "$APP"

resolve_signing_identity() {
  if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
    echo "$MACOS_SIGNING_IDENTITY"
    return
  fi
  local identity
  identity="$(security find-identity -v -p codesigning | awk -F'"' '/Developer ID Application:/{print $2; exit}')"
  if [[ -n "$identity" ]]; then
    echo "$identity"
    return
  fi
  security find-identity -v -p codesigning | awk -F'"' '/Apple Development:/{print $2; exit}'
}

is_distribution_identity() {
  [[ "$1" == *"Developer ID Application"* ]]
}

sign_app() {
  local identity="$1"
  echo "🔏 使用证书签名: $identity"

  local -a sign_args=(
    --force
    --timestamp
    --entitlements "$ENTITLEMENTS"
    --sign "$identity"
  )

  if is_distribution_identity "$identity"; then
    sign_args+=(--options runtime)
  fi

  # 从内到外签名：dylib → framework/bundle → 主程序 → .app
  while IFS= read -r -d '' dylib; do
    codesign "${sign_args[@]}" "$dylib"
  done < <(find "$APP" -type f -name "*.dylib" -print0)

  while IFS= read -r -d '' bundle; do
    codesign "${sign_args[@]}" "$bundle"
  done < <(find "$APP" \( -name "*.framework" -o -name "*.plugin" \) -print0)

  codesign "${sign_args[@]}" "$APP/Contents/MacOS/$(basename "$APP" .app)"
  codesign "${sign_args[@]}" "$APP"
  codesign --verify --deep --strict --verbose=2 "$APP"
  echo "✅ 签名验证通过"
}

notarize_zip() {
  local zip_path="$1"
  echo "📤 提交 Apple 公证: $zip_path"

  local -a notary_args=(submit "$zip_path" --wait)
  if [[ -n "${APPLE_NOTARY_KEYCHAIN_PROFILE:-}" ]]; then
    notary_args+=(--keychain-profile "$APPLE_NOTARY_KEYCHAIN_PROFILE")
  else
    [[ -n "${APPLE_ID:-}" ]] || { echo "❌ 公证需要 APPLE_ID 或 APPLE_NOTARY_KEYCHAIN_PROFILE"; exit 1; }
    [[ -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]] || { echo "❌ 公证需要 APPLE_APP_SPECIFIC_PASSWORD"; exit 1; }
    [[ -n "${APPLE_TEAM_ID:-}" ]] || { echo "❌ 公证需要 APPLE_TEAM_ID"; exit 1; }
    notary_args+=(--apple-id "$APPLE_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD" --team-id "$APPLE_TEAM_ID")
  fi

  xcrun notarytool "${notary_args[@]}"
  xcrun stapler staple "$APP"
  echo "✅ 公证完成并已 staple 到 app"
}

create_zip() {
  local zip_path="$1"
  rm -f "$zip_path"
  ditto -c -k --sequesterRsrc --keepParent "$APP" "$zip_path"
  echo "✅ 已生成: $zip_path ($(du -h "$zip_path" | awk '{print $1}'))"
}

IDENTITY="$(resolve_signing_identity || true)"
if [[ -n "$IDENTITY" ]]; then
  sign_app "$IDENTITY"

  if is_distribution_identity "$IDENTITY" && \
     { [[ -n "${APPLE_NOTARY_KEYCHAIN_PROFILE:-}" ]] || \
       { [[ -n "${APPLE_ID:-}" ]] && [[ -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]] && [[ -n "${APPLE_TEAM_ID:-}" ]]; }; }; then
    NOTARIZE_TMP="$(mktemp -t RemotePro-notarize.XXXXXX.zip)"
    trap 'rm -f "$NOTARIZE_TMP"' EXIT
    create_zip "$NOTARIZE_TMP"
    notarize_zip "$NOTARIZE_TMP"
    create_zip "$ZIP_OUT"
  else
    if is_distribution_identity "$IDENTITY"; then
      echo "⚠️  未配置公证凭据，跳过公证（仅签名）。"
    else
      echo "ℹ️  使用 Apple Development 证书（与 iOS 相同），跳过公证。"
    fi
    create_zip "$ZIP_OUT"
  fi
else
  echo "⚠️  未找到签名证书，跳过签名。"
  echo "    通过 fastlane 导入 certs/development.p12，或设置 MACOS_SIGNING_IDENTITY。"
  create_zip "$ZIP_OUT"
fi
